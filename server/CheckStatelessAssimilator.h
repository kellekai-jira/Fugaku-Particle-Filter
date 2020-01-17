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

class CheckStatelessAssimilator: public Assimilator {
    Field & field;
private:
    const double mean_value = 4.3;
    const double magnitude  = 0.1;
    std::vector<std::vector<double>> init_states;
    std::vector<std::vector<double>> correct_states;
public:
    CheckStatelessAssimilator(Field & field_);
    virtual int do_update_step();
};

#endif /* CHECKSTATELESSASSIMILATOR_H_ */
