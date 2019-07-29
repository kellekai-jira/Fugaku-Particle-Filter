#include <map>
#include <string>
#include <cstdlib>
#include <cassert>
#include <memory>

#include <mpi.h>
#include "zmq.h"

#include "../common/messages.h"
#include "../common/n_to_m.h"

#include "../common/utils.h"

#include <csignal>

// TODO ensure sizeof(size_t is the same on server and api... also for other types?? but the asserts are doing this already at the beginning as we receive exactly 2 ints....
// Forward declarations:
void melissa_finalize();

using namespace std;

// zmq context:
void *context;

const int getSimuId() {
  return atoi(getenv("MELISSA_SIMU_ID"));
}

/// Communicator used for simulation
MPI_Comm comm;


/// if node name = my nodename, replace by localhost!
string fix_port_name(const char * port_name_)
{
	string port_name(port_name_);
	char my_port_name[MPI_MAX_PROCESSOR_NAME];
	melissa_get_node_name(my_port_name, MPI_MAX_PROCESSOR_NAME);
	size_t found = port_name.find(my_port_name);
	// check if found and if hostname is between tcp://<nodename>:port
	if (found != string::npos && port_name[found-1] == '/' && port_name[found + strlen(my_port_name)] == ':') {
		port_name = port_name.substr(0, found) + "localhost" + port_name.substr(found + strlen(my_port_name));
	}
	return port_name;
}

/**
 * Returns simulation's rank
 */
int getCommRank()
{
  int rank;
  MPI_Comm_rank(comm, &rank);
  return rank;
}

/**
 * Returns Simulations comm size
 */
int getCommSize()
{
  int size;
  MPI_Comm_size(comm, &size);
  return size;
}


struct ServerRankConnection
{
  void * data_request_socket;


  ServerRankConnection(const char * addr_request)
  {
    data_request_socket = zmq_socket (context, ZMQ_REQ);
    assert(data_request_socket);
    string cleaned_addr = fix_port_name(addr_request);
    D("Data Request Connection to %s", cleaned_addr.c_str());
    //ZMQ_CHECK();
    int ret = zmq_connect (data_request_socket, cleaned_addr.c_str());
    assert(ret == 0);

    D("connect socket %p", data_request_socket);
  }

  ~ServerRankConnection()
  {
    D("closing socket %p", data_request_socket);
    zmq_close(data_request_socket);
  }

  void send(double * values, const size_t doubles_to_send, const int current_state_id, const int timestamp, const char * field_name)
  {
    // send simuid, rank, stateid, timestamp, field_name next message: doubles
    zmq_msg_t msg_header, msg_data;
    zmq_msg_init_size(&msg_header, 4 * sizeof(int) + MPI_MAX_PROCESSOR_NAME * sizeof(char));
    int * header = reinterpret_cast<int*>(zmq_msg_data(&msg_header));
    header[0] = getSimuId();
    header[1] = getCommRank();
    header[2] = current_state_id;
    header[3] = timestamp;  // is incremented on the server side
    strcpy(reinterpret_cast<char*>(&header[4]), field_name);
    D("sending on socket %p", data_request_socket);
    ZMQ_CHECK(zmq_msg_send(&msg_header, data_request_socket, ZMQ_SNDMORE));

    D("-> Simulation simuid %d, rank %d sending statid %d timestamp=%d fieldname=%s, %lu bytes",
    		getSimuId(), getCommRank(), current_state_id, timestamp, field_name, doubles_to_send * sizeof(double));
    D("values[0]  = %.3f", values[0]);
    D("values[1]  = %.3f", values[1]);
    D("values[5]  = %.3f", values[5]);
    D("values[35] = %.3f", values[35]);
    zmq_msg_init_data(&msg_data, values, doubles_to_send * sizeof(double), NULL, NULL);
    ZMQ_CHECK(zmq_msg_send(&msg_data, data_request_socket, 0));
  }

  void receive(double * out_values, size_t doubles_expected, int * out_current_state_id, int *out_timestamp)
  {
// receive a first message that is 1 if we want to change the state, otherwise 0 or 2 if we want to quit.
// the first message also contains out_current_state_id and out_timestamp
// the 2nd message just consists of doubles that will be put into out_values
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    zmq_msg_recv(&msg, data_request_socket, 0);
    D("Received message size = %lu", zmq_msg_size(&msg));
    assert(zmq_msg_size(&msg) == 3 * sizeof(int));
    int * buf = reinterpret_cast<int*>(zmq_msg_data(&msg));
    *out_current_state_id = buf[0];
    *out_timestamp = buf[1];
    int type = buf[2];
    zmq_msg_close(&msg);

    if (type == CHANGE_STATE)
    {
      assert_more_zmq_messages(data_request_socket);

      // zero copy is for sending only!
      zmq_msg_init(&msg);

      zmq_msg_recv(&msg, data_request_socket, 0);

      D("<- Simulation got %lu bytes, expected %lu bytes... for state %d, timestamp=%d",
      		zmq_msg_size(&msg), doubles_expected * sizeof(double), *out_current_state_id, *out_timestamp);

      assert(zmq_msg_size(&msg) == doubles_expected * sizeof(double));

      double * buf = reinterpret_cast<double*>(zmq_msg_data(&msg));
      copy(buf, buf + doubles_expected, out_values);

      print_vector(vector<double>(out_values, out_values + doubles_expected));
      zmq_msg_close(&msg);
      assert_no_more_zmq_messages(data_request_socket);
    }
    else if (type == END_SIMULATION)
    { // TODO use zmq cpp for less errors!
      melissa_finalize();
    }
    else
    {
      assert(type == KEEP_STATE);
      // TODO: unimplemented
      assert(false);
    }
  }
};



