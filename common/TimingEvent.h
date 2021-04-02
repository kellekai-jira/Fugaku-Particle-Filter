#include <chrono>
#include <list>
#include <iostream>
#include <iomanip>
#include <array>
#include <fstream>

#include "utils.h"
#include "melissa_da_config.h"


enum TimingEventType
{
    // REM: ADD_RUNNER has no closing event....
    ADD_RUNNER                        =  0,  // parameter = runner_id
    REMOVE_RUNNER                     =  1,  // parameter = runner_id
    START_ITERATION                   =  2,  // parameter = timestep
    STOP_ITERATION                    =  3,  // parameter = timestep
    START_FILTER_UPDATE               =  4,  // parameter = timestep
    STOP_FILTER_UPDATE                =  5,  // parameter = timestep
    START_IDLE_RUNNER                 =  6,  // parameter = runner_id
    STOP_IDLE_RUNNER                  =  7,  // parameter = runner_id
    START_PROPAGATE_STATE             =  8,  // parameter = state_id, in p2p server: runner_id
    STOP_PROPAGATE_STATE              =  9,  // parameter = state_id,
    NSTEPS                            = 10, // parameter = nsteps, only used by runner so far
    INIT                              = 11, // no parameter  // defines init ... it is not always 0 as the timing api is not called at the NULL environment variable...


    // p2p added stuff:
    // see https://gitlab.inria.fr/melissa/melissa-da/-/issues/65
    // App Core:
    START_INIT                    = 12,  // time from comm_init till first melissa expose  or first call to callback in head
    STOP_INIT                     = 13,
    //START_IDLE_RUNNER                 = 14, //(time in melissa expose)
    //STOP_IDLE_RUNNER                  = 15, //(time in melissa expose)
    START_JOB_REQUEST                 = 16,
    STOP_JOB_REQUEST                  = 17, // parameter = next_state.id (=job.id)
    START_LOAD                        = 18,  // parameter = parent_state.t
    STOP_LOAD                         = 19,  // parameter = parent_state.id
    START_CHECK_LOCAL                 = 20, // parameter = parent_state.t
    STOP_CHECK_LOCAL                  = 21, // parameter = parent_state.id
    START_WAIT_HEAD                   = 22,// parameter = job.t
    STOP_WAIT_HEAD                    = 23,// parameter = job.id
    //START_PROPAGATE_STATE             = 24,
    //STOP_PROPAGATE_STATE              = 25,
    START_CALC_WEIGHT                 = 26, // parameter = job.t
    STOP_CALC_WEIGHT                  = 27, // parameter = job.id
    START_LOAD_OBS                    = 28,  //(needs to be done in python)
    STOP_LOAD_OBS                     = 29,  //(needs to be done in python)
    START_PUSH_WEIGHT_TO_HEAD         = 30, // parameter = job.t
    STOP_PUSH_WEIGHT_TO_HEAD          = 31, // parameter = job.id
    START_STORE                       = 80, // parameter = job.t
    STOP_STORE                        = 81, // parameter = job.id

    // FTI core:
    //START_INIT_FTI_HEAD               = 32,
    //STOP_INIT_FTI_HEAD                = 33,
    START_IDLE_FTI_HEAD               = 34,
    STOP_IDLE_FTI_HEAD                = 35,
    START_PUSH_STATE_TO_PFS           = 36,
    STOP_PUSH_STATE_TO_PFS            = 37,
    START_PREFETCH                    = 38, //(sum of the following subregions)
    STOP_PREFETCH                     = 39, //(sum of the following subregions)
    START_PREFETCH_REQ                = 40, //(time only for the request)
    STOP_PREFETCH_REQ                 = 41, //(time only for the request)
    START_REQ_RUNNER                  = 42, //(for each runner that is tried)
    STOP_REQ_RUNNER                   = 43, //(for each runner that is tried)
    START_COPY_STATE_FROM_RUNNER      = 44, // parameter = foreign runner id
    STOP_COPY_STATE_FROM_RUNNER       = 45, // parameter = foreign runner id
    START_COPY_STATE_TO_RUNNER        = 82, // parameter = foreign runner id
    STOP_COPY_STATE_TO_RUNNER         = 83, // parameter = foreign runner id
    START_COPY_STATE_FROM_PFS         = 46,
    STOP_COPY_STATE_FROM_PFS          = 47,
    START_DELETE                      = 48, //(sum of following subregions)
    STOP_DELETE                       = 49, //(sum of following subregions)
    START_DELETE_REQ                  = 50,
    STOP_DELETE_REQ                   = 51,
    START_DELETE_LOCAL                = 52,
    STOP_DELETE_LOCAL                 = 53,
    START_DELETE_PFS                  = 54,
    STOP_DELETE_PFS                   = 55,



