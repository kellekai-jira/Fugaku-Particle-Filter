#include <map>
#include <string>
#include <cstdlib>
#include <cassert>

// memcopy:
#include <cstring>

#include <mpi.h>
#include "zmq.h"

#include "../common/messages.h"
#include "../common/n_to_m.h"
#include "../common/utils.h"

#include <time.h>

// TODO: configure this somehow better:
const int ENSEMBLE_SIZE = 1;
const int SIMULATIONS_COUNT = 1;
const int FIELDS_COUNT = 1;  // multiple fields is stupid!
const long long MAX_SIMULATION_TIME = 600;  // 10 min max timeout for simulations.

using namespace std;

const int TAG_NEW_TASK = 42;

int IDENTITY_SIZE = 0;

// Server comm_size and Server rank
int comm_size;
int comm_rank;

void * context;
void * data_response_socket;

int current_timestamp = 0;  // will effectively start at 1.

long long get_due_date() {
	time_t seconds;
	seconds = time (NULL);
	return seconds + MAX_SIMULATION_TIME;
}
struct Part
{
	int rank_simu;
	int local_vector_offset;
	int send_count;
};

struct EnsembleMember
{
	double *state_analysis;  // really tODO use vector! vector.data() will give the same... raw pointer!
	double *state_background;
	int size;
	int received_state_background = 0;

	void set_ensemble_size(int size_)
	{
		// TODO: if this is called 2 we get memory leaks. This is a dirty replacement of a con
		size = size_;

		// TODO: replace all mallocs by new? Actually it is dirty to use new[].... better use a vector. but I'm not sure if that is compatible with c and our double arrays ;) https://stackoverflow.com/questions/4754763/object-array-initialization-without-default-constructor
		state_analysis = new double[size];
		state_background = new double[size];
	}

	~EnsembleMember()
	{
		if (size != 0) {
			delete [] state_analysis;
			delete [] state_background;
		}
	}

	bool finished()
	{
		return size == received_state_background;
	}

	void store_background_state_part(int local_offset, double * values, int amount_of_doubles)
	{
		memcpy(reinterpret_cast<void*>(&state_background[local_offset]), reinterpret_cast<void*>(values), amount_of_doubles * sizeof(double));
		received_state_background += amount_of_doubles;
	}
};

struct Field {
	// index: state id.
	EnsembleMember * ensemble_members;
	int ensemble_size;

	int * local_vect_sizes_simu;
	int simu_comm_size;
	vector<n_to_m> parts;

	Field(int simu_comm_size_, int ensemble_size_)
	{
		simu_comm_size = simu_comm_size_;
		local_vect_sizes_simu = new int[simu_comm_size];
		ensemble_size = ensemble_size_;
		ensemble_members = new EnsembleMember[ensemble_size];
		for (int i = 0; i < ensemble_size; ++i)
		{
			ensemble_members[i].set_ensemble_size(ensemble_size);
		}
	}

