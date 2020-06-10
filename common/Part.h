#ifndef PART_H_
#define PART_H_
#include <vector>
#include <cstring>  // for size_t on intel compilers

struct Part
{
    int rank_runner;
    size_t local_offset_runner;

    int rank_server;
    size_t local_offset_server;

    size_t send_count;
};

void calculate_local_vect_sizes_server(const int comm_size_server, const
                                   size_t global_vect_size,
                                   size_t * local_vect_sizes_server);

std::vector<Part> calculate_n_to_m(const int comm_size_server, const
                                   std::vector<size_t> &local_vect_sizes_runner);

#endif
