// TODOS:
// TODO 1. refactoring
// TODO 2. error prone ness
// TODO 3. check with real world sim and DA.

#include <map>
#include <string>
#include <cstdlib>
#include <cassert>

#include <vector>

#include <mpi.h>
#include "zmq.h"

#include "../common/messages.h"
#include "../common/n_to_m.h"
#include "../common/utils.h"

#include <time.h>

// TODO: configure this somehow better:
const int ENSEMBLE_SIZE = 5;
const int FIELDS_COUNT = 1;  // multiple fields is stupid!
const long long MAX_SIMULATION_TIMEOUT = 600;  // 10 min max timeout for simulations.
const int MAX_TIMESTAMP = 5;

using namespace std;

const int TAG_NEW_TASK = 42;

unsigned int IDENTITY_SIZE = 0;

// Server comm_size and Server rank
int comm_size;
int comm_rank;

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
	int local_vector_offset;
	int send_count;
};

struct EnsembleMember
{
	vector<double> state_analysis;  // really tODO use vector! vector.data() will give the same... raw pointer!
	vector<double> state_background;
	int received_state_background = 0;

	void set_local_vect_size(int local_vect_size)
	{
		// TODO: replace all mallocs by new? Actually it is dirty to use new[].... better use a vector. but I'm not sure if that is compatible with c and our double arrays ;) https://stackoverflow.com/questions/4754763/object-array-initialization-without-default-constructor
		state_analysis.reserve(local_vect_size);
		state_analysis.resize(local_vect_size);
		state_background.reserve(local_vect_size);
		state_background.resize(local_vect_size);
	}

	void store_background_state_part(const n_to_m & part, const double * values)
	{
		D("before_assert %d %d %lu", part.send_count, part.local_offset_server, state_background.size());
		// TODO https://stackoverflow.com/questions/40807833/sending-size-t-type-data-with-mpi
		assert(part.send_count + part.local_offset_server <= state_background.size());
		copy(values, values + part.send_count, state_background.data() + part.local_offset_server);
		received_state_background += part.send_count;
	}
};

struct Field {
	// index: state id.
	vector<EnsembleMember> ensemble_members;

	int local_vect_size;
	vector<int> local_vect_sizes_simu;
	vector<n_to_m> parts;

	Field(int simu_comm_size_, int ensemble_size_)
	{
		local_vect_size = 0;
		local_vect_sizes_simu.resize(simu_comm_size_);
		ensemble_members.resize(ensemble_size_);
	}

	// TODO: naming: server_comm size or ranks_server? same for simu!
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
			}
		}

		for (auto ens_it = ensemble_members.begin(); ens_it != ensemble_members.end(); ens_it++)
		{

			ens_it->set_local_vect_size(local_vect_size);  // low: better naming: local state size is in doubles not in bytes!
		}
	}

	/// Finds the part of the field with the specified simu_rank.
	n_to_m & getPart(int simu_rank)
	{
		for (auto part_it = parts.begin(); part_it != parts.end(); part_it++)
		{
			if (part_it->rank_server == comm_rank && part_it->rank_simu == simu_rank) {
				return *part_it;
			}
		}
		assert(false); // Did not find the part!
	}

	// checks if all ensemble members received all the data needed.
	bool finished() {
		for (auto ens_it = ensemble_members.begin(); ens_it != ensemble_members.end(); ens_it++)
		{
			assert(local_vect_size != 0);
			if (ens_it->received_state_background != local_vect_size)
			{
				return false;
			}
		}
		return true;
	}

};

map<string, Field*> fields;

struct Task {
	int state_id;
	long long due_date;

	bool operator!=(const Task &rhs) {
		return state_id != rhs.state_id || due_date != rhs.due_date;
	}
};


/// used to transmit new tasks to clients.
struct NewTask {
	int task_id;
	int simu_id;  // simu id on which to run the task
	Task task;
};

const Task WANT_WORK          = Task{-1, -1};
const Task TASK_UNINITIALIZED = Task{-2, -2};


