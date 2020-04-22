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

void PrintIndexMapAssimilator::gather_and_print(std::ofstream &os, size_t global_vect_size,
        size_t local_vect_size, const int local_index_map_data[], bool print_it)

{
    std::vector<int> global_index_map(global_vect_size);
    std::vector<size_t> local_vect_sizes(mpi.size());
    MPI_Gather(&local_vect_size, 1, my_MPI_SIZE_T, local_vect_sizes.data(), 1,
            my_MPI_SIZE_T, 0, mpi.comm());
    int displs[mpi.size()];
    int last_displ = 0;
    int rcounts [mpi.size()];
    // convert to int...
    std::copy(local_vect_sizes.begin(), local_vect_sizes.end(), rcounts);
    for (int i=0; i<mpi.size(); ++i) {
        displs[i] = last_displ;
        last_displ += local_vect_sizes[i];
    }

    MPI_Gatherv( local_index_map_data, local_vect_size, MPI_INT,
            global_index_map.data(), rcounts, displs, MPI_INT, 0, mpi.comm());

    if (print_it)
    {
        for (auto it : global_index_map)
        {
            os << it << std::endl;
        }
    }
}
int PrintIndexMapAssimilator::do_update_step(const int current_step) {

    L("Doing Printing Index Map...\n");
    MPI_Barrier(mpi.comm());




    D("Gathered file");

    std::ofstream myfile;

    bool print_it = mpi.rank() == 0;

    if (print_it)
    {
        D("Writing file");
        // Rewrite file every update step...
        myfile.open ("index-map.csv", std::ios::trunc);

        myfile << "index_map" << std::endl;

    }

    print_vector(field.local_index_map);
    print_vector(field.local_index_map_hidden);
    gather_and_print(myfile, field.globalVectSize(),
            field.local_vect_size, field.local_index_map.data(), print_it);

    if (print_it)
    {
        myfile << "index_map_hiddden" << std::endl;
    }
    gather_and_print(myfile, field.globalVectSizeHidden(),
            field.local_vect_size_hidden, field.local_index_map_hidden.data(), print_it);

    if (print_it)
    {
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
