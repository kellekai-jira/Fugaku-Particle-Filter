/*
 * update-step.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef ASSIMILATOR_H_
#define ASSIMILATOR_H_

#include <mpi.h>
#include "Field.h"

class Assimilator {
public:
	// called if every state was saved.
	// TODO: at the moment we do not need those things, but later it might be good to use them to diminish coupling....
	//void put_part(const Part & part, const double * data);
	//double * get_part(const Part & part) = 0;


	virtual void do_update_step(Field & field) = 0;


	virtual ~Assimilator() = default;
};


#endif /* ASSIMILATOR_H_ */