// TODOS:
// TODO 1. refactoring
//   DONE: use zmq cpp?? --> no
// lower depth of if trees!
// no long comments
// TODO 2. error prone ness
// TODO: check ret values!
// TODO: heavely test fault tollerance with a good testcase.
// TODO 3. check with real world sim and DA.
// TODO: clean up L logs and D debug logs
//
// TODO: check for other erase bugs...(erasing from a container while iterating
// over the same container)

#include "Assimilator.h"
#include "Field.h"
#include "LauncherConnection.h"
#include "MpiManager.h"
#include "Part.h"
#include "ServerTiming.h"
#include "ZeroMQ.h"
#include "melissa_da_config.h"
#include "memory.h"
#include "messages.h"
#include "utils.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
// one could also use an unordered set which might be faster but we would need
// to define hash functions for all the classes we puit in this container.
#include <set>
#include <string>
#include <utility>
#include <vector>

#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
#include "FTmodule.h"
#endif

#include "melissa_messages.h"
#include "melissa_utils.h" // melissa_utils from melissa-sa for melissa_get_node_name


// Activate this include manually before build if you need score-p  user regions
// for polling. At the same time you will need to use the scorep wrappers for
// compilation. see compile.sh and profiling/ for more information
#if 0
#include <scorep/SCOREP_User.h>
#else
#define SCOREP_USER_REGION_DEFINE(...)
#define SCOREP_USER_REGION_BEGIN(...)
#define SCOREP_USER_REGION_END(...)
#endif

extern int ENSEMBLE_SIZE;

int ENSEMBLE_SIZE = 5;

const int MAX_FAIL_COUNT = 5;

#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
FTmodule FT;
#endif
MpiManager mpi;

AssimilatorType ASSIMILATOR_TYPE = ASSIMILATOR_DUMMY;


std::shared_ptr<LauncherConnection> launcher;


// in seconds:
long long MAX_RUNNER_TIMEOUT = 5;

const int TAG_NEW_TASK = 42;
const int TAG_KILL_RUNNER = 43;
const int TAG_RANK_FINISHED = 44;
const int TAG_ALL_FINISHED = 45;

int highest_received_task_id = 0;

std::map<int, bool> launcher_notified_about_runner;

// only important on ranks != 0:
int highest_sent_task_id = 0;

size_t IDENTITY_SIZE = 0;
using ConnectionIdentity = std::unique_ptr<char[]>;

void* context;
void* data_response_socket;

unsigned int assimilation_cycles = 0; // only used for logging stats at the end.

/// Will be counted as announced by chosen assimilator. This does not forcibly
/// correspond to any time counting as done in the runner code Will also be
/// checkpointed to restart...
int current_step = 0; // will effectively start at 1.

/// current_nsteps is set to -1 if we want to end.
/// current_nsteps is set to -2 if there was a crash and we want to end all
/// runners otherwise it is set to the steps >0 each runner shall propagate
int current_nsteps = 1; // this is important if there are less model task
                        // runners than ensemble members. for every model task
                        // runner at the beginning an ensemble state will be
                        // generated.

int get_due_date() {
    time_t seconds;
    seconds = time(NULL);
    // low: we might use a bigger data type here...
    return static_cast<int>(seconds + MAX_RUNNER_TIMEOUT);
}

struct Task
{
    int state_id;
    int runner_id;
};

bool operator<(const Task& lhs, const Task& rhs) {
    return lhs.state_id < rhs.state_id
           || (lhs.state_id == rhs.state_id && lhs.runner_id < rhs.runner_id);
}

std::set<Task> killed; // when a runner from this list connects we thus respond
                       // with a kill message. if one rank receives a kill
                       // message it has to call exit so all runner ranks are
                       // quit.


std::unique_ptr<Field> field(nullptr);

#ifdef REPORT_TIMING
std::unique_ptr<ServerTiming> timing(nullptr);
#endif

void report_timing() {
#ifdef REPORT_TIMING
    if (comm_rank == 0)
    {
        timing->report(
            field->local_vect_sizes_runner.size(), comm_size, ENSEMBLE_SIZE,
            field->globalVectSize(), field->globalVectSizeHidden());
    }
#ifdef REPORT_TIMING_ALL_RANKS
    const std::array<EventTypeTranslation, 4> event_type_translations = {
        {{START_ITERATION, STOP_ITERATION, "Iteration"},
            {START_FILTER_UPDATE, STOP_FILTER_UPDATE, "Filter Update"},
            {START_IDLE_RUNNER, STOP_IDLE_RUNNER, "Runner idle"},
            {START_PROPAGATE_STATE, STOP_PROPAGATE_STATE,
             "State propagation"}}};
    timing->write_region_csv(
        event_type_translations, "melissa_server", comm_rank);
#endif
#endif
}


struct RunnerRankConnection
{
    ConnectionIdentity identity_;

    RunnerRankConnection(ConnectionIdentity&& id)
        : identity_(std::move(id)) {
        assert(identity_);
    }

    void launch_sub_task(const int runner_rank, const int state_id) {
        // get state and send it to the runner rank...
        assert(identity_);

        zmq::send_n(
            data_response_socket, identity_.get(), IDENTITY_SIZE, ZMQ_SNDMORE);
        zmq::send_empty(data_response_socket, ZMQ_SNDMORE);

        int header[4] = {state_id, current_step, CHANGE_STATE, current_nsteps};

        zmq::send_n(data_response_socket, header, 4, ZMQ_SNDMORE);
        // we do not know when it will really send. send is non blocking!

        const Part& part = field->getPart(runner_rank);
        auto data_msg = zmq::msg_init_n(
            field->ensemble_members.at(state_id).state_analysis.data()
            + part.local_offset_server,
            part.send_count);

        if (runner_rank == 0)
        {
            M_TRIGGER(START_PROPAGATE_STATE, state_id);
        }

        const Part& hidden_part = field->getPartHidden(runner_rank);
        MDBG("-> Server sending %lu + %lu hidden bytes for state %d, timestep=%d",
          part.send_count * sizeof(VEC_T),
          hidden_part.send_count * sizeof(VEC_T), header[0], header[1]);
        MDBG("local server offset %lu, local runner offset %lu, sendcount=%lu",
          field->getPart(runner_rank).local_offset_server,
          field->getPart(runner_rank).local_offset_runner,
          field->getPart(runner_rank).send_count);
        // print_vector(field->ensemble_members.at(state_id).state_analysis);
        MDBG("content=[%d,%d,%d,%d,%d...]",
          field->ensemble_members.at(state_id).state_analysis.data()[0],
          field->ensemble_members.at(state_id).state_analysis.data()[1],
          field->ensemble_members.at(state_id).state_analysis.data()[2],
          field->ensemble_members.at(state_id).state_analysis.data()[3],
          field->ensemble_members.at(state_id).state_analysis.data()[4]);

        int flag;
        if (hidden_part.send_count > 0)
        {
            flag = ZMQ_SNDMORE;
        }
        else
        {
            flag = 0;
        }
        zmq::send(*data_msg, data_response_socket, flag);

        if (flag == ZMQ_SNDMORE)
        {
            auto data_msg_hidden = zmq::msg_init_n(
                field->ensemble_members.at(state_id).state_hidden.data()
                + hidden_part.local_offset_server,
                hidden_part.send_count);
            // VEC_T* tmp =
            //    field->ensemble_members.at(state_id).state_hidden.data();
            // tmp += hidden_part.local_offset_server;
            // MDBG("Hidden values to send:");
            // print_vector(std::vector<VEC_T>(tmp, tmp +
            // hidden_part.send_count));

            zmq::send(*data_msg_hidden, data_response_socket);
        }

        // close connection:
        // but do not free it. send is going to free it.
        identity_ = nullptr;
    }