void my_free(void * data, void * hint)
{
	//delete [] data;
	//delete [] reinterpret_cast<int*>(data);
	free(data);
}

struct ConnectedSimulationRank {
	// task id (to keep them in order)
	map<int, Task> waiting_tasks;

	Task current_task = TASK_UNINITIALIZED;

	void * connection_identity = NULL;

	~ConnectedSimulationRank() {
		if (connection_identity != NULL) {
			free(connection_identity);
		}
	}


	/// returns true if a new was sent.
	bool try_to_start_task(int simu_rank) {

		// already working?
		if (current_task != WANT_WORK)
			return false;

		// Do we have waiting tasks?
		if (waiting_tasks.size() == 0)
			return false;

		// get front task and send it ...
		// then delete it from thye waiting_tasks list.
		auto first_elem = waiting_tasks.begin();

		//int header[3] = { first_elem->second.state_id, current_timestamp, CHANGE_STATE };  this does not work as send is non blocking...
		int * header = new int[3];  // needto do it like this to be sure it stays in memory as send is non blocking!
		header[0] = first_elem->second.state_id;
		header[1] = current_timestamp;
		header[2] = CHANGE_STATE;

		zmq_msg_t identity_msg;
		zmq_msg_t empty_msg;
		zmq_msg_t header_msg;
		zmq_msg_t data_msg;

		zmq_msg_init_data(&identity_msg, connection_identity, IDENTITY_SIZE, my_free, NULL);
		zmq_msg_send(&identity_msg, data_response_socket, ZMQ_SNDMORE);

		zmq_msg_init(&empty_msg);
		zmq_msg_send(&empty_msg, data_response_socket, ZMQ_SNDMORE);

		ZMQ_CHECK(zmq_msg_init_data(&header_msg, header, 3 * sizeof(int), NULL, NULL));
		zmq_msg_send(&header_msg, data_response_socket, ZMQ_SNDMORE);
		// we do not know when it will really send. send is non blocking!

		zmq_msg_init_data(&data_msg,
				(fields.begin()->second->ensemble_members.at(first_elem->second.state_id).state_analysis.data() + fields.begin()->second->getPart(simu_rank).local_offset_server),
				fields.begin()->second->getPart(simu_rank).send_count * sizeof(double), NULL, NULL);

		D("-> Server sending %lu bytes for state %d, timestamp=%d",
				fields.begin()->second->getPart(simu_rank).send_count * sizeof(double),
				header[0], header[1]);
		D("local server offset %d, local simu offset %d, sendcount=%d", fields.begin()->second->getPart(simu_rank).local_offset_server, fields.begin()->second->getPart(simu_rank).local_offset_simu, fields.begin()->second->getPart(simu_rank).send_count);
		print_vector(fields.begin()->second->ensemble_members.at(first_elem->second.state_id).state_analysis);

		zmq_msg_send(&data_msg, data_response_socket, 0);

		current_task = first_elem->second;
		waiting_tasks.erase(first_elem);

		// close connection:
		// but do not free it. send is going to free it.
		connection_identity = NULL;

		return true;
	}

	void end() {
		// some connection_identities will be 0 if some simulation ranks are connected to another server rank at the moment.
		if (connection_identity == NULL)
			return;

		int * header = new int[3];  // needto do it like this to be sure it stays in memory as send is non blocking!
		header[0] = -1;
		header[1] = current_timestamp;
		header[2] = END_SIMULATION;

		zmq_msg_t identity_msg;
		zmq_msg_t empty_msg;
		zmq_msg_t header_msg;

		zmq_msg_init_data(&identity_msg, connection_identity, IDENTITY_SIZE, my_free, NULL);
		zmq_msg_send(&identity_msg, data_response_socket, ZMQ_SNDMORE);

		zmq_msg_init(&empty_msg);
		zmq_msg_send(&empty_msg, data_response_socket, ZMQ_SNDMORE);

		ZMQ_CHECK(zmq_msg_init_data(&header_msg, header, 3 * sizeof(int), NULL, NULL));
		zmq_msg_send(&header_msg, data_response_socket, 0);

		D("Send end message");

		// but don't delete it. this is done in the message.
		connection_identity = NULL;
	}
};




