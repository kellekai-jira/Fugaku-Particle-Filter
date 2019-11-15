#include <vector>
#include <time.h>  // TODO:  use clock instead??!
#include <iostream>

struct TimingIteration {
    time_t walltime; // in seconds
    int min_runners = -1;
    int max_runners = 0;
};

class Timing {
private:
    std::vector<TimingIteration> info;
    std::vector<TimingIteration>::iterator cur;
public:
    Timing(const int total_steps) {
        info.resize(total_steps);
    }

    inline void start_iteration() {
        cur = info.begin();
        cur->walltime = time(NULL);
    }

    inline void set_active_modeltask_runners(int runners) {
            if (runners < cur->min_runners || cur->min_runners == -1) {
            cur->min_runners = runners;
        } 

        if (runners > cur->max_runners) {
            cur->max_runners = runners;
        }
    }
    inline void stop_iteration() {
        cur->walltime = time(NULL) - cur->walltime;
        cur++;
    }


    void report() {
        std::cout << "------------------- Timing information(csv): -------------------" << std::endl;
        std::cout << "iteration,walltime,min_runners,max_runners" << std::endl;
        int index = 0;
        for (auto it = info.begin(); it != info.end(); it++)
        {
            std::cout << index << ',';
            std::cout << it->walltime << ',';
            std::cout << it->min_runners << ',';
            std::cout << it->max_runners;
            std::cout << std::endl;
            index ++;
        }
        
    }
};