    // TODO: clean up error messages in the case of ending runners on the api
    // side...
    void stop(const int end_flag = END_RUNNER) {
        // some connection_identities will be 0 if some runner ranks are
        // connected to another server rank at the moment.
        if (!identity_)
        {
            return;
        }

        zmq::send_n(data_response_socket, identity_.get(), IDENTITY_SIZE);
        zmq::send_empty(data_response_socket, ZMQ_SNDMORE);

        auto nsteps = 0;
        int header[] = {-1, current_step, end_flag, nsteps};

        zmq::send_n(data_response_socket, header, 4);

        MDBG("Send end message");

        identity_ = nullptr;
    }
};


struct Runner // Server perspective of a Model task runner
{
    // model task runner ranks
    std::map<int, RunnerRankConnection> connected_runner_ranks;

    void stop(int end_flag) {
        for (auto cs = connected_runner_ranks.begin();
             cs != connected_runner_ranks.end(); cs++)
        {
            MDBG("xxx end connected runner rank...");
            cs->second.stop(end_flag);
        }
    }

    ~Runner() {
        // try to kill remaining runners if there are still some.
        stop(KILL_RUNNER);
    }
};

// might also contain half empty stuff
std::map<int, std::shared_ptr<Runner> > idle_runners;

std::map<int, std::shared_ptr<Runner> >::iterator get_completely_idle_runner() {
    // finds a runner that is completely idle. Returns idle_runners.end() if
    // none was found.
    auto result = std::find_if(
        idle_runners.begin(), idle_runners.end(),
        [](std::pair<int, std::shared_ptr<Runner> > elem) {
        return elem.second->connected_runner_ranks.size()
        == field->connected_runner_ranks.size();
    });
    return result;
}

std::set<int> unscheduled_tasks;
std::vector<int> failcount;

// only important on rank 0:
// if == comm_size we start the update step. is reseted when a state is
// rescheduled!
int finished_ranks = -1;

/// Used to link states to runner id's
struct NewTask
{
    int runner_id;
    int state_id;
    int due_date;
    int task_id;
};


/// used to transmit new tasks to clients. A subtask is the state part whih is
/// sent / expected to be received by this server rank from a single simulation
/// rank
struct SubTask
{
    int runner_id;
    int runner_rank;
    int state_id;
    int due_date;
    // low: set different due dates so not all ranks communicate at the same
    // time to the server when it gets bypassed ;)
    SubTask(NewTask& new_task, int runner_rank_) {
        runner_id = new_task.runner_id;
        runner_rank = runner_rank_;
        state_id = new_task.state_id;
        due_date = new_task.due_date;
    }
};

// REM: works as fifo! (that is why we are not using sets here: they do not keep
// the order.) fifo with tasks that are running, running already on some model
// task runner ranks or not these are checked for the due dates! if we get
// results for the whole task we remove it from the scheduled_tasks list.
typedef std::list<std::shared_ptr<SubTask> > SubTaskList;
SubTaskList scheduled_sub_tasks; // could be ordered sets! this prevents us from
                                 // adding 2 the same!? No would not work as
                                 // (ordered) sets order by key and not by time
                                 // of insertion.
SubTaskList running_sub_tasks;
SubTaskList finished_sub_tasks; // TODO: why do we need to store this? actually
                                // not needed.
// TODO: extract fault tolerant n to m code to use it elsewhere?

void register_runner_id(
    zmq::Message& msg, void* configuration_socket,
    char* data_response_port_names) {
    assert(zmq::size(msg) == 2 * sizeof(int));

    static bool register_field = true;

    int int_buffer[] = {-1, -1};

    std::memcpy(int_buffer, zmq::data(msg), zmq::size(msg));

    auto runner_id = int_buffer[1];
    auto found = std::find_if(
        killed.begin(), killed.end(),
        [runner_id](const Task& task) {
        return task.runner_id == runner_id;
    });

    assert(found == killed.end()); // runner was not dead yet


    MDBG("Server registering Runner ID %d", runner_id);

    // notify launcher on first expose. This way we are sure that then we always
    // have an open connection and we see if the runner breaks if it doesn't
    // respond on this connection.
    auto res = launcher_notified_about_runner.emplace(
        std::pair<int, bool>(runner_id, false));
    if (!res.second)
    {
        // element existed already. This is probably a runner restart. So
        // renotify launcher when runner ready
        launcher_notified_about_runner.at(runner_id) = false; // TODO: can this
                                                              // really happen
                                                              // in the latest
                                                              // protocol?
    }

    // At the moment we request field registration from the runner who connects
    // first. Be fault tollerant during server init too? - actually we do not
    // want to. Faults during init may make it crashing...

    int output[] = {register_field ? 1 : 0, comm_size};
    zmq::send_n(configuration_socket, output, 2, ZMQ_SNDMORE);

    register_field = false;

    auto msg_reply2 = zmq::msg_init_n(
        data_response_port_names, comm_size * MPI_MAX_PROCESSOR_NAME);
    zmq::send(*msg_reply2, configuration_socket);
}

std::vector<INDEX_MAP_T> global_index_map;
std::vector<INDEX_MAP_T> global_index_map_hidden;

void register_field(zmq::Message& field_msg, void* configuration_socket) {
    // we accept new fields only if in initialization phase.
    assert(phase == PHASE_INIT);
    assert(
        zmq::size(field_msg) == 4 * sizeof(int) + MPI_MAX_PROCESSOR_NAME);
    assert(field == nullptr); // we accept only one field for now.

    int buf[4] = {0};

    std::memcpy(buf, zmq::data(field_msg), sizeof(buf));

    int runner_comm_size = buf[1];
    // TODO: rename hidden state into something more useful. hidden can be
    // confused with the hidden state in HMM...
    int bytes_per_element = buf[2];
    // TODO: testcase where bytes for hidden are different from assimilated
    // state elements
    int bytes_per_element_hidden = buf[3];
    char field_name[MPI_MAX_PROCESSOR_NAME + 1] = {0};

    std::strncpy(
        field_name,
        zmq::data(field_msg) + 4 * sizeof(int),
        sizeof(field_name) - 1);

    field = std::make_unique<Field>(
        field_name, runner_comm_size, ENSEMBLE_SIZE, bytes_per_element,
        bytes_per_element_hidden);

    MDBG("Server registering Field %s, runner_comm_size = %d", field_name,
      runner_comm_size);

    assert_more_zmq_messages(configuration_socket);

    auto msg = zmq::recv(configuration_socket);
    assert(zmq::size(*msg) == runner_comm_size * sizeof(size_t));
    std::memcpy(
        field->local_vect_sizes_runner.data(), zmq::data(*msg),
        runner_comm_size * sizeof(size_t));

    // always await a hidden state
    assert_more_zmq_messages(configuration_socket);
    msg = zmq::recv(configuration_socket);
    assert(zmq::size(*msg) == runner_comm_size * sizeof(size_t));
    std::memcpy(
        field->local_vect_sizes_runner_hidden.data(), zmq::data(*msg),
        runner_comm_size * sizeof(size_t));


    // always await global_index_map now
    size_t global_vect_size = sum_vec(field->local_vect_sizes_runner);
    assert(global_vect_size % bytes_per_element == 0);
    global_index_map.resize(global_vect_size / bytes_per_element);
    assert_more_zmq_messages(configuration_socket);
    msg = zmq::recv(configuration_socket);
    assert(zmq::size(*msg) == global_index_map.size() * sizeof(INDEX_MAP_T));
    std::memcpy(
        global_index_map.data(), zmq::data(*msg),
        global_index_map.size() * sizeof(INDEX_MAP_T));

    size_t global_vect_size_hidden =
        sum_vec(field->local_vect_sizes_runner_hidden);
    assert(global_vect_size_hidden % bytes_per_element_hidden == 0);
    global_index_map_hidden.resize(
        global_vect_size_hidden / bytes_per_element_hidden);
    assert_more_zmq_messages(configuration_socket);
    msg = zmq::recv(configuration_socket);
    assert(
        zmq::size(*msg)
        == global_index_map_hidden.size() * sizeof(INDEX_MAP_T));

    // only copy if we have a non zero index map!
    if (global_index_map_hidden.data())
    {
        std::memcpy(
            global_index_map_hidden.data(), zmq::data(*msg),
            global_index_map_hidden.size() * sizeof(INDEX_MAP_T));
    }

    // scatter index map is done in broadcast field info
    // msg is closed outside by caller...

    MDBG("indexmapsize: %lu", global_index_map.size());
    MDBG("hiddenindexmapsize: %lu", global_index_map_hidden.size());
    // print_vector(global_index_map);

    field->name = field_name;

    // ack
    zmq::send_empty(configuration_socket);
}

