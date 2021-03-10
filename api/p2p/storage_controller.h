#ifndef _STORAGE_CONTROLLER_H_
#define _STORAGE_CONTROLLER_H_

#include "peer_controller.h"
#include <cstddef>
#include <fti.h>

enum StorageLevel {
  MELISSA_CACHE_L1, // RAMdisk
  MELISSA_CACHE_L2, // SSD or NVMe
  MELISSA_CACHE_L3  // PFS or BurstBuffer
};

enum StateStatus {
  MELISSA_STATE_BUSY,
  MELISSA_STATE_IDLE,
  MELISSA_STATE_LOAD,
};

struct StateInfo_t {
  StateStatus status;
  StorageLevel device;
};

class StateServer {
  public:
    StateServer(void * context);
  private:
    void listen();

    static StateServer s;
};

class StorageController {
  
  public:
    StorageController( int request_interval, int nbody ) : 
      m_request_interval(request_interval),
      m_request_counter(0),
      m_nbody(nbody)
    {}
    // function to register for FTI
    void callback();

  private:

    //##> FUNCTIONS FOR REQUESTS

    // (1) state request from home runner
    void handle_state_request_home();
    
    // (2) state request from peer runner
    void handle_state_request_peer();
    
    // (3) update-message from home runner
    void handle_update_message_home();
    
    // (4) delete request
    void handle_delete_request();
    
    // (5) prefetch request
    void handle_prefetch_request();
    
    // (6) stage request
    void handle_stage_request();
    
    // request state cache and peer info from server
    void query_runtime_info_server();

    //##> HELPER FUNCTIONS

    bool update_info() { return (m_request_interval % m_request_counter++) == 0; }

    //##> VARIABLES

    PeerController m_peers;
    std::map<int,StateInfo_t> m_states;

    size_t m_request_counter;

    // FTI sleeps 500 us each iteration. Thus, a request interval of 2
    // coresponds to a server info request each second.
    int m_request_interval;
    
    int m_nbody; 

    //##> CONSTANTS

    static const int TAG_OFFSET = 1000000;

    enum FTI_TAG {
      TAG_REQUEST_HOME = TAG_OFFSET,
      TAG_INFO_HOME,
      TAG_DELETE,
      TAG_PREFETCH,
      TAG_STAGE
    };
};

#endif // _STORAGE_CONTROLLER_H_
