#include "melissa_p2p_app_core.h"
#include "../../server-p2p/messages/cpp/control_messages.pb.h"

#include "api_common.h"

int fti_init( int runner_id, MPI_Comm & comm_ )
{
    assert( !fti_is_init && "FTI must not have been initialized before!");
    fti_cfg_path = GetEnv( "FTI_CFG_PATH" );
    assert( fti_cfg_path != "" && "FTI config path is not set!");
    std::string cfg = fti_cfg_path + "/config-" + std::to_string(runner_id) + ".fti";
    FTI_Init(cfg.c_str(), comm_);
    fti_is_init = true;
}

int fti_protect( int* step, int* state_id, double* state_buffer, size_t buffer_count )
{
    assert( fti_is_init && "FTI must be initialized before!");
    FTI_Protect( 0, step, 1, FTI_INTG );
    FTI_Protect( 1, state_id, 1, FTI_INTG );
    FTI_Protect( 2, state_buffer, buffer_count, FTI_DBLE );
}

int fti_finalize()
{
    assert( fti_is_init && "FTI must be initialized before!");
    FTI_Finalize();
    fti_is_init = false;
}

int fti_checkpoint( int id )  // FIXME: why int?
{
  assert( fti_is_init && "FTI must be initialized before!");
  FTI_Checkpoint( id , FTI_L1 );
}


void* job_socket;
void* gp_socket;

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

    fti_init(runner_id, comm_);


    fti_protect( &step, &state_id, state_buffer, buffer_count);  // FIXME: also checkpoint hidden or remove hidden
    // assuming the pointer to the data to checkpoint doesn't change...

    // open sockets to server on rank 0:
    if (getCommRank() == 0)
    {
        char * melissa_server_master_node = getenv(
            "MELISSA_SERVER_MASTER_NODE");
        if (melissa_server_master_node == nullptr)
        {
            L(
                "you must set the MELISSA_SERVER_MASTER_NODE environment variable before running!");
            assert(false);
        }
        char * melissa_server_master_weight_node = getenv(
            "MELISSA_SERVER_MASTER_GP_NODE");
        if (melissa_server_master_node == nullptr)
        {
            L(
                "you must set the MELISSA_SERVER_MASTER_GP_NODE environment variable before running!");
            assert(false);
        }

        job_socket = zmq_socket(context, zmq_REQ);
        std::string addr = "tcp://" + melissa_server_master_node;
        D("connect to job request server at %s", addr);
        int req = zmq_connect(job_socket, addr.c_str());
        assert(req == 0);

        job_socket = zmq_socket(context, zmq_PUSH);
        addr = "tcp://" + melissa_server_master_node;
        D("connect to weight push server at %s", addr);
        req = zmq_connect(job_socket, addr.c_str());
        assert(req == 0);


        // init state server ranks by sending a message to head rank 0:
        FTI_AppSend(request state server ranks);
        FTI_Recv (...my_state_server_ranks );
    }
}


// FIXME assuming fti checkpoint id's are ints
typedef int FTI_ID_T;

FTI_ID_T hash_fti_id(int cycle, int state_id)
{
    // this should work for up to 10000 members!
    assert(state_id < 10000);
    return cycle*10000 + state_id
}

double calculate_weight()
{
    // Warning returns correct weight only on rank 0! other ranks return -1.0
    // use python interface, export index map
    return 0.7;
}

void push_weight_to_server(double weight)
{
    ::melissa_p2p::Message m;
    m.mutable_weight()->mutable_state_id()->set_t(current_step);
    m.mutable_weight()->mutable_state_id()->set_id(current_id);
    m.mutable_weight()->set_runner_id(runner_id);
    m.mutable_weight()->set_weight(weight);


    m.google::protobuf::Message::ByteSize()
    // TODO: zmq send
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, m.ByteSize());
    m.SerializeToArray(zmq_msg_data(&msg), m.ByteSize());
    ZMQ_CHECK(zmq_msg_send(&msg, gp_socket, 0));  // TODO: use new api!
}