void answer_configuration_message(
    void* configuration_socket, char* data_response_port_names) {
    assert(configuration_socket);
    assert(data_response_port_names);

    auto msg = zmq::recv(configuration_socket);

    if (zmq::size(*msg) < sizeof(int))
    {
        std::fprintf(
            stderr,
            "expected configuration message with at least four bytes, got %zu "
            "bytes\n",
            zmq::size(*msg));
        std::_Exit(EXIT_FAILURE);
    }

    auto task = -1;

    std::memcpy(&task, zmq::data(*msg), sizeof(int));

    if (task == REGISTER_RUNNER_ID)
    {
        register_runner_id(
            *msg, configuration_socket, data_response_port_names);
    }
    else if (task == REGISTER_FIELD)
    {
        register_field(*msg, configuration_socket);
    }
    else
    {
        // Bad message type
        assert(false);
        exit(1);
    }
}

void scatter_index_map(
    size_t global_vect_size, size_t local_vect_size,
    INDEX_MAP_T global_index_map_data[], INDEX_MAP_T local_index_map_data[],
    const int bytes_per_element) {
    size_t scounts[comm_size];
    calculate_local_vect_sizes_server(
        comm_size, global_vect_size, scounts, bytes_per_element);

    // transform size_t to mpi's int
    size_t displs[comm_size];
    size_t last_displ = 0;
    for (int i = 0; i < comm_size; ++i)
    {
        displs[i] = last_displ;
        assert(scounts[i] % bytes_per_element == 0);
        scounts[i] /= bytes_per_element;
        last_displ += scounts[i];
    }

    slow_MPI_Scatterv(
        global_index_map_data, scounts, displs, MPI_MY_INDEX_MAP_T,
        local_index_map_data, local_vect_size / bytes_per_element,
        MPI_MY_INDEX_MAP_T, 0, mpi.comm());
}

void broadcast_field_information_and_calculate_parts() {
    char field_name[MPI_MAX_PROCESSOR_NAME];
    int runner_comm_size; // Very strange bug: if I declare this variable in the
                          // if / else scope it does not work!. it gets
                          // overwritten by the mpi_bcast for the
                          // runner_comm_size
    int bytes_per_element;
    int bytes_per_element_hidden;

    if (comm_rank == 0)
    {
        strcpy(field_name, field->name.c_str());
        runner_comm_size = field->local_vect_sizes_runner.size();
        bytes_per_element = field->bytes_per_element;
        bytes_per_element_hidden = field->bytes_per_element_hidden;
    }


    // FIXME: see if there is unproper use of comm somewhere or if everywhere
    // mpi.comm() is used!

    MPI_Bcast(
        field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0,
        mpi.comm()); // 1:fieldname
    MPI_Bcast(
        &runner_comm_size, 1, MPI_INT, 0,
        mpi.comm()); // 2:runner_comm_size

    MPI_Bcast(
        &bytes_per_element, 1, MPI_INT, 0,
        mpi.comm()); // 3:runner_comm_size

    MPI_Bcast(
        &bytes_per_element_hidden, 1, MPI_INT, 0,
        mpi.comm()); // 4:runner_comm_size

    if (comm_rank != 0)
    {
        field = std::make_unique<Field>(
            field_name, runner_comm_size, ENSEMBLE_SIZE, bytes_per_element,
            bytes_per_element_hidden);
    }

    MDBG("local_vect_sizes");
    print_vector(field->local_vect_sizes_runner);

    MPI_Bcast(
        field->local_vect_sizes_runner.data(), runner_comm_size, my_MPI_SIZE_T,
        0, mpi.comm()); // 5:local_vect_sizes_runner

    MPI_Bcast(
        field->local_vect_sizes_runner_hidden.data(), runner_comm_size,
        my_MPI_SIZE_T, 0, mpi.comm()); // 6:local_vect_sizes_runner_hidden

    field->calculate_parts(comm_size);

    // 7 and 8: Scatter the field transform (index_maps)
    scatter_index_map(
        field->globalVectSize(), field->local_vect_size,
        global_index_map.data(), field->local_index_map.data(),
        bytes_per_element);

    MDBG("rank %d index map:", comm_rank);
    // print_vector(field->local_index_map);

    scatter_index_map(
        field->globalVectSizeHidden(), field->local_vect_size_hidden,
        global_index_map_hidden.data(), field->local_index_map_hidden.data(),
        bytes_per_element_hidden);

    // printf("rank %d hidden index map:", comm_rank);
    // print_vector(field->local_index_map_hidden);
}

/// returns true if could send the sub_task on a connection.
bool try_launch_subtask(std::shared_ptr<SubTask>& sub_task) {
    // tries to send this task.
    auto found_runner = idle_runners.find(sub_task->runner_id);
    if (found_runner == idle_runners.end())
    {
        MDBG("could not send: Did not find idle runner");
        return false;
    }

    auto found_rank = found_runner->second->connected_runner_ranks.find(
        sub_task->runner_rank);
    if (found_rank == found_runner->second->connected_runner_ranks.end())
    {
        MDBG("could not send: Did not find rank");
        return false;
    }

    MDBG("Send after adding subtask! to runner_id %d", sub_task->runner_id);

    found_rank->second.launch_sub_task(
        sub_task->runner_rank, sub_task->state_id);

    found_runner->second->connected_runner_ranks.erase(found_rank);
    if (found_runner->second->connected_runner_ranks.empty())
    {
        idle_runners.erase(found_runner);
    }

    if (sub_task->runner_rank == 0)
    {
        M_TRIGGER(STOP_IDLE_RUNNER, sub_task->runner_id);
    }

    return true;
}

