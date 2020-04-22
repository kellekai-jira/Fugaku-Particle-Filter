#include <chrono>
#include <list>
#include <iostream>
#include <iomanip>
#include <array>
#include <fstream>

#include "utils.h"

typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;

enum TimingEventType
{
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

struct EventTypeTranslation
{
    int enter_type;
    int leave_type;
    const char * name;
};

class Timing
{
private:
    std::chrono::high_resolution_clock::time_point null_time;
    double to_millis(const TimePoint &lhs) {

        return std::chrono::duration<double, std::milli>(lhs - null_time).count();
    }

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

    Timing() :
        null_time(std::chrono::milliseconds(atoll(getenv("MELISSA_TIMING_NULL"))))
    {
    }

    template<std::size_t SIZE>
    void write_region_csv(const std::array<EventTypeTranslation, SIZE> &event_type_translations, const char * base_filename, int rank) {
        std::ofstream outfile ( "trace." + std::string(base_filename) + "." + std::to_string(rank) + ".csv");

        outfile << "rank,start_time,end_time,region,parameter" << std::endl;

        std::list<TimingEvent> open_events;

        for (auto evt : events)
        {
            // Opening event: push on stack
            bool found_anything = false;
            for(auto ett : event_type_translations)
            {
                if (evt.type == ett.enter_type)
                {
                    //D("Pushing event");
                    open_events.push_back( evt );
                    found_anything = true;
                    break;
                } else if (evt.type == ett.leave_type) {
                    for (auto oevt = open_events.rbegin(); oevt != open_events.rend(); ++oevt)
                    {
                        // REM: -1 as parameter closes last...
                        if (oevt->type == ett.enter_type && (oevt->parameter == evt.parameter || evt.parameter == -1))
                        {
                            //D("Popping event and writing region");
                            outfile << rank
                                << ',' << std::setprecision( 11 ) << to_millis(oevt->time)
                                << ',' << std::setprecision( 11 ) << to_millis(evt.time)
                                << ',' << ett.name
                                << ',' << oevt->parameter
                                << std::endl;

                            // remove from stack:
                            open_events.erase( --(oevt.base()) );  // hope this reverse operator works

                            found_anything = true;
                            break;
                        }
                    }
                    if (found_anything)
                    {
                        break;
                    }
                    else
                    {
                        D("Did not find enter region event for %d %d at %f ms", evt.type,
                                evt.parameter, to_millis(evt.time));
                    }
                }
                if (found_anything) {
                    break;
                }
            }
            if (!found_anything)
            {
                D("Event %d is no enter/leave region event", evt.type);
            }

        }

        outfile.close();
    }
};


