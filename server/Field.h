/*
 * Field.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef FIELD_H_
#define FIELD_H_
#include <vector>
#include <set>

#include "EnsembleMember.h"

#include "../common/utils.h"

struct Field {
	// index: state id.
	std::vector<EnsembleMember> ensemble_members;

	size_t local_vect_size;
	std::vector<size_t> local_vect_sizes_simu;
	std::vector<n_to_m> parts;

	std::set<int> connected_simulation_ranks;

	Field(int simu_comm_size_, size_t ensemble_size_);
	void calculate_parts(int server_comm_size);
	n_to_m & getPart(int simu_rank);
};

#endif /* FIELD_H_ */
