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


void report() {
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
}

};
