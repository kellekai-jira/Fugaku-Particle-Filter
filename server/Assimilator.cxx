/*
 * Assimilator.cxx
 *
 *  Created on: Aug 22, 2019
 *      Author: friese
 */


#include "Assimilator.h"
#include "DummyAssimilator.h"
#include "PDAFAssimilator.h"
#include "EmptyAssimilator.h"
#include "CheckStatelessAssimilator.h"

std::shared_ptr<Assimilator> Assimilator::create(AssimilatorType
                                                 assimilator_type,
                                                 Field & field, const int
                                                 total_steps)
{
    switch (assimilator_type)
    {
    case ASSIMILATOR_DUMMY:
        L("Chosing Dummy Assimilator");
        return std::make_shared<DummyAssimilator>(field, total_steps);
        break;
    case ASSIMILATOR_PDAF:
        L("Chosing PDAF Assimilator");
        return std::make_shared<PDAFAssimilator>(field, total_steps);
        break;
    case ASSIMILATOR_EMPTY:
        L("Chosing Empty Assimilator");
        return std::make_shared<EmptyAssimilator>(field, total_steps);
        break;
    case ASSIMILATOR_CHECK_STATELESS:
        L("Chosing Assimilator used to check if stateless");
        return std::make_shared<CheckStatelessAssimilator>(field, total_steps);
        break;
    default:
        assert(false);         // should never be reached.
        return nullptr;
    }
}

