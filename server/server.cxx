// TODOS:
// TODO 1. refactoring
//   TODO: use zmq cpp?? --> no
// TODO 2. error prone ness
		// TODO: check ret values!
// TODO 3. check with real world sim and DA.
//
// TODO: there are some creepy race condition bugs in there. best would be to always copy states into the good lists. in case of crash: remove from list.
// TODO:
// Flow: if simulation requests work see if work in list. if so launch it. if not save connection
//       if scheduling message from rank 0: run message
//       if message kill message from rank 0 : move state back into states to do. blacklist simuid, remove state-simu pair from running/finished lists. remove simu id from scheduled list.
//       check for due dates. if detected: black list simulation. send state AND simulatoin to rank0
//
//       rank 0:
//       if simulatoin request work see if work in list. if so: launch it. if not check if we have more work to do and schedule it on this simulation. send this to all clients. best would be blocking to be sure that it arrives and we do no reschedule before. maybe ISend
//       at the same time check if some client reports a crash (verschachteltes Isend)
//       check for due dates. if detected: black list simulation. and do the same as if I had a kill message from rank 0:
//       if message kill message from rank 0 : move state back into states to do. blacklist simuid, remove state-simu pair from running/finished lists. remove simu id from scheduled list. and reschedule state.

#include <map>
#include <string>
#include <cstdlib>
#include <cassert>

#include <vector>
#include <set>
#include <list>
#include <memory>
#include <algorithm>

#include <mpi.h>
#include "zmq.h"

#include "../common/messages.h"
#include "../common/n_to_m.h"
#include "../common/utils.h"

#include <time.h>

// TODO: configure this somehow better:
const int ENSEMBLE_SIZE = 42;
const int FIELDS_COUNT = 1;  // multiple fields is stupid!
//const long long MAX_SIMULATION_TIMEOUT = 600;  // 10 min max timeout for simulations.
const long long MAX_SIMULATION_TIMEOUT = 5;  // 10 min max timeout for simulations.
const int MAX_TIMESTAMP = 5000;
//const int MAX_TIMESTAMP = 155;

using namespace std;

const int TAG_NEW_TASK = 42;
const int TAG_KILL_SIMULATION = 43;
const int TAG_RANK_FINISHED = 44;
const int TAG_ALL_FINISHED = 45;

int highest_received_task_id = 0;

// only important on ranks != 0:
int highest_sent_task_id = 0;

size_t IDENTITY_SIZE = 0;

// Server comm_size and Server rank
int comm_size;

void * context;
void * data_response_socket;

int current_timestamp = 0;  // will effectively start at 1.

long long get_due_date() {
	time_t seconds;
	seconds = time (NULL);
	return seconds + MAX_SIMULATION_TIMEOUT;
}

struct Part
{
	int rank_simu;
	size_t local_vector_offset;
	size_t send_count;
};

struct EnsembleMember
{
	vector<double> state_analysis;
	vector<double> state_background;

	void set_local_vect_size(int local_vect_size)
	{
		state_analysis.reserve(local_vect_size);
		state_analysis.resize(local_vect_size);
		state_background.reserve(local_vect_size);
		state_background.resize(local_vect_size);
	}

	void store_background_state_part(const n_to_m & part, const double * values)
	{
		D("before_assert %lu %lu %lu", part.send_count, part.local_offset_server, state_background.size());
		assert(part.send_count + part.local_offset_server <= state_background.size());
		copy(values, values + part.send_count, state_background.data() + part.local_offset_server);
	}
};


set<int> killed_simulations;  // when a simulation from this list connects we thus respond with a kill message. if one rank receives a kill message it has to call exit so all simulation ranks are quit.

struct Field {
	// index: state id.
	vector<EnsembleMember> ensemble_members;

	size_t local_vect_size;
	vector<size_t> local_vect_sizes_simu;
	vector<n_to_m> parts;

	set<int> connected_simulation_ranks;

	Field(int simu_comm_size_, size_t ensemble_size_)
	{
		local_vect_size = 0;
		local_vect_sizes_simu.resize(simu_comm_size_);
		ensemble_members.resize(ensemble_size_);
	}

	/// Calculates all the state vector parts that are send between the server and the
	/// simulations
	void calculate_parts(int server_comm_size)
	{
		parts = calculate_n_to_m(server_comm_size, local_vect_sizes_simu);
		for (auto part_it = parts.begin(); part_it != parts.end(); part_it++)
		{
			if (part_it->rank_server == comm_rank)
			{
				local_vect_size += part_it->send_count;
				connected_simulation_ranks.emplace(part_it->rank_simu);
			}
		}

		for (auto ens_it = ensemble_members.begin(); ens_it != ensemble_members.end(); ens_it++)
		{

			ens_it->set_local_vect_size(local_vect_size);  // low: better naming: local state size is in doubles not in bytes!
		}
		D("Calculated parts");
	}

	/// Finds the part of the field with the specified simu_rank.
	n_to_m & getPart(int simu_rank)
	{
		assert(parts.size() > 0);
		for (auto part_it = parts.begin(); part_it != parts.end(); part_it++)
		{
			if (part_it->rank_server == comm_rank && part_it->rank_simu == simu_rank) {
				return *part_it;
			}
		}
		assert(false); // Did not find the part!
	}
};

