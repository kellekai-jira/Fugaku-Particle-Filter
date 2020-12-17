/*
 * CheckStatelessAssimilator.cpp
 *
 *  Created on: Jan 16, 2020
 *      Author: friese
 */

#include "CheckStatelessAssimilator.h"
#include <algorithm>
#include <cmath>

CheckStatelessAssimilator::CheckStatelessAssimilator(Field & field_, const int
                                                     total_steps, MpiManager & mpi_) :
    field(field_)
{
    L("**** Performing the stateless checking instead of assimilation...");
    nsteps = 1;

    // otherwise release mode will make problems!
    init_states.resize(field.ensemble_members.size());
    correct_states.resize(field.ensemble_members.size());

    init_states_hidden.resize(field.ensemble_members.size());
    correct_states_hidden.resize(field.ensemble_members.size());

    int index = 0;
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        init_states[index].resize(ens_it->state_analysis.size());
        correct_states[index].resize(ens_it->state_analysis.size());

        init_states_hidden[index].resize(ens_it->state_hidden.size());
        correct_states_hidden[index].resize(ens_it->state_hidden.size());

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
    L("**** and inited due to the first received state!)");
}

int CheckStatelessAssimilator::do_update_step(const int current_step)
{
    // TODO: assert: not recovering from checkpoint. this will break things here as we do not use current_step
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
            std::copy(ens_it->state_hidden.begin(),
                      ens_it->state_hidden.end(),
                      correct_states_hidden[index].begin());

            // set analysis state back to init to recalculate the same timestep again.
            // then we will check in the else branch if the results are equal!
            std::copy(init_states[index].begin(),
                      init_states[index].end(),
                      ens_it->state_analysis.begin());
            std::copy(init_states_hidden[index].begin(),
                      init_states_hidden[index].end(),
                      ens_it->state_hidden.begin());

            index++;
        }
        isFirst = false;
    }
    else
    {
        int index = 0;
#define BINARY_MODE
#ifdef BINARY_MODE
        D("Comparing while assuming all state variables are binary data...");
#else
        D("Comparing while casting all state variables into doubles all state variables as doubles...");
#endif
        for (auto &ens_it : field.ensemble_members)
        {
#ifdef BINARY_MODE
            for (size_t i = 0; i < correct_states.at(index).size(); ++i)
            {
                int a = static_cast<int>(correct_states.at(index).at(i));
                int b = static_cast<int>(ens_it.state_background.at(i));

                int diff = std::abs(a-b);

                if (diff != 0) {
                    L("Binary data differs");
                    print_result(false);
                    return -1;
                }
            }
            index++;
#else
            // analysis state is enough:
            //

            // calculate max diff
            const double * cstate =
                reinterpret_cast<const double*>(correct_states[index].data());
            const double * cstate_hidden =
                reinterpret_cast<const double*>(correct_states_hidden[index].data());

            const double * bstate =
                reinterpret_cast<const double*>(ens_it->state_background.data());
            const double * hstate =
                reinterpret_cast<const double*>(ens_it->state_hidden.data());

            double max_diff = 0.0;
            double min_value = cstate[0];
            double max_value = cstate[0];
            for (size_t i = 0; i < correct_states[index].size()/sizeof(double); ++i)
            {

                assert(correct_states.size() % sizeof(double) == 0);
                assert(correct_states_hidden.size() % sizeof(double) == 0);

                double a = cstate[i];
                double b = bstate[i];
                if (std::isnan(a) && std::isnan(b))
                {
                    continue;
                }
                if (std::isnan(-a) && std::isnan(-b))
                {
                    continue;
                }

                min_value = std::min(min_value, cstate[i]);
                min_value = std::min(min_value, bstate[i]);

                max_value = std::max(max_value, cstate[i]);
                max_value = std::max(max_value, bstate[i]);

                double ndiff = std::abs(cstate[i] -
                                        bstate[i]);
                max_diff = std::max(ndiff, max_diff);

            }
            for (size_t i = 0; i < correct_states_hidden[index].size() / sizeof(double); ++i)
            {
                double a = cstate_hidden[i];
                double b = hstate[i];
                if (std::isnan(a) && std::isnan(b))
                {
                    continue;
                }
                if (std::isnan(-a) && std::isnan(-b))
                {
                    continue;
                }

                min_value = std::min(min_value,
                                     cstate_hidden[i]);
                min_value = std::min(min_value, hstate[i]);

                max_value = std::max(max_value,
                                     cstate_hidden[i]);
                max_value = std::max(max_value, hstate[i]);

                double ndiff = std::abs(cstate_hidden[i] -
                                        hstate[i]);
                max_diff = std::max(ndiff, max_diff);

            }


            const double eps = 0.00001;
            L("Max diff: %f, value range: %f .. %f", max_diff, min_value,
              max_value);

            if (max_diff > eps)
            {
                L(
                    "Error: Vectors are not equal (max diff >%f). Is there some hidden state?",
                    eps);
                //print_vector(bstate);
                L("!=");
                //print_vector(cstate);
                print_result(false);
                return -1;      // stop assimilation
            }

            index++;
#endif
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
                                                      VEC_T * values,
                                                      const Part & hidden_part,
                                                      const
                                                      VEC_T * values_hidden)
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

    // hidden state:
    std::copy(values_hidden, values_hidden + hidden_part.send_count,
              init_states_hidden[ensemble_member_id].data() +
              hidden_part.local_offset_server);

    // Also copy into field's hidden state to send it back right again!
    std::copy(values_hidden, values_hidden + hidden_part.send_count,
              member.state_hidden.data() +
              hidden_part.local_offset_server);
}



void CheckStatelessAssimilator::on_init_state(const int runner_id, const
                                              Part & part, const
                                              VEC_T * values, const
                                              Part & hidden_part,
                                              const VEC_T * values_hidden)
{
    // let's use this to set the init.
    // you may not have more runners than ensemble members here! Otherwise some
    // would stay uninitialized!
    assert(static_cast<size_t>(runner_id) < field.ensemble_members.size());
    field.ensemble_members[runner_id].
    store_background_state_part(part,
                                values, hidden_part, values_hidden);
    store_init_state_part(runner_id, part,
                          values, hidden_part, values_hidden);
}
