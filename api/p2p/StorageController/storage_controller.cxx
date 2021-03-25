#include "storage_controller.hpp"
#include "storage_controller_impl.hpp"

#include "utils.h"
#include <memory>
#include <numeric>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cmath>
#include <string>

#include "api_common.h"
// TODO error checking!

//======================================================================
//  STORAGE CONTROLLER INITIALIZATION
//======================================================================

// init MPI
// init IO

void StorageController::io_init( MpiController* mpi, IoController* io ) {

  m_mpi = mpi;
  m_io = io;
  m_io->register_callback( StorageController::callback );

  // heads dont return from init_io !!!
  m_io->init_io(m_mpi);

  m_io->init_core();

}

void StorageController::init( double capacity_dp, double state_size_dp ) {
  // this is only called by app cores
  size_t capacity = std::floor(capacity_dp);
  size_t state_size_proc = std::ceil(state_size_dp);

  // propagate state size to head
  m_io->send( &capacity, sizeof(size_t), IO_TAG_POST, IO_MSG_ONE );
  m_io->send( &state_size_proc, sizeof(size_t), IO_TAG_POST, IO_MSG_ALL );

  m_worker_thread = false;

}


void StorageController::fini() {
  printf("Finalizing storage controller\n");
  delete m_peer;
  int dummy[2];
  m_io->sendrecv( &dummy[0], &dummy[1], sizeof(int), sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
  m_mpi->barrier("fti_comm_world");
  m_io->fini();

  zmq_ctx_destroy(m_zmq_context);
}



//======================================================================
//  STATIC CALLBACK FUNCTION
//======================================================================

// Callback called in the FTI head loop:
void StorageController::callback() {

  static bool init = false;
  if( !init ) {

    storage.m_zmq_context = zmq_ctx_new();  // TODO: simplify context handling
    storage.server.init();
    storage.m_io->init_core();
    std::vector<std::string> files;
    storage.m_io->filelist_local( {0,1}, files );

    size_t capacity;
    storage.m_io->recv( &capacity, sizeof(size_t), IO_TAG_POST, IO_MSG_ONE );
    size_t size_proc[storage.m_io->m_dict_int["app_procs_node"]], size_node;
    storage.m_io->recv( size_proc, sizeof(size_t), IO_TAG_POST, IO_MSG_ALL );

    size_t state_size_node = 0;
    for(int i=0; i<storage.m_io->m_dict_int["app_procs_node"]; i++) {
      state_size_node += size_proc[i];
    }

    // in slots
    capacity = capacity / state_size_node;

    // 2 model checkpoints
    // 1 for sending to peer (needs memory copy of ckpt file)
    size_t minimum_storage_requirement = 3;

    IO_TRY( minimum_storage_requirement > capacity, false, "Insufficiant storage capacity!" );

    // 1 model checkpoints
    // 1 for sending to peer (needs memory copy of ckpt file)
    size_t minimum_storage_reservation = 2;

    capacity -= minimum_storage_reservation;

    size_t prefetch_capacity = capacity - minimum_storage_reservation;

    prefetch_capacity = (prefetch_capacity<STORAGE_MAX_PREFETCH) ? prefetch_capacity : STORAGE_MAX_PREFETCH;

    storage.state_pool.init( prefetch_capacity );

    storage.m_peer = new PeerController( storage.m_io, storage.m_zmq_context, storage.m_mpi );
    init = true;

    if(storage.m_io->m_dict_bool["master_global"]) {
      std::cout << "storage capacity [# slots]: " << capacity << std::endl;
      std::cout << "state size per node [Mb]: " << ((double)state_size_node)/(1024*1024) << std::endl;
      std::cout << "prefetch capacity [# slots]: " << storage.state_pool.capacity() << std::endl;
      std::cout << "free [# slots]: " << storage.state_pool.free() << std::endl;
    }

  }

  // EVENT
  // message from alice, sent after completion of the forecast
  // containing (1) forecast state id and (2) WEIGHT.
  IO_PROBE( IO_TAG_POST, storage.m_request_post() );

  // EVENT
  // Alice cannot find the state locally. She sends a message
  // containing the state id.
  IO_PROBE( IO_TAG_LOAD, storage.m_request_load() );

  // QUERY (every x seconds)
  // request -> send (1) cached state list and (2) number of free slots to server
  // response -> receive up to 2 states to prefetch and up to 2 states to delete if neccessary
  // storage.m_query_server();

  // EVENT
  // this event is triggered, when the storage controller gets a
  // prefetch instruction from the weight server [requested in 'm_query_server']
  IO_PROBE( IO_TAG_PULL, storage.m_request_pull() );

  // EVENT
  // Bob check and serve peer requests
  storage.m_request_peer();

  // app core may tell the head to finish application. This will end in a call to fti_finalize on all cores (app and fti head cores) to ensure that the last checkpoint finished writing
  IO_PROBE( IO_TAG_FINI, storage.m_request_fini() );

}

//======================================================================
//  STORAGE CONTROLLER API
//======================================================================

int StorageController::protect( void* buffer, size_t size, io_type_t type) {
  m_io->protect(buffer, size, type);
}

int StorageController::update( io_id_t id, void* buffer, size_t size ) {
  m_io->update(id, buffer, size);
}

void StorageController::load( io_state_id_t state_id ) {
  if( m_worker_thread ) {
    return m_load_head( state_id );
  } else {
    return m_load_user( state_id );
  }
}

void StorageController::pull( io_state_id_t state_id ) {
  if( m_worker_thread ) {
    return m_pull_head( state_id );
  } else {
    return m_pull_user( state_id );
  }
}

void StorageController::store( io_state_id_t state_id ) {
  if( m_worker_thread ) {
    return m_store_head( state_id );
  } else {
    return m_store_user( state_id );
  }
}

void StorageController::copy( io_state_id_t state_id, io_level_t from, io_level_t to) {
  if( to == IO_STORAGE_L1 && state_pool.free() == 0 ) {
    server.delete_request(this);
  }
  m_io->copy(state_id, from, to);
  if( to == IO_STORAGE_L1 ) {
    state_pool++;
  }
}

void StorageController::m_pull_head( io_state_id_t state_id ) {
  assert( !m_io->is_local( state_id ) && "state is already local!" );
  if( state_pool.free() == 0 ) {
    server.delete_request(this);
  }
  if( !m_io->is_local( state_id ) ) {
    m_peer->mirror( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    sleep(2);
    int id, t;
    m_io->copy( state_id, IO_STORAGE_L2, IO_STORAGE_L1 );
  }
  m_cached_states.insert( std::pair<io_id_t,io_state_id_t>( to_ckpt_id(state_id), state_id ) );
  assert( m_io->is_local( state_id ) && "state should be local now!" );
  state_pool++;
}

void StorageController::m_pull_user( io_state_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_load_head( io_state_id_t state ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_load_user( io_state_id_t state ) {
  if( !m_io->is_local( state ) ) {
    sleep(1);
    int status;
    m_io->sendrecv( &state, &status, sizeof(io_state_id_t), sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
    // TODO check status
  }
  assert( m_io->is_local( state ) && "unable to load state to local storage" );
  m_io->load( state );
}

void StorageController::m_store_head( io_state_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_store_user( io_state_id_t state_id ) {
  m_io->store( state_id );
  assert( m_io->is_local( state_id ) && "unable to store state to local storage" );
}

//======================================================================
//  SERVER QUERY
//======================================================================

void StorageController::m_query_server() {
  if( m_trigger_query() ) server.prefetch_request( this );
}

//======================================================================
//  REQUESTS
//======================================================================

void StorageController::m_request_post() {
  if(m_io->m_dict_bool["master_global"]) std::cout << "head received INFORMATION request" << std::endl;

  Message weight_message;
  int msg_size;
  m_io->get_message_size( &msg_size, IO_TAG_POST, IO_MSG_ONE );
  char buffer[msg_size];
  m_io->recv( buffer, msg_size, IO_TAG_POST, IO_MSG_ONE );
  weight_message.ParseFromArray( buffer, msg_size );

  assert( weight_message.has_weight() && "m does not contain a weight" );

  io_state_id_t state_id( weight_message.weight().state_id().t(), weight_message.weight().state_id().id() );
  io_id_t ckpt_id = to_ckpt_id( state_id );

  m_io->copy( state_id, IO_STORAGE_L1, IO_STORAGE_L2 );

  m_ckpted_states.insert( std::pair<io_id_t, io_state_id_t>( ckpt_id, state_id ) );
  m_cached_states.insert( std::pair<io_id_t, io_state_id_t>( ckpt_id, state_id ) );

  // create symbolic link
  if(m_io->m_dict_bool["master_global"]) m_create_symlink( state_id );
  m_push_weight_to_server( weight_message );

  // if slot free, keep checkpoint. If not ask to delete a state
  if( state_pool.free() == 0 ) {
    server.delete_request(this);
  } else {
    state_pool++;
  }

  // ask if something to prefetch
  server.prefetch_request( this );

}

// (1) state request from user to worker
void StorageController::m_request_load() {
  if(m_io->m_dict_bool["master_global"]) std::cout << "head received LOAD request" << std::endl;
  io_state_id_t *state_id = new io_state_id_t[m_io->m_dict_int["app_procs_node"]];
  int result=(int)IO_SUCCESS;
  m_io->recv( state_id, sizeof(io_state_id_t), IO_TAG_LOAD, IO_MSG_ALL );
  pull( state_id[0] );
  m_io->send( &result, sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
  delete[] state_id;
}

// (2) state request from peer to worker
void StorageController::m_request_peer() {
  m_peer->handle_requests();
}

void StorageController::m_request_pull() {
  if( m_io->probe( IO_TAG_PULL ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
    pull( m_io->m_state_pull_requests.front() );
    m_io->m_state_pull_requests.pop();
  }
}


//======================================================================
//  FINALIZE REQUEST
//======================================================================

void StorageController::m_request_fini() {
  if(m_io->m_dict_bool["master_global"]) std::cout << "head received FINALIZATION request" << std::endl;
  /// Request handled to end the Runner
  int dummy;
  m_io->recv( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
  //m_request_post();
  //while( m_io->probe( IO_TAG_PUSH ) ) {
  //  if(m_io->m_dict_bool["master_global"]) std::cout << "head received PUSH request: " << std::endl;
  //  m_io->copy( m_io->m_state_push_requests.front(), IO_STORAGE_L1, IO_STORAGE_L2 );
  //  m_io->m_state_push_requests.pop();
  //}
  //while( m_io->probe( IO_TAG_PULL ) ) {
  //  if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
  //  pull( m_io->m_state_pull_requests.front() );
  //  m_io->m_state_pull_requests.pop();
  //}
  server.fini();
  m_io->send( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
}

//======================================================================
//  State Pool
//======================================================================

void StorageController::StatePool::init( size_t capacity ) {
  m_capacity = capacity;
}

// prefix increment
StorageController::StatePool& StorageController::StatePool::operator++() {
  m_used_slots++;
  assert( m_used_slots <= m_capacity && "state pool overflow!" );
  return *this;
}

// postfix increment
StorageController::StatePool StorageController::StatePool::operator++(int) {
  StorageController::StatePool tmp = *this;
  ++*this;
  assert( m_used_slots <= m_capacity && "state pool overflow!" );
  return tmp;
}

// prefix decrement
StorageController::StatePool& StorageController::StatePool::operator--() {
  m_used_slots--;
  assert( m_used_slots >= 0 && "state pool underflow!" );
  return *this;
}

// postfix decrement
StorageController::StatePool StorageController::StatePool::operator--(int) {
  StorageController::StatePool tmp = *this;
  --*this;
  assert( m_used_slots >= 0 && "state pool underflow!" );
  return tmp;
}


//======================================================================
//  SERVER REQUESTS
//======================================================================

void StorageController::Server::init() { // FIXME: why not simply using constructor and destructor instead of init and fini ?
  char * melissa_server_master_gp_node = getenv(
      "MELISSA_SERVER_MASTER_GP_NODE");
  if (melissa_server_master_gp_node == nullptr)
  {
    L(
        "you must set the MELISSA_SERVER_MASTER_GP_NODE environment variable before running!");
    assert(false);
  }

  m_socket = zmq_socket(storage.m_zmq_context, ZMQ_REQ);
  std::string port_name = fix_port_name(melissa_server_master_gp_node);
  assert( zmq_connect(m_socket, port_name.c_str()) == 0 );
}

void StorageController::Server::fini() {
  zmq_close(m_socket);
}

void StorageController::Server::prefetch_request( StorageController* storage ) {

  // TODO server needs to assure that the minimum storage requirements
  // are fullfilled (minimum 2 slots for ckeckpoints and other peer requests).
  Message request;

  for( const auto& pair : storage->m_cached_states ) {
    auto state = request.mutable_prefetch_request()->add_cached_states();
    state->set_t(pair.second.t);
    state->set_id(pair.second.id);
  }
  request.mutable_prefetch_request()->set_capacity(storage->state_pool.capacity());
  request.mutable_prefetch_request()->set_free(storage->state_pool.free());
  request.set_runner_id(storage->m_runner_id);

  send_message(m_socket, request);

  auto response = receive_message(m_socket);

  auto pull_states = response.prefetch_response().pull_states();
  for(auto it=pull_states.begin(); it!=pull_states.end(); it++) {
    io_state_id_t state = { it->t(), it->id() };
    storage->m_io->m_state_pull_requests.push( state );
  }

  auto dump_states = response.prefetch_response().dump_states();
  for(auto it=dump_states.begin(); it!=dump_states.end(); it++) {
    storage->m_io->remove( { it->t(), it->id() }, IO_STORAGE_L1 );
    storage->state_pool--;
  }

}

void StorageController::Server::delete_request( StorageController* storage ) {

  Message request;

  for( const auto& pair : storage->m_cached_states ) {
    auto state = request.mutable_delete_request()->add_cached_states();
    state->set_t(pair.second.t);
    state->set_id(pair.second.id);
  }
  request.set_runner_id(storage->m_runner_id);

  send_message(m_socket, request);

  Message response = receive_message(m_socket);

  auto pull_state = response.delete_response().to_delete();
  io_state_id_t state_id( pull_state.t(), pull_state.id() );

  storage->m_io->remove( state_id, IO_STORAGE_L1 );
  storage->state_pool--;

}


//======================================================================
//  HELPER FUNCTIONS
//======================================================================

void StorageController::m_push_weight_to_server(const Message & m ) {
//  send_message(server.m_socket, m);
//  zmq::recv(server.m_socket);  // receive ack
}

void StorageController::m_create_symlink( io_state_id_t state_id ) {

  std::stringstream target_global, link_global;
  std::stringstream target_meta, link_meta;

  target_global << m_io->m_dict_string["global_dir"] << "/" << m_io->m_dict_string["exec_id"] << "/l4/" << to_ckpt_id(state_id);
  link_global << m_io->m_dict_string["global_dir"] << "/" << to_ckpt_id(state_id);
  symlink( target_global.str().c_str(), link_global.str().c_str() );

  target_meta << m_io->m_dict_string["meta_dir"] << "/" << m_io->m_dict_string["exec_id"] << "/l4/" << to_ckpt_id(state_id);
  link_meta << m_io->m_dict_string["meta_dir"] <<  "/" << to_ckpt_id(state_id);
  symlink( target_meta.str().c_str(), link_meta.str().c_str() );

}
