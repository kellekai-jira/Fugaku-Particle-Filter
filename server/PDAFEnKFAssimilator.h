/*
 * PDAFEnKFAssimilator.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef PDAFENKFASSIMILATOR_H_
#define PDAFENKFASSIMILATOR_H_

#include "Field.h"
#include <vector>
#include "Assimilator.h"

class PDAFEnKFAssimilator : public Assimilator {
public:
	~PDAFEnKFAssimilator();
	PDAFEnKFAssimilator();
	virtual void do_update_step(Field &field);
};

#endif /* PDAFENKFASSIMILATOR_H_ */
