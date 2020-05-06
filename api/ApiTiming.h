#include <list>
#include <chrono>  // TODO:  use clock instead??!
#include <iostream>
#include <numeric>
#include <map>
#include <utility>
#include <fstream>

#include "utils.h"
#include "TimingEvent.h"


// double get_walltime(const TimingEvent &a, const TimingEvent &b) {
//// in milliseconds
// return std::chrono::duration<double, std::milli>(a.time-b.time).count();
// }

class ApiTiming : public Timing
{
public:
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
            "iteration,timesteps,compute walltime (ms),idle walltime (ms),idle + compute walltime(ms)"
                  << std::endl;
        TimePoint *iteration_start = nullptr;
        TimePoint *idle_start = nullptr;
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
            case START_IDLE_RUNNER: {
                last_idle_time = 0.0;
                idle_start = &it->time;
                break;
            }
            case STOP_IDLE_RUNNER: {
                assert(last_idle_time == 0.0);  // called twice stop?
                last_idle_time = diff_to_millis(it->time, *idle_start);
                break;
            }


            case START_ITERATION: {
                iteration_start = &it->time;
                last_propagated_steps = it->parameter;
                break;
            }
            case STOP_ITERATION: {
                double wt = diff_to_millis(it->time, *iteration_start);
                os << iterations << ',';
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
            default:
            {
                L(
                    "ERROR: Wrong timing event found! this should never ever happen!");
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
            "cores simulation,runtime per iteration (idle + compute) mean (ms),runtime per iteration (compute) mean (ms), runtime per iteration (idle) mean (ms),local state size,mean (idle + compute) bandwidth of this core (MB/s),iterations used for means,iterations (propagated states),timesteps"
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
