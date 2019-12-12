/*
 * EnsembleMember.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "EnsembleMember.h"
#include <cassert>
#include "../common/utils.h"
#include <fti.h>

void EnsembleMember::set_local_vect_size(int local_vect_size)
{
    state_analysis.reserve(local_vect_size);
    state_analysis.resize(local_vect_size);
    state_background.reserve(local_vect_size);
    state_background.resize(local_vect_size);
}

void EnsembleMember::store_background_state_part(const Part & part, const
                                                 double * values, int state_id)
{
    int myRank; MPI_Comm_rank(FTI_COMM_DUP, &myRank);
    D("before_assert %lu %lu %lu", part.send_count,
      part.local_offset_server, state_background.size());
    assert(part.send_count + part.local_offset_server <=
           state_background.size());
    std::copy(values, values + part.send_count, state_background.data() + part.local_offset_server);
    std::string key("state");
    key += "_" + std::to_string( state_id ) + "_" + std::to_string( part.rank_runner );
    std::cout << "[" << myRank << "] fti_dbg -> store_bg " << key << "ptr: " << state_background.data() + part.local_offset_server << " count: " << part.send_count <<std::endl;
}