	// TODO: naming: server_comm size or ranks_server? same for simu!
	/// Calculates all the state vector parts that are send between the server and the
	/// simulations
	void calculate_parts(int server_comm_size)
	{
		parts = calculate_n_to_m(server_comm_size, simu_comm_size, local_vect_sizes_simu);
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


	~Field()
	{
		delete [] local_vect_sizes_simu;
		delete [] ensemble_members;
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

const Task NO_WORK = Task{-1,-1};

void delete_array(void * data, void * hint)
{
	delete [] data;
}

struct ConnectedSimulationRank {
	// task id (to keep them in order)
	map<int, Task> waiting_tasks;

	Task current_task = NO_WORK;

	void * connection_identity = NULL;

	~ConnectedSimulationRank() {
		if (connection_identity != NULL) {
			free(connection_identity);
		}
	}


	/// returns true if a new was sent.
	bool try_to_start_task(int simu_rank) {

		// already working?
		if (current_task != NO_WORK)
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

		zmq_msg_init_data(&identity_msg, connection_identity, IDENTITY_SIZE, delete_array, NULL);
		zmq_msg_send(&identity_msg, data_response_socket, ZMQ_SNDMORE);

		//zmq_msg_init_size(&empty_msg, 0);
		zmq_msg_init(&empty_msg);
		zmq_msg_send(&empty_msg, data_response_socket, ZMQ_SNDMORE);

		ZMQ_CHECK(zmq_msg_init_data(&header_msg, header, 3 * sizeof(int), NULL, NULL));
		zmq_msg_send(&header_msg, data_response_socket, ZMQ_SNDMORE);
		// we do not know when it will really send. send is non blocking!

		zmq_msg_init_data(&data_msg, reinterpret_cast<void*>(fields.begin()->second->ensemble_members[first_elem->second.state_id].state_analysis),
				fields.begin()->second->getPart(simu_rank).send_count * sizeof(double), NULL, NULL);

		D("Server sending %d bytes for state %d, timestamp=%d",
				fields.begin()->second->getPart(simu_rank).send_count * sizeof(double),
				header[0], header[1]);

		zmq_msg_send(&data_msg, data_response_socket, 0);

		// TODO: don't close messages after send! see other send's too..
//		zmq_msg_close(&identity_msg);
//		zmq_msg_close(&empty_msg);
//		zmq_msg_close(&header_msg);
//		zmq_msg_close(&data_msg);

		current_task = first_elem->second;
		waiting_tasks.erase(first_elem);
		// close connection:
		connection_identity = NULL;

		return true;
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
		assert(buf[1] > 0);  // simu_id  > 0 TODO: why? can't i delete this line?
		// we add the simulation if we first receive data from it.
		zmq_msg_close(&msg);

		zmq_msg_init_size(&msg, sizeof(int));
		buf = reinterpret_cast<int*>(zmq_msg_data(&msg));
		*buf = comm_size;
		zmq_msg_send(&msg, configuration_socket, ZMQ_SNDMORE);
		zmq_msg_close(&msg);

		zmq_msg_init_data(&msg, data_response_port_names,
				comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(char), NULL, NULL);
		zmq_msg_send(&msg, configuration_socket, 0);
		zmq_msg_close(&msg);
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

		assert_more_zmq_messages(configuration_socket);
    zmq_msg_init(&msg);
		zmq_msg_recv(&msg, configuration_socket, 0);
		assert(zmq_msg_size(&msg) == simu_comm_size * sizeof(int));
		memcpy (newField->local_vect_sizes_simu, zmq_msg_data(&msg), simu_comm_size * sizeof(int));
		zmq_msg_close(&msg);

		// ack
		zmq_msg_init(&msg);
		zmq_msg_send(&msg, configuration_socket, 0);
		zmq_msg_close(&msg);

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
			MPI_Bcast(&field_it->second->simu_comm_size, 1, MPI_INT, 0, MPI_COMM_WORLD);  // 2:simu_comm_size
			MPI_Bcast(&field_it->second->local_vect_sizes_simu, field_it->second->simu_comm_size,
					MPI_INT, 0, MPI_COMM_WORLD);                                             // 3:local_vect_sizes_simu

			field_it->second->calculate_parts(comm_size);
			field_it++;
		} else {
			int simu_comm_size;
			MPI_Bcast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);  // 1: fieldname
			MPI_Bcast(&simu_comm_size, 1, MPI_INT, 0, MPI_COMM_WORLD);                   // 2:simu_comm_size

			Field * newField = new Field(simu_comm_size, ENSEMBLE_SIZE);

			MPI_Bcast(newField->local_vect_sizes_simu, simu_comm_size,
					MPI_INT, 0, MPI_COMM_WORLD);                                              // 3:local_vect_sizes_simu

			newField->calculate_parts(comm_size);

			fields.emplace(field_name, newField);
		}
	}
}

vector<int> unscheduled_tasks;
// returns true if it scheduled a new task for the given simuid
bool create_new_task(int simuid)
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

void init_new_timestamp()
{
	current_timestamp++;
	if (comm_rank == 0) {
		assert(unscheduled_tasks.size() == 0);
		for (int i = 0; i < ENSEMBLE_SIZE; i++) {
			unscheduled_tasks.push_back(i);
			for (auto field_it = fields.begin(); field_it != fields.end(); field_it++)
			{
				auto &member = field_it->second->ensemble_members[i];
				assert(member.received_state_background = member.size);
				member.received_state_background = 0;
			}
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
}

int main(int argc, char * argv[])
{
	int major, minor, patch;
	zmq_version (&major, &minor, &patch);
	D("Current 0MQ version is %d.%d.%d\n", major, minor, patch);
	MPI_Init(NULL, NULL);
	context = zmq_ctx_new ();

	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

	// Get the rank of the process
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
	void * configuration_socket = NULL;

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
	melissa_get_node_name(hostname);
	sprintf(data_response_port_name, "tcp://%s:%d", hostname, 5000+comm_rank);

	char data_response_port_names[MPI_MAX_PROCESSOR_NAME * comm_size];
	MPI_Gather(data_response_port_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
			data_response_port_names, MPI_MAX_PROCESSOR_NAME * comm_size, MPI_CHAR, 0, MPI_COMM_WORLD);

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
			ZMQ_CHECK(zmq_poll (items, 1, -1));
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
			zmq_msg_t identity_msg, empty_msg, header_msg, data_msg;
			zmq_msg_init(&identity_msg);
			zmq_msg_init(&empty_msg);
			zmq_msg_init(&header_msg);
			zmq_msg_init(&data_msg);

			zmq_msg_recv(&identity_msg, data_response_socket, 0);
			//uint32_t routing_id = zmq_msg_routing_id (&identity_msg); assert (routing_id);

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

			if (simu_timestamp == current_timestamp)
			{
				fields[field_name]->ensemble_members[simu_state_id].store_background_state_part(part.local_offset_server,
						reinterpret_cast<double*>(zmq_msg_data(&data_msg)), part.send_count);
			}


			// Save connection - basically copy identity pointer...
			simu.connected_simulation_ranks[simu_rank].connection_identity = identity;
			simu.connected_simulation_ranks[simu_rank].current_task = NO_WORK;

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
					create_new_task(simu_id);

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
				for (int i = 0; i != field_it->second->ensemble_size; i++)
				{
					finished = field_it->second->ensemble_members[i].finished();
					if (!finished)
						break;
				}
				if (!finished)
					break;
			}

			if (finished)
			{
				do_update_step();
				init_new_timestamp();
				// After update step: loop over all simu_id's sending them a new state vector part they have to propagate.
				for (auto simu_it = simulations.begin(); simu_it != simulations.end(); simu_it++)
				{
					create_new_task(simu_it->first);
				}
			}
		}


		// TODO: check ret values!
		// TODO: remove compile warnings!		zmq_msg_close(&data_msg);

		if (phase == PHASE_SIMULATION && comm_rank != 0)
		{
			// Check if I have to schedule new tasks:
			int received;

			MPI_Iprobe(0, TAG_NEW_TASK, MPI_COMM_WORLD, &received, MPI_STATUS_IGNORE);
			if (received)
			{
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

	// TODO: check if we need to delete some more stuff!
	zmq_ctx_destroy(context);
	MPI_Finalize();
}
