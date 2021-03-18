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
    size_t capacity, size_t checkpoint_size ) {

  m_capacity = capacity;
  m_checkpoint_size = checkpoint_size;
  
  // 2 model checkpoints + 1 runner prefetch request
  size_t minimum_storage_requirement = 3 * checkpoint_size;
  
  IO_TRY( minimum_storage_requirement > capacity, false, "Insufficiant storage capacity!" );

  m_prefetch_capacity = ( m_capacity - minimum_storage_requirement ) / checkpoint_size; 

  m_peer = new PeerController();
  m_io = io;
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
}

//======================================================================
//  STATIC CALLBACK FUNCTION
//======================================================================

// Callback called in the FTI head loop:
void StorageController::callback() {
  
  //void* context;
  static bool init = false;
  if( !init ) {
    storage.m_io->init_core();
    storage.m_comm_worker_size = storage.m_io->m_dict_int["nodes"]; 
    storage.m_comm_runner_size = storage.m_io->m_dict_int["nodes"] * (storage.m_io->m_dict_int["procs_node"]-1);
    init = true;
  }
  //static std::unique_ptr<StateServer> state_server(nullptr);
  
  //if (state_server.get() == nullptr) {
  //  // open all sockets necessary
  //  state_server = std::make_unique<StateServer>(context);
  //  storage.m_worker_thread = true;
  //}

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

void StorageController::m_load_head( io_id_t state_id ) {
  if( !m_io->is_local( state_id ) ) {
    //m_peer->request( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    sleep(2);
    m_io->copy( m_known_states[state_id], IO_STORAGE_L2, IO_STORAGE_L1 );
  }
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
  // TODO keep 2 states fresh
  // TODO send free slots
  
  // update peers 
  static int count = 0; 
  // update states
  // [dummy impl zum testen]
  if(count == 30) {
    for(int i=2; i<11; i++) {
      io_state_t state_info;
      state_info.id = i;
      state_info.t = 0;
      io_id_t id = to_ckpt_id( 0, i );
    }
  }
  count++;
}

//======================================================================
//  REQUESTS
//======================================================================

void StorageController::m_request_post() {
  if(m_io->m_dict_bool["master_global"]) std::cout << "head received INFORMATION request" << std::endl;

  Weight m;
  char* buffer = new char[256];
  m_io->recv( buffer, 256, IO_TAG_POST, IO_MSG_ONE );
  m.ParseFromString( buffer );
  
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
  load( state_id[0] );
  m_io->send( &result, sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
  delete[] state_id;
}

// (2) state request from peer to worker
void StorageController::m_request_peer() {
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
    m_io->copy( m_known_states[m_io->m_state_push_requests.front()], IO_STORAGE_L1, IO_STORAGE_L2 );
    m_io->m_state_push_requests.pop();
  }
  while( m_io->probe( IO_TAG_PULL ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
    load( m_io->m_state_pull_requests.front() );
    m_io->m_state_pull_requests.pop();
  }
  m_io->send( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
}

//======================================================================
//  SERVER REQUESTS
//======================================================================

void StorageController::Server::request( StorageController* storage ) {
  // generate buffer
  WorkerRequest request;
  // set runner id
  request.set_peer_id(storage->m_runner_id);
  // set states
  for( auto& pair : storage->m_known_states ) {
  }
}

void StorageController::m_push_weight_to_server( io_state_t state_info, double weight ) {
  
  Message m;
  m.set_runner_id(m_runner_id);
  m.mutable_weight()->mutable_state_id()->set_t(state_info.t);
  m.mutable_weight()->mutable_state_id()->set_id(state_info.id);
  m.mutable_weight()->set_weight(weight);

  //send_message(gp_socket, m);
  //zmq::recv(gp_socket);  // receive ack
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
