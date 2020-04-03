#include <mpi.h>
#include <cassert>
#include <vector>
#include <algorithm>

#include <unistd.h>

#include <iostream>
#include <fstream>

#include "../api/melissa_api.h"

#include <csignal>


int GLOBAL_VECT_SIZE = 40;
// const int GLOBAL_VECT_SIZE = 1000;
// const int GLOBAL_VECT_SIZE = 1000*100*10;
// const int GLOBAL_VECT_SIZE = 1000*1000*10;

using namespace std;

int comm_rank = -1;
#define printf(x ...) if (comm_rank == 0) { printf(x); }

int main(int argc, char * args[])
{
    if (argc > 1)
    {
        GLOBAL_VECT_SIZE = atoi(args[1]);
        printf("Changed global vect size to %d\n", GLOBAL_VECT_SIZE);
    }

    int comm_size;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

    int local_vect_size = GLOBAL_VECT_SIZE / comm_size;
    // do I have one more?
    if (GLOBAL_VECT_SIZE % comm_size != 0 && comm_rank < GLOBAL_VECT_SIZE %
        comm_size)
    {
        local_vect_size += 1;
    }

    // how many pixels are left of me?
    vector<int> offsets(comm_size);
    vector<int> counts(comm_size);
    fill(offsets.begin(), offsets.end(), 0);
    fill(counts.begin(), counts.end(), 0);
    int next_offset = 0;
    for (int rank = 0; rank < comm_size; rank++)
    {
        offsets[rank] = next_offset;

        counts[rank] = GLOBAL_VECT_SIZE / comm_size;
        // add one for all who have one more.
        if (GLOBAL_VECT_SIZE % comm_size != 0 && rank <
            GLOBAL_VECT_SIZE % comm_size)
        {
            counts[rank] += 1;
        }

        next_offset += counts[rank];
    }

    melissa_init("variableX",
                 local_vect_size,
                 0,
                 MPI_COMM_WORLD);         // do some crazy shit (dummy mpi implementation?) if we compile without mpi.
    vector<double> state1(local_vect_size);
    fill(state1.begin(), state1.end(), 0);
    // printf("offset %d on rank %d \n", offsets[comm_rank], comm_rank);


    static bool is_first_timestep = true;
    int nsteps = 1;
    do
    {

        // simulate some calculation
        // If the simulations are too fast our testcase will not use all model task runners (Assimilation stopped before they could register...)
        // usleep(10000);
        // usleep(1000000);

        nsteps = melissa_expose("variableX", state1.data(), nullptr);
        // printf("calculating from timestep %d\n",
        //       melissa_get_current_step());

        if (nsteps > 0 && is_first_timestep)
        {
            printf("First time step to propagate: %d\n",
                   melissa_get_current_step());
            is_first_timestep = false;
        }

        // TODO does not work if we remove this for reasons.... (it will schedule many many things as simulation ranks finish too independently!
        MPI_Barrier(MPI_COMM_WORLD);
    } while (nsteps > 0);
    int ret = MPI_Finalize();
    assert(ret == MPI_SUCCESS);
}


