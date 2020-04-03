/*
 * update-step.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef ASSIMILATOR_H_
#define ASSIMILATOR_H_

#include <memory>
#include "Field.h"
#include "MpiManager.h"

enum AssimilatorType
{
    ASSIMILATOR_DUMMY = 0,
    ASSIMILATOR_PDAF = 1,
    ASSIMILATOR_EMPTY = 2,
    ASSIMILATOR_CHECK_STATELESS = 3
};

class Assimilator
{
protected:
    int nsteps = -1;
    // FIXME: probably need to add mpi manager here!
public:
// REM: the constructor must init the ensemble! (analysis states!)
// called if every state was saved.
// TODO: at the moment we do not need those things, but later it might be good to use them to diminish coupling....
// void put_part(const Part & part, const double * data);
// double * get_part(const Part & part) = 0;


// returns how many steps must be performed by the model in the next iteration
// returns -1 if it wants to quit.
    virtual int do_update_step() = 0;

// executed when first state messages are received from the simulations.
// normally such messages are discarded and a proper analysis state is sent back
// see the CheckStatelessAssimilator to see how to use this function.
    virtual void on_init_state(const int runner_id, const Part & part, const
                               double * values, const Part & hidden_part,
                               const double * values_hidden)
    {
    };


    int getNSteps() const {
        return nsteps;
    };

    virtual ~Assimilator() = default;

    static std::shared_ptr<Assimilator> create(AssimilatorType assimilator_type,
                                               Field & field, const int
                                               total_steps);
};




#endif /* ASSIMILATOR_H_ */
