#include "storage_controller.hpp"
#include "storage_impl.hpp"

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

  // check and handle state request from Alice
  storage.m_state_request_user();

  // check and handle state request from Bob
  storage.m_state_request_peer();

  // check and handle acknowledge info from Alice
  storage.m_state_info_user();

  // check and handle delete request
  storage.m_erase();

  // check and handle move request
  storage.m_move();

  // check and handle push request
  storage.m_push();

  // check and handle prefetch request
  storage.m_prefetch();

  // query info from server
  storage.m_query_server();
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