map<string, Field*> fields;

void my_free(void * data, void * hint)
{
	free(data);
}

struct SimulationRankConnection {
	void * connection_identity;

	SimulationRankConnection(void * identity) {
		connection_identity = identity;
	}

	void send_task(int simu_rank, int state_id) {
		// get state and send it to the simu rank...
		assert(connection_identity != NULL);

		zmq_msg_t identity_msg;
		zmq_msg_t empty_msg;
		zmq_msg_t header_msg;
		zmq_msg_t data_msg;

		zmq_msg_init_data(&identity_msg, connection_identity, IDENTITY_SIZE, my_free, NULL);
		zmq_msg_send(&identity_msg, data_response_socket, ZMQ_SNDMORE);

		zmq_msg_init(&empty_msg);
		zmq_msg_send(&empty_msg, data_response_socket, ZMQ_SNDMORE);

		ZMQ_CHECK(zmq_msg_init_size(&header_msg, 3 * sizeof(int)));
		int * header = reinterpret_cast<int*>(zmq_msg_data(&header_msg));
		header[0] = state_id;
		header[1] = current_timestamp;
		header[2] = CHANGE_STATE;
		zmq_msg_send(&header_msg, data_response_socket, ZMQ_SNDMORE);
		// we do not know when it will really send. send is non blocking!

		zmq_msg_init_data(&data_msg,
				(fields.begin()->second->ensemble_members.at(state_id).state_analysis.data() + fields.begin()->second->getPart(simu_rank).local_offset_server),
				fields.begin()->second->getPart(simu_rank).send_count * sizeof(double), NULL, NULL);

		D("-> Server sending %lu bytes for state %d, timestamp=%d",
				fields.begin()->second->getPart(simu_rank).send_count * sizeof(double),
				header[0], header[1]);
		D("local server offset %lu, local simu offset %lu, sendcount=%lu", fields.begin()->second->getPart(simu_rank).local_offset_server, fields.begin()->second->getPart(simu_rank).local_offset_simu, fields.begin()->second->getPart(simu_rank).send_count);
		print_vector(fields.begin()->second->ensemble_members.at(state_id).state_analysis);

		zmq_msg_send(&data_msg, data_response_socket, 0);

		// close connection:
		// but do not free it. send is going to free it.
		connection_identity = NULL;
	}

	void end(int end_flag=END_SIMULATION) {
		// some connection_identities will be 0 if some simulation ranks are connected to another server rank at the moment.
		if (connection_identity == NULL)
			return;

		zmq_msg_t identity_msg;
		zmq_msg_t empty_msg;
		zmq_msg_t header_msg;

		zmq_msg_init_data(&identity_msg, connection_identity, IDENTITY_SIZE, my_free, NULL);
		zmq_msg_send(&identity_msg, data_response_socket, ZMQ_SNDMORE);

		zmq_msg_init(&empty_msg);
		zmq_msg_send(&empty_msg, data_response_socket, ZMQ_SNDMORE);

		zmq_msg_init_size(&header_msg, 3 * sizeof(int));

		int * header = reinterpret_cast<int*>(zmq_msg_data(&header_msg));
		header[0] = -1;
		header[1] = current_timestamp;
		header[2] = end_flag;

		zmq_msg_send(&header_msg, data_response_socket, 0);

		D("Send end message");

		// but don't delete it. this is done in the message.
		connection_identity = NULL;
	}
};


struct Simulation  // Model process runner
{
	// simulations rank
	map<int, SimulationRankConnection> connected_simulation_ranks;

	void end(int end_flag) {
		for (auto cs = connected_simulation_ranks.begin(); cs != connected_simulation_ranks.end(); cs++) {
			D("xxx end connected simulation rank...");
			cs->second.end(end_flag);
		}
	}

	~Simulation() {
		// try to kill remaining simulations if there are still some.
		end(KILL_SIMULATION);
	}

};

// simu_id, Simulation:
map<int, shared_ptr<Simulation>> idle_simulations;

set<int> unscheduled_tasks;

// only important on rank 0:
// if == comm_size we start the update step. is reseted when a simulation is rescheduled!
int finished_ranks = -1;

struct NewTask {
	int simu_id;
	int state_id;
	int due_date;
	int task_id;
};


/// used to transmit new tasks to clients.
struct SubTask {
	int simu_id;
	int simu_rank;
	int state_id;
	int due_date;
		// TODO: set different due dates so not all ranks communicate at the same time to the server when it gets bypassed ;)
	SubTask(NewTask &new_task, int simu_rank_) {
		simu_id = new_task.simu_id;
		simu_rank = simu_rank_;
		state_id = new_task.state_id;
		due_date = new_task.due_date;
	}
};

