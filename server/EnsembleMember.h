/*
 * EnsembleMember.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef ENSEMBLEMEMBER_H_
#define ENSEMBLEMEMBER_H_

#include <vector>

#include "../common/n_to_m.h"

class EnsembleMember
{
public:
	std::vector<double> state_analysis;
	std::vector<double> state_background;

	void set_local_vect_size(int local_vect_size);
	void store_background_state_part(const n_to_m & part, const double * values);
};


#endif /* ENSEMBLEMEMBER_H_ */