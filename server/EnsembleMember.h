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
    std::vector<STYPE> state_analysis;
    std::vector<STYPE> state_background;

    std::vector<STYPE> state_hidden;

    void set_local_vect_size(const int local_vect_size, const int
                             local_vect_size_hidden);
    void store_background_state_part(const Part & part, const
                                     STYPE * values, const Part & hidden_part,
                                     const STYPE * hidden_values);
};


#endif /* ENSEMBLEMEMBER_H_ */
