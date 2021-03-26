#include "app_core.h"
#include "StorageController/helpers.h"
#include "p2p.pb.h"

#include "api_common.h"
#include "StorageController/fti_controller.hpp"
#include "StorageController/mpi_controller.hpp"
#include "StorageController/storage_controller_impl.hpp"

#include "StorageController/helpers.h"


#include <cstdlib>
#include <memory>

#include "PythonInterface.h"

void* job_socket;

std::vector<INDEX_MAP_T> local_index_map;
std::vector<INDEX_MAP_T> local_index_map_hidden;

FtiController io;
MpiController mpi;
int fti_protect_id;

MPI_Fint melissa_io_init_f(const MPI_Fint *comm_fortran)
{
    runner_id = atoi(getenv("MELISSA_DA_RUNNER_ID"));
    printf("Hello! I'm runner %d\n", runner_id);

    // TODO: only do this if is_p2p() !
    MPI_Comm comm_c = MPI_Comm_f2c(*comm_fortran);
    mpi.init( comm_c );
    storage.io_init( &mpi, &io );
    comm = mpi.comm();  // TODO: use mpi controller everywhere in api


    return MPI_Comm_c2f(comm);
}

void melissa_p2p_init(const char *field_name,
                  const size_t local_vect_size,
                  const size_t local_hidden_vect_size,
                  const int bytes_per_element,
                  const int bytes_per_element_hidden,
                  const INDEX_MAP_T local_index_map_[],
                  const INDEX_MAP_T local_index_map_hidden_[]
                  )
{

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (local_index_map_ != nullptr) {
    // store local_index_map to reuse in weight calculation
        local_index_map.resize(local_vect_size);
        local_index_map_hidden.resize(local_hidden_vect_size);
        std::copy(local_index_map_, local_index_map_ + local_vect_size,  local_index_map.data());
    }
    if (local_index_map_hidden_ != nullptr) {
        std::copy(local_index_map_hidden_, local_index_map_hidden_ + local_hidden_vect_size,
                local_index_map_hidden.data());
    }

    // there is no real PHASE_INIT in p2p since no configuration messages need to be
    // exchanged at the beginning
    phase = PHASE_SIMULATION;

    // TODO move storage.init in constructor and use smartpointer instead
    assert(local_hidden_vect_size == 0 && "Melissa-P2P does not yet work if there is a hidden state");
    storage.init( 2*1024L*1024L*1024L, local_vect_size);  // 2GB storage for now
    fti_protect_id = storage.protect( NULL, local_vect_size, IO_BYTE );

    // open sockets to server on rank 0:
    if (mpi.rank() == 0)
    {
        char * melissa_server_master_node = getenv(
            "MELISSA_SERVER_MASTER_NODE");

        if (! melissa_server_master_node) {
            L(
                "you must set the MELISSA_SERVER_MASTER_NODE environment variable before running!");
            assert(false);
        }

        // Refactor: put all sockets in a class
        job_socket = zmq_socket(context, ZMQ_REQ);
        std::string port_name = fix_port_name(melissa_server_master_node);
        D("connect to job request server at %s", port_name.c_str());
        int req = zmq_connect(job_socket, port_name.c_str());
        assert(req == 0);
    }
}


double calculate_weight(VEC_T *values, VEC_T *hidden_values)
{
    // Warning returns correct weight only on rank 0! other ranks return -1.0
    // use python interface, export index map
    static std::unique_ptr<PythonInterface> pi;  // gets destroyed when static scope is left!
    static bool is_first_time = true;
    if (is_first_time) {
        pi = std::unique_ptr<PythonInterface>(new PythonInterface());
        is_first_time = false;
    }

    pi->calculate_weight(field.current_step, field.current_state_id, values,
            hidden_values);
}



void push_weight_to_head(double weight)
{
    static mpi_request_t req;
    static bool wait = false;
    if( wait ) {
      if( io.m_dict_bool["master_local"] ) req.wait();  // be sure that there is nothing else in the mpi send queue
      int dummy; io.recv( &dummy, sizeof(int), IO_TAG_POST, IO_MSG_ALL );
    }
    ::melissa_p2p::Message m;
    m.set_runner_id(runner_id);
    m.mutable_weight()->mutable_state_id()->set_t(field.current_step);
    m.mutable_weight()->mutable_state_id()->set_id(field.current_state_id);
    m.mutable_weight()->set_weight(weight);

    char buf[m.ByteSize()];  // TODO: change bytesize to bytesize long
    m.SerializeToArray(buf, m.ByteSize());
    io.isend( buf, m.ByteSize(), IO_TAG_POST, IO_MSG_ONE, req );
    wait = true;
}

::melissa_p2p::JobResponse request_work_from_server() {
    ::melissa_p2p::Message m;
    m.set_runner_id(runner_id);
    m.mutable_job_request();  // create job request

    send_message(job_socket, m);

    auto r = receive_message(job_socket);

    return r.job_response();
}

int melissa_p2p_expose(VEC_T *values,
                   VEC_T *hidden_values)
{
    // Update pointer
    storage.update( fti_protect_id, values, field.local_vect_size );

    // store to ram disk
    // TODO: call protect direkt in chunk based api

    if (field.current_step == 0) {
        // if checkpointing initial state, use runner_id as state id
        field.current_state_id = runner_id; // We are beginning like this...
    }

    io_state_id_t state = { field.current_step, field.current_state_id };

    storage.store( state );

    // 2. calculate weight and synchronize weight on rank 0
    double weight = calculate_weight(values, hidden_values);

    int nsteps = 0;

    int parent_t, parent_id, job_t, job_id;

    // Rank 0:
    if (mpi.rank() == 0) {
        // 3. push weight to server (via the head who forwards as soon nas the state is checkpointed to the pfs)
        push_weight_to_head(weight);

        // 4. ask server for more work
        auto job_response = request_work_from_server();
        parent_t = job_response.parent().t();
        parent_id = job_response.parent().id();
        job_t = job_response.job().t();
        job_id = job_response.job().id();
        nsteps = job_response.nsteps();
    }

    // Broadcast to all ranks:
    MPI_Bcast(&parent_t, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&parent_id, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&job_t, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&job_id, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&nsteps, 1, MPI_INT, 0, mpi.comm());


    if (nsteps > 0) {
        // called by every app core
        if (parent_t < 1) {
            if (field.current_step == 0) {
                printf("Not performing a state load as good init state already in memory");
            } else {
                // load my very own checkpoint from t=0:
                // in this case the runner id is the state id
                storage.load(io_state_id_t(0, runner_id));
            }
        } else if (field.current_step == parent_t && field.current_state_id == parent_id) {
            // TODO: KAI: or is this handled elsewhere already?
                printf("Not performing a state load as good state already in memory");
        } else {
            storage.load(io_state_id_t(parent_t, parent_id));
        }

        field.current_step = job_t;
        field.current_state_id = job_id;
    }
    else
    {
        zmq_close(job_socket);

        storage.fini();

        field.current_step = -1;
        field.current_state_id = -1;
    }


    return nsteps;
}








