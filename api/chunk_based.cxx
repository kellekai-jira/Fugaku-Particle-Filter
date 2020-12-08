
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <mpi.h>
#include "melissa_api.h"

struct Chunk {
    void * values;
    const size_t size_per_element;
    const size_t amount;
    const bool is_assimilated;

    Chunk(void * values, const size_t size_per_element,
            const size_t amount, const bool is_assimilated)
        :  values(values), size_per_element(size_per_element), amount(amount),
        is_assimilated(is_assimilated) {}
};

// TODO: later have chunk notioin also on server as it might be useful to init everything!
std::vector<Chunk> chunks;
template <class T>
void melissa_add_chunk(T * values, const size_t amount, const bool is_assimilated)
{
    if (is_assimilated) {
        assert(sizeof(T) == sizeof(double)); // TODO: assert that T is double actually!
    }


    chunks.push_back(Chunk(reinterpret_cast<void *>(values), sizeof(T), amount,
                is_assimilated));


}

// chunk adder functions for each fortran data type!
// real, integer, double, logical, char
#define add_chunk_wrapper(TYPELETTER, CTYPE) \
    void melissa_add_chunk_##TYPELETTER(CTYPE * values, const int amount, \
            const int is_assimilated) \
        { melissa_add_chunk(values, amount, is_assimilated == 0); }

    add_chunk_wrapper(r, float)
    add_chunk_wrapper(i, int)
    add_chunk_wrapper(d, double)
    add_chunk_wrapper(l, bool)
    add_chunk_wrapper(c, char)

#undef add_chunk_wrapper

int melissa_commit_chunks(MPI_Comm comm_) {
    size_t hidden_size = 0;
    size_t assimilated_size = 0;

    // Calculate size to send
    std::for_each(chunks.cbegin(), chunks.cend(),
            [&hidden_size, &assimilated_size](const Chunk& chunk) {
            // low: one could calculate this only once on the fly while adding chunks...
                if (chunk.is_assimilated) {
                    assimilated_size += chunk.size_per_element * chunk.amount;
                } else {
                    hidden_size += chunk.size_per_element * chunk.amount;
                }
            });

    static bool is_inited = false;
    const size_t hidden_size_in_doubles = std::ceil(hidden_size / sizeof(double));
    const size_t assimilated_size_in_doubles = std::ceil(assimilated_size / sizeof(double));

    // Init on first expose
    if (!is_inited) {
        melissa_init("data",  // TODO: actually this is nonsense to give a field name here!
                  assimilated_size_in_doubles,
                  hidden_size_in_doubles,
                  comm_);
        is_inited = true;
    }


    std::vector<double> buf_assimilated(assimilated_size_in_doubles);
    std::vector<double> buf_hidden(hidden_size_in_doubles);

    // FIXME: for now we are not inplace at ALL! But thats ok as this is a proof of
    // concept!

    // Model -> buffer
    void * pos_assimilated = reinterpret_cast<void*>(buf_assimilated.data());
    void * pos_hidden = reinterpret_cast<void*>(buf_hidden.data());
    std::for_each(chunks.cbegin(), chunks.cend(),
            [&buf_hidden, &buf_assimilated, &pos_assimilated, &pos_hidden](const Chunk& chunk) {
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
    pos_assimilated = reinterpret_cast<void*>(buf_assimilated.data());
    pos_hidden = reinterpret_cast<void*>(buf_hidden.data());
    std::for_each(chunks.begin(), chunks.end(),
            [&buf_hidden, &buf_assimilated, &pos_assimilated, &pos_hidden](const Chunk& chunk) {
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



