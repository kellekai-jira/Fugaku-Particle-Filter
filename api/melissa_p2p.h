#ifndef API_MELISSA_P2P_H_
#define API_MELISSA_P2P_H_

#include "utils.h"
#include "melissa_da_api.h"


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
    // TODO: store local_index_map in singleton to reuse in weight calculation!

    phase = PHASE_SIMULATION;
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
    // 2. calculate weight
    // 3. synchronize weight on rank 0
    // Rank 0:
    // 4. push weight to server
    // 5. ask server for more work
    // 6. check if job's parent state in memory. if not tell fti head to organize it
    // 7. wait until parent state is ready
    // All ranks:
    // 8. broadcast which state to propagate next to all ranks and how much nsteps
    return nsteps;
}



#endif
