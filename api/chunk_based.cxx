
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
    STYPE * values;
    const int size_per_element;
    const int amount;
    const bool is_assimilated;

    Chunk(const int varid, const int * index_map, STYPE * values,
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
        const int amount, const bool is_assimilated)
{
    chunks.push_back(Chunk(varid, index_map, reinterpret_cast<STYPE *>(values), sizeof(T),
                amount, is_assimilated));
}

// chunk adder functions for each fortran data type!
// real, integer, double, logical, char
#define add_chunk_wrapper(TYPELETTER, CTYPE) \
    void melissa_add_chunk_##TYPELETTER(const int * varid, const int * index_map, \
            CTYPE * values, const int * amount, \
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
    std::for_each(chunks.cbegin(), chunks.cend(),
            [&hidden_size, &assimilated_size](const Chunk& chunk) {
            // low: one could calculate this only once on the fly while adding chunks...
                if (chunk.is_assimilated) {
                    // we converte the assimilated state always into doubles!
                    assimilated_size += chunk.size_per_element * chunk.amount;
                } else {
                    hidden_size += chunk.size_per_element * chunk.amount;
                }
            });

    static bool is_inited = false;

    // Init on first expose
    if (!is_inited) {

        MPI_Comm comm = MPI_Comm_f2c(*comm_fortran);
        melissa_init("data",  // TODO: actually this is nonsense to give a field name here!
                  assimilated_size,
                  hidden_size,
                  1,  // do it with 1 byte per element and write in the index....
                  1,  //  TODO; write a proper index max!
                  comm);
        is_inited = true;
    }


    std::vector<STYPE> buf_assimilated(assimilated_size);
    std::vector<STYPE> buf_hidden(hidden_size);

    // FIXME: for now we are not inplace at ALL! But thats ok as this is a proof of
    // concept!

    // Model -> buffer
    STYPE * pos_hidden = reinterpret_cast<STYPE*>(buf_hidden.data());
    STYPE * pos_assimilated = reinterpret_cast<STYPE*>(buf_assimilated.data());
    std::for_each(chunks.cbegin(), chunks.cend(),
            [&pos_assimilated, &pos_hidden](const Chunk& chunk) {
                const size_t bytes_to_copy = chunk.size_per_element * chunk.amount;
                if (chunk.is_assimilated) {
                    memcpy(pos_assimilated, chunk.values, bytes_to_copy);
                    pos_assimilated += bytes_to_copy;
                } else {
                    memcpy(pos_hidden, chunk.values, bytes_to_copy);
                    pos_hidden += bytes_to_copy;
                }
            });

    // expose buffer
    melissa_expose("data", buf_assimilated.data(), buf_hidden.data());

    // buffer -> model
    pos_hidden = reinterpret_cast<STYPE*>(buf_hidden.data());
    pos_assimilated = reinterpret_cast<STYPE*>(buf_assimilated.data());
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
}



