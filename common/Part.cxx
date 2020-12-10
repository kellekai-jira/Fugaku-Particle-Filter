#include "Part.h"
#include "utils.h"

void calculate_local_vect_sizes_server(const int comm_size_server, const
                                   size_t global_vect_size,
                                   size_t * local_vect_sizes_server,
                                   const int bytes_per_element) {


    // transform from bytes into elements
    int global_elements = global_vect_size/bytes_per_element;
    for (int i = 0; i < comm_size_server; ++i)
    {
        // every server rank gets the same amount
        local_vect_sizes_server[i] = global_elements /
                                     comm_size_server;

        // let n be the rest of this division
        // the first n server ranks get one more to split the rest fair up...
        size_t n_rest = global_elements - size_t(global_elements /
                                                  comm_size_server) *
                        comm_size_server;
        if (size_t(i) < n_rest)
        {
            local_vect_sizes_server[i]++;
        }

        // now transform back from  elementcount to bytes
        local_vect_sizes_server[i] *= bytes_per_element;
    }
}



/// This never splits elements where one element consists of bytes_per_element bytes.
std::vector<Part> calculate_n_to_m(const int comm_size_server, const
                                   std::vector<size_t> &local_vect_sizes_runner,
                                   const int bytes_per_element)
{
    std::vector <Part> parts(0);
    size_t local_vect_sizes_server[comm_size_server];
    size_t global_vect_size = 0;
    global_vect_size = sum_vec(local_vect_sizes_runner);

    if (global_vect_size == 0)
    {
        return parts;
    }

    calculate_local_vect_sizes_server(comm_size_server, global_vect_size,
            local_vect_sizes_server, bytes_per_element);


    parts.push_back({0, 0, 0, 0, 0});
    size_t index_in_runner = 0;
    size_t index_in_server = 0;
    Part * last = parts.data();

    for (size_t i = 0; i < global_vect_size; i++)
    {
        bool added_part = false;

        if (index_in_runner >
            local_vect_sizes_runner[last->rank_runner]-1)
        {
            // new part as we cross simulation domain border:
            parts.push_back(*last);
            last = &parts[parts.size()-1];

            last->rank_runner++;
            last->local_offset_runner = 0;
            index_in_runner = 0;

            last->local_offset_server = index_in_server;
            last->send_count = 0;

            added_part = true;
        }
        if (index_in_server >
            local_vect_sizes_server[last->rank_server]-1)
        {
            if (!added_part)
            {
                // if server and simulation domain borders are at the same time...
                parts.push_back(*last);
                last = &parts[parts.size()-1];
            }
            // new part as we cross server domain border:
            last->rank_server++;
            last->local_offset_server = 0;
            index_in_server = 0;

            last->local_offset_runner = index_in_runner;
            last->send_count = 0;
        }

        last->send_count++;

        index_in_runner++;
        index_in_server++;
    }

    return parts;
}
