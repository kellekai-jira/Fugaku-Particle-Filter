#include "storage_controller.hpp"
#include "storage_controller_impl.hpp"

#include "utils.h"
#include <memory>
#include <numeric>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>

// TODO error checking!

//======================================================================
//  STORAGE CONTROLLER INITIALIZATION
//======================================================================

void StorageController::init( MpiController* mpi, IoController* io,
    size_t capacity, size_t state_size, void* zmq_socket ) {

  m_capacity = capacity;
  m_state_size_node = 0xffffffffffffffff; // TODO compute state_size_local (state size on this node);

  // 2 model checkpoints
  // 2 for prefetching
  // 1 for alice load request
  // 1 for sending to peer (needs memory copy of ckpt file)
  size_t minimum_storage_requirement = 6 * m_state_size_node;

  IO_TRY( minimum_storage_requirement > capacity, false, "Insufficiant storage capacity!" );

  m_prefetch_capacity = ( m_capacity - minimum_storage_requirement ) / m_state_size_node;

  server.init();

  m_io = io;
  m_peer = new PeerController( io );
  m_mpi = mpi;
  m_io->register_callback( StorageController::callback );
  m_comm_global_size = m_mpi->size();
  // heads dont return here!
  m_io->init_io(m_mpi);
  m_io->init_core();
  m_comm_worker_size = m_io->m_dict_int["nodes"];
  m_comm_runner_size = m_io->m_dict_int["nodes"] * (m_io->m_dict_int["procs_node"]-1);
  m_worker_thread = false;

}


