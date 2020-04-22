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


void PrintIndexMapAssimilator::index_map_to_file()
{
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

    gather_and_print(myfile, field.globalVectSize(),
            field.local_vect_size, field.local_index_map.data(), print_it);

    if (print_it)
    {
        myfile.close();
        myfile.open ("index-map-hidden.csv", std::ios::trunc);
        myfile << "index_map_hiddden" << std::endl;
    }
    gather_and_print(myfile, field.globalVectSizeHidden(),
            field.local_vect_size_hidden, field.local_index_map_hidden.data(), print_it);

    if (print_it)
    {
        myfile.close();
    }
}

PrintIndexMapAssimilator::PrintIndexMapAssimilator(Field & field_, const int total_steps_, MpiManager & mpi_) :
    field(field_), mpi(mpi_)
{

    // otherwise release mode will make problems!
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        // analysis state is enough:
        std::fill(ens_it->state_analysis.begin(),
                  ens_it->state_analysis.end(), 0.0);
    }

    index_map_to_file();

    nsteps = 1;
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


    // Do not really assimilate...
    return -1;
}
