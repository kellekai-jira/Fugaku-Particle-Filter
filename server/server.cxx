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



#include <map>
#include <string>
#include <cstdlib>
#include <cassert>
#include <extrae.h>

#include <pthread.h>
#include <utility>
#include <vector>
#include <list>
#include <memory>
#include <algorithm>
#include <fstream>
#include <iostream>

#include <mpi.h>
#include "zmq.h"

#include "Field.h"
#include "FTmodule.h"
#include "messages.h"
#include "Part.h"
#include "utils.h"

#include <time.h>

// one could also use an set which might be faster but we would need to
// define hash functions for all the classes we puit in this container.
#include <set>

#include "Assimilator.h"

#include <cstring>

#include <fti.h>

std::vector<pthread_t> fti_threads;

FTmodule FT;

std::unique_ptr<Field> field(nullptr);
extern int ENSEMBLE_SIZE;
extern int TOTAL_STEPS;  // refactor to total_steps as it is named in pdaf.

int ENSEMBLE_SIZE = 5;
int TOTAL_STEPS = 5;

AssimilatorType ASSIMILATOR_TYPE=ASSIMILATOR_DUMMY;

const long long MAX_RUNNER_TIMEOUT = 10000000;  // 10 min max timeout for runner.

const int TAG_NEW_TASK = 42;
const int TAG_KILL_RUNNER = 43;
const int TAG_RANK_FINISHED = 44;
const int TAG_ALL_FINISHED = 45;

int highest_received_task_id = 0;

// only important on ranks != 0:
int highest_sent_task_id = 0;

size_t IDENTITY_SIZE = 0;

void * context;
void * data_response_socket;

int isRecovery;
int current_step = 0;  // will effectively start at 1.
int current_nsteps = 1;  // this is important if there are less model task runners than ensemble members. for every model task runner at the beginning an ensemble state will be generated.

// only important on rank 0:
// if == comm_size we start the update step. is reseted when a state is rescheduled!
int finished_ranks = -1;

/// schedules a new task on a model task runner and tries to run it.
static int task_id = 1; // low: aftrer each update step one could reset the task id and also the highest sent task id and so on to never get overflows!

int get_due_date() 
{
    time_t seconds;
    seconds = time (NULL);
    // low: we might use a bigger data type here...
    return static_cast<int>(seconds + MAX_RUNNER_TIMEOUT);
}

void my_free(void * data, void * hint)
{
    free(data);
}

struct Task
{
    int state_id;
    int runner_id;
};

/// Used to link states to runner id's
struct NewTask
{
    int runner_id;
    int state_id;
    int due_date;
    int task_id;
};

/// used to transmit new tasks to clients. A subtask is the state part whih is sent / expected to be received by this server rank from a single simulation rank
struct SubTask
{
    int runner_id;
    int runner_rank;
    int state_id;
    int due_date;
    // low: set different due dates so not all ranks communicate at the same time to the server when it gets bypassed ;)
    SubTask(NewTask &new_task, int runner_rank_) {
        runner_id = new_task.runner_id;
        runner_rank = runner_rank_;
        state_id = new_task.state_id;
        due_date = new_task.due_date;
    }
};

bool operator<(const Task &lhs, const Task &rhs) 
{
    return lhs.state_id < rhs.state_id || (lhs.state_id == rhs.state_id &&
                                           lhs.runner_id < rhs.runner_id);
}

/* 
 REM: works as fifo! (that is why we are not using sets here: they do not keep the order.)
 fifo with tasks that are running, running already on some model task runner ranks or not
 these are checked for the due dates!
 if we get results for the whole task we remove it from the scheduled_tasks list.
*/
typedef std::list<std::shared_ptr<SubTask> > SubTaskList;

struct RunnerRankConnection
{
    void * connection_identity;

    RunnerRankConnection(void * identity) {
        connection_identity = identity;
    }

