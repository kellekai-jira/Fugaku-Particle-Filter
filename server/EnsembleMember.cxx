/*
 * EnsembleMember.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "EnsembleMember.h"
#include <cassert>
#include "../common/utils.h"

void EnsembleMember::set_local_vect_size(int local_vect_size)
{
        state_analysis.reserve(local_vect_size);
        state_analysis.resize(local_vect_size);
        state_background.reserve(local_vect_size);
        state_background.resize(local_vect_size);
}

void EnsembleMember::store_background_state_part(const Part & part, const
                                                 double * values)
{
        D("before_assert %lu %lu %lu", part.send_count,
          part.local_offset_server, state_background.size());
        assert(part.send_count + part.local_offset_server <=
               state_background.size());
        std::copy(values, values + part.send_count, state_background.data() +
                  part.local_offset_server);
}
