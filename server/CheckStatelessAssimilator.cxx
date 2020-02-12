/*
 * CheckStatelessAssimilator.cpp
 *
 *  Created on: Jan 16, 2020
 *      Author: friese
 */

#include "CheckStatelessAssimilator.h"
#include <algorithm>
#include <cmath>

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
            // calculate max diff
            double max_diff = 0.0;
            double min_value = correct_states[index][0];
            double max_value = correct_states[index][0];
            for (int i = 0; i < correct_states[index].size(); ++i)
            {
                double a = correct_states[index][i];
                double b = ens_it->state_background[i];
                if (std::isnan(a) && std::isnan(b))
                {
                    continue;
                }
                if (std::isnan(-a) && std::isnan(-b))
                {
                    continue;
                }

                min_value = std::min(min_value, correct_states[index][i]);
                min_value = std::min(min_value, ens_it->state_background[i]);

                max_value = std::max(max_value, correct_states[index][i]);
                max_value = std::max(max_value, ens_it->state_background[i]);

                double ndiff = std::abs(correct_states[index][i] -
                                        ens_it->state_background[i]);
                max_diff = std::max(ndiff, max_diff);

            }


            const int eps = 0.00001;
            L("Max diff: %f, value range: %f .. %f", max_diff, min_value,
              max_value);

            if (max_diff > eps)
            {
                L(
                    "Error: Vectors are not equal (max diff >%f). Is there some hidden state?",
                    eps);
                print_vector(ens_it->state_background);
                L("!=");
                print_vector(correct_states[index]);
                print_result(false);
                return -1;      // stop assimilation
            }

            index++;
        }

        // we did the check. so quit now!
        print_result(true);
        return -1;  // stop assimilation
    }


    return 1;
}

void CheckStatelessAssimilator::store_init_state_part(const int
                                                      ensemble_member_id, const
                                                      Part & part, const
                                                      double * values)
{
    EnsembleMember & member = field.ensemble_members[ensemble_member_id];
    assert(part.send_count + part.local_offset_server <=
           member.state_background.size());
    std::copy(values, values + part.send_count,
              init_states[ensemble_member_id].data() +
              part.local_offset_server);

    // Also copy into analysis state to send it back right again!
    std::copy(values, values + part.send_count,
              member.state_analysis.data() +
              part.local_offset_server);
}



void CheckStatelessAssimilator::on_init_state(const int runner_id, const
                                              Part & part, const
                                              double * values)
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
