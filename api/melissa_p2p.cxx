#include "melissa_p2p.h"

int fti_init( MPI_Comm & comm_ )
{
    assert( !fti_is_init && "FTI must not have been initialized before!");

    fti_cfg_path = GetEnv( "FTI_CFG_PATH" );

    assert( fti_cfg_path != "" && "FTI config path is not set!");

    std::string cfg = fti_cfg_path + "/config-" + std::to_string(runner_id) + ".fti";

    FTI_Init(cfg.c_str(), comm_);

    MPI_Comm_dup(FTI_COMM_WORLD, &comm);

    fti_master = (getCommRank() == 0);

    fti_is_init = true;
}

int fti_protect( int local_vect_size )
{
    assert( fti_is_init && "FTI must be initialized before!");

    init_fti_parameters( local_vect_size );

    hsize_t dim = 1;
    hsize_t offset = 0;
    hsize_t count = 1;

    FTI_DefineGlobalDataset( 0, 1, &dim, "epoch", NULL, FTI_INTG );
    FTI_Protect( 0, &fti_current_step, 1, FTI_INTG );
    FTI_AddSubset( 0, 1, &offset, &count, 0 );

    FTI_DefineGlobalDataset( 1, 1, &dim, "state_id", NULL, FTI_INTG );
    FTI_Protect( 1, &fti_current_state_id, 1, FTI_INTG );
    FTI_AddSubset( 1, 1, &offset, &count, 1 );

    FTI_DefineGlobalDataset(2, 1, &global_field_dim_hsize_t, "forecast", NULL, FTI_DBLE);
    FTI_Protect( 2, ckp, field.local_vect_size, FTI_DBLE );
    FTI_AddSubset(2, 1, &offset_hsize_t, &local_field_dim_hsize_t, 2);
}

int fti_finalize()
{
    assert( fti_is_init && "FTI must be initialized before!");

    FTI_Finalize();

    //if( getCommRank() == 0 ) {
    //    std::stringstream ss;
    //    ss << "Global/VPR-FORECAST-" << state_id << "-ID";
    //    ss << std::setw(8) << std::setfill('0') << step-1;
    //    ss << ".h5";
    //    std::string fn = ss.str();
    //    std::remove(fn.c_str());
    //}

    fti_is_init = false;
}

int fti_checkpoint()  // FIXME: why int?
{
  assert( fti_is_init && "FTI must be initialized before!");
  FTI_Protect( 2, ckp, field.local_vect_size, FTI_DBLE );
  std::string prefix("VPR-FORECAST-");
  prefix += std::to_string(fti_current_state_id);  // FIXME: can't we just use the current_state_id as defined in melissa_da_api.cxx ?
  FTI_SetHdf5FileName( 0, prefix.c_str() , FTI_H5SF_PREFIX );
  std::cout << "prefix set to: " << prefix << " current state id: " << fti_current_state_id << std::endl;
  FTI_Checkpoint(fti_current_step, FTI_L4_H5_SINGLE);// FIXME: can't we just use the current_step as defined in melissa_da_api.cxx ?
  //FTI_Checkpoint(fti_ckpt_counter++, FTI_L1);
}

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

    fti_init(comm_);


    fti_protect(local_vect_size);  // FIXME: also checkpoint hidden or remove hidden
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
        nsteps = request_work_from_server(&parent_t, &parent_id);
        FTI_ID_T checkpoint_id;

        if (nsteps > 0) {
            // 5. check if job's parent state in memory. if not tell fti head to organize it
            checkpoint_id = hash_fti_id(parent_t, parent_id);
            FTIT_stat st;
            FTI_Stat( id, &st );
            if (FTI_ST_IS_LOCAL( st.level )) {
                D("Parent state was found locally");
            } else {
                // TODO: pack message with protobuf
                size_t len = 42;
                MPI_Send(&len, 1, my_MPI_SIZE_T, fti_head_rank_0,
                        MELISSA_USER_MESSAGE, MPI_COMM_WORLD);
                // send packed protobuf message
                MPI_Send(&msg_serialized, len, MPI_BYTE, fti_head_rank_0,
                        MELISSA_USER_MESSAGE, MPI_COMM_WORLD);


                // 6. wait until parent state is ready
                int response = 0;  // FIXME: wait for a protobuf message here too??!!
                MPI_Recv(&response, 1, MPI_INT, fti_head_rank_0, MELISSA_USER_MESSAGE,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);

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








