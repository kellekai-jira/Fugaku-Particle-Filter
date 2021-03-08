#include "melissa_p2p.h"

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

// PROBABLY NOT NEEDED ANYMORE
/*
void init_fti_parameters( int local_vector_size) {
  // add local data chunk
  size_t global_field_dim;
  MPI_Allreduce(&local_vector_size, &global_field_dim, 1, my_MPI_SIZE_T,
                  MPI_SUM, comm);
  global_field_dim_hsize_t = global_field_dim;
  std::vector<size_t> local_vect_sizes(getCommSize());

  MPI_Allgather(&local_vector_size, 1, my_MPI_SIZE_T,
                local_vect_sizes.data(), 1, my_MPI_SIZE_T,
                comm);

  offset_hsize_t = 0;
  for(int i=0; i<getCommRank(); i++) {
    offset_hsize_t += local_vect_sizes[i];
  }
  local_field_dim_hsize_t = local_vector_size;
}
*/


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
        print(os.getenv("MELISSA_SERVER_MASTER_NODE"))
        MELISSA_SERVER_MASTER_NODE = os.getenv("MELISSA_SERVER_MASTER_NODE")[6:].split(
            ':')[0]
        if MELISSA_SERVER_MASTER_NODE == get_node_name():
            MELISSA_SERVER_MASTER_NODE = "127.0.0.1"

        job_req_socket = context.socket(zmq.REQ)
        addr = "tcp://%s:%d" % (MELISSA_SERVER_MASTER_NODE, 6666)
        print('connect to melissa server at', addr)
        job_req_socket.connect(addr)

        weight_push_socket = context.socket(zmq.PUSH)
        addr = "tcp://%s:%d" % (MELISSA_SERVER_MASTER_NODE, 6667)
        print('connect weight puhser to', addr)
        weight_push_socket.connect(addr)
    }
}


// FIXME assuming fti checkpoint id's are ints
typedef FTI_ID_T int;

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
    weight_push_socket
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
        int parent_t, parent_id;
        
        // [KAI] FIXME I think we should also ask this from the heads
        nsteps = request_work_from_server(&parent_t, &parent_id);
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
                size_t len = 42;
                /* [KAI] replaced by FTI API functions
                MPI_Send(&len, 1, my_MPI_SIZE_T, fti_head_rank_0,
                        MELISSA_USER_MESSAGE, MPI_COMM_WORLD);
                // send packed protobuf message
                MPI_Send(&msg_serialized, len, MPI_BYTE, fti_head_rank_0,
                        MELISSA_USER_MESSAGE, MPI_COMM_WORLD);
                */
                FTI_AppSend( &len, sizeof(size_t), MELISSA_USER_MESSAGE, FTI_HEAD_MODE_SING );
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