void kill_task(Task t) {
    MPRT("killing state %d runner %d", t.state_id, t.runner_id);
    unscheduled_tasks.insert(t.state_id);
    idle_runners.erase(t.runner_id);
    killed.emplace(t);
    auto f = [&t](std::shared_ptr<SubTask>& task) {
                 return task->state_id == t.state_id && task->runner_id ==
                        t.runner_id;
             };
    running_sub_tasks.remove_if(f);
    finished_sub_tasks.remove_if(f);
    scheduled_sub_tasks.remove_if(f);

    // there might be more states scheduled to this runner! these will initiate
    // their own violation and will be rescheduled later ;) if we would kill
    // just by state id we would need to synchronize the killing (what if we
    // rescheduled already the state on the next and then the kill message is
    // coming..... so we would kill it from the next.....)
}

/// adds a subtask for each runner rank.
// either add subtasks to list of scheduled subtasks or runs them directly
// adding them to running sub tasks.
void add_sub_tasks(NewTask& new_task) {
    int ret = unscheduled_tasks.erase(new_task.state_id);
    assert(ret == 1);
    auto& csr = field->connected_runner_ranks;
    assert(csr.size() > 0); // connectd runner ranks must be initialized...
    for (auto it = csr.begin(); it != csr.end(); it++)
    {
        std::shared_ptr<SubTask> sub_task(new SubTask(new_task, *it));
        MDBG("Adding subtask for runner rank %d", *it);

        if (try_launch_subtask(sub_task))
        {
            running_sub_tasks.push_back(sub_task);
        }
        else
        {
            scheduled_sub_tasks.push_back(sub_task);
        }
    }
}

/// schedules a new task on a model task runner and tries to run it.
static int task_id = 1; // low: aftrer each update step one could reset the task
                        // id and also the highest sent task id and so on to
                        // never get overflows!
bool schedule_new_task(const int runner_id) {
    assert(comm_rank == 0);
    if (unscheduled_tasks.size() <= 0)
    {
        return false;
    }
    task_id++;
    int state_id = *(unscheduled_tasks.begin());

    NewTask new_task({runner_id, state_id, get_due_date(), task_id});

    MPRT("Schedule task with task id %d", task_id);

    finished_ranks = 0;

    add_sub_tasks(new_task);


    // Send new scheduled task to all server ranks! This makes sure that
    // everybody receives it!
    MPI_Request requests[comm_size - 1];
    for (int receiving_rank = 1; receiving_rank < comm_size; receiving_rank++)
    {
        // REM: MPI_Ssend to be sure that all messages are received!
        // MPI_Ssend(&new_task, sizeof(NewTask), MPI_BYTE, receiving_rank,
        // TAG_NEW_TASK, mpi.comm());
        MPI_Isend(
            &new_task, sizeof(NewTask), MPI_BYTE, receiving_rank, TAG_NEW_TASK,
            mpi.comm(), &requests[receiving_rank - 1]);
    }

    int ret = MPI_Waitall(comm_size - 1, requests, MPI_STATUSES_IGNORE);
    assert(ret == MPI_SUCCESS);


    return true;
}

/// checks if the server added new tasks... if so tries to run them.
void check_schedule_new_tasks() {
    assert(comm_rank != 0);
    int received;

    MPI_Iprobe(0, TAG_NEW_TASK, mpi.comm(), &received, MPI_STATUS_IGNORE);
    if (!received)
        return;

    MDBG("Got task to send...");

    // we are not finished anymore so resend if we are finished:

    NewTask new_task;
    MPI_Recv(
        &new_task, sizeof(new_task), MPI_BYTE, 0, TAG_NEW_TASK, mpi.comm(),
        MPI_STATUS_IGNORE);

    highest_received_task_id =
        std::max(new_task.task_id, highest_received_task_id);

    // Remove all tasks with the same id!
    // REM: we assume that we receive new_task messages in the right order! This
    // is done by ISend on rank 0 and the wait all behind ;)
    auto f = [&new_task](std::shared_ptr<SubTask> st) {
                 if (st->state_id == new_task.state_id)
                 {
                     bool is_new =
                         killed.emplace(Task({st->state_id,
                                              st->runner_id})).second;
                     if (is_new)
                     {
                         MPRT(
                             "The state %d was before scheduled on runner id %d as we "
                             "reschedule now we know that this runnerid was killed.",
                             st->state_id, st->runner_id);
                         // REM: not necessary to resend. rank0 should already know it
                         // from its own!
                         unscheduled_tasks.insert(st->state_id);
                         idle_runners.erase(st->runner_id);
                         killed.insert(Task({st->state_id, st->runner_id}));
                     }
                     return true;
                 }
                 else
                 {
                     return false;
                 }
             };

    scheduled_sub_tasks.remove_if(f);
    running_sub_tasks.remove_if(f);
    finished_sub_tasks.remove_if(f);

    add_sub_tasks(new_task);
}


void end_all_runners() {
    for (auto runner_it = idle_runners.begin(); runner_it != idle_runners.end();
         runner_it++)
    {
        runner_it->second->stop(END_RUNNER);
    }
}

void kill_all_runners() {
    for (auto runner_it = idle_runners.begin(); runner_it != idle_runners.end();
         runner_it++)
    {
        runner_it->second->stop(KILL_RUNNER);
    }
}

void init_new_timestep() {
    size_t connections = field->connected_runner_ranks.size();
    // init or finished....
    assert(
        assimilation_cycles == 0
        || finished_sub_tasks.size() == ENSEMBLE_SIZE * connections);

    assert(scheduled_sub_tasks.size() == 0);
    assert(running_sub_tasks.size() == 0);

    finished_sub_tasks.clear();
    highest_received_task_id = 0;
    highest_sent_task_id = 0;
    task_id = 1;

    current_step += current_nsteps;

    M_TRIGGER(START_ITERATION, current_step);
    for (auto it = idle_runners.begin(); it != idle_runners.end(); it++)
    {
        M_TRIGGER(START_IDLE_RUNNER, it->first);
    }

    assert(unscheduled_tasks.size() == 0);

    for (int i = 0; i < ENSEMBLE_SIZE; i++)
    {
        unscheduled_tasks.insert(i);
    }
    std::fill(failcount.begin(), failcount.end(), 0);
}

void fail_state(int state_id) {
    assert(comm_rank == 0);
    failcount.at(state_id)++;
    int failed = failcount.at(state_id);
    if (failed > MAX_FAIL_COUNT)
    {
        MPRT("Propagating ensemble member %d failed %d times in a row on "
          "different runners. Go fix your ensemble. Checkpointing now and "
          "quitting.",
          state_id, failed);

        // TODO: maybe send a little error message to the launcher!
        launcher.reset();
        exit(1);
    }
}

void reset_due_dates() {
    auto reset_date = [&](std::shared_ptr<SubTask>& it) {
                          it->due_date = get_due_date();
                      };

    std::for_each(
        scheduled_sub_tasks.begin(), scheduled_sub_tasks.end(), reset_date);
}

