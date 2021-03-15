#include "storage_controller.hpp"
#include "storage_controller_impl.hpp"

#include "utils.h"
#include <memory>
#include <numeric>

// TODO error checking!

#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

void StorageController::init( MpiController* mpi, IoController* io ) { 
  m_peer = new PeerController();
  m_io = io;
  m_mpi = mpi;
  m_io->register_callback( StorageController::callback );
  m_comm_global_size = m_mpi->size();
  // heads dont return here!
  m_io->init_io(m_mpi);
  m_io->init_core(m_mpi);
  m_comm_worker_size = m_io->m_dict_int["nodes"]; 
  m_comm_runner_size = m_io->m_dict_int["nodes"] * (m_io->m_dict_int["procs_node"]-1);
  m_worker_thread = false;
}

void StorageController::fini() {
  delete m_peer;
  m_io->fini();
}

// Callback called in the FTI head loop:
void StorageController::callback() {
  
  //void* context;
  static bool init = false;
  if( !init ) {
    storage.m_io->init_core(storage.m_mpi);
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

  // query info from server
  // submit:
  // - remove requests
  // - load requests for prefetching
  storage.m_query_server();

  // has to have the highest priority!!!
  storage.m_state_info_user();

  // check and handle state request from Alice
  storage.m_state_request_user();

  // check and handle push request
  storage.m_state_request_push();

  // check and handle state request from Bob
  storage.m_state_request_peer();

  // check and handle delete request
  storage.m_state_request_remove();

  // check and handle delete request
  storage.m_state_request_load();
}

void StorageController::load( int state_id ) {
  if( m_worker_thread ) {
    return m_load_core( state_id );
  } else {
    return m_load_user( state_id );
  }
}

void StorageController::store( int state_id ) {
  if( m_worker_thread ) {
    return m_store_core( state_id );
  } else {
    return m_store_user( state_id );
  }
}

int StorageController::protect( void* buffer, size_t size, io_type_t type) {
  m_io->protect(buffer, size, type);  
}

void StorageController::copy( int id, io_level_t from, io_level_t to) {
  m_io->copy(id, from, to);  
}

void StorageController::m_load_core( int state_id ) {
  if( !m_io->is_local( state_id ) ) {
    m_peer->request( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    sleep(2);
    m_io->copy( state_id, IO_STORAGE_L2, IO_STORAGE_L1 );
  }
}

void StorageController::m_load_user( int state_id ) {
  if( !m_io->is_local( state_id ) ) {
    sleep(1);
    int status;
    m_io->sendrecv( &state_id, &status, sizeof(int), IO_TAG_REQUEST, IO_MSG_ALL );
    // TODO check status
  }
  assert( m_io->is_local( state_id ) && "unable to load state to local storage" );
  m_io->load( state_id );
}
    
void StorageController::m_store_core( int state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_store_user( int state_id ) {
  m_io->store( state_id );
  assert( m_io->is_local( state_id ) && "unable to store state to local storage" );
}