    // Additional single events:
    PEER_HIT                          = 56,    //(state available at peer) parameter = runner_id
    PEER_MISS                         = 57,    //(state not available at peer) parameter = runner_id
    PFS_PULL                          = 58,
    LOCAL_HIT                         = 59,    //(state found local) parameter = id
    LOCAL_MISS                        = 60,   //(state not found local)
    LOCAL_DELETE                      = 61, //(local states deleted)
    PFS_DELETE                        = 62    //(global states deleted)

    //Server :  --> python!
    //(these are easily comparable with the old melissa-da measures)
    //
    //
    //also time all requests on the server side!

        // REM: we are not measuring extra memcopies introduced for melissa

    //ITERATION (time per assimilation cycle)

    //FILTER_UPDATE (time to resample)

    //PROPAGATE_STATE (time a particle takes in total from sending out until receiving the weight)
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

    time_t report_time = 0;

public:
    std::list<TimingEvent> events;  // a big vector should be more performant!

    Timing() :
        // Time since epoch in ms. Set from the launcher.
        null_time(std::chrono::milliseconds(atoll(getenv("MELISSA_TIMING_NULL")))),
        fifo_os(getenv("MELISSA_DA_TEST_FIFO"))
    {
        if (getenv("MELISSA_DA_TIMING_REPORT")) {
            report_time = atoll(getenv("MELISSA_DA_TIMING_REPORT"));

            if (report_time < time(NULL)) {
                report_time = 0;
                L("MELISSA_DA_TIMING_REPORT time was before. No report will be generated before the greaceful end of this runner");
            } else {
                L("Will report timing information at %lu unix seconds (in %lu seconds)",
                        report_time, report_time - time(NULL));
            }
        }

        const char * fifo_file = getenv("MELISSA_DA_TEST_FIFO");
        if (fifo_file != nullptr) {
            timing_to_fifo_testing = true;
            fifo_os = std::ofstream(fifo_file);
            std::fprintf(
                    stderr, "connected to fifo %s\n",
                    fifo_file
                    );

            D("connected to fifo %s", fifo_file);
        }

        this->trigger_event(INIT, 0); // == trigger(INIT, 0)
        const auto p1 = std::chrono::system_clock::now();
        const unsigned long long ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                p1.time_since_epoch()).count();


        L("Init timing at %llu ms since epoch", ms_since_epoch);

        L("Null time   at %llu ms since epoch (timing-events csv are relative to this)", atoll(getenv("MELISSA_TIMING_NULL")));

    }

    inline void trigger_event(TimingEventType type, const int parameter) {
        events.push_back(TimingEvent(type, parameter));

        if (timing_to_fifo_testing) {
            fifo_os << type << "," << parameter << std::endl;
            fifo_os.flush();
        }
    }

    inline bool is_time_to_write() {
        static bool wrote = false;
        if (wrote) {
            // write only once!
            return false;
        }
        // in seconds
        if (report_time != 0 && time(NULL) >= report_time) {
            wrote = true;
            return true;
        } else {
            return false;
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
    void write_region_csv(const std::array<EventTypeTranslation, SIZE> &event_type_translations, const char * base_filename, int rank, bool close_different_parameter=false) {
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
                        if (oevt->type == ett.enter_type &&
                                (close_different_parameter || oevt->parameter == evt.parameter || evt.parameter == -1))
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