void check_due_dates() {
    time_t now;
    now = time(NULL);

    std::set<Task> to_kill;

    auto check_date = [&now, &to_kill](std::shared_ptr<SubTask>& it) {
                          if (now > it->due_date)
                          {
                              to_kill.emplace(Task({it->state_id,
                                                    it->runner_id}));
                          }
                      };

    std::for_each(
        scheduled_sub_tasks.begin(), scheduled_sub_tasks.end(), check_date);
    std::for_each(
        running_sub_tasks.begin(), running_sub_tasks.end(), check_date);

    if (to_kill.size() > 0)
    {
        MPRT("Need to redo %lu states", to_kill.size());
    }


    for (auto it = to_kill.begin(); it != to_kill.end(); it++)
    {
        MPRT("Due date passed for state id %d , runner_id %d at %lu s ",
          it->state_id, it->runner_id, now);

        kill_task(*it);

        if (comm_rank == 0)
        {
            M_TRIGGER(REMOVE_RUNNER, it->runner_id);
            launcher->notify(it->runner_id, TIMEOUT);

            fail_state(it->state_id);

            // reschedule directly if possible
            if (get_completely_idle_runner() != idle_runners.end())
            {
                MPRT("Rescheduling after due date violation detected by rank 0");
                // REM: schedule only once as there is only on more task after
                // the kill it. later we might want to schedule multiple times
                // if we clear runners that are still scheduled or running on
                // the broken runner id...
                schedule_new_task(idle_runners.begin()->first);
            }
        }
        else
        {
            // Send to rank 0 that the runner that calcultated this state is to
            // kill

            MPRT("Sending kill request to rank 0");

            int buf[2] = {it->state_id, it->runner_id};
            // bsend does not work...
            // MPI_Bsend(buf, 2, MPI_INT, 0, TAG_KILL_RUNNER,
            // mpi.comm());
            // if BSend does not find memory use the send version
            MPI_Send(buf, 2, MPI_INT, 0, TAG_KILL_RUNNER, mpi.comm());
            MPRT("Finished kill request to rank 0");
        }
    }
}

void check_kill_requests() {
    assert(comm_rank == 0);
    for (int detector_rank = 1; detector_rank < comm_size; detector_rank++)
    {
        int received;
        MPI_Iprobe(
            detector_rank, TAG_KILL_RUNNER, mpi.comm(), &received,
            MPI_STATUS_IGNORE);
        if (!received)
            continue;

        int buf[2];
        MPI_Recv(
            buf, 2, MPI_INT, detector_rank, TAG_KILL_RUNNER, mpi.comm(),
            MPI_STATUS_IGNORE);
        Task t({buf[0], buf[1]});
        MPRT("Got state_id to kill... %d, killing runner_id %d", t.state_id,
          t.runner_id);

#ifdef REPORT_TIMING
        // only trigger REMOVE_RUNNER if this runner was not yet removed. Since
        // the same runner can have 2 different tasks it is possible that it
        // will be emplaced 2 times (with different task state_ids on killed.)
        auto found =
            std::find_if(killed.begin(), killed.end(), [&t](const Task& task) {
            return task.runner_id == t.runner_id;
        });
        if (found == killed.end())
        {
            M_TRIGGER(REMOVE_RUNNER, t.runner_id);
            launcher->notify(t.runner_id, TIMEOUT);
        }
#endif

        bool is_new = killed.emplace(t).second;
        if (is_new)
        {
            fail_state(t.state_id);
        }
        else
        {
            // don't kill it a second time!
            MPRT("I already knew this");
            continue;
        }

        // REM: do not do intelligent killing of other runners. no worries,
        // their due dates will fail soon too ;)
        kill_task(t);

        // reschedule directly if possible
        if (get_completely_idle_runner() != idle_runners.end())
        {
            MPRT("Rescheduling after due date violation detected by detector_rank "
              "%d",
              detector_rank);
            // REM: schedule only once as there is only on more task after the
            // kill it. later we might want to schedule multiple times if we
            // clear runners that are still scheduled or running on the broken
            // runner id...
            schedule_new_task(idle_runners.begin()->first);
        }
    }
}

