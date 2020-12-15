#include "utils.h"
#include <cassert>
#include "zmq.h"
#include <cstring>

#include <mpi.h>
#include <iostream>
#include <fstream>

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



#define pass MPI_Barrier(comm); return
void slow_MPI_Scatterv(const void *sendbuf, const size_t *sendcounts, const size_t *displs,
                 MPI_Datatype sendtype, void *recvbuf, size_t recvcount,
                 MPI_Datatype recvtype,
                 int root, MPI_Comm comm)
{
    pass;


    assert(sendtype == recvtype);

    int rank, type_size;
    MPI_Comm_rank(comm, &rank);
    MPI_Type_size(sendtype, &type_size);

    assert(root == 0);  // not messing with modulo here so other stuff is not implemented

    std::string base_name("/tmp/gather");
    if (rank == root) {
        const char * p = reinterpret_cast<const char*>(sendbuf);
        size_t cumul_send_counts = 0;
        int size;
        MPI_Comm_size(comm, &size);
        for (int i = root; i < size; ++i)
        {
            assert(cumul_send_counts = displs[i]);  // atm displs does not really work

            std::string name = base_name + std::to_string(i);
            std::ofstream os(name, std::ios::binary | std::ios::app);
            os.write(p, sendcounts[i]);
            p += sendcounts[i];
            cumul_send_counts += sendcounts[i];
        }
    }
    MPI_Barrier(comm);

    std::string name = base_name + std::to_string(rank);
    std::ifstream is(name, std::ios::ate | std::ios::binary);

    std::streamsize read_size = is.tellg();
    is.seekg(0, std::ios::beg);

    assert(read_size == recvcount * type_size);
    is.read(reinterpret_cast<char*>(recvbuf), read_size);
}


void slow_MPI_Gatherv(const void *sendbuf, size_t sendcount, MPI_Datatype sendtype,
                void *recvbuf, const size_t *recvcounts, const size_t *displs,
                MPI_Datatype recvtype, int root, MPI_Comm comm)
{
    pass;
    assert(sendtype == recvtype);

    int rank;
    MPI_Comm_rank(comm, &rank);

    assert(root == 0);  // not messing with modulo here so other stuff is not implemented

    std::string base_name("/tmp/scatter");
    int type_size;
    MPI_Type_size(sendtype, &type_size);

    std::string name = base_name + std::to_string(rank);
    std::ofstream os(name, std::ios::binary | std::ios::app);
    os.write(reinterpret_cast<const char*>(sendbuf), sendcount * type_size);
    os.close();

    MPI_Barrier(comm);

    if (rank == root) {
        int size;
        MPI_Comm_size(comm, &size);
        for (int i = root; i < size; ++i)
        {
            std::string name = base_name + std::to_string(i);
            std::ifstream is(name, std::ios::binary | std::ios::ate);
            std::streamsize read_size = is.tellg();
            is.seekg(0, std::ios::beg);

            assert(read_size == recvcounts[i] * type_size);
            is.read(&reinterpret_cast<char *>(recvbuf)[displs[i]*type_size],
                        read_size);
        }
    }


}
