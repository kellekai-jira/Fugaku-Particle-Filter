/*
 * CheckStatelessAssimilator.cpp
 *
 *  Created on: Jan 16, 2020
 *      Author: friese
 */

#include "CheckStatelessAssimilator.h"
#include <algorithm>

CheckStatelessAssimilator::CheckStatelessAssimilator(Field & field_) :
    field(field_)
{
  L("**** Performing the stateless checking instead of assimilation...");
    nsteps = 1;

    // otherwise release mode will make problems!
    init_states.resize(field.ensemble_members.size());
    correct_states.resize(field.ensemble_members.size());
    int index = 0;
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        init_states[index].resize(ens_it->state_analysis.size());
        correct_states[index].resize(ens_it->state_analysis.size());

        index++;
    }


}


void CheckStatelessAssimilator::print_result(const bool good)
{

    if (good)
    {
        L("**** Check Successful! Simulation seems stateless!");
    }
    else
    {
        L("**** Check NOT Successful! Simulation seems stateful!");
    }
    L("**** (at least over one timestep on %lu ensemble members",
      field.ensemble_members.size());
    L("**** members and inited due to the first received state!");
}

int CheckStatelessAssimilator::do_update_step()
{
    static bool isFirst = true;
    if (isFirst)
    {
        // analysis state keeps the same
        // store output
        int index = 0;
        for (auto ens_it = field.ensemble_members.begin(); ens_it !=
             field.ensemble_members.end(); ens_it++)
        {
            std::copy(ens_it->state_background.begin(),
                      ens_it->state_background.end(),
                      correct_states[index].begin());

            // set analysis state back to init to recalculate the same timestep again.
            // then we will check in the else branch if the results are equal!
            std::copy(init_states[index].begin(),
                      init_states[index].end(),
                      ens_it->state_analysis.begin());

            index++;
        }
        isFirst = false;
    }
    else
    {
        int index = 0;
        for (auto ens_it = field.ensemble_members.begin(); ens_it !=
             field.ensemble_members.end(); ens_it++)
        {
            // analysis state is enough:
            if (ens_it->state_background != correct_states[index])
            {
                L("Error: Vectors are not equal. Is there some hidden state?");
                print_vector(ens_it->state_background);
                L("!=");
                print_vector(correct_states[index]);

                print_result(false);
                return -1;  // stop assimilation
            }
            index++;
        }

        // we did the check. so quit now!
        print_result(true);
        return -1;  // stop assimilation
    }


    return 1;
}

void CheckStatelessAssimilator::store_init_state_part(const int ensemble_member_id, const Part & part, const
                                                 double * values)
{
    EnsembleMember & member = field.ensemble_members[ensemble_member_id];
    assert(part.send_count + part.local_offset_server <=
           member.state_background.size());
    std::copy(values, values + part.send_count, init_states[ensemble_member_id].data() +
              part.local_offset_server);

    // Also copy into analysis state to send it back right again!
    std::copy(values, values + part.send_count,
        member.state_analysis.data() +
              part.local_offset_server);
}



int CheckStatelessAssimilator::on_init_state(const int runner_id, const Part & part, const double * values)
{
    // let's use this to set the init.
    // you may not have more runners than ensemble members here! Otherwise some
    // would stay uninitialized!
    assert(runner_id < field.ensemble_members.size());
    field.ensemble_members[runner_id].
        store_background_state_part(part,
                values);
    store_init_state_part(runner_id, part,
            values);
}
