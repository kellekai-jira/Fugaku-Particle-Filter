#include <map>
#include <string>
#include <cstdlib>
#include <cassert>
#include <memory>
#include <csignal>
#include <cstring>

#include <mpi.h>
#include "zmq.h"

#include "messages.h"
#include "Part.h"

#include "utils.h"

#include "melissa_utils.h"

#include "melissa-da_config.h"
#include "ApiTiming.h"
#include "melissa_api.h"

// TODO ensure sizeof(size_t is the same on server and api... also for other types?? but the asserts are doing this already at the beginning as we receive exactly 2 ints....
// Forward declarations:
void melissa_finalize();

// zmq context:
void *context;

int runner_id = -1;

int getRunnerId() {
    assert(runner_id != -1);      // must be inited
    return runner_id;
}

#ifdef REPORT_TIMING
std::unique_ptr<ApiTiming> timing(nullptr);
#endif

/// Communicator used for simulation/Runner
MPI_Comm comm;


/// if node name = my nodename, replace by localhost!
std::string fix_port_name(const char * port_name_)
{
    std::string port_name(port_name_);
    char my_host_name[MPI_MAX_PROCESSOR_NAME];
    melissa_get_node_name(my_host_name, MPI_MAX_PROCESSOR_NAME);
    size_t found = port_name.find(my_host_name);
    // check if found and if hostname is between tcp://<nodename>:port
    if (found != std::string::npos && port_name[found-1] == '/' &&
        port_name[found + strlen(my_host_name)] == ':')
    {
        port_name = port_name.substr(0, found) + "127.0.0.1" +
                    port_name.substr(found + strlen(my_host_name));
    }
    return port_name;
}

/**
 * Returns simulation's rank
 * is the same as the runner rank.
 */
int getCommRank()
{
    int rank;
    MPI_Comm_rank(comm, &rank);
    return rank;
}

/**
 * Returns Simulations comm size
 * is the same as the runner comm size.
 */
int getCommSize()
{
    int size;
    MPI_Comm_size(comm, &size);
    return size;
}


// One of these exists to abstract the connection to every server rank that needs to be connected with this model task runner rank
struct ServerRankConnection
{
    void * data_request_socket;

    ServerRankConnection(const char * addr_request)
    {
        data_request_socket = zmq_socket (context, ZMQ_REQ);
        assert(data_request_socket);
        std::string cleaned_addr = fix_port_name(addr_request);
        D("Data Request Connection to %s", cleaned_addr.c_str());
        // ZMQ_CHECK();
        int ret = zmq_connect (data_request_socket,
                               cleaned_addr.c_str());
        assert(ret == 0);

        D("connect socket %p", data_request_socket);
    }

    ~ServerRankConnection()
    {
        D("closing socket %p", data_request_socket);
        zmq_close(data_request_socket);
    }

    void send(double * values, const size_t doubles_to_send,
              double * values_hidden, const size_t doubles_to_send_hidden,
              const int
              current_state_id, const int current_step, const
              char * field_name)
    {
        // send simuid, rank, stateid, current_step, field_name next message: doubles
        zmq_msg_t msg_header, msg_data, msg_data_hidden;
        zmq_msg_init_size(&msg_header, 4 * sizeof(int) +
                          MPI_MAX_PROCESSOR_NAME * sizeof(char));
        int * header = reinterpret_cast<int*>(zmq_msg_data(
                                                  &msg_header));
        header[0] = getRunnerId();
        header[1] = getCommRank();
        header[2] = current_state_id;
        header[3] = current_step;         // is incremented on the server side
        strcpy(reinterpret_cast<char*>(&header[4]), field_name);
        D("sending on socket %p", data_request_socket);
        ZMQ_CHECK(zmq_msg_send(&msg_header, data_request_socket,
                               ZMQ_SNDMORE));

        D(
            "-> Simulation runnerid %d, rank %d sending stateid %d current_step=%d fieldname=%s, %lu+%lu hidden bytes",
            getRunnerId(), getCommRank(), current_state_id,
            current_step,
            field_name, doubles_to_send * sizeof(double),
            doubles_to_send_hidden * sizeof(double));
        D("values[0]  = %.3f", values[0]);
        D("values[1]  = %.3f", values[1]);
        D("values[2]  = %.3f", values[2]);
        // D("hidden values[0]  = %.3f", values_hidden[0]);
        // D("hidden values[1]  = %.3f", values_hidden[1]);
        // D("hidden values[2]  = %.3f", values_hidden[2]);
        // D("values[35] = %.3f", values[35]);
        zmq_msg_init_data(&msg_data, values, doubles_to_send *
                          sizeof(double), NULL, NULL);
        int flag;
        if (doubles_to_send_hidden > 0)
        {
            flag = ZMQ_SNDMORE;
        }
        else
        {
            flag = 0;
        }

        ZMQ_CHECK(zmq_msg_send(&msg_data, data_request_socket, flag));

        if (doubles_to_send_hidden > 0)
        {
            zmq_msg_init_data(&msg_data_hidden, values_hidden,
                              doubles_to_send_hidden *
                              sizeof(double), NULL, NULL);
            ZMQ_CHECK(zmq_msg_send(&msg_data_hidden, data_request_socket, 0));
        }
    }