struct Server
{
  int comm_size = 0;
  vector<char> port_names;
};
Server server;

struct ServerRanks {
	static map<int, unique_ptr<ServerRankConnection>> ranks;

	static ServerRankConnection &get(int server_rank)
	{
		auto found = ranks.find(server_rank);
		if (found == ranks.end())
		{
			// connect to this server rank
			// we use unique_ptr's as other wise we would create a ServerRank locally, we than would copy all its values in the ranks map
			// and then we would destroy it. unfortunately this also closes the zmq connection !
			auto res = ranks.emplace(server_rank,
					unique_ptr<ServerRankConnection>(new ServerRankConnection(server.port_names.data() + server_rank * MPI_MAX_PROCESSOR_NAME)));
			return *res.first->second;
		}
		else
		{
			return *(found->second);
		}
	}
};
map<int, unique_ptr<ServerRankConnection>> ServerRanks::ranks;

struct ConnectedServerRank {
  size_t send_count;
  size_t local_vector_offset;
  ServerRankConnection &server_rank;
};

struct Field {
  int current_state_id;
  int timestamp;
  size_t local_vect_size;
  vector<ConnectedServerRank> connected_server_ranks;
  void initConnections(const vector<size_t> &local_vect_sizes) {
    vector<n_to_m> parts = calculate_n_to_m(server.comm_size, local_vect_sizes);
    for (auto part=parts.begin(); part != parts.end(); ++part)
    {
      if (part->rank_simu == getCommRank())
      {
        connected_server_ranks.push_back({part->send_count, part->local_offset_simu, ServerRanks::get(part->rank_server)});
      }
    }
  }

  // TODO: this will crash if there are more than two fields? maybe use dealer socket that supports send send recv recv scheme.
  void putState(double * values, const char * field_name) {
    // send every state part to the right server rank
    for (auto csr = connected_server_ranks.begin(); csr != connected_server_ranks.end(); ++csr) {
    	D("put state, local offset: %lu, send count: %lu", csr->local_vector_offset, csr->send_count);
      csr->server_rank.send(&values[csr->local_vector_offset], csr->send_count, current_state_id, timestamp, field_name);
    }

    current_state_id = -1;

  }

  void getState(double * values) {
    for (auto csr = connected_server_ranks.begin(); csr != connected_server_ranks.end(); ++csr) {
      // receive state parts from every serverrank.
    	D("get state, local offset: %lu, send count: %lu", csr->local_vector_offset, csr->send_count);

    	// do not try to receive if we are finalizeing already. Even check if the last receive might have started finalization.
    	if (phase == PHASE_FINAL)
    	{
    		return;
    	}

      csr->server_rank.receive(&values[csr->local_vector_offset], csr->send_count, &current_state_id, &timestamp);
    }
  }
};


map<string, Field> fields;


// TODO: kill if no server response for a timeout...

struct ConfigurationConnection
{
  void * socket;
  ConfigurationConnection()
  {
    socket = zmq_socket (context, ZMQ_REQ);
    string port_name = fix_port_name(getenv("MELISSA_SERVER_MASTER_NODE"));
    D("Configuration Connection to %s", port_name.c_str());
    zmq_connect (socket, port_name.c_str());
  }

  void register_simu_id(Server * out_server)
  {
    zmq_msg_t msg_request, msg_reply;
    zmq_msg_init_size(&msg_request, sizeof(int) + sizeof(int));
    int * header = reinterpret_cast<int*>(zmq_msg_data(&msg_request));
    header[0] = REGISTER_SIMU_ID;
    header[1] = getSimuId();
    ZMQ_CHECK(zmq_msg_send(&msg_request, socket, 0));

    zmq_msg_init(&msg_reply);
    zmq_msg_recv(&msg_reply, socket, 0);
    assert(zmq_msg_size(&msg_reply) == sizeof(int));
    memcpy(&out_server->comm_size, zmq_msg_data(&msg_reply), sizeof(int));
    zmq_msg_close(&msg_reply);

    size_t port_names_size = out_server->comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(char);

    assert_more_zmq_messages(socket);
    zmq_msg_init(&msg_reply);
    zmq_msg_recv(&msg_reply, socket, 0);

    assert(zmq_msg_size(&msg_reply) == port_names_size);

    out_server->port_names.resize(port_names_size);

    copy(reinterpret_cast<char*>(zmq_msg_data(&msg_reply)),
    		reinterpret_cast<char*>(zmq_msg_data(&msg_reply)) + port_names_size, out_server->port_names.begin());

    zmq_msg_close(&msg_reply);
  }

