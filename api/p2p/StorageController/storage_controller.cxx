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
  
  IO_TRY( minimum_storage_requirement > capacity, true, "Insufficiant storage capacity!" );

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

  storage.m_finalize_worker();
  
  // query info from server
  // submit:
  // - remove requests
  // - load requests for prefetching
  storage.m_query_server();

  // has to have the highest priority!!!
  if( storage.m_io->probe( IO_TAG_LOAD ) ) {
    storage.m_state_info_user();
    return;
  }

  // check and handle state request from Alice
  storage.m_state_request_user();

  // check and handle push request
  storage.m_state_request_push();

  // check and handle state request from Bob
  storage.m_state_request_peer();

  // check and handle delete request
  storage.m_state_request_dump();

  // check and handle delete request
  storage.m_state_request_load();
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
  m_io->copy(m_states[state_id], from, to);  
}

void StorageController::m_load_head( io_id_t state_id ) {
  if( !m_io->is_local( state_id ) ) {
    //m_peer->request( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    sleep(2);
    m_io->copy( m_states[state_id], IO_STORAGE_L2, IO_STORAGE_L1 );
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
  // update peers 
  static int count = 0; 
  // update states
  // [dummy impl zum testen]

  if(count == 30) {
    for(int i=2; i<11; i++) {
      io_state_t fake_state_info;
      fake_state_info.status = IO_STATE_IDLE;
      fake_state_info.id = i;
      fake_state_info.exec_id = m_io->m_dict_string["exec_id"]; 
      fake_state_info.runner_id = m_runner_id;
      fake_state_info.cycle = 0;
      fake_state_info.parent_id = i;
      io_id_t fake_state_id = generate_state_id( 0, i );
      m_states.insert( std::pair<io_id_t, io_state_t>( fake_state_id, fake_state_info ) );
      m_io->m_state_pull_requests.push( i );
    }
  }
  count++;
}

//======================================================================
//  ASYNC USER MESSAGE
//======================================================================

void StorageController::m_state_info_user() {
  if( m_io->probe( IO_TAG_POST ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received INFORMATION request" << std::endl;
    io_id_t info[4];
    // [0]->cycle_active, [1]->parent_id_active, [2]->cycle_finished, [3]->parent_id_finished
    m_io->recv( &info, sizeof(io_id_t)*4, IO_TAG_POST, IO_MSG_ONE );
    io_state_t active_state_info;
    io_state_t finished_state_info;
    io_id_t active_state_id = generate_state_id( info[0], info[1] ); 
    io_id_t finished_state_id = generate_state_id( info[2], info[3] ); 
    if( m_states.count( active_state_id ) == 0 ) {
      active_state_info.status = IO_STATE_BUSY;
      active_state_info.id = active_state_id;
      active_state_info.exec_id = m_io->m_dict_string["exec_id"]; 
      active_state_info.runner_id = m_runner_id;
      active_state_info.cycle = info[0];
      active_state_info.parent_id = info[1];
      m_states.insert( std::pair<io_id_t, io_state_t>( active_state_id, active_state_info ) );
    } else {
      m_states[active_state_id].status = IO_STATE_BUSY;
    }
    finished_state_info.id = finished_state_id;
    finished_state_info.status = IO_STATE_IDLE;
    finished_state_info.exec_id = m_io->m_dict_string["exec_id"]; 
    finished_state_info.runner_id = m_runner_id;
    finished_state_info.cycle = info[2];
    finished_state_info.parent_id = info[3];
    m_states.insert( std::pair<io_id_t, io_state_t>( finished_state_id, finished_state_info ) );
    m_io->m_state_push_requests.push(finished_state_id);
  }
}

//======================================================================
//  REQUESTS
//======================================================================

// (1) state request from user to worker
void StorageController::m_state_request_user() {
  if( m_io->probe( IO_TAG_LOAD ) ) {
    io_id_t *state_id = new io_id_t[m_io->m_dict_int["procs_app"]];
    int result=(int)IO_SUCCESS; 
    m_io->recv( state_id, sizeof(io_id_t), IO_TAG_LOAD, IO_MSG_ALL );
    load( state_id[0] );
    m_io->send( &result, sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
    delete[] state_id;
  }
}

// organize storage
void StorageController::m_state_request_push() {
  if( m_io->probe( IO_TAG_PUSH ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PUSH request: " << std::endl;
    m_io->copy( m_states[m_io->m_state_push_requests.front()], IO_STORAGE_L1, IO_STORAGE_L2 );
    io_dump_request_t dump = { m_io->m_state_push_requests.front(), IO_STORAGE_L1 };
    m_io->m_state_dump_requests.push( dump ); 
    m_io->m_state_push_requests.pop();
  }
}

// (2) state request from peer to worker
void StorageController::m_state_request_peer() {
  if( m_io->probe( IO_TAG_PEER ) ) {
  }
}

void StorageController::m_state_request_dump() {
  if( m_io->probe( IO_TAG_DUMP ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received REMOVE request" << std::endl;
    io_dump_request_t dump = m_io->m_state_dump_requests.front();
    m_io->remove( dump.state_id, dump.level );
    m_states.erase( dump.state_id );
    m_io->m_state_dump_requests.pop();
    
  }
}

void StorageController::m_state_request_load() {
  if( m_io->probe( IO_TAG_PULL ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
    load( m_io->m_state_pull_requests.front() );
    m_io->m_state_pull_requests.pop();
  }
}

//======================================================================
//  FINALIZE REQUEST
//======================================================================

void StorageController::m_finalize_worker() {
  if( m_io->probe( IO_TAG_FINI ) ) {
    int dummy;
    m_io->recv( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
    m_state_info_user();
    while( m_io->probe( IO_TAG_PUSH ) ) {
      if(m_io->m_dict_bool["master_global"]) std::cout << "head received PUSH request: " << std::endl;
      m_io->copy( m_states[m_io->m_state_push_requests.front()], IO_STORAGE_L1, IO_STORAGE_L2 );
      m_io->m_state_push_requests.pop();
    }
    while( m_io->probe( IO_TAG_PULL ) ) {
      if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
      load( m_io->m_state_pull_requests.front() );
      m_io->m_state_pull_requests.pop();
    }
    m_io->send( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
  }
}

//======================================================================
//  SERVER REQUESTS
//======================================================================

void StorageController::Server::request( StorageController* storage ) {
  // generate buffer
  melissa_p2p::WorkerRequest request;
  // set runner id
  request.set_id(storage->m_runner_id);
  // set states
  for( auto& pair : storage->m_states ) {
    auto state = request.add_states();
    // set state id
    state->mutable_id()->set_id(pair.second.id);
    // set state status
    switch( pair.second.status ) {
      case IO_STATE_BUSY: 
        state->set_status(melissa_p2p::WorkerRequest_StateStatus_BUSY);
        break;
      case IO_STATE_IDLE: 
        state->set_status(melissa_p2p::WorkerRequest_StateStatus_IDLE);
        break;
      case IO_STATE_DONE: 
        state->set_status(melissa_p2p::WorkerRequest_StateStatus_DONE);
        break;
    }
  }
}
