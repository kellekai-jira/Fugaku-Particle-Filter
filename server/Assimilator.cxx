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

std::shared_ptr<Assimilator> Assimilator::create(AssimilatorType
                                                 assimilator_type,
                                                 Field & field)
{
    switch (assimilator_type)
    {
    case ASSIMILATOR_DUMMY:
        L("Chosing Dummy Assimilator");
        return std::make_shared<DummyAssimilator>(field);
        break;
    case ASSIMILATOR_PDAF:
        L("Chosing PDAF Assimilator");
        return std::make_shared<PDAFAssimilator>(field);
        break;
    case ASSIMILATOR_EMPTY:
        L("Chosing Empty Assimilator");
        return std::make_shared<EmptyAssimilator>(field);
        break;
    default:
        assert(false);         // should never be reached.
    }
}