    void launch_sub_task(const int runner_rank, const int state_id) {
        // get state and send it to the runner rank...
        assert(connection_identity != NULL);

        zmq_msg_t identity_msg;
        zmq_msg_t empty_msg;
        zmq_msg_t header_msg;
        zmq_msg_t data_msg;

        zmq_msg_init_data(&identity_msg, connection_identity,
                          IDENTITY_SIZE, my_free, NULL);
        zmq_msg_send(&identity_msg, data_response_socket, ZMQ_SNDMORE);

        zmq_msg_init(&empty_msg);
        zmq_msg_send(&empty_msg, data_response_socket, ZMQ_SNDMORE);

        ZMQ_CHECK(zmq_msg_init_size(&header_msg, 4 * sizeof(int)));
        int * header = reinterpret_cast<int*>(zmq_msg_data(
                                                  &header_msg));
        header[0] = state_id;
        header[1] = current_step;
        header[2] = CHANGE_STATE;
        header[3] = current_nsteps;
        zmq_msg_send(&header_msg, data_response_socket, ZMQ_SNDMORE);
        // we do not know when it will really send. send is non blocking!

        zmq_msg_init_data(&data_msg,
                          field->ensemble_members.at(
                              state_id).state_analysis.data() +
                          field->getPart(
                              runner_rank).local_offset_server,
                          field->getPart(runner_rank).send_count *
                          sizeof(double), NULL, NULL);

        D("-> Server sending %lu bytes for state %d, timestamp=%d",
          field->getPart(runner_rank).send_count *
          sizeof(double),
          header[0], header[1]);
        D(
            "local server offset %lu, local runner offset %lu, sendcount=%lu",
            field->getPart(runner_rank).local_offset_server,
            field->getPart(runner_rank).local_offset_runner,
            field->getPart(runner_rank).send_count);
        /*print_vector(field->ensemble_members.at(
                         state_id).state_analysis);
*/
        zmq_msg_send(&data_msg, data_response_socket, 0);

        // close connection:
        // but do not free it. send is going to free it.
        connection_identity = NULL;
    }

    void end(const int end_flag=END_RUNNER) {
        // some connection_identities will be 0 if some runner ranks are connected to another server rank at the moment.
        if (connection_identity == NULL)
            return;

        zmq_msg_t identity_msg;
        zmq_msg_t empty_msg;
        zmq_msg_t header_msg;

        zmq_msg_init_data(&identity_msg, connection_identity,
                          IDENTITY_SIZE, my_free, NULL);
        zmq_msg_send(&identity_msg, data_response_socket, ZMQ_SNDMORE);

        zmq_msg_init(&empty_msg);
        zmq_msg_send(&empty_msg, data_response_socket, ZMQ_SNDMORE);

        zmq_msg_init_size(&header_msg, 4 * sizeof(int));

        int * header = reinterpret_cast<int*>(zmq_msg_data(
                                                  &header_msg));
        header[0] = -1;
        header[1] = current_step;
        header[2] = end_flag;
        header[3] = 0;          // nsteps

        zmq_msg_send(&header_msg, data_response_socket, 0);

        D("Send end message");

        // but don't delete it. this is done in the zmq_msg_send.
        connection_identity = NULL;
    }
};

struct Runner  // Server perspective of a Model task runner
{
    // model task runner ranks
    std::map<int, RunnerRankConnection> connected_runner_ranks;

    void end(int end_flag) {
        for (auto cs = connected_runner_ranks.begin(); cs !=
             connected_runner_ranks.end(); cs++)
        {
            D("xxx end connected runer rank...");
            cs->second.end(end_flag);
        }
    }

    ~Runner() {
        // try to kill remaining runners if there are still some.
        end(KILL_RUNNER);
    }

};

std::set<Task> killed;  // when a runner from this list connects we thus respond with a kill message. if one rank receives a kill message it has to call exit so all runner ranks are quit.

std::map<int, std::shared_ptr<Runner> > idle_runners;

std::set<int> unscheduled_tasks;

SubTaskList scheduled_sub_tasks;  // could be ordered sets! this prevents us from adding 2 the same!? No would not work as (ordered) sets order by key and not by time of insertion.
SubTaskList running_sub_tasks;
SubTaskList finished_sub_tasks;  // TODO: why do we need to store this? actually not needed.
// TODO: extract fault tolerant n to m code to use it elsewhere?

void register_runner_id(zmq_msg_t &msg, const int * buf,
                        void * configuration_socket,
                        char * data_response_port_names) 
{
    static int highest_runner_id = 0;
    assert(zmq_msg_size(&msg) == sizeof(int));

    D("Server registering Runner ID %d", buf[1]);
    //DF("SERVER", "Server registering Runner ID %d", buf[1]);

    zmq_msg_t msg_reply1, msg_reply2;
    zmq_msg_init_size(&msg_reply1, 3 * sizeof(int));

    int request_register_field =  highest_runner_id == 0 ? 1 : 0;

    int * out_buf = reinterpret_cast<int*>(zmq_msg_data(&msg_reply1));
    // every model task runner gets an other runner id.
    out_buf[0] = highest_runner_id++;              
    out_buf[1] = request_register_field;
    out_buf[2] = comm_size;
    zmq_msg_send(&msg_reply1, configuration_socket, ZMQ_SNDMORE);

    zmq_msg_init_data(&msg_reply2, data_response_port_names,
                      comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(char),
                      NULL, NULL);
    zmq_msg_send(&msg_reply2, configuration_socket, 0);

}

