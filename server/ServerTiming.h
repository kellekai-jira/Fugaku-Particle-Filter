#include <list>
#include <chrono>
#include <iostream>
#include <numeric>
#include <map>
#include <utility>
#include <fstream>
#include <iomanip>

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
    std::chrono::high_resolution_clock::time_point null_time;

    double to_millis(const TimePoint &lhs) {

        return std::chrono::duration<double, std::milli>(lhs - null_time).count();
    }

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
    ServerTiming(long long milliseconds_since_epoch) : Timing(),
    null_time(std::chrono::milliseconds(milliseconds_since_epoch))

    {
    }

    void report(const int cores_simulation, const int cores_server, const int
                ensemble_members, const size_t state_size) {


        print_events();

        std::cout <<
            "------------------- Timing information(csv): -------------------"
                  <<
            std::endl;
        std::cout <<
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
                std::cout << iterations << ',';
                std::cout << wt << ',';
                std::cout << filter_update_walltime << ',';
                std::cout << job_max_wt << ',';
                std::cout << min_runners << ',';
                std::cout << max_runners << ',';
                std::cout << accumulated_idle_time << ',';
                std::cout << corresponding_idle_time << ',';
                std::cout << corresponding_idle_time/accumulated_idle_time;
                std::cout << std::endl;

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
        std::cout <<
            "------------------- End Timing information -------------------" <<
            std::endl;


        std::cout <<
            "------------------- Run information(csv): -------------------" <<
            std::endl;
        std::cout <<
            "cores simulation,number runners(max),cores server,runtime per iteration mean (ms),ensemble members,state size,iterations,mean bandwidth (MB/s),iterations used for means"
                  << std::endl;
        if (iterations - warmup < 10)    // have at least 10 iterations for stats
        {   // 10 warmup and 10 cooldown ... FIXME: no warmup/cooldown for now!
            sum_runtime = 0.0;
        }

        double mean_runtime = sum_runtime / static_cast<double>(iterations -
                                                                warmup);



        std::cout << cores_simulation << ',';
        std::cout << number_runners_max << ',';
        std::cout << cores_server << ',';
        std::cout << mean_runtime << ',';
        std::cout << ensemble_members << ',';
        std::cout << state_size << ',';
        std::cout << iterations << ',';
        std::cout << 8*state_size*ensemble_members*2.0/mean_runtime*1000/1024/
            1024 <<
            ',';
        std::cout << (iterations-warmup);
        std::cout << std::endl;
        std::cout <<
            "------------------- End Run information -------------------" <<
            std::endl;
    }


    void write_region_csv(int rank) {
        std::ofstream outfile ("trace_melissa_server." + std::to_string(rank) + ".csv");

        outfile << "rank,start_time,end_time,event,event_parameter" << std::endl;

        std::list<TimingEvent*> open_events;
        const std::array<int, 4> opening_event_types = {
                START_ITERATION,
                START_FILTER_UPDATE,
                START_IDLE_RUNNER,
                START_PROPAGATE_STATE
        };
        struct ClosingRegion
        {
            int type;
            const char * name;
        };
        const std::array<ClosingRegion, 4> closing_regions = {{
            {STOP_ITERATION, "Iteration"},
            {STOP_FILTER_UPDATE, "Filter Update"},
            {STOP_IDLE_RUNNER, "Runner idle"},
            {STOP_PROPAGATE_STATE, "State propagation"}
        }};

        for (auto it = events.begin(); it != events.end(); ++it)
        {
            // Opening event: push on stack
            bool goto_next_event = false;
            for (auto i = opening_event_types.begin(); i != opening_event_types.end(); ++i)
            {
                if (it->type == *i)
                {
                    open_events.push_back( &(*it) );
                    goto_next_event = true;
                    break;
                }
            }

            if (goto_next_event)
            {
                continue;
            }

            // Closing event: pop from stack, write event into csv...
            for (auto cr = closing_regions.begin(); cr != closing_regions.end(); ++cr)
            {
                if (it->type == cr->type)
                {
                    // find last corresponding open event...
                    for (auto oevt = open_events.rbegin(); oevt != open_events.rend(); ++oevt)
                    {
                        if ((*oevt)->type == it->type-1 && (*oevt)->parameter == it->parameter)
                        {
                            outfile << rank
                                << ',' << std::fixed << std::setw( 11 ) << std::setprecision( 6 ) << to_millis((*oevt)->time)
                                << ',' << std::fixed << std::setw( 11 ) << std::setprecision( 6 ) << to_millis(it->time)
                                << ',' << cr->name
                                << ',' << it->parameter
                                << std::endl;

                            // remove from stack:
                            open_events.erase( --(oevt.base()) );  // hope this reverse operator works

                            goto_next_event = true;
                            break;
                        }
                    }
                    //assert(goto_next_event);
                    if (!goto_next_event) {
                        D("Did not find enter region event for %d %d at %f ms", it->type,
                                it->parameter, to_millis(it->time));
                    }
                    break;
                }
            }
        }

        outfile.close();
    }

};
