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
#include "utils.h"

// FIXME: move all assimilators into folder

enum AssimilatorType
{
    ASSIMILATOR_DUMMY = 0,
    ASSIMILATOR_PDAF = 1,
    ASSIMILATOR_EMPTY = 2,
    ASSIMILATOR_CHECK_STATELESS = 3,
    ASSIMILATOR_PRINT_INDEX_MAP = 4,
    ASSIMILATOR_WRF = 5,
    ASSIMILATOR_PYTHON = 6
};

class Assimilator
{
protected:
    int nsteps = -1;
public:
// REM: the constructor must init the ensemble! (analysis states!)
// called if every state was saved.
// TODO: at the moment we do not need those things, but later it might be good to use them to diminish coupling....
// void put_part(const Part & part, const VEC_T * data);
// VEC_T * get_part(const Part & part) = 0;


// returns how many steps must be performed by the model in the next iteration
// returns -1 if it wants to quit.
    virtual int do_update_step(const int current_step) = 0;

// executed when first state messages are received from the simulations.
// normally such messages are discarded and a proper analysis state is sent back
// see the CheckStatelessAssimilator to see how to use this function.
    virtual void on_init_state(const int runner_id, const Part & part, const
                               VEC_T * values, const Part & hidden_part,
                               const VEC_T * values_hidden)
    {
    };


    int getNSteps() const {
        return nsteps;
    };

    virtual ~Assimilator() = default;

    static std::shared_ptr<Assimilator> create(AssimilatorType assimilator_type,
                                               Field & field, const int
                                               total_steps,
                                               MpiManager & _mpi);
};




#endif /* ASSIMILATOR_H_ */
