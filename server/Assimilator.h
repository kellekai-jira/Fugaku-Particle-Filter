/*
 * update-step.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef ASSIMILATOR_H_
#define ASSIMILATOR_H_

#include <mpi.h>
#include <memory>
#include "Field.h"

enum AssimilatorType {
	ASSIMILATOR_DUMMY = 0,
	ASSIMILATOR_PDAF = 1
};

class Assimilator {
public:
	// called if every state was saved.
	// TODO: at the moment we do not need those things, but later it might be good to use them to diminish coupling....
	//void put_part(const Part & part, const double * data);
	//double * get_part(const Part & part) = 0;


	// returns how many steps must be performed by the model in the next iteration
	virtual int do_update_step() = 0;


	virtual ~Assimilator() = default;

  static std::shared_ptr<Assimilator> create(AssimilatorType assimilator_type, Field & field);
};




#endif /* ASSIMILATOR_H_ */
