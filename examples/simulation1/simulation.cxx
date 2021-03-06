#include <mpi.h>
#include <cassert>
#include <vector>
#include <algorithm>

#include <unistd.h>

#include <iostream>
#include <fstream>

#include <melissa_da_api.h>

#include <csignal>


//int GLOBAL_VECT_SIZE = 40* 10000;
int GLOBAL_VECT_SIZE = 40;
// const int GLOBAL_VECT_SIZE = 1000;
// const int GLOBAL_VECT_SIZE = 1000*100*10;
// const int GLOBAL_VECT_SIZE = 1000*1000*10;

// Define this if you want to check if the statefullnes test can fail:
// #define BE_STATEFUL // defined by cmake for the state ful simulation exec

using namespace std;

int main(int argc, char * args[])
{
#ifdef BE_STATEFUL
    vector<double> secret_state {3.0, 4.0, 5.0};
#endif
    if (argc > 1)
    {
        GLOBAL_VECT_SIZE = atoi(args[1]);
        printf("Changed global vect size to %d\n", GLOBAL_VECT_SIZE);
    }

    int comm_size;
    int comm_rank;

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
    vector<double> state1(local_vect_size);

#ifdef WITH_INDEX_MAP
    std::vector<INDEX_MAP_T> local_index_map(state1.size());
    int last_entry = 0;
    int entry = 1;
    int new_entry;
    // calculate entries left of this rank:
    for (int i = 0; i < offsets[comm_rank]; i++) {
        new_entry = last_entry + entry;
        last_entry = entry;
        entry = new_entry;
    }

    // now fill vector elements:
//    int i = offsets[comm_rank];
    for (auto &it : local_index_map) {
        new_entry = last_entry + entry;
        last_entry = entry;
        entry = new_entry;
        it.index = new_entry;
//        it.index = i++;
        it.varid = 1;
        cout << "index_map_entry: " << it.varid << ':' << it.index << std::endl;
    }




#ifdef USE_HIDDEN_STATE
    std::vector<INDEX_MAP_T> local_index_map_hidden(secret_state.size());
    last_entry = 0;
    entry = 1;
    for (int i = 0; i < comm_rank * secret_state.size(); i++) {
        new_entry = last_entry + entry;
        last_entry = entry;
        entry = new_entry;
    }

    // now fill vector elements:
    for (auto &it : local_index_map_hidden) {
        new_entry = last_entry + entry;
        last_entry = entry;
        entry = new_entry;
        it.index = new_entry;
        it.varid = 2;
        cout << "index_map_entry (hidden): " << it.varid << ':' << it.index << std::endl;
    }

#endif


#endif

#ifdef USE_HIDDEN_STATE
#ifdef WITH_INDEX_MAP
    melissa_init_with_index_map("variableX",
                 local_vect_size*sizeof(double),
                 secret_state.size()*sizeof(double),
                 sizeof(double),
                 sizeof(double),
                 MPI_COMM_WORLD,
                 local_index_map.data(),
                 local_index_map_hidden.data()
                 );
#else
    melissa_init("variableX",
                 local_vect_size*sizeof(double),
                 secret_state.size()*sizeof(double),
                 sizeof(double),
                 sizeof(double),
                 MPI_COMM_WORLD
                 );
#endif
#else
#ifdef WITH_INDEX_MAP
    melissa_init_with_index_map("variableX",
                 local_vect_size*sizeof(double),
                 0,
                 sizeof(double),
                 sizeof(double),
                 MPI_COMM_WORLD,
                 local_index_map.data(),
                 nullptr
                 );
#else
    melissa_init("variableX",
                 local_vect_size*sizeof(double),
                 0,
                 sizeof(double),
                 sizeof(double),
                 MPI_COMM_WORLD);         // do some crazy shit (dummy mpi implementation?) if we compile without mpi.
#endif
#endif

    fill(state1.begin(), state1.end(), 0);
    printf("offset %d on rank %d \n", offsets[comm_rank], comm_rank);


    static bool is_first_timestep = true;
    int nsteps = 1;
    do
    {
#ifdef BE_STATEFUL
        // advance secret state
        secret_state.at(0)++;
        secret_state.at(1)++;
#endif
        for (int step = 0; step < nsteps; step++)
        {
            int i = 0;
            for (auto it = state1.begin(); it != state1.end(); it++)
            {
                *it += offsets[comm_rank] + i;
#ifdef BE_STATEFUL
                *it += secret_state.at(0) + secret_state.at(1);
#endif

                i++;
            }
        }

        // simulate some calculation
        // If the simulations are too fast our testcase will not use all model task runners (Assimilation stopped before they could register...)
        //usleep(10000);
        // usleep(800000);
        // Activate the following e.g. for elasticity testing
        uint32_t sd = rand()%1000 * 2000;  // 2 s max
        usleep(sd);
        printf("Worked %d s \n", sd/1000000);


#ifdef USE_HIDDEN_STATE
        nsteps = melissa_expose_d("variableX", state1.data(),
                                secret_state.data());
#else
        nsteps = melissa_expose_d("variableX", state1.data(), nullptr);
#endif

#ifdef DEADLOCK
        while (true) {
            sleep(1);
        }
#endif

        if (nsteps > 0 && is_first_timestep)
        {
            printf("First timestep to propagate: %d\n",
                   melissa_get_current_step());
            is_first_timestep = false;

#ifndef USE_HIDDEN_STATE
#ifdef BE_STATEFUL
            // Recover secret/hidden state from current_step
            int step = melissa_get_current_step();
            for (int i = 1; i < step; ++i)
            {
                // advance secret state:
                secret_state.at(0)++;
                secret_state.at(1)++;
            }
#endif
#endif
        }

        // TODO does not work if we remove this for reasons.... (it will schedule many many things as simulation ranks finish too independently!
        MPI_Barrier(MPI_COMM_WORLD);

        // file output of allways ensemble member 0
        // TODO: maybe move this functionality into api?
        if (nsteps > 0 && melissa_get_current_state_id() == 1)
        {
            // raise(SIGINT);
            if (comm_rank == 0)
            {
                ofstream myfile;
                // 1 is the smallest time step.
                if (melissa_get_current_step() == 1)
                {
                    // remove old file...
                    myfile.open ("output.txt", ios::trunc);
                }
                else
                {
                    myfile.open ("output.txt", ios::app);
                }
                vector<double> full_state(GLOBAL_VECT_SIZE);

                MPI_Gatherv(state1.data(), state1.size(),
                            MPI_DOUBLE,
                            full_state.data(), counts.data(),
                            offsets.data(),
                            MPI_DOUBLE, 0, MPI_COMM_WORLD);
                for (auto it = full_state.begin(); it !=
                     full_state.end(); it++)
                {
                    myfile << *it << ",";
                }
                myfile << "\n";
                myfile.close();
            }
            else
            {
                MPI_Gatherv(state1.data(), state1.size(),
                            MPI_DOUBLE,
                            NULL, NULL, NULL,
                            MPI_DOUBLE, 0, MPI_COMM_WORLD);
            }

        }

    } while (nsteps > 0);
    int ret = MPI_Finalize();
    assert(ret == MPI_SUCCESS);
}


