/*
 * PDAFEnKFAssimilator.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef PDAFASSIMILATOR_H_
#define PDAFASSIMILATOR_H_

#include "Field.h"
#include <vector>
#include "Assimilator.h"

class PDAFAssimilator : public Assimilator
{
private:
    Field & field;
    MpiManager & mpi;
    std::vector<int> index_map_transformed;
    std::vector<int> index_map_transformed_hidden;

    void getAllEnsembleMembers();
public:
    ~PDAFAssimilator();
    PDAFAssimilator(Field & field, const int total_steps, MpiManager & mpi);
    virtual int do_update_step(const int current_step);
};

#endif /* PDAFASSIMILATOR_H_ */
