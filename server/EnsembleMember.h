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

#include "utils.h"

class EnsembleMember
{
public:
    std::vector<VEC_T> state_analysis;
    std::vector<VEC_T> state_background;

    std::vector<VEC_T> state_hidden;

    void set_local_vect_size(const int local_vect_size, const int
                             local_vect_size_hidden);
    void store_background_state_part(const Part & part, const
                                     VEC_T * values, const Part & hidden_part,
                                     const VEC_T * hidden_values);
};


#endif /* ENSEMBLEMEMBER_H_ */
