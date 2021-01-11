#include <list>
#include <chrono>  // TODO:  use clock instead??!
#include <iostream>
#include <numeric>
#include <map>
#include <utility>
#include <fstream>

#include "utils.h"
#include "TimingEvent.h"



class ApiTiming : public Timing
{
public:
    /// Writes report of simulation run into a csv file that is easily readable by, e.g.,
    /// pandas. There is one file containing runner information on all assimilation
    /// cycles (e.g. runner-000.timing-information.csv) and another one having general
    /// run information for the whole run (runner-000.run-information.csv)
    void report(const int cores_simulation, const size_t state_size, const int runner_id) {

        char fname[256];
        sprintf(fname, "runner-%03d", runner_id);
        print_events(fname, comm_rank);

        sprintf(fname, "runner-%03d.timing-information.csv", runner_id);
        std::ofstream os(fname, std::ofstream::app);
        assert (os.is_open());

        std::cout <<
            "------------------- Writing Timing information(csv): -------------------"
                  <<
            std::endl;
        os <<
            "iteration,runner id,assimilation cycle,state id,timesteps,compute walltime (ms),idle walltime (ms),idle + compute walltime(ms)"
                  << std::endl;
        TimePoint *propagation_start = nullptr;
        TimePoint *idle_start = nullptr;

        int current_step = -2;
        int current_state = -2;
        int iterations = 0;

        double sum_runtime = 0.0;
        double sum_runtime_iteration = 0.0;
        double sum_runtime_idle = 0.0;

        double last_idle_time = 0.0;

        int sum_propagated_steps = 0;
        int last_propagated_steps = 0;

        for (auto it = events.begin(); it != events.end(); it++)
        {
            switch (it->type)
            {
            case START_ITERATION: {
                current_step = it->parameter;
                break;
            }
            case STOP_ITERATION: {
                break;
            }
            case NSTEPS: {
                last_propagated_steps = it->parameter;
                break;
            }


            case START_IDLE_RUNNER: {
                last_idle_time = 0.0;
                idle_start = &it->time;
                assert(runner_id == it->parameter);
                break;
            }
            case STOP_IDLE_RUNNER: {
                assert(last_idle_time == 0.0);  // called twice stop?
                assert(runner_id == it->parameter);
                last_idle_time = diff_to_millis(it->time, *idle_start);
                break;
            }


            case START_PROPAGATE_STATE: {
                propagation_start = &it->time;
                current_state = it->parameter;
                break;
            }
            case STOP_PROPAGATE_STATE: {
                assert(it->parameter == current_state);
                double wt = diff_to_millis(it->time, *propagation_start);
                os << iterations << ',';
                os << runner_id << ',';
                os << current_step << ',';
                os << current_state << ',';
                os << last_propagated_steps << ',';
                os << wt << ',';
                os << last_idle_time << ',';
                os << (last_idle_time + wt);
                os << std::endl;

                // calculate some stats for later too:
                sum_runtime += last_idle_time + wt;
                sum_runtime_iteration += wt;
                sum_runtime_idle += last_idle_time;
                sum_propagated_steps += last_propagated_steps;

                iterations++;


                break;
            }
            case INIT: {
                break;
            }
            default:
            {
                L(
                    "ERROR: Wrong timing event (%d) found! this should never ever happen!", it->type);
                exit(1);
                break;
            }
            }
        }


        sprintf(fname, "runner-%03d.run-information.csv", runner_id);
        std::ofstream osr(fname, std::ofstream::app);
        assert (osr.is_open());
        std::cout <<
            "------------------- Writing Run information(csv): -------------------" <<
            std::endl;
        osr <<
            "runner id,cores simulation,runtime per iteration (idle + compute) mean (ms),runtime per iteration (compute) mean (ms), runtime per iteration (idle) mean (ms),local state size,mean (idle + compute) bandwidth of this core (MB/s),iterations used for means,iterations (propagated states),timesteps"
                  << std::endl;
        if (iterations < 10)    // have at least 10 iterations for stats
        {   // 10 warmup and 10 cooldown ... FIXME: no warmup/cooldown for now!
            sum_runtime = 0.0;
            sum_runtime_idle = 0.0;
            sum_runtime_iteration = 0.0;
        }

        double mean_runtime = sum_runtime / static_cast<double>(iterations);
        double mean_runtime_iteration = sum_runtime /
                                        static_cast<double>(iterations);
        double mean_runtime_idle = sum_runtime /
                                   static_cast<double>(iterations);




        osr << runner_id << ',';
        osr << cores_simulation << ',';
        osr << mean_runtime << ',';
        osr << mean_runtime_iteration << ',';
        osr << mean_runtime_idle << ',';
        osr << state_size << ',';
        osr << 8.0*state_size*iterations*2.0/sum_runtime*1000.0/1024.0/
            1024.0 << ',';
        osr << (sum_runtime == 0.0 ? 0 : iterations) << ',';
        osr << iterations << ',';
        osr << sum_propagated_steps;
        osr << std::endl;
        osr.close();
    }

};
