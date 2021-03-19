#include "app_core.h"
#include "../../server-p2p/messages/cpp/control_messages.pb.h"

#include "api_common.h"

void* job_socket;

std::vector<INDEX_MAP_T> local_index_map;
std::vector<INDEX_MAP_T> local_index_map_hidden;

void melissa_p2p_init(const char *field_name,
                  const size_t local_vect_size,
                  const size_t local_hidden_vect_size,
                  const int bytes_per_element,
                  const int bytes_per_element_hidden,
                  MPI_Comm comm_,
                  const INDEX_MAP_T local_index_map_[],
                  const INDEX_MAP_T local_index_map_hidden_[]
                  )
{

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // store local_index_map to reuse in weight calculation
    local_index_map.resize(local_vect_size);
    local_index_map_hidden.resize(local_hidden_vect_size);
    std::copy(local_index_map_, local_index_map_ + local_vect_size,  local_index_map);
    std::copy(local_index_map_hidden_, local_index_map_hidden_ + local_vect_size_hidden,
            local_index_map_hidden);

    // there is no real PHASE_INIT in p2p since no configuration messages need to be
    // exchanged at the beginning
    phase = PHASE_SIMULATION;

    // TODO move storage.init in constructor and use smartpointer instead
    FtiController io;
    mpi.init( comm );

    assert(local_vect_size_hidden == 0 && "Melissa-P2P does not yet work if there is a hidden state");
    storage.init( &mpi, &io, capacity, local_vect_size);
    fti_protect_id = storage.protect( NULL, local_vect_size, IO_BYTE );

    // open sockets to server on rank 0:
    if (mpi.rank() == 0)
    {
        char * melissa_server_master_node = getenv(
            "MELISSA_SERVER_MASTER_NODE");
            L(
                "you must set the MELISSA_SERVER_MASTER_NODE environment variable before running!");
            assert(false);
        }

        // Refactor: put all sockets in a class
        job_socket = zmq_socket(context, zmq_REQ);
        std::string addr = "tcp://" + melissa_server_master_node;
        D("connect to job request server at %s", addr);
        int req = zmq_connect(job_socket, addr.c_str());
        assert(req == 0);
    }
}

double calculate_weight()
{
    // Warning returns correct weight only on rank 0! other ranks return -1.0
    // use python interface, export index map
    return 0.7;
}



void push_weight_to_head(double weight)
{
    static mpi_request_t req;
    req.wait();  // be sure that there is nothing else in the mpi send queue

    ::melissa_p2p::Message m;
    m.set_runner_id(runner_id);
    m.mutable_weight()->mutable_state_id()->set_t(current_step);
    m.mutable_weight()->mutable_state_id()->set_id(current_id);
    m.mutable_weight()->set_weight(weight);

    char buf[m.ByteSize()];
    m.SerializeToArray(buf);
    io.isend( buf, m.ByteSize(), IO_TAG_POST, IO_MSG_ONE, req );

}

::melissa_p2p::StateServer my_state_server_ranks;

::melissa_p2p::JobResponse request_work_from_server() {
    ::melissa_p2p::Message m;
    m.set_runner_id(runner_id);
    m.mutable_job_request();  // create job request

    message_send(job_socket, m);

    auto r = message_receive(job_socket);

    return r.job_response();
}

int melissa_p2p_expose(VEC_T *values,
                   VEC_T *hidden_values)
{
    // Update pointer
    storage.update( fti_protect_id, values, local_vect_size );
    
    // store to ram disk
    // TODO: call protect direkt in chunk based api
    io_state_t state = { current_step, current_state_id };
    storage.store( state );

    // 2. calculate weight and synchronize weight on rank 0
    double weight = calculate_weight();

    int nsteps = 0;

    int parent_t, parent_id, job_t, job_id, nsteps;

    // Rank 0:
    if (mpi.comm() == 0) {
        // 3. push weight to server
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
        storage.load(io_state_t(job_t, job_id));

        current_step = job_t;
        current_id = job_id;
    }
    else
    {
        zmq_disconnect(job_socket);

        storage.fini();


        current_step = -1;
        current_id = -1;
    }


    return nsteps;
}