    int receive(double * out_values, size_t doubles_expected,
                double * out_values_hidden, size_t doubles_expected_hidden,
                int * out_current_state_id, int *out_current_step)
    {
// receive a first message that is 1 if we want to change the state, otherwise 0 or 2 if we want to quit.
// the first message also contains out_current_state_id and out_current_step
// the 2nd message just consists of doubles that will be put into out_values
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        ZMQ_CHECK(zmq_msg_recv(&msg, data_request_socket, 0));
        D("Received message size = %lu", zmq_msg_size(&msg));
        assert(zmq_msg_size(&msg) == 4 * sizeof(int));
        int * buf = reinterpret_cast<int*>(zmq_msg_data(&msg));
        *out_current_state_id = buf[0];
        *out_current_step = buf[1];
        int type = buf[2];
        int nsteps = buf[3];
        zmq_msg_close(&msg);

        if (type == CHANGE_STATE)
        {
            assert_more_zmq_messages(data_request_socket);

            // zero copy is for sending only!
            zmq_msg_init(&msg);

            ZMQ_CHECK(zmq_msg_recv(&msg, data_request_socket, 0));

            D(
                "<- Simulation got %lu bytes, expected %lu + %lu hidden bytes... for state %d, current_step=%d, nsteps=%d (socket=%p)",
                zmq_msg_size(&msg), doubles_expected *
                sizeof(double),
                doubles_expected_hidden * sizeof(double),
                *out_current_state_id, *out_current_step, nsteps,
                data_request_socket);

            assert(zmq_msg_size(&msg) == doubles_expected *
                   sizeof(double));

            double * buf = reinterpret_cast<double*>(zmq_msg_data(
                                                         &msg));
            std::copy(buf, buf + doubles_expected, out_values);

            // print_vector(std::vector<double>(out_values,
            //                                 out_values +
            //                                 doubles_expected));
            zmq_msg_close(&msg);

            if (doubles_expected_hidden > 0)
            {
                assert_more_zmq_messages(data_request_socket);

                // zero copy is for sending only!
                zmq_msg_init(&msg);

                ZMQ_CHECK(zmq_msg_recv(&msg, data_request_socket, 0));
                assert(zmq_msg_size(&msg) == doubles_expected_hidden *
                       sizeof(double));

                buf = reinterpret_cast<double*>(zmq_msg_data(
                                                    &msg));
                std::copy(buf, buf + doubles_expected_hidden,
                          out_values_hidden);

                // print_vector(std::vector<double>(out_values_hidden,
                // out_values_hidden +
                // doubles_expected_hidden));
                zmq_msg_close(&msg);  // TODO; should work all with the same message!
            }

            assert_no_more_zmq_messages(data_request_socket);
        }
        else if (type == END_RUNNER)
        {         // TODO use zmq cpp for less errors!
                  // TODO: is this really an error?
            printf(
                "Error: Server decided to end this runner now.\n");
            melissa_finalize();
            // calculate 0 steps now.
            nsteps = 0;
        }
        else if (type == KILL_RUNNER)
        {
            printf(
                "Error: Server decided that this Runner crashed. So killing it now.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            exit(1);
        }
        else
        {
            assert(type == KEEP_STATE);
            // TODO: unimplemented
            assert(false);
        }
        return nsteps;
    }
};



struct Server
{
    int comm_size = 0;
    std::vector<char> port_names;
};
Server server;

struct ServerRanks
{
    static std::map<int, std::unique_ptr<ServerRankConnection> > ranks;

