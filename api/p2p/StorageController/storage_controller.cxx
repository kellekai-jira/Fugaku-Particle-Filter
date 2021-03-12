#include "storage_controller.h"

#include "utils.h"
#include <memory>

// TODO error checking!

#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

StorageController::StorageController( bool init, int request_interval, int comm_model_size, 
    std::unique_ptr<IoController> & io, std::unique_ptr<PeerController> & peer) : 
  m_initialized(init),
  m_request_interval(request_interval),
  m_request_counter(0),
  m_comm_model_size(comm_model_size),
  m_io(io),
  m_peer(peer),
  m_worker_thread(true) {

  assert( init && "StorageController not initialized" );

  m_io->register_callback( StorageController::callback );
}

// Callback called in the FTI head loop:
void StorageController::callback() {
  
  void* context;
  static StorageController& storage = StorageController::getInstance();
  static std::unique_ptr<StateServer> state_server(nullptr);
  
  if (state_server.get() == nullptr) {
    // open all sockets necessary
    state_server = std::make_unique<StateServer>(context);
    storage.m_worker_thread = true;
  }

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