struct Simulation  // Model process runner
{
	// simulations rank
	map<int, ConnectedSimulationRank> connected_simulation_ranks; // TODO: rename in SimulationRankConnection, also on server side?

	Simulation()
	{
		auto &parts = fields.begin()->second->parts;
		for (auto part_it = parts.begin(); part_it != parts.end(); part_it++)
		{
			if (part_it->rank_server == comm_rank)
			{
				connected_simulation_ranks.emplace(part_it->rank_simu, ConnectedSimulationRank());
			}
		}
	}

	void addTask(const int task_id, const Task &task) {
		for (auto cs = connected_simulation_ranks.begin(); cs != connected_simulation_ranks.end(); cs++) {
			cs->second.waiting_tasks.emplace(task_id, task);
		}
	}

	void try_to_start_task() {// todo: replace fields.begin() by field as there will be only on field soon.
		for (auto cs = connected_simulation_ranks.begin(); cs != connected_simulation_ranks.end(); cs++) {
			cs->second.try_to_start_task(cs->first);
		}
	}

	void end() {
		for (auto cs = connected_simulation_ranks.begin(); cs != connected_simulation_ranks.end(); cs++) {
			D("xxx end connected simulation rank...");
			cs->second.end();
		}
	}

//	// return true if it is ready to take work as doing nothing and it has a connection identity...
//	bool ready() {
//		for (auto cs = connected_simulation_ranks.begin(); cs != connected_simulation_ranks.end(); cs++) {
//			if (cs->second.current_task != WANT_WORK || cs->second.connection_identity == NULL) {
//				return false;
//			}
//		}
//		return true;
//	}
};

// simu_id, Simulation:
map<int, Simulation> simulations;



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
		assert(zmq_msg_size(&msg) == simu_comm_size * sizeof(int));
		// TODO: can be done 0copy!
		memcpy (newField->local_vect_sizes_simu.data(), zmq_msg_data(&msg), simu_comm_size * sizeof(int));
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
	int field_count = fields.size();
	MPI_Bcast(&field_count, 1, MPI_INT, 0, MPI_COMM_WORLD);                          // 0:field_count
	auto field_it = fields.begin();
	for (int i = 0; i < field_count; i++)
	{
		char field_name[MPI_MAX_PROCESSOR_NAME];
		if (comm_rank == 0) {
			strcpy(field_name, field_it->first.c_str());
			MPI_Bcast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);  // 1:fieldname
			int simu_comm_size = field_it->second->local_vect_sizes_simu.size();
			MPI_Bcast(&simu_comm_size, 1, MPI_INT, 0, MPI_COMM_WORLD);                   // 2:simu_comm_size

			D("local_vect_sizes");
			print_vector(field_it->second->local_vect_sizes_simu);

			MPI_Bcast(field_it->second->local_vect_sizes_simu.data(), simu_comm_size,
					MPI_INT, 0, MPI_COMM_WORLD);                                             // 3:local_vect_sizes_simu

			field_it->second->calculate_parts(comm_size);
			field_it++;
		} else {
			int simu_comm_size;
			MPI_Bcast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);  // 1: fieldname
			MPI_Bcast(&simu_comm_size, 1, MPI_INT, 0, MPI_COMM_WORLD);                   // 2:simu_comm_size

			Field * newField = new Field(simu_comm_size, ENSEMBLE_SIZE);

			D("local_vect_sizes");
			print_vector(newField->local_vect_sizes_simu);
			MPI_Bcast(newField->local_vect_sizes_simu.data(), simu_comm_size,
					MPI_INT, 0, MPI_COMM_WORLD);                                              // 3:local_vect_sizes_simu

			newField->calculate_parts(comm_size);

			fields.emplace(field_name, newField);
		}
	}
}

