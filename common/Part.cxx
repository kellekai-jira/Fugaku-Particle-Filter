#include "Part.h"


std::vector<Part> calculate_n_to_m(const int comm_size_server, const
                                   std::vector<size_t> &local_vect_sizes_runner)
{
    size_t comm_size_runner = local_vect_sizes_runner.size();
    std::vector <Part> parts;
    size_t local_vect_sizes_server[comm_size_server];
    size_t global_vect_size = 0;
    for (size_t i = 0; i < comm_size_runner; ++i)
    {
        global_vect_size += local_vect_sizes_runner[i];
    }

    for (int i = 0; i < comm_size_server; ++i)
    {
        // every server rank gets the same amount
        local_vect_sizes_server[i] = global_vect_size /
                                     comm_size_server;

        // let n be the rest of this division
        // the first n server ranks get one more to split the rest fair up...
        size_t n_rest = global_vect_size - size_t(global_vect_size /
                                                  comm_size_server) *
                        comm_size_server;
        if (size_t(i) < n_rest)
        {
            local_vect_sizes_server[i]++;
        }
    }

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
