/*
 * EnsembleMember.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "EnsembleMember.h"
#include <cassert>
#include "utils.h"

void EnsembleMember::set_local_vect_size(const int local_vect_size, const int
                                         local_vect_size_hidden)
{
    state_analysis.reserve(local_vect_size);
    state_analysis.resize(local_vect_size);
    state_background.reserve(local_vect_size);
    state_background.resize(local_vect_size);

    state_hidden.reserve(local_vect_size_hidden);
    state_hidden.resize(local_vect_size_hidden);
}

void EnsembleMember::store_background_state_part(const Part & part, const
                                                 VEC_T * values, const
                                                 Part & hidden_part, const
                                                 VEC_T * values_hidden)
{
    D("before_assert %lu %lu %lu", part.send_count,
      part.local_offset_server, state_background.size());
    assert(part.send_count + part.local_offset_server <=
           state_background.size());
    std::copy(values, values + part.send_count, state_background.data() +
              part.local_offset_server);

    assert(hidden_part.send_count + hidden_part.local_offset_server <=
           state_hidden.size());
    std::copy(values_hidden, values_hidden + hidden_part.send_count,
              state_hidden.data() +
              hidden_part.local_offset_server);
}
