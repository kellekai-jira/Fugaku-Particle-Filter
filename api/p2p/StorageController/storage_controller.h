#ifndef _STORAGE_CONTROLLER_H_
#define _STORAGE_CONTROLLER_H_

#include "io_controller.h"
#include "peer_controller.h"
#include <cstddef>
#include <cassert>
#include <memory>

template<typename T>
std::unique_ptr<T>& unique_nullptr() { 
  static std::unique_ptr<T> ptr = std::unique_ptr<T>(nullptr);
  return ptr;
}

enum state_status_t {
  MELISSA_STATE_BUSY,
  MELISSA_STATE_IDLE,
  MELISSA_STATE_LOAD,
};

struct state_info_t {
  state_status_t status;
  io_level_t level;
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
    // SINGLETON
    static StorageController& getInstance()
    {
      return _getInstance();
    }
    static void create( int request_interval, int comm_model_size ) // enable moving in
    {
      _getInstance( true, request_interval, comm_model_size );
    }
    StorageController(StorageController const&) = delete;
    void operator=(StorageController const&) = delete;

    // CALLBACK FOR FTI HEADS
    static void callback();
    
    // API
    void load( int state_id );
    void store( int state_id );

  private:
    
    static StorageController& _getInstance(bool init = false, int request_interval = -1, 
        int comm_model_size = -1, std::unique_ptr<IoController> & io = unique_nullptr<IoController>(),
        std::unique_ptr<PeerController> & peer = unique_nullptr<PeerController>())
    {
      static StorageController instance{ init, request_interval, comm_model_size, io, peer };
      return instance;
    }
    
    StorageController( bool init, int request_interval, int comm_model_size, 
        std::unique_ptr<IoController> & io, std::unique_ptr<PeerController> & peer);

    void m_load_core( int state_id );
    void m_load_user( int state_id );
    
    void m_store_core( int state_id );
    void m_store_user( int state_id );
    
    // (1) state request from user to worker
    void m_state_request_user();
    
    // (2) state request from peer to worker
    void m_state_request_peer();
    
    // (3) state info from user
    void m_state_info_user();
    
    // organize storage
    void m_prefetch();
    void m_erase();
    void m_move();
    void m_push();
    
    // request state cache and peer info from server
    void m_query_server();

    bool m_trigger_query() { return (m_request_interval % m_request_counter++) == 0; }

//----------------------------------------------------------------------------------------
//  VARIABLES
//----------------------------------------------------------------------------------------
    
    bool m_initialized;
    
    std::unique_ptr<IoController>& m_io;
    std::unique_ptr<PeerController>& m_peer;

    std::map<int,state_info_t> m_states;
    
    bool m_worker_thread;
    size_t m_request_counter;

    // FTI sleeps 500 us each iteration. Thus, a request interval of 2
    // coresponds to a server info request each second.
    int m_request_interval;
    
    int m_comm_model_size; 

};

#endif // _STORAGE_CONTROLLER_H_