void handle_data_response(std::shared_ptr<Assimilator>& assimilator) {
    // TODO: move to send and receive function as on api side... maybe use
    // zproto library?
    auto identity_msg = zmq::recv(data_response_socket);
    assert_more_zmq_messages(data_response_socket);
    auto empty_msg = zmq::recv(data_response_socket);
    assert_more_zmq_messages(data_response_socket);
    auto header_msg = zmq::recv(data_response_socket);
    assert_more_zmq_messages(data_response_socket);
    auto data_msg = zmq::recv(data_response_socket);

    assert(IDENTITY_SIZE == 0 || IDENTITY_SIZE == zmq::size(*identity_msg));

    IDENTITY_SIZE = zmq::size(*identity_msg);

    auto identity = ConnectionIdentity(new char[IDENTITY_SIZE]);

    std::memcpy(
        identity.get(), zmq::data(*identity_msg), zmq::size(*identity_msg));

    assert(
        zmq::size(*header_msg)
        == 4 * sizeof(int) + MPI_MAX_PROCESSOR_NAME * sizeof(char));

    int header[4] = {0};

    std::memcpy(header, zmq::data(*header_msg), sizeof(header));

    int runner_id = header[0];
    int runner_rank = header[1];
    int runner_state_id = header[2]; // = ensemble_member_id;
    int runner_timestep = header[3];
    char field_name[MPI_MAX_PROCESSOR_NAME + 1] = {0};

    std::strncpy(
        field_name, zmq::data(*header_msg) + 4 * sizeof(int),
        sizeof(field_name) - 1);



    // notify launcher about newly connected runner.
    if (comm_rank == 0)
    {
        bool launcher_notified = true;
        try
        {
            launcher_notified = launcher_notified_about_runner.at(runner_id);
        } catch (const std::out_of_range& oor)
        {
            std::cerr << "Out of Range error?" << oor.what() << '\n';
            MERR("a different runner (id=%d) that never registered connected. "
              "Probably this is some zombie runner after a server restart",
              runner_id);
        }
        if (launcher_notified == false)
        {
            launcher->notify(runner_id, RUNNING);
            launcher_notified_about_runner.at(runner_id) = true;

            // Add runner only now for timing and testsuite as now it is really
            // ready (and only now it can also recognized failing from the
            // server. Runner crashes before are only recognized by the
            // launcher)
            M_TRIGGER(ADD_RUNNER, runner_id);
        }
    }

    RunnerRankConnection csr(std::move(identity));
    auto found =
        std::find_if(killed.begin(), killed.end(), [runner_id](Task p) {
        return p.runner_id == runner_id;
    });
    if (found != killed.end())
    {
        MPRT("Ending Model Task Runner killed by timeout violation runner=%d",
          runner_id);
        csr.stop(KILL_RUNNER);
    }
    else
    {
        // good runner_rank, good state id?
        auto running_sub_task = std::find_if(
            running_sub_tasks.begin(), running_sub_tasks.end(),
            [runner_id, runner_rank,
             runner_state_id](std::shared_ptr<SubTask>& st) {
            return st->runner_id == runner_id && st->state_id == runner_state_id
            && st->runner_rank == runner_rank;
        });

        assert(
            runner_timestep == 0
            || running_sub_task != running_sub_tasks.end());


        MDBG("Finished state propagation at %ld s unix time", time(NULL));

        // This is necessary if a task was finished on rank 0. then it crashes
        // on another rank. so rank 0 needs to undo this!
        if (running_sub_task != running_sub_tasks.end())
        {
            // only if we are not in timestep  0:
            finished_sub_tasks.push_back(*running_sub_task);

            running_sub_tasks.erase(running_sub_task);
        }

        // good timestep? There are 2 cases: timestep 0 or good timestep...
        assert(runner_timestep == 0 || runner_timestep == current_step);


        // Save state part in background_states.
        assert(field->name == field_name);
        const Part& part = field->getPart(runner_rank);
        assert(zmq::size(*data_msg) == part.send_count * sizeof(VEC_T));
        MDBG("<- Server received %lu/%lu bytes of %s from runner id %d, runner "
          "rank %d, state id %d, timestep=%d",
          zmq::size(*data_msg), part.send_count * sizeof(VEC_T), field_name,
          runner_id, runner_rank, runner_state_id, runner_timestep);
        MDBG("local server offset %lu, sendcount=%lu", part.local_offset_server,
          part.send_count);

        const Part& hidden_part = field->getPartHidden(runner_rank);
        VEC_T* values_hidden = nullptr;

        auto data_msg_hidden = zmq::MessageRef(nullptr, nullptr);

        if (hidden_part.send_count > 0)
        {
            assert_more_zmq_messages(data_response_socket);
            data_msg_hidden = zmq::recv(data_response_socket);
            assert(
                zmq::size(*data_msg_hidden)
                == hidden_part.send_count * sizeof(VEC_T));
            values_hidden =
                reinterpret_cast<VEC_T*>(zmq::data(*data_msg_hidden));
        }


        if (runner_timestep == current_step)
        {
            if (runner_rank == 0)
            {
                M_TRIGGER(STOP_PROPAGATE_STATE, runner_state_id); // will trigger
                                                                // for
                                                                // runnerrank
                                                                // 0...
            }
            MDBG("storing this timestep!...");
            // zero copy is unfortunately for send only. so copy internally...
            // TODO at the same time we might try to keep the msg data structure
            // intact and send it back when needed.
            field->ensemble_members[runner_state_id]
            .store_background_state_part(
                part, reinterpret_cast<VEC_T*>(zmq::data(*data_msg)),
                hidden_part, values_hidden);
            data_msg_hidden.reset();
#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
            FT.store_subset(field, runner_state_id, runner_rank);
            // FIXME: store hidden state here too!
#endif
        }
        else if (runner_state_id == -1)
        {
            // we are getting the very first timestep here!
            // Some assimilators depend on something like this.
            // namely the CheckStateless assimilator
            assimilator->on_init_state(
                runner_id, part, reinterpret_cast<VEC_T*>(zmq::data(*data_msg)),
                hidden_part, values_hidden);
        }
        // otherwise we throw away timestep 0 as we want to init the simulation!
        // One could indeed try to generate ensemble members from the initial
        // state but this is a too special case so we rely on other mechanics
        // for initialization of an initial state.


        // Check if we can answer directly with new data... means starting of a
        // new model task
        auto found = std::find_if(
            scheduled_sub_tasks.begin(), scheduled_sub_tasks.end(),
            [runner_id, runner_rank](std::shared_ptr<SubTask>& st) {
            return st->runner_id == runner_id
            && st->runner_rank == runner_rank;
        });

        if (found != scheduled_sub_tasks.end())
        {
            // Found a new task. Send back directly!

            MDBG("send after receive! to runner rank %d on runner_id %d",
              runner_rank, runner_id);
            csr.launch_sub_task(runner_rank, (*found)->state_id);
            // don't need to delete from connected runner as we were never in
            // there...  TODO: do this somehow else. probably not csr.send but
            // another function taking csr as parameter...
            running_sub_tasks.push_back(*found);
            scheduled_sub_tasks.erase(found);
        }
        else
        {
            // Save connection - basically copy identity pointer...
            auto& runner =
                idle_runners
                .emplace(runner_id, std::shared_ptr<Runner>(new Runner()))
                .first->second;

            if (runner_rank == 0)
            {
                M_TRIGGER(START_IDLE_RUNNER, runner_id);
            }

            runner->connected_runner_ranks.emplace(runner_rank, std::move(csr));
            MDBG("save connection runner_id %d, runner rank %d", runner_id,
              runner_rank);

            if (comm_rank == 0)
            {
                // If we could not start a new model task try to schedule a new
                // one. This is initiated by server rank 0 ( no new model task
                // means that at least this rank is finished. so the others will
                // finish soon too as we assum synchronism in the simulaitons)
                // check if there is no other task running on this runner
                if (idle_runners[runner_id]->connected_runner_ranks.size()
                    == field->connected_runner_ranks.size())
                {
                    schedule_new_task(runner_id);
                }
            }
        }
    }
}

// returns true if the whole assimilation (all time steps) finished
bool check_finished(std::shared_ptr<Assimilator> assimilator) {
    // check if all data was received. If yes: start Update step to calculate
    // next analysis state

    size_t connections = field->connected_runner_ranks.size();

    bool finished;
    // would this rank finish?
    bool would_finish = unscheduled_tasks.size() == 0
                        && scheduled_sub_tasks.size() == 0 &&
                        running_sub_tasks.size() == 0
                        && finished_sub_tasks.size() == ENSEMBLE_SIZE *
                        connections;
    if (!would_finish)
    {
        return false;
    }
#ifdef RUNNERS_MAY_CRASH // This needs refactoring I guess
    if (comm_rank == 0) // this if only exists if RUNNERS_MAY_CRASH
    {
        // I would finish. So now lets probe the others
        // try to know if somebody else finished
        for (int rank = 1; rank < comm_size; rank++)
        {
            int received;

            MPI_Iprobe(
                rank, TAG_RANK_FINISHED, mpi.comm(), &received,
                MPI_STATUS_IGNORE);
            if (received)
            {
                int highest_task_id;
                MPI_Recv(
                    &highest_task_id, 1, MPI_INT, rank, TAG_RANK_FINISHED,
                    mpi.comm(), MPI_STATUS_IGNORE);
                if (highest_task_id == task_id)
                {
                    // this message was up to date....
                    finished_ranks++;
                }
            }
        }
        // comm size without rank 0 ... the others would finish too...
        finished = would_finish && finished_ranks == comm_size - 1;

        if (finished)
        {
            MPRT("Sending tag all finished message for timestep %d to %d other "
              "server ranks",
              current_step, comm_size - 1);
            // tell everybody that everybody finished!
            for (int rank = 1; rank < comm_size; rank++)
            {
                MPI_Send(
                    nullptr, 0, MPI_BYTE, rank, TAG_ALL_FINISHED, mpi.comm());
            }
        }
    }
    else
    {
        // rank != 0:
        finished = false;

        // REM: we need to synchronize when we finish as otherwise processes are
        // waiting in the mpi barrier to do the update step while still some
        // ensemble members need to be repeated due to failing runner.


        if (highest_sent_task_id < highest_received_task_id)
        {
            // only send once when we finished a new task that was received
            // later.  REM: using Bsend as faster as buffere, buffering not
            // necessary here...

            MPI_Send(
                &highest_received_task_id, 1, MPI_INT, 0, TAG_RANK_FINISHED,
                mpi.comm());
            highest_sent_task_id = highest_received_task_id; // do not send
                                                             // again..
        }

        // now wait for rank 0 to tell us that we can finish.
        int received;
        MPI_Iprobe(
            0, TAG_ALL_FINISHED, mpi.comm(), &received, MPI_STATUS_IGNORE);
        if (received)
        {
            MPRT("receiving tag all finished message");
            MPI_Recv(
                nullptr, 0, MPI_BYTE, 0, TAG_ALL_FINISHED, mpi.comm(),
                MPI_STATUS_IGNORE);
            finished = true;
        }
    }
#else
    // RUNNERS MAY NOT CRASH:
    // if everything finished and nothing to reschedule: all fine!
    finished = would_finish;
#endif

    if (finished)
    {
#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
        // FIXME: shortcut to here if recovering (do not do first background
        // state calculation!)
        FT.recover();
#endif
        bool launcher_ok = true;
        if (comm_rank == 0)
        {
            launcher_ok = launcher->checkLauncherDueDate(); // did the launcher
                                                            // already timeout
                                                            // before the long
                                                            // update step?
        }
        // get new analysis states from update step
        MPRT("====> Update step %d", current_step);
        M_TRIGGER(START_FILTER_UPDATE, current_step);
        // TODO: don't allow writing to background state for checkpointing and
        // assimilator!
        current_nsteps =
            assimilator->do_update_step(current_step); // FIXME do time
                                                       // dependent update
                                                       // step!!, completely
                                                       // integrated?
        M_TRIGGER(STOP_FILTER_UPDATE, current_step);

#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
        // REM: we do not profile the time for checkpointing for now
        FT.flushCP(); // TODO: put into one function
        FT.finalizeCP();
#endif
        if (comm_rank == 0 && !launcher_ok)
        {
            // Launcher died! Wait for next update step and send back to all
            // simulations to shut themselves down. This way we are sure to send
            // to all and we can finish the current update step gracefully.
            MPRT("ERROR: Launcher did not answer. Due date violation. Crashing "
              "Server now.");
            current_nsteps = -2;
        }


        // Get current_nsteps from rank 0 in case it wants to Crash!
        MPI_Bcast(&current_nsteps, 1, MPI_INT, 0, mpi.comm());
        assimilation_cycles++;

        for (auto it = idle_runners.begin(); it != idle_runners.end(); it++)
        {
            M_TRIGGER(STOP_IDLE_RUNNER, it->first);
        }
        M_TRIGGER(STOP_ITERATION, current_step);

        if (current_nsteps == -1)
        {
            MDBG("The assimilator decided to end now.");
            end_all_runners();
            return true;
        }
        else if (current_nsteps == -2)
        {
            kill_all_runners();
            exit(1);
            return true;
        }

        init_new_timestep();

        // After update step: rank 0 loops over all runner_id's sending them a
        // new state vector part they have to propagate.
        if (comm_rank == 0)
        {
            // As in this loop we might erase some idle runners from the list we
            // may not do a simple for loop.
            auto runner_it = get_completely_idle_runner();
            while (runner_it != idle_runners.end()
                   && unscheduled_tasks.size() > 0)
            {
                MPRT("Rescheduling after update step for timestep %d",
                  current_step);
                schedule_new_task(runner_it->first);
                runner_it = get_completely_idle_runner();
            }
        }
#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
        FT.initCP(current_step);
#endif
        MDBG("Finished update step at %ld s unix time", time(NULL));
    }

    return false;
}

