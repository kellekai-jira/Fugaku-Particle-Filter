/*
 * EnsembleMember.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef ENSEMBLEMEMBER_H_
#define ENSEMBLEMEMBER_H_

#include <vector>

#include "Part.h"

class EnsembleMember
{
public:
    std::vector<double> state_analysis;
    std::vector<double> state_background;

    std::vector<double> state_hidden;

    void set_local_vect_size(const int local_vect_size, const int
                             local_vect_size_hidden);
    void store_background_state_part(const Part & part, const
                                     double * values, const Part & hidden_part,
                                     const double * hidden_values);
};


#endif /* ENSEMBLEMEMBER_H_ */