void register_field(zmq_msg_t &msg, const int * buf,
                    void * configuration_socket)
{
    // we accept new fields only if in initialization phase.
    assert(phase == PHASE_INIT);       
    assert(zmq_msg_size(&msg)==sizeof(int)+sizeof(int)+MPI_MAX_PROCESSOR_NAME*sizeof(char));
    // TODO: does the following line really work?
    // we accept only one field for now.
    assert(field == nullptr);              

    int runner_comm_size = buf[1];

    char field_name[MPI_MAX_PROCESSOR_NAME];
    strcpy(field_name, reinterpret_cast<const char*>(&buf[2]));
    zmq_msg_close(&msg);

    field = std::make_unique<Field>(field_name, runner_comm_size,ENSEMBLE_SIZE);

    D("Server registering Field %s, runner_comm_size = %d", field_name,runner_comm_size);

    assert_more_zmq_messages(configuration_socket);
    zmq_msg_init(&msg);
    zmq_msg_recv(&msg, configuration_socket, 0);
    assert(zmq_msg_size(&msg) == runner_comm_size * sizeof(size_t));
    memcpy (field->local_vect_sizes_runner.data(),zmq_msg_data(&msg),runner_comm_size * sizeof(size_t));
    field->name = field_name;

    zmq_msg_t msg_reply;
    zmq_msg_init(&msg_reply);
    zmq_msg_send(&msg_reply, configuration_socket, 0);

}

void answer_configuration_message(void * configuration_socket,
                                  char * data_response_port_names)
{
	static int req_runner = 0;
	static int req_field = 0;
    
	zmq_msg_t msg;
    zmq_msg_init(&msg);
    zmq_msg_recv(&msg, configuration_socket, 0);
    int * buf = reinterpret_cast<int*>(zmq_msg_data(&msg));
    if (buf[0] == REGISTER_RUNNER_ID)
    {
		//DF("SERVER", "[%d] register runner", req_runner++ );
        register_runner_id(msg, buf, configuration_socket,
                           data_response_port_names);
    }
    else if (buf[0] == REGISTER_FIELD)
    {
		//DF("SERVER", "[%d] register field", req_field++ );
        register_field(msg, buf, configuration_socket);
    }
    else
    {
        // Bad message type
        assert(false && "Bad message type");
        exit(1);
    }
    zmq_msg_close(&msg);
}

void broadcast_field_information_and_calculate_parts() 
{
    char field_name[MPI_MAX_PROCESSOR_NAME];
    /* 
        Very strange bug: if I declare this variable in the if / else 
        scope it does not work!. it gets overwritten by the mpi_bcast 
        for the runner_comm_size
    */
    int runner_comm_size;      

    if (comm_rank == 0)
    {
        strcpy(field_name, field->name.c_str());
        runner_comm_size = field->local_vect_sizes_runner.size();
    }
    
    // 1:fieldname
    MPI_Bcast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, FTI_COMM_DUP); 
    // 2:runner_comm_size
    MPI_Bcast(&runner_comm_size, 1, my_MPI_SIZE_T, 0, FTI_COMM_DUP); 

    if (comm_rank != 0)
    {
        field = std::make_unique<Field>(field_name, runner_comm_size,ENSEMBLE_SIZE);
    }

    // 3:local_vect_sizes_runner
    MPI_Bcast(field->local_vect_sizes_runner.data(),runner_comm_size,my_MPI_SIZE_T, 0, FTI_COMM_DUP);  

    field->calculate_parts(comm_size);
    
}

/// returns true if could send the sub_task on a connection.
bool try_launch_subtask(std::shared_ptr<SubTask> &sub_task) 
{
    // tries to send this task.
    auto found_runner = idle_runners.find(sub_task->runner_id);
    if (found_runner == idle_runners.end())
    {
        D("could not send: Did not find idle runner");
        return false;
    }

    auto found_rank = found_runner->second->connected_runner_ranks.find(
        sub_task->runner_rank);
    if (found_rank == found_runner->second->connected_runner_ranks.end())
    {
        D("could not send: Did not find rank");
        return false;
    }

    D("Send after adding subtask! to runner_id %d", sub_task->runner_id);

    found_rank->second.launch_sub_task(sub_task->runner_rank,
                                       sub_task->state_id);

    found_runner->second->connected_runner_ranks.erase(found_rank);
    if (found_runner->second->connected_runner_ranks.empty())
    {
        idle_runners.erase(found_runner);
    }

    return true;
}

void kill_task(Task t) 
{
    L("killing state %d runner %d", t.state_id, t.runner_id);
    unscheduled_tasks.insert(t.state_id);
    idle_runners.erase(t.runner_id);
    killed.emplace(t);
    auto f = [&t](std::shared_ptr<SubTask> &task) {
                 return task->state_id == t.state_id &&
                        task->runner_id == t.runner_id;
             };
    running_sub_tasks.remove_if(f);
    finished_sub_tasks.remove_if(f);
    scheduled_sub_tasks.remove_if(f);

}

