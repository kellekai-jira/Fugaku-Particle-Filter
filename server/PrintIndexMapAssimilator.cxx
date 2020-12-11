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

    gather_and_print(myfile, field.local_index_map, print_it);

    if (print_it)
    {
        myfile.close();
        myfile.open ("index-map-hidden.csv", std::ios::trunc);
        myfile << "index_map_hidden" << std::endl;
    }
    gather_and_print(myfile, field.local_index_map_hidden, print_it);

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

void PrintIndexMapAssimilator::gather_and_print(std::ofstream &os,
        const std::vector<INDEX_MAP_T> & local_index_map, bool print_it)

{
    std::vector<int> local_index_map_sizes(mpi.size());
    int local_index_map_size = local_index_map.size();
    MPI_Gather(&local_index_map_size, 1, MPI_INT, local_index_map_sizes.data(), 1,
            MPI_INT, 0, mpi.comm());
    std::vector<INDEX_MAP_T> global_index_map(sum_vec(local_index_map_sizes));
    int displs[mpi.size()];
    int last_displ = 0;
    int rcounts [mpi.size()];
    // convert to int...
    std::copy(local_index_map_sizes.begin(), local_index_map_sizes.end(), rcounts);
    for (int i=0; i<mpi.size(); ++i) {
        displs[i] = last_displ;
        last_displ += local_index_map_sizes.at(i);
    }

    MPI_Gatherv( local_index_map.data(), local_index_map.size(), MPI_INDEX_MAP_T,
            global_index_map.data(), rcounts, displs, MPI_INDEX_MAP_T, 0, mpi.comm());

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
