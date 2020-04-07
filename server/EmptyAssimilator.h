/*
 * EmptyAssimilator.h
 *
 *  Created on: Nov 18, 2019
 *      Author: friese
 */

#ifndef EMPTYASSIMILATOR_H_
#define EMPTYASSIMILATOR_H_

#include "Assimilator.h"


class EmptyAssimilator : public Assimilator
{
private:
    Field & field;
    const int total_steps;
    MpiManager & mpi;
public:
    EmptyAssimilator(Field & field_, const int total_steps, MpiManager & mpi_);
    virtual int do_update_step(const int current_step);
};

#endif /* EMPTYASSIMILATOR_H_ */
