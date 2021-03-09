#include "melissa_da_api.h"
#include "ApiTiming.h"
#include "Part.h"
#include "ZeroMQ.h"
#include "melissa_da_config.h"
#include "melissa_utils.h"
#include "messages.h"
#include "utils.h"

#include <cassert>
#include <csignal>
#include <cstdlib>
#include <cstring>

#include <map>
#include <memory>
#include <string>
#include <type_traits>

#include <mpi.h>

// TODO ensure sizeof(size_t is the same on server and api... also for other
// types?? but the asserts are doing this already at the beginning as we receive
// exactly 2 ints.... Forward declarations:
void melissa_finalize();

// zmq context:
void* context;

int runner_id = -1;

int getRunnerId() {
    assert(runner_id != -1); // must be inited
    return runner_id;
}


int last_step = -2;

#ifdef REPORT_TIMING
std::unique_ptr<ApiTiming> timing(nullptr);
#endif

/// Communicator used for simulation/Runner
MPI_Comm comm;


/// if node name = my nodename, replace by localhost!
std::string fix_port_name(const char* port_name_) {
    std::string port_name(port_name_);
    char my_host_name[MPI_MAX_PROCESSOR_NAME + 1] = {0};
    melissa_get_node_name(my_host_name, MPI_MAX_PROCESSOR_NAME);
    size_t found = port_name.find(my_host_name);
    // check if found and if hostname is between tcp://<nodename>:port
    if(found != std::string::npos && port_name[found - 1] == '/'
       && port_name[found + strlen(my_host_name)] == ':')
    {
        port_name = port_name.substr(0, found) + "127.0.0.1"
                    + port_name.substr(found + strlen(my_host_name));
    }
    return port_name;
}

/**
 * Returns simulation's rank
 * is the same as the runner rank.
 */
int getCommRank() {
    int rank;
    MPI_Comm_rank(comm, &rank);
    return rank;
}

/**
 * Returns Simulations comm size
 * is the same as the runner comm size.
 */
int getCommSize() {
    int size;
    MPI_Comm_size(comm, &size);
    return size;
}


// One of these exists to abstract the connection to every server rank that
// needs to be connected with this model task runner rank
struct ServerRankConnection
{
    void* data_request_socket;

    ServerRankConnection(const char* addr_request) {
        data_request_socket = zmq_socket(context, ZMQ_REQ);
        assert(data_request_socket);
        std::string cleaned_addr = fix_port_name(addr_request);
        D("Data Request Connection to %s", cleaned_addr.c_str());
        // ZMQ_CHECK();
        int ret = zmq_connect(data_request_socket, cleaned_addr.c_str());
        assert(ret == 0);

        D("connect socket %p", data_request_socket);
    }

    ~ServerRankConnection() {
        D("closing socket %p", data_request_socket);
        zmq_close(data_request_socket);
    }

    void send(
        VEC_T* values, const size_t bytes_to_send, VEC_T* values_hidden,
        const size_t bytes_to_send_hidden, const int current_state_id,
        const int current_step, const char* field_name) {
        // send simuid, rank, stateid, current_step, field_name next message:
        // doubles

        // current_step is incremented on the server side
        int header[] = {
            getRunnerId(), getCommRank(), current_state_id, current_step};
        auto msg_header =
            zmq::msg_init(sizeof(header) + MPI_MAX_PROCESSOR_NAME);

        std::memset(zmq::data(*msg_header), 0, zmq::size(*msg_header));
        std::memcpy(zmq::data(*msg_header), header, sizeof(header));
        std::strncpy(
            zmq::data(*msg_header) + sizeof(header),
            field_name, MPI_MAX_PROCESSOR_NAME);

        D("sending on socket %p", data_request_socket);

        zmq::send(*msg_header, data_request_socket, ZMQ_SNDMORE);

        D("-> Simulation runnerid %d, rank %d sending stateid %d "
          "current_step=%d fieldname=%s, %lu+%lu hidden bytes",
          getRunnerId(), getCommRank(), current_state_id, current_step,
          field_name, bytes_to_send, bytes_to_send_hidden);
        // D("values[0]  = %.3f", values[0]);
        // D("values[1]  = %.3f", values[1]);
        // D("values[2]  = %.3f", values[2]);
        // D("hidden values[0]  = %.3f", values_hidden[0]);
        // D("hidden values[1]  = %.3f", values_hidden[1]);
        // D("hidden values[2]  = %.3f", values_hidden[2]);
        // D("values[35] = %.3f", values[35]);
        int flag = (bytes_to_send_hidden > 0) ? ZMQ_SNDMORE : 0;

        // required for zmq::send_n in combination with a number of bytes
        // (instead of an element count)
        static_assert(std::is_same<VEC_T, char>::value, "");

        zmq::send_n(data_request_socket, values, bytes_to_send, flag);

        if(bytes_to_send_hidden > 0)
        {
            zmq::send_n(
                data_request_socket, values_hidden, bytes_to_send_hidden);
        }
    }

