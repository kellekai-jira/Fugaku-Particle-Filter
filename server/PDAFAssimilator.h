/*
 * PDAFEnKFAssimilator.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef PDAFASSIMILATOR_H_
#define PDAFASSIMILATOR_H_

#include "Field.h"
#include <vector>
#include "Assimilator.h"

class PDAFAssimilator : public Assimilator
{
private:
Field & field;

void getAllEnsembleMembers();
public:
~PDAFAssimilator();
PDAFAssimilator(Field & field);
virtual int do_update_step();
virtual void on_init_state(const int runner_id, const Part & part, const
                          double * values);
};

#endif /* PDAFASSIMILATOR_H_ */