/// Call:
///
/// melissa_server <total steps> <ensemble size> <assimilator type id> \
// ///     <max runner timeout in s> <server slowdown factor> <launcher host name>
///
/// The Server slowdown factor is only taken into account for debug builds. It
/// is useful to slowdown the CPU usage while e.g. in a GDB session.
int main(int argc, char* argv[]) {

    assert(argc == 7);

    int param_total_steps = 5;
    int server_slowdown_factor = 1;

    // Read in configuration from command line
    if (argc >= 2)
    {
        param_total_steps = atoi(argv[1]);
    }
    if (argc >= 3)
    {
        ENSEMBLE_SIZE = atoi(argv[2]);
    }
    if (argc >= 4)
    {
        ASSIMILATOR_TYPE = static_cast<AssimilatorType>(atoi(argv[3]));
    }
    if (argc >= 5)
    {
        MAX_RUNNER_TIMEOUT = atoi(argv[4]);
    }
    if (argc >= 6)
    {
        server_slowdown_factor = atoi(argv[5]); // will wait this time * 10
                                                // useconds every server
                                                // mainloop...
        MDBG("using server slowdown factor of %d", server_slowdown_factor);
    }
    // 7th argument must be the launcher host name!


    assert(ENSEMBLE_SIZE > 0);

    mpi.init();

    init_utils();

#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
    FT.init(mpi, current_step);
#endif

    comm_size = mpi.size();
    comm_rank = mpi.rank();

    std::shared_ptr<Assimilator> assimilator; // will be inited later when we
                                              // know the field dimensions.

    failcount.resize(ENSEMBLE_SIZE);
    std::fill(failcount.begin(), failcount.end(), 0);

    context = zmq_ctx_new();
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    MDBG("Current 0MQ version is %d.%d.%d", major, minor, patch);
    MDBG("**server rank = %d", comm_rank);
    MPRT("Start server with %d ensemble members", ENSEMBLE_SIZE);

    char hostname[MPI_MAX_PROCESSOR_NAME];
    melissa_get_node_name(hostname, MPI_MAX_PROCESSOR_NAME);

    // Start sockets:
    void* configuration_socket = NULL;
    if (comm_rank == 0)
    {
        configuration_socket = zmq_socket(context, ZMQ_REP);
        const char* configuration_socket_addr =
            "tcp://*:4000"; // to be put
                            // into
                            // environment
                            // variable
                            // MELISSA_SERVER_MASTER_NODE
                            // on simulation
                            // start
        int rc = zmq_bind(configuration_socket, configuration_socket_addr);
        if (rc != 0 && errno == 98)
        {
            // Address already in use. Try once more...
            MPRT("Address %s already in use. Retrying ONCE again to bind in 5s...",
              configuration_socket_addr);
            sleep(1);
            rc = zmq_bind(configuration_socket, configuration_socket_addr);
        }

        MPRT("Configuration socket listening on port %s",
          configuration_socket_addr);
        ZMQ_CHECK(rc);

        launcher = std::make_shared<LauncherConnection>(context, argv[6]);
    }

    data_response_socket = zmq_socket(context, ZMQ_ROUTER);
    char data_response_port_name[MPI_MAX_PROCESSOR_NAME + 32] = {0};
    snprintf(
        data_response_port_name, sizeof(data_response_port_name), "tcp://*:%d",
        5000 + comm_rank);
    ZMQ_CHECK(zmq_bind(data_response_socket, data_response_port_name));

    snprintf(
        data_response_port_name, sizeof(data_response_port_name), "tcp://%s:%d",
        hostname, 5000 + comm_rank);

    char data_response_port_names[MPI_MAX_PROCESSOR_NAME * comm_size];
    MPI_Gather(
        data_response_port_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
        data_response_port_names, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0,
        mpi.comm());

#ifdef REPORT_TIMING
    // Start Timing:
#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
        timing = std::make_unique<ServerTiming>();
    }
#endif

#ifdef REPORT_MEMORY
    // for memory benchmarking:
    int last_seconds_memory = 0;
#endif


#ifdef REPORT_WORKLOAD
    // for memory benchmarking:
    int cycles = 0;