    int receive(
        VEC_T* out_values, size_t bytes_expected, VEC_T* out_values_hidden,
        size_t bytes_expected_hidden, int* out_current_state_id,
        int* out_current_step) {
        // receive a first message that is 1 if we want to change the state,
        // otherwise 0 or 2 if we want to quit. the first message also contains
        // out_current_state_id and out_current_step the 2nd message just
        // consists of bytes that will be put into out_values

        auto msg = zmq::recv(data_request_socket);
        D("Received message size = %lu", zmq::size(*msg));
        assert(zmq::size(*msg) == 4 * sizeof(int));

        int state[4] = {0};

        std::memcpy(state, zmq::data(*msg), sizeof(state));

        *out_current_state_id = state[0];
        *out_current_step = state[1];
        int type = state[2];
        int nsteps = state[3];

        if(type == CHANGE_STATE)
        {
            assert_more_zmq_messages(data_request_socket);

            // zero copy is for sending only!
            msg = zmq::recv(data_request_socket);

            D("<- Simulation got %lu bytes, expected %lu + %lu hidden bytes... "
              "for state %d, current_step=%d, nsteps=%d (socket=%p)",
              zmq::size(*msg), bytes_expected, bytes_expected_hidden,
              *out_current_state_id, *out_current_step, nsteps,
              data_request_socket);

            assert(zmq::size(*msg) == bytes_expected);

            std::memcpy(out_values, zmq::data(*msg), bytes_expected);

            // print_vector(std::vector<double>(out_values,
            //                                 out_values +
            //                                 doubles_expected));

            if(bytes_expected_hidden > 0)
            {
                assert_more_zmq_messages(data_request_socket);

                // zero copy is for sending only!
                msg = zmq::recv(data_request_socket);

                assert(zmq::size(*msg) == bytes_expected_hidden);
                std::memcpy(out_values_hidden, zmq::data(*msg),
                            bytes_expected_hidden);

                // print_vector(std::vector<double>(out_values_hidden,
                // out_values_hidden +
                // doubles_expected_hidden));
            }

            assert_no_more_zmq_messages(data_request_socket);
        }
        else if(type == END_RUNNER)
        {
            // TODO: is this really an error?
            printf("Error: Server decided to end this runner now.\n");
            melissa_finalize();
            // calculate 0 steps now.
            nsteps = 0;
        }
        else if(type == KILL_RUNNER)
        {
            printf("Error: Server decided that this Runner crashed. So killing "
                   "it now.\n");
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

    static ServerRankConnection& get(int server_rank) {
        auto found = ranks.find(server_rank);
        if(found == ranks.end())
        {
            // connect to this server rank
            // we use unique_ptr's as other wise we would create a ServerRank
            // locally, we than would copy all its values in the ranks map and
            // then we would destroy it. unfortunately this also closes the zmq
            // connection !
            auto res = ranks.emplace(
                server_rank,
                std::unique_ptr<ServerRankConnection>(new ServerRankConnection(
                                                          server.port_names.data()
                                                          + server_rank *
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

    ServerRankConnection& server_rank;
};

struct Field
{
    std::string name;
    int current_state_id;
    int current_step;
    size_t local_vect_size;
    size_t local_hidden_vect_size;
    std::vector<ConnectedServerRank> connected_server_ranks;
    void initConnections(
        const std::vector<size_t>& local_vect_sizes,
        const std::vector<size_t>& local_hidden_vect_sizes,
        const int bytes_per_element, const int bytes_per_element_hidden) {
        std::vector<Part> parts = calculate_n_to_m(
            server.comm_size, local_vect_sizes, bytes_per_element);
        std::vector<Part> parts_hidden = calculate_n_to_m(
            server.comm_size, local_hidden_vect_sizes,
            bytes_per_element_hidden);

        assert(parts_hidden.size() == 0 || parts_hidden.size() == parts.size());
        // for this to hold true the hidden state should be at least as big as
        // server.comm_size

        auto hidden_part = parts_hidden.begin();
        for(auto part = parts.begin(); part != parts.end(); ++part)
        {

            if(part->rank_runner == getCommRank())
            {
                size_t hidden_sendcount = 0;
                size_t hidden_local_offest_runner = 0;
                if(parts_hidden.size() > 0)
                {
                    // assume the same parts (just different sizes) exist for
                    // the hidden state
                    assert(
                        hidden_part->rank_runner == getCommRank()); // Same
                                                                    // part...

                    hidden_sendcount = hidden_part->send_count;
                    hidden_local_offest_runner =
                        hidden_part->local_offset_runner;
                }

                connected_server_ranks.push_back(
                    {part->send_count, part->local_offset_runner,
                     hidden_sendcount, hidden_local_offest_runner,
                     ServerRanks::get(part->rank_server)});
            }
            if(parts_hidden.size() > 0) // doing the hidden parts thing...
            {
                hidden_part++;
            }
        }
    }

    // TODO: change MPI_INT to size_t where possible, also in server.cxx
    // TODO: this will crash if there are more than two fields? maybe use dealer
    // socket that supports send send recv recv scheme.
    void putState(VEC_T* values, VEC_T* hidden_values, const char* field_name) {
        // send every state part to the right server rank
        for(auto csr = connected_server_ranks.begin();
            csr != connected_server_ranks.end(); ++csr)
        {
            D("put state, local offset: %lu, send count: %lu",
              csr->local_vector_offset, csr->send_count);
            csr->server_rank.send(
                &values[csr->local_vector_offset], csr->send_count,
                &hidden_values[csr->local_vector_offset_hidden],
                csr->send_count_hidden, current_state_id, current_step,
                field_name);
        }

        current_state_id = -1;
    }

    int getState(VEC_T* values, VEC_T* values_hidden) {
        int nsteps = -1;
        // TODO: an optimization would be to poll instead of receiving directly.
        // this way we receive first whoever comes first. but as we need to
        // synchronize after it probably does not matter a lot?
        for(auto csr = connected_server_ranks.begin();
            csr != connected_server_ranks.end(); ++csr)
        {
            // receive state parts from every serverrank.
            D("get state, local offset: %lu, send count: %lu",
              csr->local_vector_offset, csr->send_count);

            // do not try to receive if we are finalizeing already. Even check
            // if the last receive might have started finalization.
            if(phase == PHASE_FINAL)
            {
                return 0;
            }

            int nnsteps = csr->server_rank.receive(
                &values[csr->local_vector_offset], csr->send_count,
                &values_hidden[csr->local_vector_offset_hidden],
                csr->send_count_hidden, &current_state_id, &current_step);
            assert(nsteps == -1 || nsteps == nnsteps); // be sure that all send
                                                       // back the same
                                                       // nsteps...
            nsteps = nnsteps;
        }
        return nsteps;
    }
};


Field field;

// TODO: kill if no server response for a timeout...

struct ConfigurationConnection
{
    void* socket;
    ConfigurationConnection() {
        socket = zmq_socket(context, ZMQ_REQ);
        char* melissa_server_master_node = getenv("MELISSA_SERVER_MASTER_NODE");
        if(melissa_server_master_node == nullptr)
        {
            L("you must set the MELISSA_SERVER_MASTER_NODE environment "
              "variable before running!");
            assert(false);
        }
        std::string port_name = fix_port_name(melissa_server_master_node);
        D("Configuration Connection to %s", port_name.c_str());
        zmq_connect(socket, port_name.c_str());
    }

    /// returns true if field registering is requested by the server
    bool register_runner_id(Server* out_server) {
        assert(std::getenv("MELISSA_DA_RUNNER_ID"));

        runner_id = atoi(getenv("MELISSA_DA_RUNNER_ID"));
        int header[] = {REGISTER_RUNNER_ID, runner_id};

        zmq::send_n(socket, header, 2);

        L("registering runner_id %d at server", runner_id);

        auto msg_reply = zmq::recv(socket);

        assert(zmq::size(*msg_reply) == 2 * sizeof(int));

        int reply[2] = {0};

        std::memcpy(reply, zmq::data(*msg_reply), sizeof(reply));

        bool request_register_field = reply[0] != 0;

        L("Registering field? %d", reply[0]);

        out_server->comm_size = reply[1];

        size_t port_names_size =
            out_server->comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(char);

        assert_more_zmq_messages(socket);
        msg_reply = zmq::recv(socket);

        assert(zmq::size(*msg_reply) == port_names_size);

        out_server->port_names.resize(zmq::size(*msg_reply));

        std::memcpy(
            out_server->port_names.data(),
            zmq::data(*msg_reply), zmq::size(*msg_reply));

        return request_register_field;
    }

    // TODO: high water mark and so on?
    void register_field(
        const char* field_name, size_t local_vect_sizes[],
        size_t local_hidden_vect_sizes[],
        std::vector<INDEX_MAP_T>& global_index_map,
        std::vector<INDEX_MAP_T>& global_index_map_hidden,
        const int bytes_per_element, const int bytes_per_element_hidden) {
        zmq_msg_t msg_header, msg_local_vect_sizes, msg_local_hidden_vect_sizes,
                  msg_index_map, msg_index_map_hidden, msg_reply;
        zmq_msg_init_size(
            &msg_header,
            sizeof(int) + sizeof(int) + sizeof(int) + sizeof(int)
            + MPI_MAX_PROCESSOR_NAME * sizeof(char));
        int* header = reinterpret_cast<int*>(zmq_msg_data(&msg_header));
        int type = REGISTER_FIELD;
        header[0] = type;
        header[1] = getCommSize();
        header[2] = bytes_per_element;
        header[3] = bytes_per_element_hidden;
        strncpy(
            reinterpret_cast<char*>(&header[4]), field_name,
            MPI_MAX_PROCESSOR_NAME);
        ZMQ_CHECK(zmq_msg_send(&msg_header, socket, ZMQ_SNDMORE));
        zmq_msg_close(&msg_header);

        zmq_msg_init_data(
            &msg_local_vect_sizes, local_vect_sizes,
            getCommSize() * sizeof(size_t), NULL, NULL);
        ZMQ_CHECK(zmq_msg_send(&msg_local_vect_sizes, socket, ZMQ_SNDMORE));
        zmq_msg_close(&msg_local_vect_sizes);

        zmq_msg_init_data(
            &msg_local_hidden_vect_sizes, local_hidden_vect_sizes,
            getCommSize() * sizeof(size_t), NULL, NULL);
        ZMQ_CHECK(
            zmq_msg_send(&msg_local_hidden_vect_sizes, socket, ZMQ_SNDMORE));
        zmq_msg_close(&msg_local_hidden_vect_sizes);

        zmq_msg_init_data(
            &msg_index_map, global_index_map.data(),
            global_index_map.size() * sizeof(INDEX_MAP_T), NULL, NULL);
        ZMQ_CHECK(zmq_msg_send(&msg_index_map, socket, ZMQ_SNDMORE));
        zmq_msg_close(&msg_index_map);

        zmq_msg_init_data(
            &msg_index_map_hidden, global_index_map_hidden.data(),
            global_index_map_hidden.size() * sizeof(INDEX_MAP_T), NULL, NULL);
        ZMQ_CHECK(zmq_msg_send(&msg_index_map_hidden, socket, 0));
        zmq_msg_close(&msg_index_map_hidden);

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
ConfigurationConnection* ccon = NULL;

/// returns true if needs field to be registered on the server. This can only
/// happen on comm_rank 0
bool first_melissa_init(MPI_Comm comm_) {
    init_utils();

    context = zmq_ctx_new();
    comm = comm_;

    // activate logging:
    comm_rank = getCommRank();

    // for convenience
    comm_size = getCommSize();

#ifdef REPORT_TIMING
    // Start Timing:
#ifndef REPORT_TIMING_ALL_RANKS
    if(comm_rank == 0)
#endif
    {
        timing = std::make_unique<ApiTiming>();
    }
    trigger(START_ITERATION, last_step);
#endif

    bool register_field = false;
    if(getCommRank() == 0)
    {
        ccon = new ConfigurationConnection();
        register_field = ccon->register_runner_id(&server);
    }

    MPI_Bcast(&runner_id, 1, MPI_INT, 0, comm);

    MPI_Bcast(&server.comm_size, 1, MPI_INT, 0, comm);
    if(getCommRank() != 0)
    {
        server.port_names.resize(server.comm_size * MPI_MAX_PROCESSOR_NAME);
    }
    D("port_names_size= %lu", server.port_names.size());
    MPI_Bcast(
        server.port_names.data(), server.comm_size * MPI_MAX_PROCESSOR_NAME,
        MPI_CHAR, 0, comm);

    // D("Portnames %s , %s ", server.port_names.data(),
    // server.port_names.data() + MPI_MAX_PROCESSOR_NAME);
    return register_field;
}

int melissa_get_current_state_id() {
    // assert(phase == PHASE_SIMULATION);
    if(phase == PHASE_SIMULATION)
    {
        return field.current_state_id;
    }
    else
    {
        return -2;
    }
}


void melissa_init(
    const char* field_name, const size_t local_vect_size,
    const size_t local_hidden_vect_size, const int bytes_per_element,
    const int bytes_per_element_hidden, MPI_Comm comm_) {
    melissa_init_with_index_map(
        field_name, local_vect_size, local_hidden_vect_size, bytes_per_element,
        bytes_per_element_hidden, comm_, nullptr, nullptr);
}

// TODO: later one could send the index map over all ranks in parallel!
void gather_global_index_map(
    const size_t local_vect_size, const INDEX_MAP_T local_index_map[],
    std::vector<INDEX_MAP_T>& global_index_map, const size_t local_vect_sizes[],
    MPI_Comm comm, const int bytes_per_element) {
    if(local_index_map == nullptr)
    {
        int i = 0;
        for(auto& e : global_index_map)
        {
            e.index = i++;
            e.varid = 0;
        }
    }
    else
    {
        size_t displs[getCommSize()];
        size_t last_displ = 0;
        size_t rcounts[getCommSize()];
        // FIXME!!!: this breaks if we have more than INTMAX array elements
        // which is the case... move to int...
        std::copy(local_vect_sizes, local_vect_sizes + getCommSize(), rcounts);
        for(int i = 0; i < getCommSize(); ++i)
        {
            rcounts[i] /= bytes_per_element;
            displs[i] = last_displ;
            last_displ += local_vect_sizes[i] / bytes_per_element;
        }


        slow_MPI_Gatherv(
            local_index_map, local_vect_size / bytes_per_element,
            MPI_MY_INDEX_MAP_T, global_index_map.data(), rcounts, displs,
            MPI_MY_INDEX_MAP_T, 0, comm);
    }
}

void melissa_init_with_index_map(
    const char* field_name, const size_t local_vect_size,
    const size_t local_hidden_vect_size, const int bytes_per_element,
    const int bytes_per_element_hidden, MPI_Comm comm_,
    const INDEX_MAP_T local_index_map[],
    const INDEX_MAP_T local_index_map_hidden[]) {
    // TODO: field_name is actually unneeded. its only used to name the output
    // files in the server side...
    bool register_field = first_melissa_init(comm_);

    // Check That bytes_per_element are the same on all ranks
    int tmp_assert = bytes_per_element;
    MPI_Bcast(&tmp_assert, 1, MPI_INT, 0, comm);
    assert(tmp_assert == bytes_per_element);

    // Check That bytes_per_element_hidden are the same on all ranks
    tmp_assert = bytes_per_element_hidden;
    MPI_Bcast(&tmp_assert, 1, MPI_INT, 0, comm);
    assert(tmp_assert == bytes_per_element_hidden);

    // create field
    field.name = field_name;
    field.current_state_id = -1; // We are beginning like this...
    field.current_step = 0;
    field.local_vect_size = local_vect_size;
    field.local_hidden_vect_size = local_hidden_vect_size;
    std::vector<size_t> local_vect_sizes(getCommSize());
    // synchronize local_vect_sizes and
    MPI_Allgather(
        &field.local_vect_size, 1, my_MPI_SIZE_T, local_vect_sizes.data(), 1,
        my_MPI_SIZE_T, comm);

    std::vector<size_t> local_hidden_vect_sizes(getCommSize());
    // synchronize local_hidden_vect_sizes and
    MPI_Allgather(
        &field.local_hidden_vect_size, 1, my_MPI_SIZE_T,
        local_hidden_vect_sizes.data(), 1, my_MPI_SIZE_T, comm);


    if(local_hidden_vect_sizes.size() >= 2)
    {
        D("vect sizes: %lu %lu", local_vect_sizes[0], local_vect_sizes[1]);
    }


    // gather the global index map from this
    std::vector<INDEX_MAP_T> global_index_map; // TODO: use special index map
                                               // type that can encode the var
                                               // name as well!!
    std::vector<INDEX_MAP_T> global_index_map_hidden;
    if(comm_rank == 0)
    {
        size_t global_vect_size = sum_vec(local_vect_sizes);
        assert(global_vect_size % bytes_per_element == 0);
        global_index_map.resize(sum_vec(local_vect_sizes) / bytes_per_element);

        size_t global_vect_size_hidden = sum_vec(local_hidden_vect_sizes);
        assert(global_vect_size_hidden % bytes_per_element_hidden == 0);
        global_index_map_hidden.resize(
            global_vect_size_hidden / bytes_per_element_hidden);
    }

    gather_global_index_map(
        local_vect_size, local_index_map, global_index_map,
        local_vect_sizes.data(), comm_, bytes_per_element);

    gather_global_index_map(
        local_hidden_vect_size, local_index_map_hidden, global_index_map_hidden,
        local_hidden_vect_sizes.data(), comm_, bytes_per_element_hidden);

    // if (comm_rank == 0)
    // {
    // D("Global index map:");
    // print_vector(global_index_map);
    // }

    if(register_field)
    {
        // Tell the server which kind of data he has to expect
        ccon->register_field(
            field_name, local_vect_sizes.data(), local_hidden_vect_sizes.data(),
            global_index_map, global_index_map_hidden, bytes_per_element,
            bytes_per_element_hidden);
    }


    // Calculate to which server ports the local part of the field will connect
    field.initConnections(
        local_vect_sizes, local_hidden_vect_sizes, bytes_per_element,
        bytes_per_element_hidden);

    trigger(START_PROPAGATE_STATE, field.current_state_id);
}
bool no_mpi = false;
// can be called from fortran or if no mpi is used (set NULL as the mpi
// communicator) TODO: check if null is not already used by something else!
void melissa_init_no_mpi(
    const char* field_name, const size_t* local_doubles_count,
    const size_t* local_hidden_doubles_count) { // comm is casted into an
                                                // pointer to an mpi
                                                // communicaotr if not null.
    MPI_Init(NULL, NULL); // TODO: maybe we also do not need to do this? what
                          // happens if we clear out this line?
    no_mpi = true;
    melissa_init(
        field_name, *local_doubles_count * sizeof(double),
        *local_hidden_doubles_count * sizeof(double), sizeof(double),
        sizeof(double), MPI_COMM_WORLD);
}


int melissa_expose(
    const char* field_name, VEC_T* values, VEC_T* hidden_values) {
    trigger(STOP_PROPAGATE_STATE, field.current_state_id);
    trigger(START_IDLE_RUNNER, getRunnerId());
    assert(phase != PHASE_FINAL);
    if(phase == PHASE_INIT)
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

    // @Kai: here we could checkpoint the values variable ! using fti. The
    // server than could recover from such a checkpoint.

    // and request new data
    int nsteps = field.getState(values, hidden_values);
    trigger(STOP_IDLE_RUNNER, getRunnerId());

    if(nsteps > 0)
    {
        if(last_step != field.current_step)
        {
            trigger(STOP_ITERATION, last_step);
            last_step = field.current_step;
            trigger(START_ITERATION, field.current_step);
        }
        trigger(START_PROPAGATE_STATE, field.current_state_id);
        trigger(NSTEPS, nsteps);
    }

    // TODO: this will block other fields!

    return nsteps;
}

void melissa_finalize() // TODO: when using more serverranks, wait until an end
                        // message was received from every before really
                        // ending... or actually not. as we always have only an
                        // open connection to one server rank...
{
    // sleep(3);
    MPI_Barrier(comm);
    // sleep(3);
    D("End Runner.");
    D("server ranks: %lu", ServerRanks::ranks.size());

    phase = PHASE_FINAL;
    // TODO: free all pointers?
    // not an api function anymore but activated if a finalization message is
    // received.
    if(ccon != NULL)
    {
        delete ccon;
    }


    // free all connections to server ranks:
    ServerRanks::ranks.clear();

    // sleep(3);
    D("Destroying zmq context");
    zmq_ctx_destroy(context);

#ifdef REPORT_TIMING
    if(comm_rank == 0)
    {
        timing->report(
            getCommSize(), field.local_vect_size + field.local_hidden_vect_size,
            runner_id);
    }
#ifdef REPORT_TIMING_ALL_RANKS
    const std::array<EventTypeTranslation, 3> event_type_translations = {{
        {START_ITERATION, STOP_ITERATION, "Iteration"},
        {START_PROPAGATE_STATE, STOP_PROPAGATE_STATE, "Propagation"},
        {START_IDLE_RUNNER, STOP_IDLE_RUNNER, "Runner idle"},
    }};
    std::string fn = "melissa_runner" + std::to_string(runner_id);
    timing->write_region_csv(event_type_translations, fn.c_str(), comm_rank);
#endif
#endif

    if(no_mpi)
    {
        // if without mpi we need to finalize mpi properly as it was only
        // created in this context.
        MPI_Finalize();
    }
}

int melissa_get_current_step() {
    assert(phase == PHASE_SIMULATION);
    return field.current_step;
}


void melissa_init_f(
    const char* field_name, const int* local_doubles_count,
    const int* local_hidden_doubles_count, MPI_Fint* comm_fortran) {

    MPI_Comm comm = MPI_Comm_f2c(*comm_fortran);
    melissa_init(
        field_name, *local_doubles_count * sizeof(double),
        *local_hidden_doubles_count * sizeof(double), sizeof(double),
        sizeof(double), comm);
}


int melissa_expose_f(const char* field_name, double* values) {
    return melissa_expose(field_name, reinterpret_cast<VEC_T*>(values), NULL);
}


/// legacy interface using doubles...
int melissa_expose_d(
    const char* field_name, double* values, double* hidden_values) {
    return melissa_expose(
        field_name, reinterpret_cast<VEC_T*>(values),
        reinterpret_cast<VEC_T*>(hidden_values));
}


int melissa_is_runner() {
    char* melissa_server_master_node = getenv("MELISSA_SERVER_MASTER_NODE");
    if(melissa_server_master_node == nullptr)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}
