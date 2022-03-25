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
    START_ITERATION                   =  2,  // parameter = timestep, in P2P: P == len(alhpa),  how many different states
    STOP_ITERATION                    =  3,  // parameter = timestep
    START_FILTER_UPDATE               =  4,  // parameter = timestep
    STOP_FILTER_UPDATE                =  5,  // parameter = timestep
    START_IDLE_RUNNER                 =  6,  // parameter = runner_id
    STOP_IDLE_RUNNER                  =  7,  // parameter = runner_id
    START_PROPAGATE_STATE             =  8,  // parameter = state_id, parent_id on runner
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
    START_PUSH_WEIGHT_TO_SERVER         = 92, // parameter = job.t
    STOP_PUSH_WEIGHT_TO_SERVER          = 93, // parameter = job.id

    START_REQ_RUNNER                  = 42, //  this region comparses all the following events in this block
    STOP_REQ_RUNNER                   = 43, // parameter = 1 if found a runner that has the state. 0 otherwise.
    START_REQ_RUNNER_LIST             = 84, // parameter = runner id
    STOP_REQ_RUNNER_LIST              = 85, // parameter = runner id
    START_REQ_STATE_FROM_RUNNER       = 86,
    STOP_REQ_STATE_FROM_RUNNER        = 87, // parameter = 1 if found a runner that has the state. 0 otherwise.
    START_COPY_STATE_FROM_RUNNER      = 44, // parameter = foreign runner id
    STOP_COPY_STATE_FROM_RUNNER       = 45, // parameter = foreign runner id

    START_HANDLE_AVAIL_REQ            = 88,
    STOP_HANDLE_AVAIL_REQ             = 89, // parameter = 1 if found a runner that has the state. 0 otherwise.
    START_HANDLE_STATE_REQ            = 90,
    STOP_HANDLE_STATE_REQ             = 91, // parameter = 1 if found a runner that has the state. 0 otherwise.
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
    PEER_MISS                         = 57,    //(state not available at peer)
    PFS_PULL                          = 58,    // state pulled from pfs as it could not be mirrored from another peer
    LOCAL_HIT                         = 59,    //(state requested by next job found local, does not concern prefetched stuff) parameter = id
    LOCAL_MISS                        = 60,    //(state requested by next not found local, does not concern prefetched stuff) parameter = id
    LOCAL_DELETE                      = 61,    //(local states deleted)
    PFS_DELETE                        = 62,    //(global states deleted)


    DIRTY_LOAD                        = 94,     // happens if a race condition on state loads occurs (is local) and the state load needs to be redone, parameter state.id
    
    START_LOAD                = 18,  // parameter = parent_state.t
    STOP_LOAD                 = 19,  // parameter = parent_state.id
    START_FTI_LOAD          = 95,  // parameter = parent_state.t
    STOP_FTI_LOAD           = 96,  // parameter = parent_state.id
    START_M_LOAD_USER          = 97,  // parameter = parent_state.t
    STOP_M_LOAD_USER           = 98,  // parameter = parent_state.id
    START_MODEL_MESSAGE                = 99, //(time only for the request)
    STOP_MODEL_MESSAGE                 = 100, //(time only for the request)
    STATE_LOCAL_CREATE          = 101,  // parameter = parent_state.t
    STATE_LOCAL_DELETE           = 102,  // parameter = parent_state.id
    START_FTI_STORE          = 103,  // parameter = parent_state.t
    STOP_FTI_STORE           = 104  // parameter = parent_state.id

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

#define M_TRIGGER(type, param) if (comm_rank == TIMED_RANK) timing->trigger_event(type, \
                                                                       param)
#else
#define M_TRIGGER(type, param)
#endif



struct TimingEvent
{

    TimingEventType type;
    TimePoint time;
    int parameter = -1;