// REM: works as fifo!
// fifo with tasks that are running, running already on some simulation ranks or not
// these are checked for the due dates!
// if we get results for the whole task we remove it from the scheduled_tasks list.
typedef list<shared_ptr<SubTask>> SubTaskList;
SubTaskList scheduled_sub_tasks;  // TODO could be ordered sets! this prevents us from adding 2 the same!
SubTaskList running_sub_tasks;
SubTaskList finished_sub_tasks;  // TODO: why do we need to store this? actually not needed.


void answer_configuration_message(void * configuration_socket, char* data_response_port_names)
{
	zmq_msg_t msg;
	zmq_msg_init(&msg);
	zmq_msg_recv(&msg, configuration_socket, 0);
	int * buf = reinterpret_cast<int*>(zmq_msg_data(&msg));
	if (buf[0] == REGISTER_SIMU_ID) {
		// Register Simu ID
		assert(zmq_msg_size(&msg) == 2 * sizeof(int));
		assert(buf[1] >= 0);  // don't allow negative simu_ids
		// we add the simulation if we first receive data from it.
		zmq_msg_close(&msg);
		D("Server registering Simu ID %d", buf[1]);

		zmq_msg_t msg_reply1, msg_reply2;
		zmq_msg_init_size(&msg_reply1, sizeof(int));
		buf = reinterpret_cast<int*>(zmq_msg_data(&msg_reply1));
		*buf = comm_size;
		zmq_msg_send(&msg_reply1, configuration_socket, ZMQ_SNDMORE);

		zmq_msg_init_data(&msg_reply2, data_response_port_names,
				comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(char), NULL, NULL);
		zmq_msg_send(&msg_reply2, configuration_socket, 0);
	}
	else if (buf[0] == REGISTER_FIELD)
	{
		assert(phase == PHASE_INIT);  // we accept new fields only if in initialization phase.
		assert(zmq_msg_size(&msg) == sizeof(int) + sizeof(int) + MPI_MAX_PROCESSOR_NAME * sizeof(char));
		assert(fields.size() == 0);  // we accept only one field for now.

		int simu_comm_size = buf[1];

		Field *newField = new Field(simu_comm_size, ENSEMBLE_SIZE);

		char field_name[MPI_MAX_PROCESSOR_NAME];
		strcpy(field_name, reinterpret_cast<char*>(&buf[2]));
		zmq_msg_close(&msg);
		D("Server registering Field %s, simu_comm_size = %d", field_name, simu_comm_size);

		assert_more_zmq_messages(configuration_socket);
    zmq_msg_init(&msg);
		zmq_msg_recv(&msg, configuration_socket, 0);
		assert(zmq_msg_size(&msg) == simu_comm_size * sizeof(size_t));
		memcpy (newField->local_vect_sizes_simu.data(), zmq_msg_data(&msg), simu_comm_size * sizeof(size_t));
		zmq_msg_close(&msg);

		// ack
		zmq_msg_t msg_reply;
		zmq_msg_init(&msg_reply);
		zmq_msg_send(&msg_reply, configuration_socket, 0);

		fields.emplace(field_name, newField);
	}
	else
	{
		// Bad message type
		assert(false);
		exit(1);
	}
}

void broadcast_field_information_and_calculate_parts() {
	size_t field_count = fields.size();
	MPI_Bcast(&field_count, 1, my_MPI_SIZE_T, 0, MPI_COMM_WORLD);                          // 0:field_count
	auto field_it = fields.begin();
	for (size_t i = 0; i < field_count; i++)
	{
		char field_name[MPI_MAX_PROCESSOR_NAME];
		if (comm_rank == 0) {
			strcpy(field_name, field_it->first.c_str());
			MPI_Bcast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);  // 1:fieldname
			int simu_comm_size = field_it->second->local_vect_sizes_simu.size();
			MPI_Bcast(&simu_comm_size, 1, my_MPI_SIZE_T, 0, MPI_COMM_WORLD);                   // 2:simu_comm_size

			D("local_vect_sizes");
			print_vector(field_it->second->local_vect_sizes_simu);

			MPI_Bcast(field_it->second->local_vect_sizes_simu.data(), simu_comm_size,
					my_MPI_SIZE_T, 0, MPI_COMM_WORLD);                                             // 3:local_vect_sizes_simu

			field_it->second->calculate_parts(comm_size);

			field_it++;
		} else {
			int simu_comm_size;
			MPI_Bcast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);  // 1: fieldname
			MPI_Bcast(&simu_comm_size, 1, my_MPI_SIZE_T, 0, MPI_COMM_WORLD);                   // 2:simu_comm_size

			Field * newField = new Field(simu_comm_size, ENSEMBLE_SIZE);

			D("local_vect_sizes");
			print_vector(newField->local_vect_sizes_simu);
			MPI_Bcast(newField->local_vect_sizes_simu.data(), simu_comm_size,
					my_MPI_SIZE_T, 0, MPI_COMM_WORLD);                                              // 3:local_vect_sizes_simu

			newField->calculate_parts(comm_size);

			fields.emplace(field_name, newField);
		}
	}
}

