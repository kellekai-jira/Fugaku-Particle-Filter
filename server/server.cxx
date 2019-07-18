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


using namespace std;

struct Part
{
  int rank_simu;
  int local_vector_offset;
  int send_count

}

struct EnsembleMember
{
  double state_analysis[];
  double state_background[];
  int size;
  EnsembleMember(int size_)
  {
    size = size_;

    // TODO: replace all mallocs by new?
    state_analysis = new double[size];
    state_background = new double[size];
  }

  ~EnsembleMember()
  {
    free(state_analysis);
    free(state_background);
  }
}

struct Field {
// index: state id.
  vector<EnsembleMember> ensemble_members

  int local_vect_sizes_simu[];
  int simu_comm_size;
  vector<n_to_m> parts;

  Field(int simu_comm_size)
  {
    simu_comm_size = simu_comm_size_;
    local_vect_sizes_simu = new int[simu_comm_size];
  }

  // TODO: naming: server_comm size or ranks_server? same for simu!
  /// Calculates all the state vector parts that are send between the server and the
  /// simulations
  void calculate_parts(int server_comm_size)
  {
    parts = calculate_n_to_m(server_comm_size, simu_comm_size, local_vect_sizes_simu);
  }


  ~Field()
  {
    delete [] local_vect_sizes_simu;
  }

  void addEnsembleMember(int comm_size_simu)
  {
    ensemble_members.
  }
};

map<string, Field*> fields;

struct ConnectedSimulationRank {
  int simu_rank;
  void * connection_identy = NULL;
  ~ConnectedSimulationRank() {
    if (connection_identy != NULL) {
      free(connection_identy);
    }
  // TODO: can we put the send function in here as well?
}

struct Simulation  // Model process runner
{
  int simu_state; // -1: doing nothing. Number i: propagating state i.
  map<ConnectedSimulationRank> connected_simulation_ranks; // TODO: rename in SimulationRankConnection, also on server side?