    static ServerRankConnection &get(int server_rank)
    {
        auto found = ranks.find(server_rank);
        if (found == ranks.end())
        {
            // connect to this server rank
            // we use unique_ptr's as other wise we would create a ServerRank locally, we than would copy all its values in the ranks map
            // and then we would destroy it. unfortunately this also closes the zmq connection !
            auto res = ranks.emplace(server_rank,
                                     std::unique_ptr<
                                         ServerRankConnection>(
                                         new
                                         ServerRankConnection(
                                             server
                                             .port_names.
                                             data() +
                                             server_rank *
                                             MPI_MAX_PROCESSOR_NAME)));
            return *res.first->second;
        }
        else
        {
            return *(found->second);
        }
    }
};

std::map<int, std::unique_ptr<ServerRankConnection> > ServerRanks::ranks;

struct ConnectedServerRank
{
    size_t send_count;
    size_t local_vector_offset;

    size_t send_count_hidden;
    size_t local_vector_offset_hidden;

    ServerRankConnection &server_rank;
};

struct Field
{
    std::string name;
    int current_state_id;
    int current_step;
    size_t local_vect_size;
    size_t local_hidden_vect_size;
    std::vector<ConnectedServerRank> connected_server_ranks;
    void initConnections(const std::vector<size_t> &local_vect_sizes, const
                         std::vector<size_t> &local_hidden_vect_sizes) {
        std::vector<Part> parts = calculate_n_to_m(server.comm_size,
                                                   local_vect_sizes);
        std::vector<Part> parts_hidden = calculate_n_to_m(server.comm_size,
                                                          local_hidden_vect_sizes);

        assert(parts_hidden.size() == 0 || parts_hidden.size() == parts.size());
        // for this to hold true the hidden state should be at least as big as
        // server.comm_size

        auto hidden_part = parts_hidden.begin();
        for (auto part=parts.begin(); part != parts.end(); ++part)
        {

            if (part->rank_runner == getCommRank())
            {
                size_t hidden_sendcount = 0;
                size_t hidden_local_offest_runner = 0;
                if (parts_hidden.size() > 0)
                {
                    // assume the same parts (just different sizes) exist for the hidden state
                    assert(hidden_part->rank_runner == getCommRank());  // Same part...

                    hidden_sendcount = hidden_part->send_count;
                    hidden_local_offest_runner =
                        hidden_part->local_offset_runner;
                }

                connected_server_ranks.push_back(
                    {part->send_count,
                     part->local_offset_runner,
                     hidden_sendcount,
                     hidden_local_offest_runner,
                     ServerRanks::get(part->rank_server)});
            }
            if (parts_hidden.size() > 0)    // doing the hidden parts thing...
            {
                hidden_part++;
            }
        }
    }

    // TODO: this will crash if there are more than two fields? maybe use dealer socket that supports send send recv recv scheme.
    void putState(double * values, double * hidden_values, const
                  char * field_name) {
        // send every state part to the right server rank
        for (auto csr = connected_server_ranks.begin(); csr !=
             connected_server_ranks.end(); ++csr)
        {
            D("put state, local offset: %lu, send count: %lu",
              csr->local_vector_offset, csr->send_count);
            csr->server_rank.send(&values[csr->local_vector_offset],
                                  csr->send_count,
                                  &hidden_values[csr->local_vector_offset_hidden
                                  ],
                                  csr->send_count_hidden, current_state_id,
                                  current_step,
                                  field_name);
        }

        current_state_id = -1;

    }

    int getState(double * values, double * values_hidden) {
        int nsteps = -1;
        // TODO: an optimization would be to poll instead of receiving directly. this way we receive first whoever comes first. but as we need to synchronize after it probably does not matter a lot?
        for (auto csr = connected_server_ranks.begin(); csr !=
             connected_server_ranks.end(); ++csr)
        {
            // receive state parts from every serverrank.
            D("get state, local offset: %lu, send count: %lu",
              csr->local_vector_offset, csr->send_count);

            // do not try to receive if we are finalizeing already. Even check if the last receive might have started finalization.
            if (phase == PHASE_FINAL)
            {
                return 0;
            }

            int nnsteps = csr->server_rank.receive(
                &values[csr->local_vector_offset],
                csr->send_count,
                &values_hidden[csr->local_vector_offset_hidden],
                csr->send_count_hidden,
                &current_state_id, &current_step);
            assert (nsteps == -1 || nsteps == nnsteps);             // be sure that all send back the same nsteps...
            nsteps = nnsteps;
        }
        return nsteps;
    }
};


Field field;

// TODO: kill if no server response for a timeout...

struct ConfigurationConnection
{
    void * socket;
    ConfigurationConnection()
    {
        socket = zmq_socket (context, ZMQ_REQ);
        char * melissa_server_master_node = getenv(
            "MELISSA_SERVER_MASTER_NODE");
        if (melissa_server_master_node == nullptr)
        {
            L(
                "you must set the MELISSA_SERVER_MASTER_NODE environment variable before running!");
            assert(false);
        }
        std::string port_name = fix_port_name(
            melissa_server_master_node);
        D("Configuration Connection to %s", port_name.c_str());
        zmq_connect (socket, port_name.c_str());
    }

