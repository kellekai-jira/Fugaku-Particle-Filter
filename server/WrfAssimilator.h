/*
 * WrfAssimilator.h
 *
 *  Created on: Dec 8, 2020
 *      Author: friese
 */

#ifndef WRFASSIMILATOR_H_
#define WRFASSIMILATOR_H_

#include "Assimilator.h"


class WrfAssimilator : public Assimilator
{
private:
    Field & field;
    const int total_steps;
    MpiManager & mpi;
public:
    WrfAssimilator(Field & field_, const int total_steps, MpiManager & mpi_);
    virtual int do_update_step(const int current_step);

    virtual void on_init_state(const int runner_id, const
                                              Part & part, const
                                              VEC_T * values, const
                                              Part & hidden_part,
                                              const VEC_T * values_hidden);
};

#endif /* WRFASSIMILATOR_H_ */