void StorageController::fini() {
  delete m_peer;
  int dummy[2];
  m_io->sendrecv( &dummy[0], &dummy[1], sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
  m_mpi->barrier("fti_comm_world");
  m_io->fini();

  server->fini();

}



//======================================================================
//  STATIC CALLBACK FUNCTION
//======================================================================

// Callback called in the FTI head loop:
void StorageController::callback() {

  static bool init = false;
  if( !init ) {
    storage.m_io->init_core();
    storage.m_comm_worker_size = storage.m_io->m_dict_int["nodes"];
    storage.m_comm_runner_size = storage.m_io->m_dict_int["nodes"] * (storage.m_io->m_dict_int["procs_node"]-1);
    init = true;
    std::vector<std::string> files;
    storage.m_io->filelist_local( 1, files );
  }


  IO_PROBE( IO_TAG_FINI, storage.m_request_fini() );

  // QUERY (every x seconds)
  // request -> send (1) cached state list and (2) number of free slots to server
  // response -> receive up to 2 states to prefetch and up to 2 states to delete if neccessary
  storage.m_query_server();

  // EVENT
  // message from alice, sent after completion of the forecast
  // containing (1) forecast state id and (2) weight.
  IO_PROBE( IO_TAG_POST, storage.m_request_post() );

  // EVENT
  // Alice cannot find the state locally. She sends a message
  // containing the state id.
  IO_PROBE( IO_TAG_LOAD, storage.m_request_load() );

  // EVENT
  // this event is triggered, when the storage controller gets a
  // prefetch instruction from the weight server [requested in 'm_query_server']
  IO_PROBE( IO_TAG_PULL, storage.m_request_pull() );

  // EVENT
  // Bob requests a state
  IO_PROBE( IO_TAG_PEER, storage.m_request_peer() );

}

//======================================================================
//  STORAGE CONTROLLER API
//======================================================================

int StorageController::protect( void* buffer, size_t size, io_type_t type) {
  m_io->protect(buffer, size, type);
}

int StorageController::update( io_id_t id, void* buffer, io_size_t size ) {
  m_io->update(id, buffer, size);
}


// TODO change head case load to pull
void StorageController::load( io_id_t state_id ) {
  if( m_worker_thread ) {
    return m_load_head( state_id );
  } else {
    return m_load_user( state_id );
  }
}

void StorageController::pull( io_id_t state_id ) {
  if( m_worker_thread ) {
    return m_pull_head( state_id );
  } else {
    return m_pull_user( state_id );
  }
}

void StorageController::store( io_id_t state_id ) {
  if( m_worker_thread ) {
    return m_store_head( state_id );
  } else {
    return m_store_user( state_id );
  }
}

void StorageController::copy( io_id_t state_id, io_level_t from, io_level_t to) {
  m_io->copy(m_known_states[state_id], from, to);
}

void StorageController::m_pull_head( io_id_t state_id ) {
  if( !m_io->is_local( state_id ) ) {
    //m_peer->request( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    sleep(2);
    int id, t;
    from_ckpt_id( state_id, &t, &id );
    io_state_t state = { t, id };
    m_io->copy( state, IO_STORAGE_L2, IO_STORAGE_L1 );
    m_known_states.insert( std::pair<io_id_t,io_state_t>( state_id, state ) );
  }
}

void StorageController::m_pull_user( io_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_load_head( io_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_load_user( io_id_t state_id ) {
  if( !m_io->is_local( state_id ) ) {
    sleep(1);
    int status;
    m_io->sendrecv( &state_id, &status, sizeof(io_id_t), IO_TAG_LOAD, IO_MSG_ALL );
    // TODO check status
  }
  assert( m_io->is_local( state_id ) && "unable to load state to local storage" );
  m_io->load( state_id );
}

void StorageController::m_store_head( io_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_store_user( io_id_t state_id ) {
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
  char* buffer = new char[256];
  m_io->recv( buffer, 256, IO_TAG_POST, IO_MSG_ONE );
  weight_message.ParseFromArray( buffer );

  assert( m.has_state_id() && "m does not contain a state-id" );

  StateId state_id = m.state_id();
  int id = state_id.id();
  int t = state_id.t();
  double weight = m.weight();

  io_state_t state_info = { t, id };

  m_known_states.insert( std::pair<io_id_t, io_state_t>( to_ckpt_id( t, id ), state_info ) );

  m_io->copy( state_info, IO_STORAGE_L1, IO_STORAGE_L2 );

  // create symbolic link
  if(m_io->m_dict_bool["master_global"]) m_create_symlink( to_ckpt_id( t, id ) );
  m_push_weight_to_server( state_info, weight );

}

// (1) state request from user to worker
void StorageController::m_request_load() {
  if(m_io->m_dict_bool["master_global"]) std::cout << "head received LOAD request" << std::endl;
  io_id_t *state_id = new io_id_t[m_io->m_dict_int["procs_app"]];
  int result=(int)IO_SUCCESS;
  m_io->recv( state_id, sizeof(io_id_t), IO_TAG_LOAD, IO_MSG_ALL );
  pull( state_id[0] );
  m_io->send( &result, sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
  delete[] state_id;
}

// (2) state request from peer to worker
void StorageController::m_request_peer() {
}

void StorageController::m_request_pull() {
  if( m_io->probe( IO_TAG_PULL ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
    int t  = m_io->m_state_pull_requests.front().t;
    int id  = m_io->m_state_pull_requests.front().id;
    pull( to_ckpt_id( t, id ) );
    m_io->m_state_pull_requests.pop();
  }
}


//======================================================================
//  FINALIZE REQUEST
//======================================================================

void StorageController::m_request_fini() {
  int dummy;
  m_io->recv( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
  m_request_post();
  while( m_io->probe( IO_TAG_PUSH ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PUSH request: " << std::endl;
    int t  = m_io->m_state_push_requests.front().t;
    int id  = m_io->m_state_push_requests.front().id;
    m_io->copy( m_known_states[to_ckpt_id( t, id )], IO_STORAGE_L1, IO_STORAGE_L2 );
    m_io->m_state_push_requests.pop();
  }
  while( m_io->probe( IO_TAG_PULL ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
    int t  = m_io->m_state_pull_requests.front().t;
    int id  = m_io->m_state_pull_requests.front().id;
    pull( to_ckpt_id( t, id ) );
    m_io->m_state_pull_requests.pop();
  }
  m_io->send( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
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

  m_socket = zmq_socket(context, zmq_REQ);
  std::string addr = "tcp://" + melissa_server_master_gp_node;
  D("connect to general purpose server at %s", addr.c_str());
  req = zmq_connect(m_socket, addr.c_str());
  assert(req == 0);
}

void StorageController::Server::fini() {
  zmq_disconnect(m_socket);
}

void StorageController::Server::prefetch_request( StorageController* storage ) {

  PrefetchRequest request;
  PrefetchResponse response;

  for( const auto& pair : storage->m_cached_states ) {
    auto state = request.add_cached_states();
    state->set_t(pair.second.t);
    state->set_id(pair.second.id);
  }
  request.set_free(storage->free());

  char sbuf[request.ByteSizeLong()];
  storage->m_serialize(request, sbuf);
  // zmq::send( sbuf, ... );

  // TODO no idea how the char buffer size handling works here...
  char rbuf[PROTOBUF_MAX_SIZE];
  int msg_size;
  // zmq::recv( rbuf, ... );

  storage->m_deserialize( response, rbuf, msg_size );

  auto pull_states = response.pull_states();
  for(auto it=pull_states.begin(); it!=pull_states.end(); it++) {
    io_state_t state = { it->t(), it->id() };
    storage->m_io->m_state_pull_requests.push( state );
  }

  auto dump_states = response.dump_states();
  for(auto it=dump_states.begin(); it!=dump_states.end(); it++) {
    io_id_t state_id = to_ckpt_id( it->t(), it->id() );
    storage->m_io->remove( state_id, IO_STORAGE_L1 );
  }

}


//======================================================================
//  HELPER FUNCTIONS
//======================================================================

void StorageController::m_push_weight_to_server(const Message & m ) {
  send_message(server.m_socket, m);
  zmq::recv(server.m_socket);  // receive ack
}

void StorageController::m_create_symlink( io_id_t ckpt_id ) {

  std::stringstream target_global, link_global;
  std::stringstream target_meta, link_meta;

  target_global << m_io->m_dict_string["global_dir"] << "/" << m_io->m_dict_string["exec_id"] << "/l4/" << ckpt_id;
  link_global << m_io->m_dict_string["global_dir"] << "/" << ckpt_id;
  symlink( target_global.str().c_str(), link_global.str().c_str() );

  target_meta << m_io->m_dict_string["meta_dir"] << "/" << m_io->m_dict_string["exec_id"] << "/l4/" << ckpt_id;
  link_meta << m_io->m_dict_string["meta_dir"] <<  "/" << ckpt_id;
  symlink( target_meta.str().c_str(), link_meta.str().c_str() );

}

template<typename T>
void StorageController::m_serialize( T& message, char* buffer ) {
  size_t size = message.ByteSizeLong();
  message.SerializeToArray(buffer, size);
}

template<typename T>
void StorageController::m_deserialize( T& message, char* buffer, int size ) {
  message.ParseFromArray(buffer, size);
}

