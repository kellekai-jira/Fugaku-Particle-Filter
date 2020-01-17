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
    nsteps = 1;

    // otherwise release mode will make problems!
    init_states.resize(field.ensemble_members.size());
    correct_states.resize(field.ensemble_members.size());
    int index = 0;
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        // analysis state is enough:
        double r = std::rand()*magnitude/RAND_MAX;  // Note: 1+rand()%6 is biased
        double value = mean_value + r;
        std::fill(ens_it->state_analysis.begin(),
                  ens_it->state_analysis.end(), value);
        init_states[index].resize(ens_it->state_analysis.size());
        std::fill(init_states[index].begin(), init_states[index].end(), value);

        correct_states[index].resize(ens_it->state_analysis.size());

        index++;
    }


}

int CheckStatelessAssimilator::do_update_step()
{
    static bool isFirst = true;
    if (isFirst) {
        // analysis state keeps the same
        // store output
        int index = 0;
        for (auto ens_it = field.ensemble_members.begin(); ens_it !=
             field.ensemble_members.end(); ens_it++)
        {
            std::copy(ens_it->state_background.begin(), ens_it->state_background.end(),
                    correct_states[index].begin());
            index++;
        }
        isFirst = false;
    } else {
        int index = 0;
        for (auto ens_it = field.ensemble_members.begin(); ens_it !=
             field.ensemble_members.end(); ens_it++)
        {
            // analysis state is enough:
            if (ens_it->state_background != correct_states[index]) {
                L("Error: Vectors are not equal. Is there some hidden state?");
                print_vector(ens_it->state_background);
                L("!=");
                print_vector(correct_states[index]);
                assert(false);
            }
            index++;
        }

        //we did the check. so quit now!
        L("**** Check successful! Simulation seems stateless !");
        L("**** (at least over one timestep on %lu ensemble", field.ensemble_members.size());
        L("**** members and initial values around %f +- %f) !", mean_value, magnitude);
        return -1;
    }


    return 1;
}

