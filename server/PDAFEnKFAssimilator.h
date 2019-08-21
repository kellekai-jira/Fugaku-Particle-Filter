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
private:
	Field & field;
public:
	~PDAFEnKFAssimilator();
	PDAFEnKFAssimilator(Field & field);
	virtual int do_update_step();
};

#endif /* PDAFENKFASSIMILATOR_H_ */