  // TODO: high water mark and so on?
  void register_field(const char * field_name, size_t local_vect_sizes[])
  {
    zmq_msg_t msg_header, msg_local_vect_sizes, msg_reply;
    zmq_msg_init_size(&msg_header, sizeof(int) + sizeof(int) + MPI_MAX_PROCESSOR_NAME * sizeof(char) );
    int * header = reinterpret_cast<int*>(zmq_msg_data(&msg_header));
    int type = REGISTER_FIELD;
    header[0] = type;
    header[1] = getCommSize();
    strcpy(reinterpret_cast<char*>(&header[2]), field_name);
    ZMQ_CHECK(zmq_msg_send(&msg_header, socket, ZMQ_SNDMORE));

    zmq_msg_init_data(&msg_local_vect_sizes, local_vect_sizes, getCommSize() * sizeof(size_t), NULL, NULL);
    ZMQ_CHECK(zmq_msg_send(&msg_local_vect_sizes, socket, 0));

    zmq_msg_init(&msg_reply);
    zmq_msg_recv(&msg_reply, socket, 0);
    // ack
    assert(zmq_msg_size(&msg_reply) == 0);
    zmq_msg_close(&msg_reply);
  }


  ~ConfigurationConnection() {
    zmq_close(socket);
  }


};
ConfigurationConnection *ccon = NULL;

void first_melissa_init(MPI_Comm comm_)
{
  context = zmq_ctx_new ();
  comm = comm_;
  if (getCommRank() == 0) {
    ccon = new ConfigurationConnection();
    ccon->register_simu_id(&server);
  }
  MPI_Bcast(&server.comm_size, 1, MPI_INT, 0, comm);
  if (getCommRank() != 0) {
  	server.port_names.resize(server.comm_size * MPI_MAX_PROCESSOR_NAME);
  }
  D("port_names_size= %lu", server.port_names.size());
  MPI_Bcast(server.port_names.data(), server.comm_size * MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, comm);

  //D("Portnames %s , %s ", server.port_names.data(), server.port_names.data() + MPI_MAX_PROCESSOR_NAME);
}

int melissa_get_current_state_id()
{
	assert(phase == PHASE_SIMULATION);
	return fields.begin()->second.current_state_id;
}

void melissa_init(const char *field_name,
                       const int  local_vect_size,
                       MPI_Comm comm_)
{
  // We do not allow multiple fiels:
  assert(fields.size() == 0);

  static bool is_first_melissa_init = true;

  if (is_first_melissa_init) {
    // register this simulation id
    first_melissa_init(comm_);
    is_first_melissa_init = false;
  }


  // create field
  Field newField;
  //newField.current_state_id = getSimuId(); // We are beginning like this...
  newField.current_state_id = -1; // We are beginning like this...
  newField.timestamp = 0;
  newField.local_vect_size = local_vect_size;
  vector<size_t> local_vect_sizes(getCommSize());
    // synchronize local_vect_sizes and
  MPI_Allgather(&newField.local_vect_size, 1, my_MPI_SIZE_T,
      local_vect_sizes.data(), 1, my_MPI_SIZE_T,
      comm);

  D("vect sizes: %lu %lu", local_vect_sizes[0], local_vect_sizes[1]);

  if (getCommRank() == 0 && getSimuId() == 0)  // TODO: what happens if simu_id 0 crashes? make this not dependend from the simuid. the server can ask the simulation after it's registration to give field infos!
  {
    // Tell the server which kind of data he has to expect
    ccon->register_field(field_name, local_vect_sizes.data());
  }

  // Calculate to which server ports the local part of the field will connect
  newField.initConnections(local_vect_sizes);

  fields.emplace(string(field_name), newField);
}


/// returns false if simulation should end now.
bool melissa_expose(const char *field_name, double *values)
{
  assert(phase != PHASE_FINAL);
  if (phase == PHASE_INIT) {
    phase = PHASE_SIMULATION;
    // finalize initializations
    // First time in melissa_expose.
    // Now we are sure all fields are registered.
    // we do not need this anymore:
    delete ccon;
    ccon = NULL;
  }

  // Now Send data to the melissa server
  fields[field_name].putState(values, field_name);
  // and request new data
  fields[field_name].getState(values);

  // TODO: this will block other fields!

  return phase != PHASE_FINAL;
}

void melissa_finalize()  // TODO: when using more serverranks, wait until an end message was received from every before really ending... or actually not. as we always have only an open connection to one server rank...
{
  //sleep(3);
	MPI_Barrier(comm);
  //sleep(3);
	D("End Simulation.");
  D("server ranks: %lu", ServerRanks::ranks.size());

  phase = PHASE_FINAL;
  // TODO: free all pointers?
  // not an api function anymore but activated if a finalization message is received.
  if (ccon != NULL) {
  	delete ccon;
  }


  // free all connections to server ranks:
  ServerRanks::ranks.clear();

  //sleep(3);
  D("Destroying zmq context");
  zmq_ctx_destroy(context);
}

int melissa_get_current_timestamp()
{
	assert(phase == PHASE_SIMULATION);
	return fields.begin()->second.timestamp;
}
