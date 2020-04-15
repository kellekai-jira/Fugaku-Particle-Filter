#include <chrono>
#include <list>
#include <iostream>

typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

enum TimingEventType
{  // REM: Start and stop events must always be corresponding in this enum
    ADD_RUNNER                  = 0,  // parameter = runner_id
    REMOVE_RUNNER               = 1,  // parameter = runner_id
    START_ITERATION             = 2,  // parameter = timestep
    STOP_ITERATION              = 3,  // parameter = timestep
    START_FILTER_UPDATE         = 4,  // parameter = timestep
    STOP_FILTER_UPDATE          = 5,  // parameter = timestep
    START_IDLE_RUNNER           = 6,  // parameter = runner_id
    STOP_IDLE_RUNNER            = 7,  // parameter = runner_id
    START_PROPAGATE_STATE       = 8,  // parameter = state_id
    STOP_PROPAGATE_STATE        = 9,  // parameter = state_id
};

#ifdef REPORT_TIMING

#ifdef REPORT_TIMING_ALL_RANKS
#define TIMED_RANK comm_rank
#else
#define TIMED_RANK 0
#endif

//#ifdef NDEBUG
#define trigger(type, param) if (comm_rank == TIMED_RANK) timing->trigger_event(type, \
                                                                       param)
//#else
//#define trigger(type, param) if (comm_rank == TIMED_RANK) { timing->trigger_event(type, \
                                                                         //param); \
                                                   //auto now = \
                                                       //std::chrono:: \
                                                       //high_resolution_clock:: \
                                                       //now(); double \
                                                       //xxxxt = \
                                                       //std::chrono::duration< \
                                                           //double, std::milli>( \
                                                           //now.time_since_epoch()) \
                                                       //. \
                                                       //count(); D( \
                                                       //"Trigger event %d with parameter %d at %f ms", \
                                                       //type, param, xxxxt); }
//#endif NDEBUG
#else
#define trigger(type, param)
#endif


double diff_to_millis(const TimePoint &lhs, const TimePoint &rhs) {
    return std::chrono::duration<double, std::milli>(lhs-rhs).count();
}

struct TimingEvent
{

    TimingEventType type;
    TimePoint time;
    int parameter = -1;

    TimingEvent(TimingEventType type_, const int parameter_) :
        type(type_), parameter(parameter_)
    {
        time = std::chrono::high_resolution_clock::now();
    }

    double operator-(const TimingEvent &rhs) const {
        // in milliseconds
        return std::chrono::duration<double, std::milli>(time-rhs.time).count();
    }


};

class Timing
{
public:
    std::list<TimingEvent> events;  // a big vector should be more performant!
    void trigger_event(TimingEventType type, const int parameter)
    {
        events.push_back(TimingEvent(type, parameter));
    }
    void print_events() {
        std::cout << "------------ Timing Event List (csv) ------------" <<
            std::endl;
        std::cout << "time first event (ms),event,parameter" << std::endl;
        const TimePoint &start=events.begin()->time;
        for (auto it = events.begin(); it != events.end(); it++)
        {
            double t = diff_to_millis(it->time, start);
            std::cout << t << ',';
            std::cout << it->type << ',';
            std::cout << it->parameter << std::endl;
        }
        std::cout << "------------ End Timing Event List (csv) ------------" <<
            std::endl;
    }
};