#endif

    int last_seconds_checks = 0;


    int items_to_poll = 1;
    if (comm_rank == 0)
    {
        // poll configuration socket
        items_to_poll += 2;
    }

    SCOREP_USER_REGION_DEFINE(zmq_poll_region_handle)

    // Server main loop:
    while (true)
    {
#ifdef NDEBUG
        // usleep(1);
#else
        usleep(10 * server_slowdown_factor); // to chill down the processor!
                                             // TODO remove when measuring!
#endif
        // Wait for requests
        /* Poll for events indefinitely */
        // REM: the poll item needs to be recreated all the time!


        SCOREP_USER_REGION_BEGIN(
            zmq_poll_region_handle, "zmq_poll", SCOREP_USER_REGION_TYPE_COMMON)
        zmq_pollitem_t items[items_to_poll];
        items[0] = {data_response_socket, 0, ZMQ_POLLIN, 0};
        if (comm_rank == 0)
        {
            items[1] = {configuration_socket, 0, ZMQ_POLLIN, 0};
            items[2] = {launcher->getTextPuller(), 0, ZMQ_POLLIN, 0};
        }


        // poll the fastest possible to be not in concurrence with the mpi probe
        // calls... (theoretically we could set this time to -1 if using only
        // one core for the server.)
        ZMQ_CHECK(zmq_poll(items, items_to_poll, 0));
        /* Returned events will be stored in items[].revents */
        SCOREP_USER_REGION_END(zmq_poll_region_handle)

        // answer requests
        if (comm_rank == 0)
        {
            if (items[1].revents & ZMQ_POLLIN)
            {
                answer_configuration_message(
                    configuration_socket, data_response_port_names);
            }

            if (items[2].revents & ZMQ_POLLIN)
            {
                launcher->receiveText();
            }
            else if (!launcher->checkLauncherDueDate())
            {
                // if we know no runners end here already!
                // (There are probably none in the queue neither as there is no
                // launcher...)
                if (scheduled_sub_tasks.size() + running_sub_tasks.size()
                    == 0)
                {
                    MERR("There are no runners and also the Launcher does not "
                      "respond. Ending the server now!");
                }
            }

            launcher->ping();
        }

        // coming from fresh init...
        if (phase == PHASE_INIT)
        {
            if ((comm_rank == 0 && field != nullptr) || comm_rank != 0)
            {
                // check if initialization on rank 0 finished
                // (rank 0 does some more intitialization than the other server
                // ranks) other ranks will just Wait for rank 0 to finish field
                // registrations. rank 0 does this in answer_configu propagate
                // all fields to the other server clients on first message
                // receive! low: if multi field: check fields! (see if the field
                // names we got are the ones we wanted) propagate all fields to
                // the other server clients on first message receive!
                broadcast_field_information_and_calculate_parts();

                // init assimilator as we know the field size now.
                assimilator = Assimilator::create(
                    ASSIMILATOR_TYPE, *field, param_total_steps, mpi);


                current_nsteps = assimilator->getNSteps();
                init_new_timestep(); // init_new_timestep needs the latest
                                     // timestep id from the assimilator!

#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
                FT.protect_background(mpi, field);
#endif

#ifdef RUNNERS_MAY_CRASH
                // Assimilator creation can take quite long. Thus reset due
                // dates here to allow tighter runner timeouts:
                reset_due_dates();
#endif

                MDBG("Change Phase");
                phase = PHASE_SIMULATION;
            }
        }

        int seconds = static_cast<int>(time(NULL));

        if (phase == PHASE_SIMULATION)
        {
            // X     if simulation requests work see if work in list. if so
            // launch it. if not save connection X     if scheduling message
            // from rank 0: try to run message. if the state was before
            // scheduled on an other point move this to the killed.... X there
            // are other states that will fail due to the due date too. for them
            // an own kill message is sent and they are rescheduled. X     check
            // for due dates. if detected: black list (move to killed) runner +
            // state. send state AND runner to rank0 X     if finished and did
            // not send yet the highest task id send to rank 0 that we finished.
            // X     check for messages from rank 0 that we finished and if so
            // start update
            //
            //       rank 0:
            // X     if runner request work see if work in list. if so: launch
            // it. if not check if we have more work to do and schedule it on
            // this runner. send this to all clients. this is blocking with
            // ISend to be sure that it arrives and we do no reschedule before.
            // this also guarantees the good order.. X     at the same time
            // check if some client reports a crash. if crash. put state to
            // killed states(blacklist it) and reschedule task. X     check for
            // due dates. if detected: black list runner and state id. and do
            // the same as if I had a kill message from rank 0: reschedule the
            // state X     if finished and all finished messages were received,
            // (finished ranks == comm ranks) send to all runners that we
            // finished  and start update


#ifdef REPORT_WORKLOAD
            if (comm_rank == 0)
            {
                cycles++;
            }
#endif

            if (last_seconds_checks < seconds)
            {
                last_seconds_checks = seconds;

#ifdef RUNNERS_MAY_CRASH
                // Checking only every second as this is the resolution of
                // timeouts... check if we have to kill some jobs as they did
                // not respond. This can Send a kill request to rank 0
                check_due_dates();

                if (comm_rank == 0)
                {
                    check_kill_requests();
                }
#endif

#ifdef REPORT_WORKLOAD
                if (comm_rank == 0)
                {
                    MPRT("Cycles last second: %d", cycles);
                    cycles = 0;
                }
#endif

#ifdef REPORT_TIMING
#ifndef REPORT_TIMING_ALL_RANKS
                if (comm_rank == 0)
#endif
                {
                    if (timing->is_time_to_write()) {
                        report_timing();
                    }
                }
#endif


            }

            if (comm_rank != 0)
            {
                // check if rank 0 wants us to schedule some new tasks.
                check_schedule_new_tasks();
            }

            if (items[0].revents & ZMQ_POLLIN)
            {
                handle_data_response(assimilator);
                // REM: We try to schedule new data after the server rank 0 gave
                // new tasks and after receiving new data. It does not make
                // sense to schedule at other times for the moment. if there is
                // more fault tollerance this needs to be changed.
            }

            if (check_finished(assimilator))
            {
                break; // all runners finished.
            }


            /// REM: Tasks are either unscheduled, scheduled, running or
            /// finished.
            size_t connections = field->connected_runner_ranks.size();
            //          MPRT("unscheduled sub tasks: %lu, scheduled sub tasks: %lu
            //          running sub tasks: %lu finished sub tasks: %lu",
            //                  unscheduled_tasks.size() * connections,  //
            //                  scale on amount of subtasks.
            //                  scheduled_sub_tasks.size(),
            //                  running_sub_tasks.size(),
            //                  finished_sub_tasks.size());

            // all tasks must be somewhere. either finished, scheduled on a
            // runner, running on a runner or unscheduled.
            assert(
                unscheduled_tasks.size() * connections + // scale on amount of
                                                         // subtasks.
                scheduled_sub_tasks.size() + running_sub_tasks.size()
                + finished_sub_tasks.size()
                == connections * ENSEMBLE_SIZE);
        }


#ifdef REPORT_MEMORY

        if (comm_rank == 0 && (seconds - 5 > last_seconds_memory))
        {
            MemoryReportValue();
            last_seconds_memory = seconds;
        }
#endif
    }

    if (comm_rank == 0)
    {
        MPRT("Gracefully ending server now.");
        MPRT("Executed %d assimilation cycles with %d ensemble members each, with "
          "a runner timeout of %lli seconds",
          assimilation_cycles, ENSEMBLE_SIZE, MAX_RUNNER_TIMEOUT);
    }


    MDBG("Ending Server.");
    // TODO: check if we need to delete some more stuff!

    if (comm_rank == 0)
    {
        // send stop message, close the launcher sockets before the context is
        // destroyed!
        launcher.reset();
    }


    report_timing();

    zmq_close(data_response_socket);
    if (comm_rank == 0)
    {
        zmq_close(configuration_socket);
    }
    zmq_ctx_destroy(context);

#if defined(WITH_FTI) && defined(WITH_FTI_CHECKOINT_DA_SERVER)
    FT.finalize();
#endif
    mpi.finalize();

    MPRT(".");
}