  Simulation() {
    simu_state = -1;
  }
};

// simu_id, Simulation:
map<int, Simulation> simulations;


// Server comm_size and Server rank
int comm_size;
int rank;

void answer_configuration_message(void * configuration_socket, char* data_response_port_names)
{
  zmq_msg_t msg;
  zmq_msg_recv(configuration_socket, &msg, 0);
  int * buf = zmq_msg_data(&msg);
  if (buf[0] == REGISTER_SIMU_ID) {
    // Register Simu ID
    assert(zmq_msg_size(&msg) == 2 * sizeof(int));
    assert(buf[1] > 0);
    simulations.emplace(buf[1], Simulation());
    zmq_msg_close(&msg);

    zmq_msg_init_size(&msg, sizeof(int));
    buf = zmq_msg_data(&msg);
    *buf = comm_size;
    zmq_msg_send(configuration_socket, &msg, ZMQ_SNDMORE);
    zmq_msg_close(&msg);

    zmq_msg_init_data(&msg, data_response_port_names,
        comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(char), NULL, NULL);
    zmq_msg_send(configuration_socket, &msg, 0);
    zmq_msg_close(&msg);
  }
  else if (buf[0] == REGISTER_FIELD)
  {
    assert(phase == PHASE_INIT);  // we accept new fields only if in initialization phase.
    assert(zmq_msg_size(&msg) == sizeof(int) + sizeof(int) + MPI_MAX_PROCESSOR_NAME * sizeof(char));

    int simu_comm_size = buf[1];

    Field *newField = new Field(simu_comm_size);

    const char field_name[MPI_MAX_PROCESSOR_NAME];
    stcpy(field_name, buf[2]);
    zmq_msg_close(&msg);

    assert_more_zmq_messages(data_request_socket);
    zmq_msg_recv(configuration_socket, &msg);
    assert(zmq_msg_size(&msg) == simu_comm_size * sizeof(int));
    memcpy (newField->local_vect_sizes_simu, zmq_msg_data(&msg), simu_comm_size * sizeof(int));
    zmq_msg_close(&msg);

    fields.emplace(field_name, newField);
    parts = calculate_n_to_m(comm_size,
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
  MPI_BCast(&field_count, 1, MPI_INT, 0, MPI_COMM_WORLD);                          // 0:field_count
  auto field_it = fields.begin()
  for (int i = 0; i < field_count; i++)
  {
    char field_name[MPI_MAX_PROCESSOR_NAME];
    if (rank == 0) {
      strcpy(field_name, field_it->first.c_char());
      MPI_BCast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);  // 1:fieldname
      MPI_BCast(&field_it->second.simu_comm_size, 1, MPI_INT, 0, MPI_COMM_WORLD);  // 2:simu_comm_size
      MPI_BCast(&field_it->second.local_vect_sizes_simu, field_it->second.simu_comm_size,
          MPI_INT, 0, MPI_COMM_WORLD);                                             // 3:local_vect_sizes_simu

      field_it->second.calculate_parts(comm_size);
      field_it++;
    } else {
      int simu_comm_size;
      MPI_BCast(field_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);  // 1: fieldname
      MPI_BCast(&simu_comm_size, 1, MPI_INT, 0, MPI_COMM_WORLD);                   // 2:simu_comm_size

      Field * newField = new Field(simu_comm_size);

      MPI_BCast(newField->local_vect_sizes_simu, simu_comm_size,
          MPI_INT, 0, MPI_COMM_WORLD);                                              // 3:local_vect_sizes_simu

      newField.calculate_parts(comm_size);

      fields.emplace(field_name, newField);
    }
  }

  // generate parts:

}

/// Finds the part of the field with the specified simu_rank.
n_to_m & getPart(const char * field_name, int simu_rank)
{
  for (auto part_it = fields[field_name]->parts.begin(); part_it < fields[field_name]->parts.end(); part_it++)
  {
    if (part_it->rank_server == rank && part_it->rank_simu == simu_rank) {
      return *part_it;
    }
  }
  assert(false); // Did not find the part!
}


void * context;
int main(int argc, char * argv[])
{
  MPI_Init(NULL, NULL)
  context = zmq_ctx_new ();

  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

  // Get the rank of the process
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  void * configuration_socket;

  // Start sockets:
  if (rank == 0)
  {
    configuration_socket = zmq_socket(context, ZMQ_REP);
    zmq_bind(configuration_socket, "tcp://*:4000");

  }

  void * data_response_socket = zmq_socket(context, ZMQ_ROUTER);
  char data_response_port_name[MPI_MAX_PROCESSOR_NAME];
  sprintf(data_response_port_name, "tcp://*:%d", 5000+rank);
  zmq_bind(data_response_socket, data_response_port_name);

  char hostname[MPI_MAX_PROCESSOR_NAME];
  melissa_get_node_name(hostname);
  sprintf(data_response_port_name, "tcp://%s:%d", hostname, 5000+rank);

  char data_response_port_names[MPI_MAX_PROCESSOR_NAME * comm_size];
  MPI_Gather(data_response_port_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
      data_response_port_names, MPI_MAX_PROCESSOR_NAME * comm_size, MPI_CHAR, 0, MPI_COMM_WORLD);

  zmq_pollitem_t items [2];
  items[0].socket = data_response_socket;
  items[0].events = ZMQ_POLLIN;
  if (rank == 0)
  {
    items[1].socket = configuration_socket;
    items[1].events = ZMQ_POLLIN;
  }
  /* Poll for events indefinitely */
  int rc = zmq_poll (items, 2, -1);
  assert (rc >= 0); /* Returned events will be stored in items[].revents */
  while (true)
  {
    // Wait for requests
    // answer them

    if (rank == 0 && (items[1].revents & ZMQ_POLLIN))
    {
      answer_configuration_message(configuration_socket);
    }

    if (phase == PHASE_INIT)
    {
        if (rank == 0)
        {
          // check if initialization finished

          if (simu_state.size() == 1 && fields.size() == 1)
          {
            // TODO: check fields!
            // propagate all fields to the other server clients on first message receive!
            broadcast_field_information_and_calculate_parts();
            phase = PHASE_SIMULATION;
          }
        }
        else
        {
          // Wait for rank 0 to finish field registrations. rank 0 does this in answer_configu
          // propagate all fields to the other server clients on first message receive!
          broadcast_field_information_and_calculate_parts();
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

      zmq_msg_recv(data_response_socket, &identity_msg);

      assert_more_zmq_messages(data_response_socket);
      zmq_msg_recv(data_response_socket, &empty_msg);
      assert_more_zmq_messages(data_response_socket);
      zmq_msg_recv(data_response_socket, &header_msg);
      assert_more_zmq_messages(data_response_socket);
      zmq_msg_recv(data_response_socket, &data_msg);
      assert_more_zmq_messages(data_response_socket);

      void * identity = malloc(zmq_msg_size(&identity_msg));
      memcpy(identity, zmq_msg_data(&identity_msg), zmq_msg_size(&identity_msg));

      assert(zmq_msg_size(&header_msg) == 4 * sizeof(int));
      int * header_buf = zmq_msg_data(&header_msg);
      int simu_id = header_buf[0];
      int simu_rank = header_buf[1];
      int simu_state_id = header_buf[2];
      int simu_timestamp = header_buf[3];
      char field_name[MPI_MAX_PROCESSOR_NAME];
      strcpy(field_name, header_buf[4]);

      assert(zmq_msg_size(&data_msg) == getPart(field_name, simu_rank).send_counts * sizeof(double));
      memcpy(, zmq_msg_data(&data_msg), zmq_msg_size(&data_msg));



      // Save connection in open connections
      basically copy identity pointer...
      if (simulations[
      simulations.emplace(
      // Save state part in background_states.
      // in simulations data structure save that this sim has a free worker [optional?]
      // check if all data was received. If yes: start Update step to calculate next analysis state
      // After update step: loop over all simu_id's sending them a new state vector part they have to propagate.
      //
      //
      // if not: ask rank 0 which state id to send to which simu id next. [TODO, atm we do not do this as every simuid propagates only one state. (MPI_BCast),
      // TODO: check ret values!
      // TODO: remove compile warnings!
      zmq_msg_close(&identity_msg);
      zmq_msg_close(&empty_msg);
      zmq_msg_close(&header_msg);
      zmq_msg_close(&data_msg);
    }


  }


  for (int // TODO: clear fields!
  zmq_ctx_destroy(context);
  MPI_Finalize();
  return 0;
}
