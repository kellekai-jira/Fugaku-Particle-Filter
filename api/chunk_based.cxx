
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <mpi.h>
#include <array>
#include "melissa_api.h"
#include "utils.h"

struct Chunk {
    const int varid;
    const int * index_map;
    VEC_T * values;
    const int size_per_element;
    const size_t amount;
    const bool is_assimilated;

    Chunk(const int varid, const int * index_map, VEC_T * values,
            const int size_per_element,
            const size_t amount, const bool is_assimilated)
        :  varid(varid), index_map(index_map), values(values),
        size_per_element(size_per_element), amount(amount),
        is_assimilated(is_assimilated) {}
};

// TODO: later have chunk notioin also on server as it might be useful to init everything!
std::vector<Chunk> chunks;
template <class T>
void melissa_add_chunk(const int varid, const int * index_map, T * values,
        const size_t amount, const bool is_assimilated)
{
    D("Adding Chunk(varid=%d) with amount %lu", varid, amount);
    chunks.push_back(Chunk(varid, index_map, reinterpret_cast<VEC_T *>(values), sizeof(T),
                amount, is_assimilated));
}

// chunk adder functions for each fortran data type!
// real, integer, double, logical, char
#define add_chunk_wrapper(TYPELETTER, CTYPE) \
    void melissa_add_chunk_##TYPELETTER(const int * varid, const int * index_map, \
            CTYPE * values, const size_t * amount, \
            const int * is_assimilated) \
        { melissa_add_chunk(*varid, index_map, values, *amount, (*is_assimilated) != 0); } \
    void melissa_add_chunk_##TYPELETTER##_d(const int * varid, const int * index_map, \
            CTYPE * values, const size_t * amount, \
            const int * is_assimilated) \
        { melissa_add_chunk(*varid, index_map, values, *amount, (*is_assimilated) != 0); }

    add_chunk_wrapper(r, float)
    add_chunk_wrapper(i, int)
    add_chunk_wrapper(d, double)
    add_chunk_wrapper(l, bool)
    add_chunk_wrapper(c, char)

#undef add_chunk_wrapper

int melissa_commit_chunks_f(MPI_Fint * comm_fortran) {
    size_t hidden_size = 0;
    size_t assimilated_size = 0;


    // Calculate size to send
    for (const auto & chunk : chunks) {
        // low: one could calculate this only once on the fly while adding chunks...
        if (chunk.is_assimilated) {
            // we converte the assimilated state always into doubles!
            assimilated_size += chunk.size_per_element * chunk.amount;
        } else {
            hidden_size += chunk.size_per_element * chunk.amount;
        }
    }

    static bool is_inited = false;

    // Init on first expose
    if (!is_inited) {

        // TODO; we might need another approach here! the index map is 8 * as big as the actual data now!
        std::vector<INDEX_MAP_T> global_index_map;
        std::vector<INDEX_MAP_T> global_index_map_hidden;
        for (const auto & c : chunks) {
            const size_t bytes = c.size_per_element * c.amount;
            int j = -1;
            for (size_t i = 0; i < bytes; i++) {
                if (i % c.size_per_element == 0) {
                    j++;
                }
                if (c.is_assimilated) {
                    global_index_map.push_back({c.index_map[j], c.varid});
                } else {
                    global_index_map_hidden.push_back({c.index_map[j], c.varid});
                }
            }
        }


        MPI_Comm comm = MPI_Comm_f2c(*comm_fortran);
        melissa_init_with_index_map("data",  // TODO: actually this is nonsense to give a field name here!
                  assimilated_size,
                  hidden_size,
                  1,  // do it with 1 byte per element and write in the index....
                  1,  //  TODO; write a proper index max!
                  comm,
                  global_index_map.data(),
                  global_index_map_hidden.data());
        is_inited = true;
    }


    std::vector<VEC_T> buf_assimilated(assimilated_size);
    std::vector<VEC_T> buf_hidden(hidden_size);

    // FIXME: for now we are not inplace at ALL! But thats ok as this is a proof of
    // concept!

    // Model -> buffer
    VEC_T * pos_hidden = reinterpret_cast<VEC_T*>(buf_hidden.data());
    VEC_T * pos_assimilated = reinterpret_cast<VEC_T*>(buf_assimilated.data());

    for (const auto &chunk : chunks) {
        const size_t bytes_to_copy = chunk.size_per_element * chunk.amount;
        if (chunk.is_assimilated) {
            memcpy(pos_assimilated, chunk.values, bytes_to_copy);
            pos_assimilated += bytes_to_copy;
        } else {
            memcpy(pos_hidden, chunk.values, bytes_to_copy);
            pos_hidden += bytes_to_copy;
        }
    }

    // expose buffer
    int nsteps = melissa_expose("data", buf_assimilated.data(), buf_hidden.data());

    // buffer -> model
    pos_hidden = reinterpret_cast<VEC_T*>(buf_hidden.data());
    pos_assimilated = reinterpret_cast<VEC_T*>(buf_assimilated.data());
    std::for_each(chunks.begin(), chunks.end(),
            [&pos_assimilated, &pos_hidden](const Chunk& chunk) {
                const size_t bytes_to_copy = chunk.size_per_element * chunk.amount;
                if (chunk.is_assimilated) {
                    memcpy(chunk.values, pos_assimilated, bytes_to_copy);
                    pos_assimilated += bytes_to_copy;
                } else {
                    memcpy(chunk.values, pos_hidden, bytes_to_copy);
                    pos_hidden += bytes_to_copy;
                }
            });

    // for now we recreate the chunk list before each expose so we can remove it here:
    chunks.clear();

    return nsteps;
}



