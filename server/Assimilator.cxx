/*
 * Assimilator.cxx
 *
 *  Created on: Aug 22, 2019
 *      Author: friese
 */


#include "Assimilator.h"
#include "DummyAssimilator.h"
#include "PDAFEnKFAssimilator.h"

std::shared_ptr<Assimilator> Assimilator::create(AssimilatorType assimilator_type, Field & field)
{
	switch (assimilator_type) {
	case ASSIMILATOR_DUMMY:
		return std::make_shared<DummyAssimilator>(field);
		break;
	case ASSIMILATOR_PDAF:
		return std::make_shared<PDAFEnKFAssimilator>(field);
		break;
	default:
		assert(false); // should never be reached.
	}
}