vector<int> unscheduled_tasks;
/// returns true if it scheduled a new task for the given simuid
/// schedules a new task on a model task runner
bool schedule_new_task(int simuid)
{
	assert(comm_rank == 0);
	static int task_id = 0;
	task_id++;
	assert(comm_rank == 0);
	if (unscheduled_tasks.size() > 0) {
		int state_id = *(unscheduled_tasks.end()-1);
		unscheduled_tasks.pop_back();

		Task task{state_id, get_due_date()};
		simulations[simuid].addTask(task_id, task);


		NewTask new_task({task_id, simuid, task});
		// Send new scheduled task to all ranks! This makes sure that everybody receives it!
		for (int receiving_rank = 1; receiving_rank < comm_size; receiving_rank++)
		{
			// low: one could use ISend and then wait for all to complete
			MPI_Send(&new_task, sizeof(NewTask), MPI_BYTE, receiving_rank, TAG_NEW_TASK, MPI_COMM_WORLD);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void end_all_simulations()
{
	for (auto simu_it = simulations.begin(); simu_it != simulations.end(); simu_it++)
	{
		simu_it->second.end();
	}
}

void init_new_timestamp()
{
	current_timestamp++;
	if (comm_rank == 0) {
		assert(unscheduled_tasks.size() == 0);
	}
	for (int i = 0; i < ENSEMBLE_SIZE; i++) {

		if (comm_rank == 0) {
			unscheduled_tasks.push_back(i);
		}

		for (auto field_it = fields.begin(); field_it != fields.end(); field_it++)
		{
			auto &member = field_it->second->ensemble_members[i];
			// at the beginning of the time or otherwise jsut finished a timestep...
			assert(current_timestamp == 1 || member.received_state_background == field_it->second->local_vect_size);
			member.received_state_background = 0;
			D("test: %d", field_it->second->ensemble_members[i].received_state_background);
		}
	}
}

Simulation & find_or_insert_simulation(int simu_id)
{
	auto res = simulations.find(simu_id);
	if (res == simulations.end())
	{
		return simulations.emplace(simu_id, Simulation()).first->second;
	}
	else
	{
		return res->second;
	}
}

void do_update_step()
{
	// TODO
	printf("Doing update step...\n");
	MPI_Barrier(MPI_COMM_WORLD);
	for (auto field_it = fields.begin(); field_it != fields.end(); field_it++)
	{
		for (auto ens_it = field_it->second->ensemble_members.begin(); ens_it != field_it->second->ensemble_members.end(); ens_it++)
		{
			assert(ens_it->state_analysis.size() == ens_it->state_background.size());
			for (int i = 0; i < ens_it->state_analysis.size(); i++) {
				// pretend to do some da...
				ens_it->state_analysis[i] = ens_it->state_background[i] + 3;
			}
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
		// Wait for requests
		/* Poll for events indefinitely */
		// REM: the poll item needs to be recreated all the time!
		zmq_pollitem_t items [2] = {
				{data_response_socket, 0, ZMQ_POLLIN, 0},
				{configuration_socket, 0, ZMQ_POLLIN, 0}
		};
		if (comm_rank == 0)
		{
			ZMQ_CHECK(zmq_poll (items, 2, -1));
		}
		else
		{
			//ZMQ_CHECK(zmq_poll (items, 1, -1));
			// Check for new tasks to schedule so do not block on polling...
			ZMQ_CHECK(zmq_poll (items, 1, 100));
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
					// TODO: check fields!
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

			// The simulations object is needed in case we have more ensemble members than connected simulations!
			Simulation &simu = find_or_insert_simulation(simu_id);

			// good simu_rank, good state id?
			assert(simu_timestamp == 0 || simu_state_id == simu.connected_simulation_ranks[simu_rank].current_task.state_id);

			// good timestamp? There are 3 cases:
			// we always throw away timestamp 0 as we want to init the simulation! (TODO! we could also use it as ensemble member...)

			assert (simu_timestamp == 0 || simu_timestamp == current_timestamp);


			// Save state part in background_states.
			n_to_m & part = fields[field_name]->getPart(simu_rank);
			assert(zmq_msg_size(&data_msg) == part.send_count * sizeof(double));
			D("<- Server received %lu/%lu bytes of %s from Simulation id %d, simulation rank %d, state id %d, timestamp=%d",
					zmq_msg_size(&data_msg), part.send_count * sizeof(double), field_name, simu_id, simu_rank, simu_state_id, simu_timestamp);
			D("local server offset %d, sendcount=%d", part.local_offset_server, part.send_count);

			D("values[0] = %.3f", reinterpret_cast<double*>(zmq_msg_data(&data_msg))[0]);
			if (simu_timestamp == current_timestamp)
			{
				// zero copy is unfortunately for send only. so copy internally...
				fields[field_name]->ensemble_members[simu_state_id].store_background_state_part(part,
						reinterpret_cast<double*>(zmq_msg_data(&data_msg)));
			}


			// Save connection - basically copy identity pointer...
			assert(simu.connected_simulation_ranks[simu_rank].connection_identity == NULL);
			simu.connected_simulation_ranks[simu_rank].connection_identity = identity;
			simu.connected_simulation_ranks[simu_rank].current_task = WANT_WORK;

			// whcih atm can not even happen if more than one fields as they do there communication one after another.
			// TODO: but what if we have multiple fields? multiple fields is a no go I think multiple fields would need also synchronism on the server side. he needs to update all the fields... as they are not independent from each other that does not work.


			zmq_msg_close(&identity_msg);
			zmq_msg_close(&empty_msg);
			zmq_msg_close(&header_msg);
			zmq_msg_close(&data_msg);

			// Check if we can answer directly with new data... means starting of a new model task
			bool got_task = simu.connected_simulation_ranks[simu_rank].try_to_start_task(simu_rank);

			// If we could not start a new model task try to schedule a new one. This is initiated by server rank 0
			if (!got_task && comm_rank == 0)
				{
					// schedule something new on this model task runner/ simulation!
					schedule_new_task(simu_id);

					// try to run it directly:
					simu.connected_simulation_ranks[simu_rank].try_to_start_task(simu_rank);
				}
			}

			// REM: We try to schedule new data after the server rank 0 gave new tasks and after receiving new data. It does not make sense to schedule at other times for the moment. if there is more fault tollerance this needs to be changed.

		if (phase == PHASE_SIMULATION)
		{
			// check if all data was received. If yes: start Update step to calculate next analysis state
			bool finished = true;
			for (auto field_it = fields.begin(); field_it != fields.end(); field_it++)
			{
				finished = field_it->second->finished();
				if (!finished)
					break;
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
					for (auto simu_it = simulations.begin(); simu_it != simulations.end(); simu_it++)
					{
						if (schedule_new_task(simu_it->first)) {
							// normally we arrive always here if there are not more model task runners than ensemble members.
							simu_it->second.try_to_start_task();
						}
					}
				}
			}
		}


		// TODO: check ret values!
		// TODO: remove compile warnings!

		if (phase == PHASE_SIMULATION && comm_rank != 0)
		{
			// Check if I have to schedule new tasks:
			int received;

			MPI_Iprobe(0, TAG_NEW_TASK, MPI_COMM_WORLD, &received, MPI_STATUS_IGNORE);
			if (received)
			{
				D("Got task to send...");
				NewTask new_task;
				MPI_Recv(&new_task, sizeof(new_task), MPI_BYTE, 0, TAG_NEW_TASK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				Simulation &simu  = find_or_insert_simulation(new_task.simu_id);
				simu.addTask(new_task.task_id, new_task.task);

				// If so try to start them directly.
				simu.try_to_start_task();
			}

		}
	}

   // TODO: if receiving message to shut down simulation: kill simulation (mpi probe for it...)
	// TODO: check if we need to kill some simulations...

	D("Ending Server.");
	// TODO: check if we need to delete some more stuff!
	// wait 3 seconds to finish sending...
	//sleep(3);
  zmq_close(data_response_socket);
  if (comm_rank == 0)
  {
		zmq_close(configuration_socket);
  }
	zmq_ctx_destroy(context);
	MPI_Finalize();

}
