#ifndef N_TO_M_H_
#define N_TO_M_H_
#include <vector>

struct n_to_m {  // TODO: rename datatype into Part
  int rank_simu;
  size_t local_offset_simu;

  int rank_server;
  size_t local_offset_server;

  size_t send_count;
};


std::vector<n_to_m> calculate_n_to_m(const int comm_size_server, const std::vector<size_t> &local_vect_sizes_simu);

#endif