/// returns true if could send the sub_task on a connection.
bool try_launch_subtask(shared_ptr<SubTask> &sub_task) {
	// tries to send this task.
	auto found_simulation = idle_simulations.find(sub_task->simu_id);
	if (found_simulation == idle_simulations.end()) {
		D("could not send: Did not find idle simulation");
		return false;
	}

	auto found_rank = found_simulation->second->connected_simulation_ranks.find(sub_task->simu_rank);
	if (found_rank == found_simulation->second->connected_simulation_ranks.end()) {
		D("could not send: Did not find rank");
		return false;
	}

	D("Send after adding subtask! to simu_id %d", sub_task->simu_id);
	found_rank->second.send_task(sub_task->simu_rank, sub_task->state_id);
	found_simulation->second->connected_simulation_ranks.erase(found_rank);
	if (found_simulation->second->connected_simulation_ranks.empty()) {
		idle_simulations.erase(found_simulation);
	}

	return true;
}

//void unschedule(int simu_id) {
//	idle_simulations.erase(simu_id);
//	killed_simulations.insert(simu_id);
//	auto f = [simu_id](shared_ptr<SubTask> &task) {
//		if (task->simu_id == simu_id) {
//			unscheduled_tasks.insert(task->state_id);
//			L("Remove by simu_id: from simuid %d rank %d task with state %d, duedate=%d", task->simu_id, task->simu_rank, task->state_id, task->due_date);
//			return true;
//		} else {
//			return false;
//		}
//	};
//	// REM: do not use algorithms remove_if as it does not work on lists.
//	scheduled_sub_tasks.remove_if(f);
//	running_sub_tasks.remove_if(f);
//	finished_sub_tasks.remove_if(f);
//}

void kill_it(int state_id, int simu_id) {
	L("killing state %d simu %d", state_id, simu_id);
	unscheduled_tasks.insert(state_id);
	idle_simulations.erase(simu_id);  // low: one could do this only the first time we find a subtask... or call unschedule after finding the according simu_id
	killed_simulations.insert(simu_id);
	auto f = [state_id, simu_id](shared_ptr<SubTask> &task) {
		return task->state_id == state_id && task->simu_id == simu_id;
	};
	running_sub_tasks.remove_if(f);
	finished_sub_tasks.remove_if(f);

	// there might be more states scheduled to this simulation!
	scheduled_sub_tasks.remove_if([simu_id](shared_ptr<SubTask> &task) {
		if (task->simu_id == simu_id) {
			unscheduled_tasks.emplace(task->state_id);
			return true;
		} else {
			return false;
		}
	});
}

// either add subtasts to list of scheduled subtasks or runs them directly adding them to running sub tasks.
void add_sub_tasks(NewTask &new_task) {
	unscheduled_tasks.erase(new_task.state_id);
	auto &csr = fields.begin()->second->connected_simulation_ranks;
	assert(csr.size() > 0);  // connectd simulation ranks must be initialized...
	for (auto it = csr.begin(); it != csr.end(); it++){
		shared_ptr<SubTask> sub_task (new SubTask(new_task, *it));
		L("Adding subtask for simu rank %d", *it);

		if (try_launch_subtask(sub_task)) {
			running_sub_tasks.push_back(sub_task);
		} else {
			scheduled_sub_tasks.push_back(sub_task);
		}
	}
}

/// schedules a new task on a model task runner and tries to run it.
static int task_id = 1; //low: aftrer each update step one could reset the task id and also the highest sent task id and so on to never get overflows!
bool schedule_new_task(int simu_id)
{
	assert(comm_rank == 0);
	if (unscheduled_tasks.size() > 0) {
		task_id++;
		int state_id = *(unscheduled_tasks.begin());

		int due_date = get_due_date();

		NewTask new_task({simu_id, state_id, due_date, task_id});

		L("Schedule task with task id %d", task_id);

		finished_ranks = 0;

		add_sub_tasks(new_task);


		// Send new scheduled task to all server ranks! This makes sure that everybody receives it!
		for (int receiving_rank = 1; receiving_rank < comm_size; receiving_rank++)
		{
			// TODO: one should use ISend and then wait for all to complete
			// REM: MPI_Ssend to be sure that all messages are received!
			//MPI_Ssend(&new_task, sizeof(NewTask), MPI_BYTE, receiving_rank, TAG_NEW_TASK, MPI_COMM_WORLD);
			MPI_Bsend(&new_task, sizeof(NewTask), MPI_BYTE, receiving_rank, TAG_NEW_TASK, MPI_COMM_WORLD);
		}

		return true;
	}
	else
	{
		return false;
	}
}

