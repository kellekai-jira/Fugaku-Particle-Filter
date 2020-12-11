/*
 * PrintIndexMapAssimilator.h
 *
 *  Created on: Apr 20, 2020
 *      Author: friese
 */

#ifndef PRINTINDEXMAPASSIMILATOR_H_
#define PRINTINDEXMAPASSIMILATOR_H_

#include "Assimilator.h"


class PrintIndexMapAssimilator : public Assimilator
{
private:
    Field & field;
    MpiManager & mpi;

    void gather_and_print(std::ofstream &os,
        const std::vector<INDEX_MAP_T> & local_index_map, bool print_it);
    void index_map_to_file();
public:
    PrintIndexMapAssimilator(Field & field_, const int total_steps, MpiManager & mpi_);
    virtual int do_update_step(const int current_step);
};

#endif /* PRINTINDEXMAPASSIMILATOR_H_ */
