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


using namespace std;

// zmq context:
void *context;

const char * getSimuId() {
  return getenv("MELISSA_SIMU_ID");
}


/// Communicator used for simulation
MPI_Comm comm;

/**
 * Returns simulation's rank
 */
int getRank()
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

struct ServerRank
{
  void * data_request_connection;

  ServerRank(const char * addr_request)
  {
    data_request_connection = zmq_socket (context, ZMQ_REQ);
    zmq_connect (data_request_connection, addr_request);
  }

  ~ServerRank()
  {
    zmq_close(data_request_connection);
  }

  void send(double * values, size_t doubles_to_send, int current_state_id, int timestamp)
  {
    // send simuid, rank, stateid, timestamp, next message: doubles
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, 4*sizeof(int));
    int * buf = zmq_msg_data(&msg);
    buf[0] = getSimuId();
    buf[1] = getRank();
    buf[2] = current_state_id;
    buf[3] = timestamp; // TODO: is incremented on the server or client side
    zmq_msg_send(data_request_connection, &msg, ZMQ_SNDMORE);
    zmq_msg_close(&msg);

    zmq_msg_init_data(&msg, values, doubles_to_send * sizeof(double), NULL, NULL);
    zmq_msg_send(data_request_connection, &msg, 0);
    zmq_msg_close(&msg);
  }

  void receive(double * out_values, size_t doubles_expected, int * out_current_state_id, int *out_timestamp)
  {
// receive a first message that is 1 if we want to change the state, otherwise 0
// the first message also contains out_current_state_id and out_timestamp
// the 2nd message just consists of doubles that will be put into out_values

    zmq_msg_init(&msg);
    zmq_msg_recv(data_request_connection, &msg, 0);
    assert(zmq_msg_size(&msg) == 3 * sizeof(int));
    int * buf = zmq_msg_data(&msg);
    *out_current_state_id = buf[0];
    *out_timestamp = buf[1];
    DataMessageType type = buf[2];
    zmq_msg_close(&msg);

    if (type == CHANGE_STATE)
    {
      int more;
      size_t more_size = sizeof (more);
      zmq_getsockopt (data_request_connection, ZMQ_RCVMORE, &more, &more_size);
      assert(more);
      zmq_msg_init_data(&msg, out_values, doubles_expected * sizeof(double), NULL, NULL);
      zmq_msg_recv(data_request_connection, &msg, 0);
      assert(zmq_msg_size(&msg) == doubles_expected * sizeof(double));
      zmq_msg_close(&msg);
    }
    else if (type == QUIT)
    {
      melissa_finalize();
    }
    else
    {
      assert(type == KEEP_STATE);
    }
  }
};


// TODO: there is probably a better data type for this.
vector<ServerRank*> server_ranks;

struct ConnectedServerRank {
  int send_count;
  int local_vector_offset;
  ServerRank *server_rank;
};

struct Server
{
  int comm_size = 0;
  char * port_names = NULL;

  ~Server() {
    if (port_names != NULL)
      free(port_names);
  }
};
Server server;

struct Field {
  int current_state_id;
  int timestamp;
  int local_vect_size;
  vector<ConnectedServerRank> connected_server_ranks;
  void initConnections(int local_vect_sizes[]) {
    vector<n_to_m> parts = calculate_n_to_m(server.comm_size, getCommSize(), local_vect_sizes);
    for (auto part=parts.begin(); part != parts.end(); ++part)
    {
      if (part->rank_simu == getRank())
      {
        connected_server_ranks.push_back({part->send_count, part->local_offset_simu, server_ranks[part->rank_server]});
      }
    }
  }

  // TODO: this will crash if there are more than two fields? maybe use dealer socket that supports send send recv recv scheme.
  void putState(double * values) {
    // send every state part to the right server rank
    for (auto csr = connected_server_ranks.begin(); csr != connected_server_ranks.end(); ++csr) {
      csr->server_rank->send(&values[csr->local_vector_offset], csr->send_count, current_state_id, timestamp);
    }
  }

  void getState(double * values) {
    for (auto csr = connected_server_ranks.begin(); csr != connected_server_ranks.end(); ++csr) {
      // receive state parts from every serverrank.
      csr->server_rank->receive(&values[csr->local_vector_offset], csr->send_count, &current_state_id, &timestamp);
    }
  }
};

enum Phase {
  PHASE_INIT,
  PHASE_SIMULATION,
  PHASE_FINAL
};

Phase phase = PHASE_INIT;

map<string, Field> fields;




struct ConfigurationConnection
{
  void * socket;
  ConfigurationConnection()
  {
    socket = zmq_socket (context, ZMQ_REQ);
    const char * port_name = getenv("MELISSA_SERVER_MASTER_NODE");
    zmq_connect (socket, port_name);
  }

