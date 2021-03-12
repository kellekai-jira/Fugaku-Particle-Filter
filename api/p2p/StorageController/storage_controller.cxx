#include "storage_controller.hpp"
#include "storage_impl.hpp"

#include "utils.h"
#include <memory>

// TODO error checking!

#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

void StorageController::init( MpiController* mpi, IoController* io ) { 
  m_peer = new PeerController();
  m_io = io;
  m_io->register_callback( StorageController::callback );
  m_io->init(mpi);
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
    std::cout << "I am a head and I got called for the first time!" << std::endl;
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