    TimingEvent(TimingEventType type_, const int parameter_) :
        type(type_), parameter(parameter_)
    {
        time = std::chrono::system_clock::now();
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
    std::chrono::system_clock::time_point null_time;
    double to_millis(const TimePoint &lhs) {

        return std::chrono::duration<double, std::milli>(lhs - null_time).count();
    }

    bool timing_to_fifo_testing = false;

    std::unique_ptr<std::ofstream> fifo_os;

    time_t report_time = 0ll;
    int m_report_cycle = -100;

public:
    std::list<TimingEvent> events;  // a big vector should be more performant!

    Timing() :
        // Time since epoch in ms. Set from the launcher.
        null_time(std::chrono::milliseconds(atoll(getenv("MELISSA_TIMING_NULL"))))
    {
        if (getenv("MELISSA_DA_TIMING_REPORT")) {
            report_time = atoll(getenv("MELISSA_DA_TIMING_REPORT"));

            if (report_time < time(NULL)) {
                report_time = 0;
                MPRT("MELISSA_DA_TIMING_REPORT time was before. No report will be generated before the greaceful end of this runner");
            } else {
                MPRT("Will report timing information at %lu unix seconds (in %lu seconds)",
                        report_time, report_time - time(NULL));
            }
        }
        
        if (getenv("MELISSA_LORENZ_ITER_MAX")) {
            m_report_cycle = atoi(getenv("MELISSA_LORENZ_ITER_MAX")) - 5;
            MPRT("Will report timing information at cycle %d", m_report_cycle);
        }

        const char * fifo_file = getenv("MELISSA_DA_TEST_FIFO");
        if (fifo_file != nullptr) {
            timing_to_fifo_testing = true;
            fifo_os = std::make_unique<std::ofstream>(fifo_file);
            std::fprintf(
                    stderr, "connected to fifo %s\n",
                    fifo_file
                    );

            MDBG("connected to fifo %s", fifo_file);
        }

        this->trigger_event(INIT, 0); // == M_TRIGGER(INIT, 0)
        const auto p1 = std::chrono::system_clock::now();
        const unsigned long long ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                p1.time_since_epoch()).count();


        MPRT("Init timing at %llu ms since epoch", ms_since_epoch);

        MPRT("Null time   at %llu ms since epoch (timing-events csv are relative to this)", atoll(getenv("MELISSA_TIMING_NULL")));

    }

    inline void trigger_event(TimingEventType type, const int parameter) {
        events.push_back(TimingEvent(type, parameter));

        if (timing_to_fifo_testing) {
            *fifo_os << type << "," << parameter << std::endl;
            fifo_os->flush();
        }
    }

    inline bool is_time_to_write(int cycle) {
        static bool wrote = false;
        if (wrote) {
            // write only once!
            return false;
        }
        // in seconds
        if (report_time != 0 && time(NULL) >= report_time) {
            wrote = true;
            return true;
        } else if ( m_report_cycle == cycle ){
            wrote = true;
            return true;
        } else {
            return false;
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
            outfile << std::setprecision( 13 ) << t << ',';
            outfile << it->type << ',';
            outfile << it->parameter << std::endl;
        }
    }

    template<std::size_t SIZE>
    void write_region_csv(const std::array<EventTypeTranslation, SIZE> &event_type_translations, const char * base_filename, int rank, bool close_different_parameter=false) {
        std::ofstream outfile ( "trace." + std::string(base_filename) + "." + std::to_string(rank) + ".csv");

        outfile << "rank,start_time,end_time,region,parameter_open,parameter_close" << std::endl;

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
                                << ',' << std::setprecision( 13 ) << to_millis(oevt->time)
                                << ',' << std::setprecision( 13 ) << to_millis(evt.time)
                                << ',' << ett.name
                                << ',' << oevt->parameter
                                << ',' << evt.parameter
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
                        MDBG("Did not find enter region event for %d %d at %f ms", evt.type,
                                evt.parameter, to_millis(evt.time));
                    }
                }
                if (found_anything) {
                    break;
                }
            }
            if (!found_anything)
            {
                MDBG("Event %d is no enter/leave region event", evt.type);
            }

        }

    }
};



