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
#include "p2p/app_core.h"
#include "api_common.h"

void melissa_finalize();

int runner_id = -1;

void* context;


int last_step = -2;

#ifdef REPORT_TIMING
std::unique_ptr<ApiTiming> timing(nullptr);
#endif

/// Communicator used for simulation/Runner
MPI_Comm comm;

calculateWeightFunction calculateWeight;

Server server;


std::map<int, std::unique_ptr<ServerRankConnection> > ServerRanks::ranks;




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
            MPRT("you must set the MELISSA_SERVER_MASTER_NODE environment "
              "variable before running!");
            assert(false);
        }
        std::string port_name = fix_port_name(melissa_server_master_node);
        MDBG("Configuration Connection to %s", port_name.c_str());
        zmq_connect(socket, port_name.c_str());
    }

    /// returns true if field registering is requested by the server
    bool register_runner_id(Server* out_server) {
        assert(std::getenv("MELISSA_DA_RUNNER_ID"));

        if( runner_id == -1 ) {
          runner_id = atoi(getenv("MELISSA_DA_RUNNER_ID"));
        }
        int header[] = {REGISTER_RUNNER_ID, runner_id};

        zmq::send_n(socket, header, 2);

        MPRT("registering runner_id %d at server", runner_id);

        auto msg_reply = zmq::recv(socket);

        assert(zmq::size(*msg_reply) == 2 * sizeof(int));

        int reply[2] = {0};

        std::memcpy(reply, zmq::data(*msg_reply), sizeof(reply));

        bool request_register_field = reply[0] != 0;

        MPRT("Registering field? %d", reply[0]);

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

        int type = REGISTER_FIELD;
        int header[] = {
            type, getCommSize(), bytes_per_element, bytes_per_element_hidden
        };
        auto msg_header = zmq::msg_init(sizeof(header) +
                                        MPI_MAX_PROCESSOR_NAME);

        std::memcpy(zmq::data(*msg_header), header, sizeof(header));
        std::strncpy(
            zmq::data(*msg_header) + sizeof(header),
            field_name,
            MPI_MAX_PROCESSOR_NAME);
        zmq::send(*msg_header, socket, ZMQ_SNDMORE);
        zmq::send_n(
            socket, local_vect_sizes, getCommSize(), ZMQ_SNDMORE);
        zmq::send_n(
            socket, local_hidden_vect_sizes, getCommSize(), ZMQ_SNDMORE);

        auto msg_index_map = zmq::msg_init_n(
            global_index_map.data(), global_index_map.size());

        zmq::send(*msg_index_map, socket, ZMQ_SNDMORE);

        auto msg_index_map_hidden = zmq::msg_init_n(
            global_index_map_hidden.data(), global_index_map_hidden.size());

        zmq::send(*msg_index_map_hidden, socket);

        auto msg_reply = zmq::recv(socket);
        (void)msg_reply;

        assert(zmq::size(*msg_reply) == 0);
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
    try_init_timing();
    M_TRIGGER(START_ITERATION, last_step);
#endif

    if( runner_id == -1 ) { 
      runner_id = atoi(getenv("MELISSA_DA_RUNNER_ID"));
    }
    if (is_p2p())
    {
        return false;
    }

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
    MDBG("port_names_size= %lu", server.port_names.size());
    MPI_Bcast(
        server.port_names.data(), server.comm_size * MPI_MAX_PROCESSOR_NAME,
        MPI_CHAR, 0, comm);

    // MDBG("Portnames %s , %s ", server.port_names.data(),
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
    
    calculateWeight = NULL;

    if (is_p2p())
    {
        melissa_p2p_init(field_name,
                  local_vect_size,
                  local_hidden_vect_size,
                  bytes_per_element,
                  bytes_per_element_hidden,
                  local_index_map,
                  local_index_map_hidden
                  );

        M_TRIGGER(START_PROPAGATE_STATE, field.current_state_id);
        return;
    }

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
        MDBG("vect sizes: %lu %lu", local_vect_sizes[0], local_vect_sizes[1]);
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
    // MDBG("Global index map:");
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

    M_TRIGGER(START_PROPAGATE_STATE, field.current_state_id);
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
    const char* field_name, VEC_T* values, int64_t size, io_type_t io_type, 
    VEC_T* hidden_values, int64_t size_hidden, io_type_t io_type_hidden, MELISSA_EXPOSE_MODE mode) {
    if( mode == MELISSA_MODE_UPDATE ) {
      M_TRIGGER(STOP_PROPAGATE_STATE, field.current_state_id);
      M_TRIGGER(START_IDLE_RUNNER, getRunnerId());
    }
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
    //assert(field.name == field_name);

    int nsteps;
    if (is_p2p())
    {
        nsteps = melissa_p2p_expose(field_name, values, size, io_type, hidden_values, size_hidden, io_type_hidden, mode);
    } else {

        field.putState(values, hidden_values, field_name);

        // @Kai: here we could checkpoint the values variable ! using fti. The server than could recover from such a checkpoint.

        // and request new data
        nsteps = field.getState(values, hidden_values);
    }
    
    if( mode == MELISSA_MODE_EXPOSE ) {
      return nsteps;
    }

    M_TRIGGER(STOP_IDLE_RUNNER, getRunnerId());

    if(nsteps > 0)
    {
        if(last_step != field.current_step)
        {
            M_TRIGGER(STOP_ITERATION, last_step);
            last_step = field.current_step;
            M_TRIGGER(START_ITERATION, field.current_step);
        }
        M_TRIGGER(START_PROPAGATE_STATE, field.current_state_id);
        M_TRIGGER(NSTEPS, nsteps);
    }
    else if (nsteps == 0)
    {
        melissa_finalize();
    }

    // TODO: this will block other fields!
    MDBG("nsteps %d", nsteps);

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
    MDBG("End Runner.");
    MDBG("server ranks: %lu", ServerRanks::ranks.size());

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
    MDBG("Destroying zmq context");
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
    const char* field_name, const int64_t* local_doubles_count,
    const int64_t* local_hidden_doubles_count, MPI_Fint* comm_fortran) {

    MPI_Comm comm = MPI_Comm_f2c(*comm_fortran);
    melissa_init(
        field_name, *local_doubles_count * sizeof(double),
        *local_hidden_doubles_count * sizeof(double), sizeof(double),
        sizeof(double), comm);
}

void melissa_register_weight_function( calculateWeightFunction func )
{
  calculateWeight = func;
}

int melissa_expose_f(const char* field_name, double* values, int64_t* size, int* mode) {
    return melissa_expose(field_name, reinterpret_cast<VEC_T*>(values), *size, IO_DOUBLE, NULL, 0, IO_DOUBLE, static_cast<MELISSA_EXPOSE_MODE>(*mode));
}


/// legacy interface using doubles...
int melissa_expose_d(
    const char* field_name, double* values, double* hidden_values) {
    return melissa_expose(
        field_name, reinterpret_cast<VEC_T*>(values), 0, IO_DOUBLE,
        reinterpret_cast<VEC_T*>(hidden_values), 0, IO_DOUBLE);
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


int melissa_get_comm_f() {
    assert(0 && "Deprecated!");
    int res =  MPI_Comm_c2f(comm);
    MDBG("melissa_get_comm in c: %d", res);
    return res;
}
