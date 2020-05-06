#include <list>
#include <chrono>
#include <iostream>
#include <numeric>
#include <map>
#include <utility>
#include <fstream>

#include "utils.h"
#include "TimingEvent.h"

// REM: STOP/START_IDLE/PROPAGATE concerns always runner rank 0 for the serverside timing.

// double get_walltime(const TimingEvent &a, const TimingEvent &b) {
//// in milliseconds
// return std::chrono::duration<double, std::milli>(a.time-b.time).count();
// }

const int warmup = 30;

class ServerTiming : public Timing
{
private:
    void calculate_runners(int *runners, int *min_runners, int *max_runners) {
        if (*runners < *min_runners || *min_runners == -1)
        {
            *min_runners = *runners;
        }

        if (*runners > *max_runners)
        {
            *max_runners = *runners;
        }
    }


public:
    void report(const int cores_simulation, const int cores_server, const int
                ensemble_members, const size_t state_size) {


        print_events("server", comm_rank);

        std::ofstream os("server.timing-information.csv", std::ofstream::app);
        assert (os.is_open());


        std::cout <<
            "------------------- Writing Timing information(csv): -------------------"
                  <<
            std::endl;
        os <<
            "iteration,walltime (ms),walltime filter update (ms),max job walltime (ms),min_runners,max_runners,accumulated runner idle time,corresponding pdaf state per runner runner idle time,pdaf slack/melissa-da slack"
                  << std::endl;
        TimePoint *iteration_start = nullptr;
        TimePoint *filter_update_start = nullptr;
        double filter_update_walltime;
        int iterations = 0;
        double sum_runtime = 0.0;
        int runners = 0;
        std::vector<double> job_walltimes(ensemble_members);
        std::vector<TimePoint> job_start_timepoint(ensemble_members);
        int number_runners_max = -1;

        std::map<const int, double> runner_idle_time;
        std::map<const int, TimePoint> runner_idle_timepoint;

        int min_runners = -1;
        int max_runners = 0;
        double job_max_wt=0.0;
        for (auto it = events.begin(); it != events.end(); it++)
        {
            switch (it->type)
            {
            case ADD_RUNNER: {
                runners++;
                number_runners_max = std::max(number_runners_max, runners);
                calculate_runners(&runners, &min_runners, &max_runners);
                break;
            }
            case REMOVE_RUNNER: {
                runners--;
                calculate_runners(&runners, &min_runners, &max_runners);
                break;
            }

            case START_IDLE_RUNNER: {
                runner_idle_timepoint[it->parameter] = it->time;
                break;
            }
            case STOP_IDLE_RUNNER: {
                runner_idle_time.emplace(it->parameter, 0.0);
                runner_idle_time[it->parameter] += diff_to_millis(it->time,
                                                                  runner_idle_timepoint
                                                                  [it->parameter
                                                                  ]);
                break;
            }

            case START_PROPAGATE_STATE: {
                job_start_timepoint[it->parameter] = it->time;
                break;
            }
            case STOP_PROPAGATE_STATE: {
                double wt = diff_to_millis(it->time,
                                           job_start_timepoint[it->parameter]);
                job_walltimes[it->parameter] = wt;
                job_max_wt = std::max(wt, job_max_wt);
                break;
            }

            case START_FILTER_UPDATE: {
                filter_update_start = &it->time;
                break;
            }
            case STOP_FILTER_UPDATE: {
                filter_update_walltime = diff_to_millis(it->time,
                                                        *filter_update_start);
                break;
            }

            case START_ITERATION: {
                iteration_start = &it->time;
                calculate_runners(&runners, &min_runners, &max_runners);
                break;
            }
            case STOP_ITERATION: {

                const double accumulated_idle_time = std::accumulate(std::begin(
                                                                         runner_idle_time),
                                                                     std::end(
                                                                         runner_idle_time),
                                                                     0.0,
                                                                     [](const
                                                                        double
                                                                        previous,
                                                                        const
                                                                        std::
                                                                        pair<
                                                                            const
                                                                            int,
                                                                            double>
                                                                        & p)
                    {
                        return previous + p.second;
                    });


                double corresponding_idle_time = 0.0;
                for (auto jwtit = job_walltimes.begin(); jwtit !=
                     job_walltimes.end();
                     jwtit++)
                {
                    corresponding_idle_time += job_max_wt - *jwtit;
                }
                corresponding_idle_time += filter_update_walltime*
                                           (ensemble_members-
                                            1);

                // As the update walltime is a subpart of the idle time...
                assert(accumulated_idle_time > filter_update_walltime*
                       min_runners);

                double wt = diff_to_millis(it->time, *iteration_start);
                os << iterations << ',';
                os << wt << ',';
                os << filter_update_walltime << ',';
                os << job_max_wt << ',';
                os << min_runners << ',';
                os << max_runners << ',';
                os << accumulated_idle_time << ',';
                os << corresponding_idle_time << ',';
                os << corresponding_idle_time/accumulated_idle_time;
                os << std::endl;

                // calculate some stats for later too:
                if (iterations >= warmup)
                {
                    sum_runtime += wt;
                }

                iterations++;
                // assert(iterations == it->parameter);  // FIXME does not work for pdaf if delt_obs != 1

                // reset stuff:
                runner_idle_time.clear();
                max_runners = 0;
                min_runners = -1;
                job_max_wt=0.0;

                break;
            }
            }
        }

        os.close();


        std::cout <<
            "------------------- Writing Run information(csv): -------------------" <<
            std::endl;
        std::ofstream osr("server.run-information.csv", std::ofstream::app);
        assert (osr.is_open());
        osr <<
            "cores simulation,number runners(max),cores server,runtime per iteration mean (ms),ensemble members,state size,iterations,mean bandwidth (MB/s),iterations used for means"
                  << std::endl;
        if (iterations - warmup < 10)    // have at least 10 iterations for stats
        {   // 10 warmup and 10 cooldown ... FIXME: no warmup/cooldown for now!
            sum_runtime = 0.0;
        }

        double mean_runtime = sum_runtime / static_cast<double>(iterations -
                                                                warmup);



        osr << cores_simulation << ',';
        osr << number_runners_max << ',';
        osr << cores_server << ',';
        osr << mean_runtime << ',';
        osr << ensemble_members << ',';
        osr << state_size << ',';
        osr << iterations << ',';
        osr << 8*state_size*ensemble_members*2.0/mean_runtime*1000/1024/
            1024 <<
            ',';
        osr << (iterations-warmup);
        osr << std::endl;
        osr.close();
    }
};