::melissa_p2p::StateServer my_state_server_ranks;

::melissa_p2p::JobResponse request_work_from_server() {
    ::melissa_p2p::Message m;
    // TODO:  one might set cached states here.
    m.mutable_job_request()->set_runner_id(runner_id);
    m.mutable_job_request()->mutable_runner()->google::protobuf::Message::CopyFrom(my_state_server_ranks);

    zmq_msg_t req, res;
    zmq_msg_init_size(&req, m.ByteSize());
    m.SerializeToArray(zmq_msg_data(&req), m.ByteSize());
    ZMQ_CHECK(zmq_msg_send(m, job_socket, 0));  // TODO: use new api

    zmq_msg_init(&res);
    ZMQ_CHECK(zmq_recv(&res, job_socket, 0));
    ::melissa_p2p::Message r;
    r.ParseFromArray(zmq_msg_data(&res), zmq_msg_size(&r));

    return r.job_response();
}

int melissa_p2p_expose(VEC_T *values,
                   VEC_T *hidden_values)
{
    // Do the following:
    // 1. Checkpoint state and in parallel:
    FTI_ID_T current_checkpoint_id = hash_fti_id(current_step, current_state_id);
    fti_checkpoint();  // This will block until data is written to local disk by each rank.

    // 2. calculate weight and synchronize weight on rank 0
    double weight = calculate_weight();

    int nsteps = 0;

    // Rank 0:
    if (getCommRank() == 0) {

        // 3. push weight to server
        push_weight_to_server(weight);

        // 4. ask server for more work

        // [KAI] FIXME I think we should also ask this from the heads
        // [SEBASTIAN] I don't think so. if the heads are blocking the runner would block!
        // + its easier to implemennt this way
        auto job_response = request_work_from_server();

        FTI_ID_T checkpoint_id;

        if (nsteps > 0) {

            // [KAI] FIXME I think it is better to let the heads check if the state is in local.
            // I think we should commubnicate with the heads in any case, so that the heads know
            // that the runner has finished working on the state.

            // 5. check if job's parent state in memory. if not tell fti head to organize it
            checkpoint_id = hash_fti_id(parent_t, parent_id);
            FTIT_stat st;
            FTI_Stat( id, &st );
            if (FTI_ST_IS_LOCAL( st.level )) {
                D("Parent state was found locally");
            } else {
                // TODO: pack message with protobuf
                ::melissa_p2p::DownloadState dls;
                dls.mutable_parent()->set_t(parent_t);
                dls.mutable_parent()->set_id(parent_id);
                for (auto & runner :
                dls.add_runners()
                int len = dls.ByteSize();
                FTI_AppSend( &len, sizeof(size_t), MELISSA_USER_MESSAGE, FTI_HEAD_MODE_SING );
                // send packed protobuf message
                FTI_AppSend( &msg_serialized, len, MELISSA_USER_MESSAGE, FTI_HEAD_MODE_SING );

                // 6. wait until parent state is ready
                int response = 0;  // FIXME: wait for a protobuf message here too??!!
                /*
                MPI_Recv(&response, 1, MPI_INT, fti_head_rank_0, MELISSA_USER_MESSAGE,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);*/
                FTI_AppRecv( &response, sizeof(int), MELISSA_USER_MESSAGE, FTI_HEAD_MODE_SING );

                if (response == 1) {
                    D("Parent state was retrieved p2p");
                } else if (FTI_ST_IS_LEVEL_1( st.level )) {
                    D("Parent state was restored from level 1"); // ist es ein level 1 checkpoint?
                } else {
                    E("Parent state was not found!");
                    // FIXME abort everything now
                    assert(false);
                }
            }
        }
    }
    // All ranks:
    // 7. broadcast which state to propagate next to all ranks and how much nsteps
    MPI_Bcast() ....

    if (nsteps == 0)
    {
        fti_finalize();
    }
    else
    {
        FTI_Recover(checkpoint id)
    }


    return nsteps;
}