  void register_simu_id(Server * out_server)
  {
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, sizeof(int) + sizeof(int));
    void * buf = zmq_msg_data(&msg);
    int type = REGISTER_SIMU_ID;
    memcpy(buf, &type, sizeof(int));
    buf += sizeof(ConfigurationMessageType);
    int simu_id = getSimuId();
    memcpy(buf, &simu_id, sizeof(int));
    zmq_msg_send(socket, &msg, 0);
    zmq_msg_close(&msg);


    zmq_msg_recv(socket, &msg, 0);
    assert(zmq_msg_size(&msg) == sizeof(int));
    memcpy(&out_server->comm_size, zmq_msg_data(&msg), sizeof(int));
    zmq_msg_close(&msg);

    size_t port_names_size = out_server->comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(char);

    int more;
    size_t more_size = sizeof (more);
    zmq_getsockopt (socket, ZMQ_RCVMORE, &more, &more_size);
    assert(more);
    zmq_msg_init(&msg);
    zmq_msg_recv(socket, &msg, 0);

    assert(zmq_msg_size(&msg) == port_names_size);

    out_server->port_names = malloc(port_names_size);
    memcpy(out_server->port_names, zmq_msg_data(&msg), port_names_size);

    zmq_msg_close(&msg);
  }

  // TODO: high water mark and so on?
  void register_field(const char * field_name, int local_vect_sizes[])
  {
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, sizeof(int) + MPI_MAX_PROCESSOR_NAME * sizeof(char) + sizeof(int));
    void * buf = zmq_msg_data(&msg);
    int type = REGISTER_FIELD;
    memcpy(buf, &type, sizeof(int));
    buf += sizeof(ConfigurationMessageType);
    strcpy(buf, field_name);
    buf += MPI_MAX_PROCESSOR_NAME * sizeof(char);
    int comm_size = getCommSize();
    memcpy(buf, &comm_size, sizeof(int));
    zmq_msg_send(socket, &msg, ZMQ_SNDMORE);
    zmq_msg_close(&msg);

    zmq_msg_init_data(&msg, local_vect_sizes, getCommSize() * sizeof(int), NULL, NULL);
    zmq_msg_send(socket, &msg, 0);
    zmq_msg_close(&msg);

    zmq_msg_init(&msg);
    zmq_msg_recv(socket, &msg, 0);
    // ack
    assert(zmq_msg_size(&msg) == 0);
    zmq_msg_close(&msg);
  }


  ~ConfigurationConnection() {
    zmq_close(socket);
  }


};
ConfigurationConnection *ccon;

void first_melissa_init(MPI_Comm comm_)
{
  context = zmq_ctx_new ();
  comm = comm_;
  if (getRank() == 0) {
    ccon = new ConfigurationConnection();
    ccon->register_simu_id(&server);
  }
  MPI_Bcast(&server.comm_size, 1, MPI_INT, 0, comm);
  MPI_Bcast(&server.port_names, server.comm_size * MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, comm);

  // Fill server ranks array...
  for (int i = 0; i < server.comm_size; ++i)
  {
    server_ranks.push_back(new ServerRank(server.port_names[i]));
  }
}

void melissa_init(const char *field_name,
                       const int  local_vect_size,
                       MPI_Comm comm_)
{
  static bool is_first_melissa_init = true;

  if (is_first_melissa_init) {
    // register this simulation id
    first_melissa_init(comm_);
    is_first_melissa_init = false;
  }


  // create field
  Field newField;
  newField.local_vect_size = local_vect_size;
  int local_vect_sizes[getCommSize()];
    // synchronize local_vect_sizes and
  MPI_Allgather(&newField.local_vect_size, 1, MPI_INT,
      local_vect_sizes, getCommSize(), MPI_INT,
      comm);
  if (getRank() == 0 && getSimuId() == 0)
  {
    // Tell the server which kind of data he has to expect
    ccon->register_field(field_name, local_vect_sizes);
  }

  // Calculate to which server ports the local part of the field will connect
  newField.initConnections(local_vect_size);

  fields.emplace(string(field_name), newField);
}

void melissa_expose(const char *field_name, double *values)
{
  assert(phase != PHASE_FINAL);
  if (phase == PHASE_INIT) {
    phase = PHASE_SIMULATION;
    // finalize initializations
    // First time in melissa_expose.
    // Now we are sure all fields are registered.
    // we do not need this anymore:
    delete ccon;
  }

  // Now Send data to the melissa server
  fields[field_name].putState(values);
  // and request new data
  fields[field_name].getState(values);
  // TODO: this will block other fields!
}


void melissa_finalize()
{
  // TODO: close all connections [should be DONE]
  // TODO: free all pointers?
  // not an api function anymore but activated if a finalization message is received.
  for (auto sr = server_ranks.begin(); sr < server_ranks.end(); sr++)
  {
    delete *sr;
  }

  // TODO change name from connections to sockets!

  zmq_ctx_destroy(context);
  exit(0);
}

