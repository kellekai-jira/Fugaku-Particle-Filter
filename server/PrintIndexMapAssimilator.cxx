/*
 * PrintIndexMapAssimilator.cxx
 *
 *  Created on: Apr 20, 2020
 *      Author: friese
 */

#include "PrintIndexMapAssimilator.h"
#include <algorithm>
#include <fstream>
#include <iostream>

#include "utils.h"

PrintIndexMapAssimilator::PrintIndexMapAssimilator(Field & field_, const int total_steps_, MpiManager & mpi_) :
    field(field_), total_steps(total_steps_), mpi(mpi_)
{
    nsteps = 1;

    // otherwise release mode will make problems!
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        // analysis state is enough:
        std::fill(ens_it->state_analysis.begin(),
                  ens_it->state_analysis.end(), 0.0);
    }

}

int PrintIndexMapAssimilator::do_update_step(const int current_step) {

    L("Doing Printing Index Map...\n");
    MPI_Barrier(mpi.comm());

    std::vector<int> global_index_map(field.globalVectSize());
    std::vector<size_t> local_vect_sizes(mpi.size());
    MPI_Gather(&field.local_vect_size, 1, my_MPI_SIZE_T, local_vect_sizes.data(), 1, my_MPI_SIZE_T, 0, mpi.comm());
    {
        int displs[comm_size];
        int last_displ = 0;
        int rcounts [comm_size];
        // move to int...
        std::copy(local_vect_sizes.begin(), local_vect_sizes.end(), rcounts);
        for (int i=0; i<comm_size; ++i) {
            displs[i] = last_displ;
            last_displ += local_vect_sizes[i];
        }

        MPI_Gatherv( field.local_index_map.data(), field.local_vect_size, MPI_INT,
                global_index_map.data(), rcounts, displs, MPI_INT, 0, mpi.comm());
    }

    std::vector<int> global_index_map_hidden(field.globalVectSize());
    std::vector<size_t> local_hidden_vect_sizes(mpi.size());
    MPI_Gather(&field.local_vect_size_hidden, 1, my_MPI_SIZE_T, local_hidden_vect_sizes.data(), 1, my_MPI_SIZE_T, 0, mpi.comm());
    {
        int displs[comm_size];
        int last_displ = 0;
        int rcounts [comm_size];
        // move to int...
        std::copy(local_hidden_vect_sizes.begin(), local_hidden_vect_sizes.end(), rcounts);
        for (int i=0; i<comm_size; ++i) {
            displs[i] = last_displ;
            last_displ += local_hidden_vect_sizes[i];
        }

        MPI_Gatherv( field.local_index_map_hidden.data(), field.local_vect_size_hidden, MPI_INT,
                global_index_map_hidden.data(), rcounts, displs, MPI_INT, 0, mpi.comm());
    }


    D("Gathered file");

    if (mpi.rank() == 0)
    {
        D("Writing file");
        std::ofstream myfile;
        // Rewrite file every update step...
        myfile.open ("index-map.csv", std::ios::trunc);

        myfile << "index_map" << std::endl;
        for (auto it : global_index_map)
        {
            myfile << it << std::endl;
        }

        myfile << "index_map_hiddden" << std::endl;
        for (auto it : global_index_map_hidden)
        {
            myfile << it << std::endl;
        }

        myfile.close();
    }

    if (current_step >= total_steps)
    {
        return -1;
    }
    else
    {
        return getNSteps();
    }
}