void add_sub_tasks(NewTask &new_task) 
{
    int ret = unscheduled_tasks.erase(new_task.state_id);
    assert(ret == 1);
    auto &csr = field->connected_runner_ranks;
    assert(csr.size() > 0);      // connectd runner ranks must be initialized...
    for (auto it = csr.begin(); it != csr.end(); it++)
    {
        std::shared_ptr<SubTask> sub_task (new SubTask(new_task, *it));
        L("Adding subtask for runner rank %d", *it);

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

bool schedule_new_task(int runner_id)
{
    assert(comm_rank == 0);
    if (unscheduled_tasks.size() <= 0)
    {
        return false;
    }
    task_id++;
    int state_id = *(unscheduled_tasks.begin());

    NewTask new_task({runner_id, state_id, get_due_date(), task_id});

    L("Schedule task with task id %d", task_id);

    finished_ranks = 0;

    add_sub_tasks(new_task);

    // Send new scheduled task to all server ranks! This makes sure that everybody receives it!
    MPI_Request requests[comm_size - 1];
    for (int receiving_rank = 1; receiving_rank < comm_size;
         receiving_rank++)
    {
        MPI_Isend(&new_task, sizeof(NewTask), MPI_BYTE, receiving_rank,
                  TAG_NEW_TASK, FTI_COMM_DUP,
                  &requests[receiving_rank-1]);
    }

    int ret = MPI_Waitall(comm_size - 1, requests, MPI_STATUSES_IGNORE);
    assert(ret == MPI_SUCCESS);

    return true;

}

void check_schedule_new_tasks()
{
    assert(comm_rank != 0);
    int received;

    MPI_Iprobe(0, TAG_NEW_TASK, FTI_COMM_DUP, &received,MPI_STATUS_IGNORE);
    
    if (!received) return;

    NewTask new_task;
    MPI_Recv(&new_task, sizeof(new_task), MPI_BYTE, 0, TAG_NEW_TASK,FTI_COMM_DUP, MPI_STATUS_IGNORE);

    highest_received_task_id = std::max(new_task.task_id,
                                        highest_received_task_id);
    auto f = [&new_task] (std::shared_ptr<SubTask> st) 
    {
        if (st->state_id == new_task.state_id)
        {
            bool is_new = killed.emplace(Task({st->state_id,st->runner_id})).second;
            if (is_new)
            {
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

void end_all_runners()
{
    for (auto runner_it = idle_runners.begin(); runner_it !=
         idle_runners.end(); runner_it++)
    {
        runner_it->second->end(END_RUNNER);
    }
}

void init_new_timestamp()
{
    size_t connections = field->connected_runner_ranks.size();
    
    // init or finished....
    bool atInit = current_step==0;
    bool allFinished = finished_sub_tasks.size()==ENSEMBLE_SIZE*connections;
    bool atRecovery = isRecovery&&(phase==PHASE_SIMULATION);
    
    assert(atInit || allFinished || atRecovery);
    assert(scheduled_sub_tasks.size() == 0 && "scheduled task failed");
    assert(running_sub_tasks.size() == 0 && "running task failed");

    finished_sub_tasks.clear();
    highest_received_task_id = 0;
    highest_sent_task_id = 0;
    task_id = 1;

    current_step += current_nsteps;

    assert(unscheduled_tasks.size() == 0);

    for (int i = 0; i < ENSEMBLE_SIZE; i++)
    {
        unscheduled_tasks.insert(i);
    }
}

void check_due_dates() 
{
    time_t now;
    now = time (NULL);

    std::set<Task> to_kill;

    auto check_date = [&now,&to_kill] (std::shared_ptr<SubTask>& it) {
                          if (now > it->due_date)
                          {
                              to_kill.emplace(Task({it->state_id,
                                                    it->runner_id}));
                          }
                      };

    std::for_each(scheduled_sub_tasks.begin(), scheduled_sub_tasks.end(),
                  check_date);
    std::for_each(running_sub_tasks.begin(), running_sub_tasks.end(),
                  check_date);

    if (to_kill.size() > 0)
    {
        L("Need to redo %lu states", to_kill.size());
    }


    for (auto it = to_kill.begin(); it != to_kill.end(); it++)
    {
        L("Due date passed for state id %d , runner_id %d at %lu s ",
          it->state_id, it->runner_id, now);

        kill_task(*it);

        if (comm_rank == 0)
        {
            // reschedule directly if possible
            if (idle_runners.size() > 0)
            {
                L("Rescheduling after due date violation detected by rank 0");
                schedule_new_task(idle_runners.begin()->first);
            }

        }
        else
        {
            // Send to rank 0 that the runner that calcultated this state is to kill

            L("Sending kill request to rank 0");

            int buf[2] = { it->state_id, it->runner_id};
            MPI_Bsend(buf, 2, MPI_INT, 0, TAG_KILL_RUNNER,
                      FTI_COMM_DUP);
            L("Finished kill request to rank 0");
        }
    }
}

void check_kill_requests() 
{
    assert(comm_rank == 0);
    for (int detector_rank = 1; detector_rank < comm_size; detector_rank++)
    {
        int received;
        MPI_Iprobe(detector_rank, TAG_KILL_RUNNER, FTI_COMM_DUP,
                   &received, MPI_STATUS_IGNORE);
        if (!received)
            continue;

        int buf[2];
        MPI_Recv(buf, 2, MPI_INT, detector_rank, TAG_KILL_RUNNER,
                 FTI_COMM_DUP, MPI_STATUS_IGNORE);
        Task t({buf[0], buf[1]});
        L("Got state_id to kill... %d, killing runner_id %d",
          t.state_id, t.runner_id);
        bool is_new = killed.emplace(t).second;
        if (!is_new)
        {
            // don't kill it a second time!
            L("I already knew this");
            continue;
        }

        // REM: do not do intelligent killing of other runners. no worries, their due dates will fail soon too ;)
        kill_task(t);

        // reschedule directly if possible
        if (idle_runners.size() > 0)
        {
            L(
                "Rescheduling after due date violation detected by detector_rank %d",
                detector_rank);
            // REM: schedule only once as there is only on more task after the kill it. later we might want to schedule multiple times if we clear runners that are still scheduled or running on the broken runner id...
            schedule_new_task(idle_runners.begin()->first);
        }
    }
}

void handle_data_response() 
{
    Extrae_user_function(1);
    // TODO: move to send and receive function as on api side... maybe use zproto library?
    zmq_msg_t identity_msg, empty_msg, header_msg, data_msg;
    zmq_msg_init(&identity_msg);
    zmq_msg_init(&empty_msg);
    zmq_msg_init(&header_msg);
    zmq_msg_init(&data_msg);

    zmq_msg_recv(&identity_msg, data_response_socket, 0);

    assert_more_zmq_messages(data_response_socket);
    zmq_msg_recv(&empty_msg, data_response_socket, 0);
    assert_more_zmq_messages(data_response_socket);
    zmq_msg_recv(&header_msg, data_response_socket, 0);
    assert_more_zmq_messages(data_response_socket);
    zmq_msg_recv(&data_msg, data_response_socket, 0);

    assert(IDENTITY_SIZE == 0 || IDENTITY_SIZE == zmq_msg_size(
               &identity_msg));

    IDENTITY_SIZE = zmq_msg_size(&identity_msg);

    void * identity = malloc(IDENTITY_SIZE);
    memcpy(identity, zmq_msg_data(&identity_msg), zmq_msg_size(
               &identity_msg));

    assert(zmq_msg_size(&header_msg) == 4 * sizeof(int) +
           MPI_MAX_PROCESSOR_NAME * sizeof(char));
    int * header_buf = reinterpret_cast<int*>(zmq_msg_data(&header_msg));
    int runner_id = header_buf[0];
    int runner_rank = header_buf[1];
    int runner_state_id = header_buf[2];      // = ensemble_member_id;
    int runner_timestamp = header_buf[3];
    char field_name[MPI_MAX_PROCESSOR_NAME];
    strcpy(field_name, reinterpret_cast<char*>(&header_buf[4]));

    // good runner_rank, good state id?
    auto running_sub_task = std::find_if(running_sub_tasks.begin(),
                                         running_sub_tasks.end(),
                                         [runner_id, runner_rank,
                                          runner_state_id](
                                             std::shared_ptr<SubTask> &
                                             st){
        return st->runner_id == runner_id && st->state_id ==
        runner_state_id && st->runner_rank == runner_rank;
    });

    RunnerRankConnection csr(identity);
    auto found = std::find_if(killed.begin(), killed.end(), [runner_id] (
                                  Task p) {
        return p.runner_id == runner_id;
    });
    if (found != killed.end())
    {
        L(
            "Ending Model Task Runner killed by timeout violation runner=%d",
            runner_id);
        csr.end(KILL_RUNNER);
    }
    else
    {
        assert(runner_timestamp == 0 || running_sub_task !=
               running_sub_tasks.end());

        // This is necessary if a task was finished on rank 0. then it crashes on another rank. so rank 0 needs to undo this!
        if (running_sub_task != running_sub_tasks.end())
        {
            // only if we are not in timestamp  0:
            finished_sub_tasks.push_back(*running_sub_task);

            running_sub_tasks.remove(*running_sub_task);
        }

        // good timestamp? There are 2 cases: timestamp 0 or good timestamp...
        assert (runner_timestamp == 0 || runner_timestamp ==
                current_step);
        // we always throw away timestamp 0 as we want to init the simulation! (TODO! we could also use it as ensemble member...)


        // Save state part in background_states.
        assert(field->name == field_name);
        Part & part = field->getPart(runner_rank);
        assert(zmq_msg_size(&data_msg) == part.send_count *
               sizeof(double));
        D(
            "<- Server received %lu/%lu bytes of %s from runner id %d, runner rank %d, state id %d, timestamp=%d",
            zmq_msg_size(&data_msg), part.send_count *
            sizeof(double),
            field_name, runner_id, runner_rank, runner_state_id,
            runner_timestamp);
        D("local server offset %lu, sendcount=%lu",
          part.local_offset_server, part.send_count);


        D("values[0] = %.3f", reinterpret_cast<double*>(zmq_msg_data(
                                                            &
                                                            data_msg))
          [0]);
        if (runner_timestamp == current_step)
        {
            D("storing this timestamp!...");
            // zero copy is unfortunately for send only. so copy internally...
            field->ensemble_members[runner_state_id].
            store_background_state_part(part,reinterpret_cast<double*>(zmq_msg_data(&data_msg)), runner_state_id);
            // checkpoint to disk 
            FT.store_subset( field, runner_state_id, runner_rank );
        }

        // Check if we can answer directly with new data... means starting of a new model task
        auto found = std::find_if(scheduled_sub_tasks.begin(),
                                  scheduled_sub_tasks.end(), [runner_id,
                                                              runner_rank
                                  ](
                                      std::shared_ptr<SubTask> &st){
            return st->runner_id == runner_id && st->runner_rank ==
            runner_rank;
        });

        if (found != scheduled_sub_tasks.end())
        {
            // found a new task. send back directly!

            D(
                "send after receive! to runner rank %d on runner_id %d",
                runner_rank, runner_id);
            csr.launch_sub_task(runner_rank, (*found)->state_id);
            // don't need to delete from connected runner as we were never in there...  TODO: do this somehow else. probably not csr.send but another function taking csr as parameter...
            running_sub_tasks.push_back(*found);
            scheduled_sub_tasks.remove(*found);

        }
        else
        {
            // Save connection - basically copy identity pointer...
            auto &runner = idle_runners.emplace(runner_id,
                                                std::shared_ptr<
                                                    Runner>(
                                                    new Runner()))
                           .first->second;
            runner->connected_runner_ranks.emplace(runner_rank,
                                                   csr);
            D("save connection runner_id %d, runner rank %d",
              runner_id, runner_rank);

            if (comm_rank == 0)
            {
                
                if (idle_runners[runner_id]->
                    connected_runner_ranks.size() ==
                    field->connected_runner_ranks.size())
                {
                    schedule_new_task(runner_id);
                }
            
            }
        }
    }

    Extrae_user_function(0);
    zmq_msg_close(&identity_msg);
    zmq_msg_close(&empty_msg);
    zmq_msg_close(&header_msg);
    zmq_msg_close(&data_msg);
}

// returns true if the whole assimilation (all time steps) finished
bool check_finished(std::shared_ptr<Assimilator> assimilator) 
{
    // check if all data was received. If yes: start Update step to calculate next analysis state

    size_t connections =
        field->connected_runner_ranks.size();

    bool finished;
    if (comm_rank == 0)
    {
        // try to know if somebody else finished
        for (int rank = 1; rank < comm_size; rank++)
        {
            int received;

            MPI_Iprobe(rank, TAG_RANK_FINISHED, FTI_COMM_DUP,
                       &received, MPI_STATUS_IGNORE);
            if (received)
            {
                L("Somebody finished... ");
                int highest_task_id;
                MPI_Recv(&highest_task_id, 1, MPI_INT, rank,
                         TAG_RANK_FINISHED, FTI_COMM_DUP,
                         MPI_STATUS_IGNORE);
                if (highest_task_id == task_id)
                {
                    // this message was up to date....
                    finished_ranks++;
                }
            }

        }

        finished = finished_ranks == comm_size-1 &&           // comm size without rank 0
                   unscheduled_tasks.size() == 0 &&
                   scheduled_sub_tasks.size() == 0 &&
                   running_sub_tasks.size() ==
                   0 &&
                   finished_sub_tasks.size() == ENSEMBLE_SIZE *
                   connections;
        // L("rank 0: D %d ", finished_ranks);

        if (finished)
        {
            // tell everybody that everybody finished!
            for (int rank = 1; rank < comm_size; rank++)
            {
                L("Sending tag all finished message");
                MPI_Send(nullptr, 0, MPI_BYTE, rank,
                         TAG_ALL_FINISHED, FTI_COMM_DUP);
            }
        }
    }
    else
    {
        // rank != 0:
        finished = false;

        // REM: we need to synchronize when we finish as otherwise processes are waiting in the mpi barrier to do the update step while still some ensemble members need to be repeated due to failing runner.

        // would this rank finish?
        bool would_finish = unscheduled_tasks.size() == 0 &&
                            scheduled_sub_tasks.size() == 0 &&
                            running_sub_tasks.size() ==
                            0 &&
                            finished_sub_tasks.size() == ENSEMBLE_SIZE *
                            connections;

        if (would_finish)
        {
            if (highest_sent_task_id < highest_received_task_id)
            {
                // only send once when we finished a new task that was received later.  REM: using Bsend as faster as buffere, buffering not necessary here...

                MPI_Send(&highest_received_task_id, 1, MPI_INT,
                         0, TAG_RANK_FINISHED, FTI_COMM_DUP);
                highest_sent_task_id = highest_received_task_id;                  // do not send again..
            }

            // now wait for rank 0 to tell us that we can finish.
            int received;
            MPI_Iprobe(0, TAG_ALL_FINISHED, FTI_COMM_DUP,
                       &received, MPI_STATUS_IGNORE);
            if (received)
            {
                L("receiving tag all finished message");
                MPI_Recv(nullptr, 0, MPI_BYTE, 0,
                         TAG_ALL_FINISHED, FTI_COMM_DUP,
                         MPI_STATUS_IGNORE);
                finished = true;
            }
        }

    }

    if (finished)
    {
        FT.flushCP();
        FT.recover();
        //FT.finalizeCP();

        std::cout << " -- 0 -- fti_dbg cur_step: " << current_step << "current_nsteps: " << current_nsteps << std::endl;
        current_nsteps = assimilator->do_update_step();
        std::cout << " -- 1 -- fti_dbg cur_step: " << current_step << "current_nsteps: " << current_nsteps << std::endl;

        FT.finalizeCP();

        if (current_nsteps == -1 || current_step >= TOTAL_STEPS)
        {
            end_all_runners();
            return true;
        }

        // TODO create fault tolerance class and outsource all FTI related stuff there.

        //int i=0;
        //FTI_Protect( i++, &current_step, sizeof(current_step), FTI_CHAR);
        //FTI_Protect( i++, &current_nsteps, sizeof(current_nsteps), FTI_CHAR);
        //for(auto it = field->ensemble_members.begin(); it != field->ensemble_members.end(); it++) {
        //    auto ptr = it->state_analysis.data();
        //    auto size = sizeof(it->state_analysis[0])*it->state_analysis.size();
        //    FTI_Protect( i++, ptr, size, FTI_CHAR);
        //}
        //if( isRecovery ) {
        //    FTI_Recover();
        //    isRecovery = 0;
        //} else {
        //    FTI_Checkpoint( current_step, 4 );
        //    //if( current_step == 5 ) *(int*)NULL = 0;
        //}

        init_new_timestamp(); 

        // After update step: rank 0 loops over all runner_id's sending them a new state vector part they have to propagate.
        if (comm_rank == 0)
        {
            for (auto runner_it = idle_runners.begin(); runner_it !=
                 idle_runners.end(); runner_it++)
            {
                L("Rescheduling after update step");
                schedule_new_task(runner_it->first);
            }
        }
        
        FT.initCP( current_step );
    
    }

    return false;
}

/// optional parameters [MAX_TIMESTAMP [ENSEMBLESIZE]]
int main(int argc, char * argv[])
{

    check_data_types();

    // Read in configuration from command line
    if (argc >= 2)
    {
        TOTAL_STEPS = atoi(argv[1]);
    }
    if (argc >= 3)
    {
        ENSEMBLE_SIZE = atoi(argv[2]);
    }
    if (argc >= 4)
    {
        ASSIMILATOR_TYPE = static_cast<AssimilatorType>(atoi(argv[3]));
    }


    assert(TOTAL_STEPS > 1);
    assert(ENSEMBLE_SIZE > 0);
    
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    if( provided < MPI_THREAD_MULTIPLE ) {
        D( "thread level is not provided!" );
        MPI_Abort( MPI_COMM_WORLD, -1 );
    }
    FT.init( MPI_COMM_WORLD, &current_step ); 
        
    isRecovery = FTI_Status();

    MPI_Comm_size(FTI_COMM_DUP, &comm_size);
    MPI_Comm_rank(FTI_COMM_DUP, &comm_rank);

    // will be inited later when we know the field dimensions.
    std::shared_ptr<Assimilator> assimilator;      
    
    context = zmq_ctx_new ();
    int major, minor, patch;
    zmq_version (&major, &minor, &patch);
    
    D("Current 0MQ version is %d.%d.%d", major, minor, patch);
    D("**server rank = %d", comm_rank);
    L("Start server for %d timesteps with %d ensemble members", TOTAL_STEPS,
      ENSEMBLE_SIZE);

    char hostname[MPI_MAX_PROCESSOR_NAME];
    melissa_get_node_name(hostname, MPI_MAX_PROCESSOR_NAME);
    
    // Start sockets:
    void * configuration_socket = NULL;
    if (comm_rank == 0)
    {
        configuration_socket = zmq_socket(context, ZMQ_REP);
        int val = 20000;
        zmq_setsockopt( configuration_socket, ZMQ_RCVTIMEO, &val, sizeof(int) );
        zmq_setsockopt( configuration_socket, ZMQ_SNDTIMEO, &val, sizeof(int) );
        char configuration_socket_addr[MPI_MAX_PROCESSOR_NAME+256];
        snprintf( configuration_socket_addr, MPI_MAX_PROCESSOR_NAME+256, "tcp://%s:4000", hostname );
        
        std::fstream file;
        file.open("configserver.txt",std::ios::out);

        // to be put into environment variable MELISSA_SERVER_MASTER_NODE on simulation start
        int rc = zmq_bind(configuration_socket, configuration_socket_addr);
        L("Configuration socket listening on port %s", configuration_socket_addr);
        ZMQ_CHECK(rc);

        file << configuration_socket_addr;

        file.close();
        assert(rc == 0);

    }

    data_response_socket = zmq_socket(context, ZMQ_ROUTER);
    int val = 20000;
    zmq_setsockopt( data_response_socket, ZMQ_RCVTIMEO, &val, sizeof(int) );
    zmq_setsockopt( data_response_socket, ZMQ_SNDTIMEO, &val, sizeof(int) );
    char data_response_port_name[MPI_MAX_PROCESSOR_NAME];
    sprintf(data_response_port_name, "tcp://*:%d", 5000+comm_rank);
    zmq_bind(data_response_socket, data_response_port_name);

    sprintf(data_response_port_name, "tcp://%s:%d", hostname, 5000+
            comm_rank);

    char data_response_port_names[MPI_MAX_PROCESSOR_NAME * comm_size];
    MPI_Gather(data_response_port_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
               data_response_port_names, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
               0, FTI_COMM_DUP);

    // Server main loop:
        
    
    while (true)
    {
#ifdef NDEBUG
        // usleep(1);
#else
        usleep(10);         // to chill down the processor! TODO remove when measuring!
#endif
        // Wait for requests
        /* Poll for events indefinitely */
        // REM: the poll item needs to be recreated all the time!
        zmq_pollitem_t items [2] = {
            {data_response_socket, 0, ZMQ_POLLIN, 0},
            {configuration_socket, 0, ZMQ_POLLIN, 0}
        };
        if (comm_rank == 0)
        {
            /*
             * Check for killed runners... (due date violation)
             * poll the fastest possible to be not in concurrence 
             * with the mpi probe calls... (theoretically we could 
             * set this time to -1 if using only one core for the server.)
             */
            ZMQ_CHECK(zmq_poll (items, 2, 0));
        }
        else
        {
            /*
             * ZMQ_CHECK(zmq_poll (items, 1, -1));
             * Check for new tasks to schedule and killed runners 
             * so do not block on polling (due date violation)...
             */
            ZMQ_CHECK(zmq_poll (items, 1, 0));
        }
        /* Returned events will be stored in items[].revents */

        // answer requests
        if (comm_rank == 0 && (items[1].revents & ZMQ_POLLIN))
        {
            answer_configuration_message(configuration_socket,data_response_port_names);
        }

        // coming from fresh init...
        if (phase == PHASE_INIT)
        {
            /*
             * first expression assures that the configuration message
             * has been received.
             */
            if ((comm_rank == 0 && field != nullptr) || comm_rank != 0)
            {
                
                broadcast_field_information_and_calculate_parts();
                
                init_new_timestamp();
                    
                // init assimilator as we know the field size now.
                assimilator = Assimilator::create(ASSIMILATOR_TYPE, *field);
                current_nsteps = assimilator->getNSteps();
                
                FT.protect_background( field );
                
                D("Change Phase");
                phase = PHASE_SIMULATION;
                
            }
        }

        if (phase == PHASE_SIMULATION)
        {

            check_due_dates();

            if (comm_rank == 0)
            {
                check_kill_requests();
            }

            if (comm_rank != 0)
            {
                check_schedule_new_tasks();
            }
        
            if (items[0].revents & ZMQ_POLLIN)
            {
                handle_data_response();
            }

            if (check_finished(assimilator))
            {
                break;                  // all runners finished.
            }


            size_t connections =
                field->connected_runner_ranks.size();
            
            assert(
                unscheduled_tasks.size() * connections +                          // scale on amount of subtasks.
                scheduled_sub_tasks.size() +
                running_sub_tasks.size() +
                finished_sub_tasks.size() == connections *
                ENSEMBLE_SIZE);
        }

    }

    D("Ending Server.");
    
    zmq_close(data_response_socket);
    
    if (comm_rank == 0)
    {
        zmq_close(configuration_socket);
    }
    
    zmq_ctx_destroy(context);
    
    FTI_Finalize();
    MPI_Finalize();

}
