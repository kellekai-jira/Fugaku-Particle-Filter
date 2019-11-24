#include <vector>
#include <chrono>  // TODO:  use clock instead??!
#include <iostream>

#include "utils.h"

struct TimingIteration
{
    std::chrono::time_point<std::chrono::high_resolution_clock> t_start;
    std::chrono::time_point<std::chrono::high_resolution_clock> t_end;
    int min_runners = -1;
    int max_runners = 0;

    inline void start() {
        t_start = std::chrono::high_resolution_clock::now();
    }
    inline void stop() {
        t_end = std::chrono::high_resolution_clock::now();
    }

    double get_walltime() {
        // in milliseconds
        return std::chrono::duration<double, std::milli>(t_end-t_start).count();
    }
};

class Timing
{
private:
    std::vector<TimingIteration> info;
    std::vector<TimingIteration>::iterator cur;

int runners = 0;

inline void calculate_runners() {
    if (runners < cur->min_runners || cur->min_runners == -1)
    {
        cur->min_runners = runners;
    }

    if (runners > cur->max_runners)
    {
        cur->max_runners = runners;
    }
}

public:
    Timing(const int total_steps) {
        info.resize(total_steps);
        cur = info.begin();
    }

    inline void add_runner() {
        runners++;
        calculate_runners();
    }

    // effectively this happens if server rank 0 recognizes a due date violation or if it gets a NEW kill request (he did not know yet...)
    inline void remove_runner() {
        runners--;
        calculate_runners();
    }

    inline void start_iteration() {
        calculate_runners();
        cur->start();
        // D("******** start iteration");
    }


    inline void stop_iteration() {
        cur->stop();
        cur++;
        // D("******** stop iteration");
    }


    void report(const int cores_simulation, const int cores_server, const int ensemble_members, const size_t state_size) {
        std::cout <<
            "------------------- Timing information(csv): -------------------" <<
            std::endl;
        std::cout << "iteration,walltime,min_runners,max_runners" << std::endl;
        int index = 0;
        for (auto it = info.begin(); it != info.end(); it++)
        {
            std::cout << index << ',';
            std::cout << it->get_walltime() << ',';
            std::cout << it->min_runners << ',';
            std::cout << it->max_runners;
            std::cout << std::endl;
            index++;
        }
        std::cout << "------------------- End Timing information -------------------" << std::endl;


        std::cout <<
            "------------------- Run information(csv): -------------------" <<
            std::endl;
        std::cout << "cores simulation,number simulations(max),cores server,runtime per iteration mean,ensemble members,state size,timesteps,mean bandwidth (MB/s)" << std::endl;
        int number_simulations_max = -1;
        double runtime = 0.0;
        if (info.size() >= 30) {  // have at least 10 iterations for stats
            // 10 warmup and 10 cooldown
            for (auto it = info.begin()+10; it != info.end()-10; it++)
            {
                if (number_simulations_max < it->max_runners) {
                    number_simulations_max = it->max_runners;
                }
                runtime += it->get_walltime();
            }
            runtime /= info.size()-20;
        }
        std::cout << cores_simulation << ',';
        std::cout << number_simulations_max << ',';
        std::cout << cores_server << ',';
        std::cout << runtime << ',';
        std::cout << ensemble_members << ',';
        std::cout << state_size << ',';
        std::cout << info.size() << ',';
        std::cout << 8*state_size*ensemble_members*2.0/runtime;

        std::cout << std::endl;
        std::cout << "------------------- End Run information -------------------" << std::endl;
    }

};
