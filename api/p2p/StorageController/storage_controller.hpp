#ifndef _STORAGE_CONTROLLER_H_
#define _STORAGE_CONTROLLER_H_

#include "mpi_controller.hpp"
#include "io_controller.hpp"
#include "peer_controller.hpp"
#include <cstddef>
#include <cassert>
#include <memory>
#include <vector>

template<typename T>
std::unique_ptr<T>& unique_nullptr() { 
  static std::unique_ptr<T> ptr = std::unique_ptr<T>(nullptr);
  return ptr;
}

static MpiController mpi_controller_null;

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
    static void create( int request_interval, MpiController & mpi, std::unique_ptr<IoController> & io ) // enable moving in
    {
      _getInstance( true, request_interval, mpi, io );
    }
    StorageController(StorageController const&) = delete;
    void operator=(StorageController const&) = delete;
    
    void fini();

    // CALLBACK FOR FTI HEADS
    static void callback();
    
    // API
    void load( int state_id );
    void store( int state_id );
    int protect( void* buffer, size_t size, io_type_t );

  private:
    
    static StorageController& _getInstance(bool init = false, int request_interval = -1, 
       MpiController & mpi = mpi_controller_null, std::unique_ptr<IoController> & io = unique_nullptr<IoController>() )
    {
      static StorageController instance{ init, request_interval, mpi, io };
      return instance;
    }
    
    StorageController( bool init, int request_interval, MpiController & mpi, std::unique_ptr<IoController> & io);

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
    PeerController* m_peer;
    MpiController& m_mpi;

    std::map<int,state_info_t> m_states;
    
    bool m_worker_thread;
    size_t m_request_counter;

    // FTI sleeps 500 us each iteration. Thus, a request interval of 2
    // coresponds to a server info request each second.
    int m_request_interval;
    
    int m_comm_model_size; 

};

#endif // _STORAGE_CONTROLLER_H_
