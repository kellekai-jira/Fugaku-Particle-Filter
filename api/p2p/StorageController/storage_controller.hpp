#ifndef _STORAGE_CONTROLLER_H_
#define _STORAGE_CONTROLLER_H_

#include "mpi_controller.hpp"
#include "io_controller.hpp"
#include "peer_controller.hpp"
#include "fti_controller.hpp"
#include <cstddef>
#include <cassert>
#include <memory>
#include <vector>

#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

inline io_id_t generate_state_id(int cycle, int parent_id) {
    // this should work for up to 10000 members!
    assert(parent_id < 10000 && "too many state_ids!");
    return cycle*10000 + parent_id;
}

static MpiController mpi_controller_null;

class StorageController {
  
  public:
    StorageController() :
      m_worker_thread(true),
      m_request_counter(0),
      m_request_interval(1) {}

    void init( MpiController* mpi, IoController* io,
      size_t capacity, size_t checkpoint_size );
    void fini();

    // CALLBACK FOR FTI HEADS
    static void callback();
    
    // API
    void load( io_id_t state_id );
    void store( io_id_t state_id );
    void copy( io_id_t state_id, io_level_t from, io_level_t to );
    int protect( void* buffer, size_t size, io_type_t );
    int update( io_id_t id, void* buffer, io_size_t size );

  private:
    
    void m_load_head( io_id_t state_id );
    void m_load_user( io_id_t state_id );
    
    void m_store_head( io_id_t state_id );
    void m_store_user( io_id_t state_id );
   
    void m_finalize_worker();

    // (1) state request from user to worker
    void m_state_request_user();
    
    // (2) state request from peer to worker
    void m_state_request_peer();
    
    // (3) state info from user
    void m_state_info_user();
    
    // organize storage
    void m_state_request_push();
    void m_state_request_dump();
    void m_state_request_load();
    
    // request state cache and peer info from server
    void m_query_server();

    bool m_trigger_query() { return (m_request_interval % m_request_counter++) == 0; }

//----------------------------------------------------------------------------------------
//  VARIABLES
//----------------------------------------------------------------------------------------
    
    IoController* m_io;
    PeerController* m_peer;
    MpiController* m_mpi;

    int m_runner_id;
    int m_cycle;

    std::map<io_id_t,io_state_t> m_states;
    
    bool m_worker_thread;
    size_t m_request_counter;

    // FTI sleeps 500 us each iteration. Thus, a request interval of 2
    // coresponds to a server info request each second.
    int m_request_interval;
    
    int m_comm_global_size; 
    int m_comm_runner_size; 
    int m_comm_worker_size; 

    // size in bytes
    size_t m_capacity;
    size_t m_checkpoint_size;
    
    // number of states
    int m_prefetch_capacity;

//----------------------------------------------------------------------------------------
//  SERVER CONNECTION
//----------------------------------------------------------------------------------------

    class Server {
      public:
        Server();
        void init_zmq_context( void* context );
        void request( StorageController* storage ); 
        void response( StorageController* storage ); 
      private:
        void update_peers( );
        void update_states( );
    };

    friend class Server;

    Server server;

};

#endif // _STORAGE_CONTROLLER_H_
