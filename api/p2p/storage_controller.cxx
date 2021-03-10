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

}

// (2) state request from peer runner
void StorageController::handle_state_request_peer(){

}

// (3) update-message from home runner
void StorageController::handle_update_message_home(){

}

// (4) delete request
void StorageController::handle_delete_request(){

}

// (5) prefetch request
void StorageController::handle_prefetch_request(){

}

// (6) stage request
void StorageController::handle_stage_request(){

}

// request state cache and peer info from server
void StorageController::query_runtime_info_server(){

}
