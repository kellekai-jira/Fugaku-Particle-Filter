/*
 * CheckStatelessAssimilator.h
 *
 *  Created on: Jan 16, 2020
 *      Author: friese
 */



// This assimilator will not produce a right data assimilation but is used to check
// if a simulation is stateless or not.

#ifndef CHECKSTATELESSASSIMILATOR_H_
#define CHECKSTATELESSASSIMILATOR_H_

#include "Assimilator.h"

class CheckStatelessAssimilator : public Assimilator
{
    Field & field;
private:
    std::vector<std::vector<double> > init_states;
    std::vector<std::vector<double> > correct_states;

    void print_result(const bool good);
    void store_init_state_part(const int ensemble_member_id, const Part & part,
                               const
                               double * values);

public:
    CheckStatelessAssimilator(Field & field_, const int total_steps);
    virtual int do_update_step();
    virtual void on_init_state(const int runner_id, const Part & part, const
                               double * values);

};

#endif /* CHECKSTATELESSASSIMILATOR_H_ */