    /// returns true if field registering is requested by the server
    bool register_runner_id(Server * out_server)
    {
        zmq_msg_t msg_request, msg_reply;
        zmq_msg_init_size(&msg_request, sizeof(int));
        int * header = reinterpret_cast<int*>(zmq_msg_data(
                                                  &msg_request));
        header[0] = REGISTER_RUNNER_ID;
        ZMQ_CHECK(zmq_msg_send(&msg_request, socket, 0));

        zmq_msg_init(&msg_reply);
        zmq_msg_recv(&msg_reply, socket, 0);
        assert(zmq_msg_size(&msg_reply) == 3 * sizeof(int));
        int * buf = reinterpret_cast<int*>(zmq_msg_data(&msg_reply));

        runner_id = buf[0];
        bool request_register_field = (buf[1] != 0);

        L("Got runner_id %d. Registering field? %d", runner_id, buf[1]);

        out_server->comm_size = buf[2];
        zmq_msg_close(&msg_reply);

        size_t port_names_size = out_server->comm_size *
                                 MPI_MAX_PROCESSOR_NAME * sizeof(char);

        assert_more_zmq_messages(socket);
        zmq_msg_init(&msg_reply);
        zmq_msg_recv(&msg_reply, socket, 0);

        assert(zmq_msg_size(&msg_reply) == port_names_size);

        out_server->port_names.resize(port_names_size);

        copy(reinterpret_cast<char*>(zmq_msg_data(&msg_reply)),
             reinterpret_cast<char*>(zmq_msg_data(&msg_reply)) +
             port_names_size, out_server->port_names.begin());

        zmq_msg_close(&msg_reply);

        return request_register_field;
    }

    // TODO: high water mark and so on?
    void register_field(const char * field_name, size_t local_vect_sizes[],
                        size_t local_hidden_vect_sizes[], std::vector<int>& global_index_map,
                        std::vector<int>& global_index_map_hidden)
    {
        zmq_msg_t msg_header, msg_local_vect_sizes, msg_local_hidden_vect_sizes,
                  msg_index_map, msg_index_map_hidden,
                  msg_reply;
        zmq_msg_init_size(&msg_header, sizeof(int) + sizeof(int) +
                          MPI_MAX_PROCESSOR_NAME * sizeof(char) );
        int * header = reinterpret_cast<int*>(zmq_msg_data(
                                                  &msg_header));
        int type = REGISTER_FIELD;
        header[0] = type;
        header[1] = getCommSize();
        strcpy(reinterpret_cast<char*>(&header[2]), field_name);
        ZMQ_CHECK(zmq_msg_send(&msg_header, socket, ZMQ_SNDMORE));

        zmq_msg_init_data(&msg_local_vect_sizes, local_vect_sizes,
                          getCommSize() * sizeof(size_t), NULL, NULL);
        ZMQ_CHECK(zmq_msg_send(&msg_local_vect_sizes, socket, ZMQ_SNDMORE));

        zmq_msg_init_data(&msg_local_hidden_vect_sizes, local_hidden_vect_sizes,
                          getCommSize() * sizeof(size_t), NULL, NULL);
        ZMQ_CHECK(zmq_msg_send(&msg_local_hidden_vect_sizes, socket, ZMQ_SNDMORE));

        zmq_msg_init_data(&msg_index_map, global_index_map.data(),
                          global_index_map.size() * sizeof(int), NULL, NULL);
        ZMQ_CHECK(zmq_msg_send(&msg_index_map, socket, ZMQ_SNDMORE));

        zmq_msg_init_data(&msg_index_map_hidden, global_index_map_hidden.data(),
                          global_index_map_hidden.size() * sizeof(int), NULL, NULL);
        ZMQ_CHECK(zmq_msg_send(&msg_index_map_hidden, socket, 0));

        zmq_msg_init(&msg_reply);
        zmq_msg_recv(&msg_reply, socket, 0);
        // ack
        assert(zmq_msg_size(&msg_reply) == 0);
        zmq_msg_close(&msg_reply);
    }


