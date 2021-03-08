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

int fti_checkpoint()
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

void melissa_p2p_init(const char *field_name,
                  const size_t local_vect_size,
                  const size_t local_hidden_vect_size,
                  const int bytes_per_element,
                  const int bytes_per_element_hidden,
                  MPI_Comm comm_,
                  const INDEX_MAP_T local_index_map[],
                  const INDEX_MAP_T local_index_map_hidden[]
                  )
{
    // TODO: store local_index_map to reuse in weight calculation!

    phase = PHASE_SIMULATION;



    fti_init(comm_);
}


double calculate_weight()
{
    // use python interface, export index map
    return 0.7;
}

int melissa_p2p_expose(VEC_T *values,
                   VEC_T *hidden_values)
{
    // Do the following:
    // 1. Checkpoint state and in parallel:
    FTI_
    // 2. calculate weight
    // 3. synchronize weight on rank 0
    // Rank 0:
    // 4. push weight to server
    // 5. ask server for more work
    // 6. check if job's parent state in memory. if not tell fti head to organize it
    // 7. wait until parent state is ready
    // All ranks:
    // 8. broadcast which state to propagate next to all ranks and how much nsteps



    if (nsteps == 0)
    {
        fti_finalize();
    }

    return nsteps;
}








