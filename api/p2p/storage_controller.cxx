#include "storage_controller.h"

#include "utils.h"
#include <memory>


#include "../../server-p2p/messages/cpp/control_messages.pb.h"

// Callback called in the FTI head loop:
void StorageController::callback() {
    void* context;
    static std::unique_ptr<StateServer> state_server(nullptr);
    if (state_server.get() == nullptr) {
        // open all sockets necessary
        state_server = std::make_unique<StateServer>(context);
    }

    /*
     * The heads have to handle 6 kinds of requests:
     * 
     * (1) state request from home runner
     * (2) state request from peer runner
     * (3) update-message from home runner
     * (4) delete request (remove deprecated states)
     * (5) prefetch request (update state cache)
     * (6) stage request (push state to PFS)
     * 
     * Furthermore, the heads frequently update the the work queue 
     * and the peer list requesting info from the server.
    */

    // (1) state request from home runner
    handle_state_request_home();
    
    // (2) state request from peer runner
    handle_state_request_peer();

    // (3) update-message from home runner
    handle_update_message_home();
    
    // (4) delete request
    handle_delete_request();
    
    // (5) prefetch request
    handle_prefetch_request();
    
    // (6) stage request
    handle_stage_request();
    
    // request state cache and peer info from server
    query_runtime_info_server();
}

// (1) state request from home runner
void StorageController::handle_state_request_home(){
  /*
   * 1) check for messages
   * 2) receive messages
   * 3) request from bob
   * 4) request from pfs
   * 5) release alice
  */
  StateInfo_t state_info;
  int peer_id;

  // 1) check for messages
  if( FTI_HeadProbe(TAG_REQUEST_HOME) ) {
    int result = -1;
    // 2) receive messages
    FTI_HeadRecv(&state_info, sizeof(StateInfo_t), TAG_REQUEST_HOME, FTI_HEAD_MODE_SING);
    if( m_peers.query( state_info.state_id, &peer_id ) ) {
      // 3) request from bob
      result = m_peers.transfer( state_info.state_id, state_info.state_rank, peer_id );
    } else {
      // 4) request from pfs
      result = FTI_Convert( peer_id, state_info.state_id, FTI_L4, FTI_L1, FTI_CONVERT_HEAD );
    }
    // 5) release alice
    FTI_HeadSend( &result, sizeof(int), TAG_REQUEST_HOME, FTI_HEAD_MODE_SING);
  }
}

// (2) state request from peer runner
void StorageController::handle_state_request_peer(){
  if( FTI_HeadProbe(TAG_REQUEST_PEER) ) {
  }
}

// (3) update-message from home runner
void StorageController::handle_update_message_home(){
  if( FTI_HeadProbe(TAG_INFO_HOME) ) {
  }
}

// (4) delete request
void StorageController::handle_delete_request(){
  if( FTI_HeadProbe(TAG_DELETE) ) {
  }
}

// (5) prefetch request
void StorageController::handle_prefetch_request(){
  if( FTI_HeadProbe(TAG_PREFETCH) ) {
  }
}

// (6) stage request
void StorageController::handle_stage_request(){
  if( FTI_HeadProbe(TAG_STAGE) ) {
  }
}

// request state cache and peer info from server
void StorageController::query_runtime_info_server(){
  if( update_info() ) {
  }
}
