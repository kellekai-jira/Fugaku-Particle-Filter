#ifndef API_MELISSA_P2P_H_
#define API_MELISSA_P2P_H_

#include "utils.h"
#include "melissa_da_api.h"
#include "StorageController/mpi_controller.hpp"
#include "StorageController/fti_controller.hpp"

void melissa_p2p_init(const char *field_name,
                  const size_t local_vect_size,
                  const size_t local_hidden_vect_size,
                  const int bytes_per_element,
                  const int bytes_per_element_hidden,
                  const INDEX_MAP_T local_index_map[],
                  const INDEX_MAP_T local_index_map_hidden[]
                  );


int melissa_p2p_expose(const char* field_name, VEC_T *values, int64_t size, 
                   VEC_T *hidden_values, int64_t size_hidden, MELISSA_EXPOSE_MODE mode = MELISSA_MODE_UPDATE);

const int MELISSA_USER_MESSAGE = 42;  // reserve a tag that hopefully is not in use by
// FTI already to communicate with head rank
//

extern FtiController io;
extern MpiController mpi;

#endif
