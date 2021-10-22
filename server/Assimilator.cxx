/*
 * Assimilator.cxx
 *
 *  Created on: Aug 22, 2019
 *      Author: friese
 */


#include "Assimilator.h"
#include "DummyAssimilator.h"
#include "PrintIndexMapAssimilator.h"
#include "PDAFAssimilator.h"
#include "EmptyAssimilator.h"
#include "CheckStatelessAssimilator.h"
#include "WrfAssimilator.h"
#include "PythonAssimilator.h"

std::shared_ptr<Assimilator> Assimilator::create(AssimilatorType
                                                 assimilator_type,
                                                 Field & field, const int
                                                 total_steps, MpiManager & mpi)
{
    switch (assimilator_type)
    {
    case ASSIMILATOR_DUMMY:
        MPRT("Chosing Dummy Assimilator");
        return std::make_shared<DummyAssimilator>(field, total_steps, mpi);
        break;
    case ASSIMILATOR_PDAF:
        MPRT("Chosing PDAF Assimilator");
        return std::make_shared<PDAFAssimilator>(field, total_steps, mpi);
        break;
    case ASSIMILATOR_EMPTY:
        MPRT("Chosing Empty Assimilator");
        return std::make_shared<EmptyAssimilator>(field, total_steps, mpi);
        break;
    case ASSIMILATOR_CHECK_STATELESS:
        MPRT("Chosing Assimilator used to check if stateless");
        return std::make_shared<CheckStatelessAssimilator>(field, total_steps, mpi);
        break;
    case ASSIMILATOR_PRINT_INDEX_MAP:
        MPRT("Chosing Assimilator that only prints out the index map");
        return std::make_shared<PrintIndexMapAssimilator>(field, total_steps, mpi);
        break;
    case ASSIMILATOR_WRF:
        MPRT("Chosing Wrf Assimilator");
        return std::make_shared<WrfAssimilator>(field, total_steps, mpi);
        break;
    case ASSIMILATOR_PYTHON:
        MPRT("Chosing Python Assimilator");
        return std::make_shared<PythonAssimilator>(field, total_steps, mpi);
        break;
    default:
        assert(false);         // should never be reached.
        return nullptr;
    }
}