/// checks if the server added new tasks... if so tries to run them.
void check_schedule_new_tasks()
{
	assert(comm_rank != 0);
	int received;

	MPI_Iprobe(0, TAG_NEW_TASK, MPI_COMM_WORLD, &received, MPI_STATUS_IGNORE);
	if (received)
	{
		D("Got task to send...");

		// we are not finished anymore so resend if we are finished:

		NewTask new_task;
		MPI_Recv(&new_task, sizeof(new_task), MPI_BYTE, 0, TAG_NEW_TASK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		// TODO: remove from old simu if it put's state on a new simu id

		highest_received_task_id = max(new_task.task_id, highest_received_task_id);

		add_sub_tasks(new_task);
	}
}


// low: use uuid instead of simuid? but this makes possibly more network traffic...
// low: maybe have a list of killed simulations. if they reconnect send end message directly! (only works with uuid...)
void end_all_simulations()
{
	for (auto simu_it = idle_simulations.begin(); simu_it != idle_simulations.end(); simu_it++)
	{
		simu_it->second->end(END_SIMULATION);
	}
}

void init_new_timestamp()
{
	// TODO: assert on finished_sub_tasks
	assert(scheduled_sub_tasks.size() == 0);
	assert(running_sub_tasks.size() == 0);

	finished_sub_tasks.clear();
	highest_received_task_id = 0;
	highest_sent_task_id = 0;
	task_id = 1;

	current_timestamp++;

	assert(unscheduled_tasks.size() == 0);

	for (int i = 0; i < ENSEMBLE_SIZE; i++) {
		unscheduled_tasks.insert(i);
	}
}

void do_update_step()
{
	// TODO
	L("Doing update step...\n");
	MPI_Barrier(MPI_COMM_WORLD);
	for (auto field_it = fields.begin(); field_it != fields.end(); field_it++)
	{
		int state_id = 0;
		for (auto ens_it = field_it->second->ensemble_members.begin(); ens_it != field_it->second->ensemble_members.end(); ens_it++)
		{
			assert(ens_it->state_analysis.size() == ens_it->state_background.size());
			for (size_t i = 0; i < ens_it->state_analysis.size(); i++) {
				// pretend to do some da...
				ens_it->state_analysis[i] = ens_it->state_background[i] + state_id;
			}
			state_id ++;
		}
	}
}

void try_kickoff_subtasks() {
	// TODO: use this method also in kill_it as it has to be done after each kill!!
	scheduled_sub_tasks.remove_if([&] (shared_ptr<SubTask> &st) {
		if (try_launch_subtask(st)) {
			L("kicking off subtask e.g. after kill");
			running_sub_tasks.push_back(st);
			return true;
		} else {
			return false;
		}
	});
}

void check_due_dates() {
	time_t now;
	now = time (NULL) - comm_rank;  // Trick so not all finish at the same time.. TODO: does not work.

	// state id, simu_id
	set<pair<int,int>> to_kill;

	for (auto it = scheduled_sub_tasks.begin(); it != scheduled_sub_tasks.end(); it++) {
		if (now > (*it)->due_date) {
			to_kill.emplace(make_pair((*it)->state_id, (*it)->simu_id));
		}
	}
	for (auto it = running_sub_tasks.begin(); it != running_sub_tasks.end(); it++) {
		if (now > (*it)->due_date) {
			to_kill.emplace(make_pair((*it)->state_id, (*it)->simu_id));
		}
	}

	if (to_kill.size() > 0) {
		L("Need to redo %d states", to_kill.size());
	}

	for (auto it = to_kill.begin(); it != to_kill.end(); it++) {
		L("Due date passed for state id %d , simu_id %d at %d s ", it->first, it->second, now);
		kill_it(it->first, it->second);
		if (comm_rank == 0) {
			// reschedule directly if possible
			if (idle_simulations.size() > 0) {
				L("Rescheduling after due date violation detected by rank 0");
				schedule_new_task(idle_simulations.begin()->first);
			}

			int buf[2] = { it->first, it->second };
			for (int send_rank = 1; send_rank < comm_size; send_rank++ ) {
				MPI_Bsend(buf, 2, MPI_INT, send_rank, TAG_KILL_SIMULATION, MPI_COMM_WORLD);
			}

		} else {
			// Send to rank 0 that the simulation that calcultated this state is to kill

			// need Ssend here to synchronize so rank 0 cannot finish into update step in the mean time....
			L("Sending kill request to rank 0");

			//             state id , simu id
			int buf[2] = { it->first, it->second };
			MPI_Ssend(buf, 2, MPI_INT, 0, TAG_KILL_SIMULATION, MPI_COMM_WORLD);
			// TODO: danger: what if now the other sends stuff...
			L("Finished kill request to rank 0");
		}
	}
}

void check_kill_requests() {
	if(comm_rank == 0) {

		//L("Checking kill req");
		for (int detector_rank = 1; detector_rank < comm_size; detector_rank++)
		{
			int received;
			MPI_Iprobe(detector_rank, TAG_KILL_SIMULATION, MPI_COMM_WORLD, &received, MPI_STATUS_IGNORE);
			if (received)
			{
				int buf[2];
				MPI_Recv(buf, 2, MPI_INT, detector_rank, TAG_KILL_SIMULATION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				L("Got state_id to kill... %d, killing simu_id %d", buf[0], buf[1]);
				bool is_new = killed_simulations.emplace(buf[1]).second;
				if (!is_new) {
					// don't kill it a second time!
					L("I already knew this");
					continue;
				}

				kill_it(buf[0], buf[1]);

				// in case we cannot reset the finished ranks we do it here.
				finished_ranks = 0;

				for (int send_rank = 1; send_rank < comm_size; send_rank++ ) {
					if (send_rank == detector_rank) { // he knows it already...
						continue;
					}

					MPI_Bsend(buf, 2, MPI_INT, send_rank, TAG_KILL_SIMULATION, MPI_COMM_WORLD);
				}

				// reschedule directly if possible
				if (idle_simulations.size() > 0) {

					L("Rescheduling after due date violation detected by detector_rank %d", detector_rank);
					schedule_new_task(idle_simulations.begin()->first);
				}
			}
		}
	} else {
		int received;
		MPI_Iprobe(0, TAG_KILL_SIMULATION, MPI_COMM_WORLD, &received, MPI_STATUS_IGNORE);
		if (received)
		{
			int buf[2];
			MPI_Recv(buf, 2, MPI_INT, 0, TAG_KILL_SIMULATION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			L("Got state_id to kill... %d, killing simu_id %d", buf[0], buf[1]);
			bool is_new = killed_simulations.emplace(buf[1]).second;
			if (!is_new) {
				return;
			}
			kill_it(buf[0], buf[1]);
			// After killing maybe other people are in the queue at first place now?
			try_kickoff_subtasks();
		}
	}
}

int main(int argc, char * argv[])
{
	assert(MAX_TIMESTAMP > 1);

	int major, minor, patch;
	zmq_version (&major, &minor, &patch);
	D("Current 0MQ version is %d.%d.%d", major, minor, patch);
	MPI_Init(NULL, NULL);
	context = zmq_ctx_new ();

	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

	// Get the rank of the process
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
	void * configuration_socket = NULL;

	D("**server rank = %d", comm_rank);

	// Start sockets:
	if (comm_rank == 0)
	{
		configuration_socket = zmq_socket(context, ZMQ_REP);
		int rc = zmq_bind(configuration_socket, "tcp://*:4000");  // to be put into MELISSA_SERVER_MASTER_NODE on simulation start
		ZMQ_CHECK(rc);
		assert(rc == 0);

	}

	data_response_socket = zmq_socket(context, ZMQ_ROUTER);
	char data_response_port_name[MPI_MAX_PROCESSOR_NAME];
	sprintf(data_response_port_name, "tcp://*:%d", 5000+comm_rank);
	zmq_bind(data_response_socket, data_response_port_name);

	char hostname[MPI_MAX_PROCESSOR_NAME];
	melissa_get_node_name(hostname, MPI_MAX_PROCESSOR_NAME);
	sprintf(data_response_port_name, "tcp://%s:%d", hostname, 5000+comm_rank);

	char data_response_port_names[MPI_MAX_PROCESSOR_NAME * comm_size];
	MPI_Gather(data_response_port_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
			data_response_port_names, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);

	while (true)
	{
		usleep(10); // to chill down the processor! TODO remove when measuring!
		// Wait for requests
		/* Poll for events indefinitely */
		// REM: the poll item needs to be recreated all the time!
		zmq_pollitem_t items [2] = {
				{data_response_socket, 0, ZMQ_POLLIN, 0},
				{configuration_socket, 0, ZMQ_POLLIN, 0}
		};
		if (comm_rank == 0)
		{
			// Check for killed simulations... (due date violation)
			// poll the fastest possible to be not in concurrence with the mpi probe calls... (theoretically we could set this time to -1 if using only one core for the server.)
			ZMQ_CHECK(zmq_poll (items, 2, 0));
		}
		else
		{
			//ZMQ_CHECK(zmq_poll (items, 1, -1));
			// Check for new tasks to schedule and killed simulations so do not block on polling (due date violation)...
			ZMQ_CHECK(zmq_poll (items, 1, 0));
		}
		/* Returned events will be stored in items[].revents */

		// answer requests
		if (comm_rank == 0 && (items[1].revents & ZMQ_POLLIN))
		{
			answer_configuration_message(configuration_socket, data_response_port_names);
		}

		// coming from fresh init...
		if (phase == PHASE_INIT)
		{
			if (comm_rank == 0)
			{
				// check if initialization on rank 0 finished
				// (rank 0 does some more intitialization than the other server ranks)
				if (fields.size() == FIELDS_COUNT)
				{
					// low: if multi field: check fields! (see if the field names we got are the ones we wanted)
					// propagate all fields to the other server clients on first message receive!
					broadcast_field_information_and_calculate_parts();
					init_new_timestamp();

					phase = PHASE_SIMULATION;
				}
			}
			else
			{
				// Wait for rank 0 to finish field registrations. rank 0 does this in answer_configu
				// propagate all fields to the other server clients on first message receive!
				broadcast_field_information_and_calculate_parts();
				init_new_timestamp();
				phase = PHASE_SIMULATION;
			}
		}
		if (phase == PHASE_SIMULATION && (items[0].revents & ZMQ_POLLIN))
		{
			// TODO: move to send and receive function as on api side...
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

			assert(IDENTITY_SIZE == 0 || IDENTITY_SIZE == zmq_msg_size(&identity_msg));

			IDENTITY_SIZE = zmq_msg_size(&identity_msg);

			void * identity = malloc(IDENTITY_SIZE);
			memcpy(identity, zmq_msg_data(&identity_msg), zmq_msg_size(&identity_msg));

			assert(zmq_msg_size(&header_msg) == 4 * sizeof(int) + MPI_MAX_PROCESSOR_NAME * sizeof(char));
			int * header_buf = reinterpret_cast<int*>(zmq_msg_data(&header_msg));
			int simu_id = header_buf[0];
			int simu_rank = header_buf[1];
			int simu_state_id = header_buf[2];  // = ensemble_member_id;
			int simu_timestamp = header_buf[3];
			char field_name[MPI_MAX_PROCESSOR_NAME];
			strcpy(field_name, reinterpret_cast<char*>(&header_buf[4]));

			// good simu_rank, good state id?
			auto running_sub_task = find_if(running_sub_tasks.begin(), running_sub_tasks.end(), [simu_id, simu_rank, simu_state_id](shared_ptr<SubTask> &st){
				return st->simu_id == simu_id && st->state_id == simu_state_id && st->simu_rank == simu_rank;
			});

			SimulationRankConnection csr(identity);
			if (killed_simulations.find(simu_id) != killed_simulations.end()) {
				L("Ending simulation killed by timeout violation simu_id=%d", simu_id);
				csr.end(KILL_SIMULATION);
			} else {
				assert(simu_timestamp == 0 || running_sub_task != running_sub_tasks.end());

				// This is necessary if a task was finished on rank 0. then it crashes on another rank. so rank 0 needs to undo this!
				if (running_sub_task != running_sub_tasks.end()) {
					// only if we are not in timestamp  0:
					finished_sub_tasks.push_back(*running_sub_task);

					running_sub_tasks.remove(*running_sub_task);
				}

				// good timestamp? There are 2 cases: timestamp 0 or good timestamp...
				assert (simu_timestamp == 0 || simu_timestamp == current_timestamp);
				// we always throw away timestamp 0 as we want to init the simulation! (TODO! we could also use it as ensemble member...)


				// Save state part in background_states.
				n_to_m & part = fields[field_name]->getPart(simu_rank);
				assert(zmq_msg_size(&data_msg) == part.send_count * sizeof(double));
				D("<- Server received %lu/%lu bytes of %s from Simulation id %d, simulation rank %d, state id %d, timestamp=%d",
						zmq_msg_size(&data_msg), part.send_count * sizeof(double), field_name, simu_id, simu_rank, simu_state_id, simu_timestamp);
				D("local server offset %lu, sendcount=%lu", part.local_offset_server, part.send_count);


				D("values[0] = %.3f", reinterpret_cast<double*>(zmq_msg_data(&data_msg))[0]);
				if (simu_timestamp == current_timestamp)
				{
					// zero copy is unfortunately for send only. so copy internally...
					fields[field_name]->ensemble_members[simu_state_id].store_background_state_part(part,
							reinterpret_cast<double*>(zmq_msg_data(&data_msg)));
				}

				// whcih atm can not even happen if more than one fields as they do there communication one after another.
				// TODO: but what if we have multiple fields? multiple fields is a no go I think multiple fields would need also synchronism on the server side. he needs to update all the fields... as they are not independent from each other that does not work.


				zmq_msg_close(&identity_msg);
				zmq_msg_close(&empty_msg);
				zmq_msg_close(&header_msg);
				zmq_msg_close(&data_msg);


				// Check if we can answer directly with new data... means starting of a new model task
				auto found = find_if(scheduled_sub_tasks.begin(), scheduled_sub_tasks.end(), [simu_id, simu_rank](shared_ptr<SubTask> &st){
					return st->simu_id == simu_id && st->simu_rank == simu_rank;
				});

				if (found != scheduled_sub_tasks.end()) {
					// found a new task. send back directly!

					D("send after receive! to simu rank %d on simu_id %d", simu_rank, simu_id);
					csr.send_task(simu_rank, (*found)->state_id);
					// don't need to delete from connected simulations as we were never in there...  TODO: do this somehow else. probably not csr.send but another function taking csr as parameter...
					running_sub_tasks.push_back(*found);
					scheduled_sub_tasks.remove(*found);

				} else {
					// Save connection - basically copy identity pointer...
					auto &simu = idle_simulations.emplace(simu_id, shared_ptr<Simulation>(new Simulation())).first->second;
					simu->connected_simulation_ranks.emplace(simu_rank, csr);
					D("save connection simuid %d, simu rank %d", simu_id, simu_rank);

					if (comm_rank == 0) {
						// If we could not start a new model task try to schedule a new one. This is initiated by server rank 0
						//( no new model task means that at least this rank is finished. so the others will finish soon too as we assum synchronism in the simulaitons)
						// check if there is no other task running on this simulation
						if (idle_simulations[simu_id]->connected_simulation_ranks.size() == fields.begin()->second->connected_simulation_ranks.size()) {
							schedule_new_task(simu_id);
						}
					}
				}
			}
		}

			// REM: We try to schedule new data after the server rank 0 gave new tasks and after receiving new data. It does not make sense to schedule at other times for the moment. if there is more fault tollerance this needs to be changed.

		if (phase == PHASE_SIMULATION)
		{
			size_t connections = fields.begin()->second->connected_simulation_ranks.size();
			// check if all data was received. If yes: start Update step to calculate next analysis state
			bool finished;
			if (comm_rank == 0) {
				// try to know if somebody else finished
				for (int rank = 1; rank < comm_size; rank++) {
					int received;

					MPI_Iprobe(rank, TAG_RANK_FINISHED, MPI_COMM_WORLD, &received, MPI_STATUS_IGNORE);
					if (received)
					{
						L("Somebody finished... ");
						int highest_task_id;
						MPI_Recv(&highest_task_id, 1, MPI_INT, rank, TAG_RANK_FINISHED, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						if (highest_task_id == task_id) {
							// this message was up to date....
							finished_ranks ++;
						}
					}

				}


				finished = finished_ranks == comm_size-1 &&   // comm size without rank 0
						unscheduled_tasks.size() == 0 && scheduled_sub_tasks.size() == 0 && running_sub_tasks.size() == 0 &&
						finished_sub_tasks.size() == ENSEMBLE_SIZE * connections;
				//L("rank 0: D %d ", finished_ranks);

				if (finished) {
					// tell everybody that everybody finished!
					for (int rank = 1; rank < comm_size; rank++) {
						int received;

						L("Sending tag all finished message");
						MPI_Send(nullptr, 0, MPI_BYTE, rank, TAG_ALL_FINISHED, MPI_COMM_WORLD);
					}
				}
			} else {
				finished = false;

				// REM: we need to synchronize when we finish as otherwise processes are waiting in the mpi barrier to do the update step while still some ensemble members need to be repeated due to failing simulations.

				// would this rank finish?
				bool would_finish = unscheduled_tasks.size() == 0 && scheduled_sub_tasks.size() == 0 && running_sub_tasks.size() == 0 &&
						finished_sub_tasks.size() == ENSEMBLE_SIZE * connections;
				if (would_finish) {
					if (highest_sent_task_id < highest_received_task_id) {
						// only send once when we finished.  REM: using Bsend as faster as buffere, buffering not necessary here...

						MPI_Send(&highest_received_task_id, 1, MPI_INT, 0, TAG_RANK_FINISHED, MPI_COMM_WORLD);
						highest_sent_task_id = highest_received_task_id;  // do not send again..
					}

					int received;
					MPI_Iprobe(0, TAG_ALL_FINISHED, MPI_COMM_WORLD, &received, MPI_STATUS_IGNORE);
					if (received) {
						L("receiving tag all finished message");
						MPI_Recv(nullptr, 0, MPI_BYTE, 0, TAG_ALL_FINISHED, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						finished = true;
					}
				}

			}

			if (finished)
			{

				do_update_step();

				if (current_timestamp >= MAX_TIMESTAMP)
				{
					end_all_simulations();
					break;
				}

				init_new_timestamp();

				// After update step: rank 0 loops over all simu_id's sending them a new state vector part they have to propagate.
				if (comm_rank == 0)
				{
					for (auto simu_it = idle_simulations.begin(); simu_it != idle_simulations.end(); simu_it++)
					{
						L("Rescheduling after update step");
						schedule_new_task(simu_it->first);
					}
				}
			}
		}



		if (phase == PHASE_SIMULATION && comm_rank != 0)
		{
			// Check if I have to schedule new tasks:
			check_schedule_new_tasks();
		}


		if (phase == PHASE_SIMULATION) {
			// check if we have to kill some jobs as they did not respond:
			check_due_dates();

			check_kill_requests();
		}


		/// REM: Tasks are either unscheduled, scheduled, running or finished.
		if (phase == PHASE_SIMULATION) {
//			size_t connections = fields.begin()->second->connected_simulation_ranks.size();
//			L("unscheduled sub tasks: %lu, scheduled sub tasks: %lu running sub tasks: %lu finished sub tasks: %lu",
//					unscheduled_tasks.size() * connections,  // scale on amount of subtasks.
//					scheduled_sub_tasks.size(),
//					running_sub_tasks.size(),
//					finished_sub_tasks.size());

			//doesnt hold true just after a simulation was deleted as sometimes the reschedule is received before the simulation kill message.
//			assert(
//					unscheduled_tasks.size() * connections +  // scale on amount of subtasks.
//					scheduled_sub_tasks.size() + running_sub_tasks.size() +
//					finished_sub_tasks.size() == connections * ENSEMBLE_SIZE);
		}

		// TODO: if receiving message to shut down simulation: kill simulation (mpi probe for it...)
		// TODO: check if we need to kill some simulations...
	}


	D("Ending Server.");
	// TODO: check if we need to delete some more stuff!

	// wait 3 seconds to finish sending... actually NOT necessary... if you need this there is probably soething else broken...
	//sleep(3);
  zmq_close(data_response_socket);
  if (comm_rank == 0)
  {
		zmq_close(configuration_socket);
  }
	zmq_ctx_destroy(context);
	MPI_Finalize();

}
