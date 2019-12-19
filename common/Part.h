#ifndef PART_H_
#define PART_H_
#include <vector>

struct Part    // TODO: rename datatype into Part
{int rank_runner;
 size_t local_offset_runner;

 int rank_server;
 size_t local_offset_server;

 size_t send_count;};


std::vector<Part> calculate_n_to_m(const int comm_size_server, const
                                   std::vector<size_t> &local_vect_sizes_runner);

#endif
