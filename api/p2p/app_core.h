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
                  );


int melissa_p2p_expose(VEC_T *values,
                   VEC_T *hidden_values);

const int MELISSA_USER_MESSAGE = 42;  // reserve a tag that hopefully is not in use by
// FTI already to communicate with head rank

#endif
