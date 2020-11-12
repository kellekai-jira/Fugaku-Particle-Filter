#include <chrono>
#include <list>
#include <iostream>
#include <iomanip>
#include <array>
#include <fstream>

#include "utils.h"


enum TimingEventType
{
    // REM: ADD_RUNNER has no closing event....
    ADD_RUNNER                  = 0,  // parameter = runner_id
    REMOVE_RUNNER               = 1,  // parameter = runner_id
    START_ITERATION             = 2,  // parameter = timestep
    STOP_ITERATION              = 3,  // parameter = timestep
    START_FILTER_UPDATE         = 4,  // parameter = timestep
    STOP_FILTER_UPDATE          = 5,  // parameter = timestep
    START_IDLE_RUNNER           = 6,  // parameter = runner_id
    STOP_IDLE_RUNNER            = 7,  // parameter = runner_id
    START_PROPAGATE_STATE       = 8,  // parameter = state_id
    STOP_PROPAGATE_STATE        = 9,  // parameter = state_id,
    NSTEPS                      = 10, // parameter = nsteps, only used by runner so far
    INIT                        = 11, // no parameter  // defines init ... it is not always 0 as the timing api is not called at the NULL environment variable...
};

#ifdef REPORT_TIMING

#ifdef REPORT_TIMING_ALL_RANKS
#define TIMED_RANK comm_rank
#else
#define TIMED_RANK 0
#endif

#define trigger(type, param) if (comm_rank == TIMED_RANK) timing->trigger_event(type, \
                                                                       param)
#else
#define trigger(type, param)
#endif



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

    bool timing_to_fifo_testing = false;
    std::ofstream fifo_os;

public:
    std::list<TimingEvent> events;  // a big vector should be more performant!

    Timing() :
        // Time since epoch in ms. Set from the launcher.
        null_time(std::chrono::milliseconds(atoll(getenv("MELISSA_TIMING_NULL"))))
    {
        const char * fifo_file = getenv("MELISSA_DA_TEST_FIFO");
        if (fifo_file != nullptr) {
            timing_to_fifo_testing = true;
            fifo_os = std::ofstream(fifo_file);
            std::fprintf(
                    stderr, "connected to fifo %s\n",
                    fifo_file
                    );
        }

        this->trigger_event(INIT, 0); // == trigger(INIT, 0)
        const auto p1 = std::chrono::system_clock::now();
        const unsigned long long ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                p1.time_since_epoch()).count();


        L("Init timing at %llu ms since epoch", ms_since_epoch);

        L("Null time   at %llu ms since epoch (timing-events csv are relative to this)", atoll(getenv("MELISSA_TIMING_NULL")));

    }

    void trigger_event(TimingEventType type, const int parameter) {
        events.push_back(TimingEvent(type, parameter));

        if (timing_to_fifo_testing) {
            fifo_os << type << "," << parameter << std::endl;
            fifo_os.flush();
        }
    }

    void print_events(const char * base_filename, const int rank) {
        std::cout << "------------ Writing Timing Event List (csv) ------------" <<
            std::endl;

        std::ofstream nullfile( "nulltime." + std::string(base_filename) + "." + std::to_string(rank) + ".csv" );

        nullfile << "null time (ms)" << std::endl;
        nullfile << getenv("MELISSA_TIMING_NULL") << std::endl;


        std::ofstream outfile ( "timing-events." + std::string(base_filename) + "." + std::to_string(rank) + ".csv");

        outfile << "time first event (ms),event,parameter" << std::endl;
        for (auto it = events.begin(); it != events.end(); it++)
        {
            double t = to_millis(it->time);
            outfile << t << ',';
            outfile << it->type << ',';
            outfile << it->parameter << std::endl;
        }
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
                            open_events.erase( --(oevt.base()) );

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

    }
};