    ~ConfigurationConnection() {
        zmq_close(socket);
    }


};
ConfigurationConnection *ccon = NULL;

/// returns true if needs field to be registered this can only happen on comm_rank 0
bool first_melissa_init(MPI_Comm comm_)
{
    check_data_types();

    context = zmq_ctx_new ();
    comm = comm_;

    // activate logging:
    comm_rank = getCommRank();

#ifdef REPORT_TIMING
    // Start Timing:
#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
        timing = std::make_unique<ApiTiming>();
    }
    trigger(START_ITERATION, 1);
#endif

    bool register_field = false;
    if (getCommRank() == 0)
    {
        ccon = new ConfigurationConnection();
        register_field = ccon->register_runner_id(&server);
    }

    MPI_Bcast(&runner_id, 1, MPI_INT, 0, comm);

    MPI_Bcast(&server.comm_size, 1, MPI_INT, 0, comm);
    if (getCommRank() != 0)
    {
        server.port_names.resize(server.comm_size *
                                 MPI_MAX_PROCESSOR_NAME);
    }
    D("port_names_size= %lu", server.port_names.size());
    MPI_Bcast(server.port_names.data(), server.comm_size *
              MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, comm);

    // D("Portnames %s , %s ", server.port_names.data(), server.port_names.data() + MPI_MAX_PROCESSOR_NAME);
    return register_field;
}

int melissa_get_current_state_id()
{
    assert(phase == PHASE_SIMULATION);
    return field.current_state_id;
}



void melissa_init(const char *field_name,
                  const int local_vect_size,
                  const int local_hidden_vect_size,
                  MPI_Comm comm_)
{
    melissa_init_with_index_map(field_name, local_vect_size, local_hidden_vect_size,
            comm_, nullptr, nullptr);
}

void melissa_init_with_index_map(const char *field_name,
                  const int local_vect_size,
                  const int local_hidden_vect_size,
                  MPI_Comm comm_,
                  const int local_index_map[],
                  const int local_index_map_hidden[]
                  )
{
    // TODO: field_name is actually unneeded. its only used to name the output files in the server side...

    bool register_field = first_melissa_init(comm_);

    // create field
    // newField.current_state_id = getSimuId(); // We are beginning like this...
    field.name = field_name;
    field.current_state_id = -1;     // We are beginning like this...
    field.current_step = 0;
    field.local_vect_size = local_vect_size;
    field.local_hidden_vect_size = local_hidden_vect_size;
    std::vector<size_t> local_vect_sizes(getCommSize());
    // synchronize local_vect_sizes and
    MPI_Allgather(&field.local_vect_size, 1, my_MPI_SIZE_T,
                  local_vect_sizes.data(), 1, my_MPI_SIZE_T,
                  comm);

    std::vector<size_t> local_hidden_vect_sizes(getCommSize());
    // synchronize local_hidden_vect_sizes and
    MPI_Allgather(&field.local_hidden_vect_size, 1, my_MPI_SIZE_T,
                  local_hidden_vect_sizes.data(), 1, my_MPI_SIZE_T,
                  comm);

    D("vect sizes: %lu %lu", local_vect_sizes[0], local_vect_sizes[1]);

    std::vector<int> global_index_map;
    std::vector<int> global_index_map_hidden;
    if (comm_rank == 0)
    {
        global_index_map.resize(sum_vec(local_vect_sizes));
        global_index_map_hidden.resize(sum_vec(local_hidden_vect_sizes));
    }

    if (local_index_map == nullptr)
    {
        int i = 0;
        for (auto &e: global_index_map) {
            e = i++;
        }
    }
    else
    {
        MPI_Gather(local_index_map, local_vect_size, MPI_INT,
               global_index_map.data(), global_index_map.size(), MPI_INT,
               0, comm_);
    }
    if (local_index_map_hidden == nullptr)
    {
        int i = 0;
        for (auto &e: global_index_map_hidden) {
            e = i++;
        }
    }
    else
    {
        MPI_Gather(local_index_map_hidden, local_vect_size, MPI_INT,
               global_index_map_hidden.data(), global_index_map_hidden.size(), MPI_INT,
               0, comm_);
    }
    if (register_field)
    {
        // Tell the server which kind of data he has to expect
        ccon->register_field(field_name, local_vect_sizes.data(),
                             local_hidden_vect_sizes.data(), global_index_map, global_index_map_hidden);
    }



    // Calculate to which server ports the local part of the field will connect
    field.initConnections(local_vect_sizes, local_hidden_vect_sizes);


}
bool no_mpi = false;
// can be called from fortran or if no mpi is used (set NULL as the mpi communicator) TODO: check if null is not already used by something else!
void melissa_init_no_mpi(const char *field_name,
                         const int  *local_vect_size,
                         const int  *local_hidden_vect_size) {     // comm is casted into an pointer to an mpi communicaotr if not null.
    MPI_Init(NULL, NULL);      // TODO: maybe we also do not need to do this? what happens if we clear out this line?
    no_mpi = true;
    melissa_init(field_name, *local_vect_size, *local_hidden_vect_size,
                 MPI_COMM_WORLD);
}


