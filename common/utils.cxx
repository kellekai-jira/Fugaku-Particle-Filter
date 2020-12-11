#include "utils.h"
#include <cassert>
#include "zmq.h"
#include <cstring>


void check_data_types() {
    // check that the size_t datatype is the same on the server and on the client side! otherwise the communication might fail.
    // for sure this could be done more intelligently in future!
    D("sizeof(size_t)=%lu", sizeof(size_t));
    assert(sizeof(size_t) == 8);
}


MPI_Datatype MPI_MY_INDEX_MAP_T;
void create_MPI_INDEX_MAP_T() {

    MPI_Aint array_of_displacements[2] =
        {offsetof(INDEX_MAP_T, index), offsetof(INDEX_MAP_T, varid)};
    MPI_Datatype array_of_types[] =
        {MPI_INT, MPI_INT};
    const int counts[2] {1, 1};
    MPI_Type_create_struct(2,
                        counts,
                         array_of_displacements,
                         array_of_types,
                         &MPI_MY_INDEX_MAP_T);
    MPI_Type_commit(&MPI_MY_INDEX_MAP_T);
}

int comm_rank (-1);
int comm_size (-1);
Phase phase (PHASE_INIT);
