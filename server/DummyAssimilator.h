/*
 * DummyAssimilator.h
 *
 *  Created on: Aug 22, 2019
 *      Author: friese
 */

#ifndef DUMMYASSIMILATOR_H_
#define DUMMYASSIMILATOR_H_

#include "Assimilator.h"


class DummyAssimilator : public Assimilator
{
private:
    Field & field;
    const int total_steps;
    MpiManager & mpi;
public:
    DummyAssimilator(Field & field_, const int total_steps, MpiManager & mpi_);
    virtual int do_update_step(const int current_step);
};

#endif /* DUMMYASSIMILATOR_H_ */