/// returns 0 if simulation should end now.
/// otherwise returns nsteps, the number of timesteps that need to be simulated.
int melissa_expose(const char *field_name, double *values,
                   double *hidden_values)
{
    trigger(STOP_ITERATION, -1);
    trigger(START_IDLE_RUNNER, -1);
    assert(phase != PHASE_FINAL);
    if (phase == PHASE_INIT)
    {
        phase = PHASE_SIMULATION;
        // finalize initializations
        // First time in melissa_expose.
        // Now we are sure all fields are registered.
        // we do not need this anymore:
        delete ccon;
        ccon = NULL;
    }

    // Now Send data to the melissa server
    assert(field.name == field_name);

    field.putState(values, hidden_values, field_name);

    // @Kai: here we could checkpoint the values variable ! using fti. The server than could recover from such a checkpoint.

    // and request new data
    int nsteps = field.getState(values, hidden_values);
    trigger(STOP_IDLE_RUNNER, -1);

    if (nsteps > 0)
    {
        trigger(START_ITERATION, nsteps);
    }
    else
    {
#ifdef REPORT_TIMING
        if (comm_rank == 0)
        {
            timing->report(getCommSize(), field.local_vect_size +
                           field.local_hidden_vect_size);
        }
#endif
    }

    // TODO: this will block other fields!

    return nsteps;
}

void melissa_finalize()  // TODO: when using more serverranks, wait until an end message was received from every before really ending... or actually not. as we always have only an open connection to one server rank...
{
    // sleep(3);
    MPI_Barrier(comm);
    // sleep(3);
    D("End Runner.");
    D("server ranks: %lu", ServerRanks::ranks.size());

    phase = PHASE_FINAL;
    // TODO: free all pointers?
    // not an api function anymore but activated if a finalization message is received.
    if (ccon != NULL)
    {
        delete ccon;
    }


    // free all connections to server ranks:
    ServerRanks::ranks.clear();

    // sleep(3);
    D("Destroying zmq context");
    zmq_ctx_destroy(context);

#ifdef REPORT_TIMING
#ifdef REPORT_TIMING_ALL_RANKS
    const std::array<EventTypeTranslation, 2> event_type_translations = {{
        {START_ITERATION, STOP_ITERATION, "Iteration"},
        {START_IDLE_RUNNER, STOP_IDLE_RUNNER, "Runner idle"},
    }};
    std::string fn = "melissa_runner" + std::to_string(runner_id);
    timing->write_region_csv(event_type_translations, fn.c_str(), comm_rank);
#endif
#endif

    if (no_mpi)
    {
        // if without mpi we need to finalize mpi properly as it was only created in this context.
        MPI_Finalize();
    }
}

int melissa_get_current_step()
{
    assert(phase == PHASE_SIMULATION);
    return field.current_step;
}


void melissa_init_f(const char *field_name,
                    int        *local_vect_size,
                    int        *local_hidden_vect_size,
                    MPI_Fint   *comm_fortran)
{

    MPI_Comm comm = MPI_Comm_f2c(*comm_fortran);
    melissa_init(field_name, *local_vect_size, *local_hidden_vect_size, comm);
}
