#ifndef _STORAGE_CONTROLLER_H_
#define _STORAGE_CONTROLLER_H_

#include "utils.h"
#include "mpi_controller.hpp"
#include "io_controller.hpp"
#include "peer_controller.hpp"
#include "fti_controller.hpp"
#include <cstddef>
#include <cassert>
#include <memory>
#include <vector>
#include "helpers.h"
#include "ZeroMQ.h"

#include "p2p.pb.h"

#define IO_PROBE( tag, func ) do { if( storage.m_io->probe( tag ) ) {func; return;} } while(0)

#define STORAGE_MAX_PREFETCH 4 // 5 states

using namespace melissa_p2p;

static MpiController mpi_controller_null;

class StorageController {

  public:
    StorageController() :
      m_worker_thread(true),
      m_request_counter(1),
      m_request_interval(10000) {}

    void io_init( MpiController* mpi, IoController* io );
    void init( double capacity, double state_size );
    void fini();

    // API
    void load( io_state_id_t state );
    void store( io_state_id_t state_id );
    int protect( void* buffer, size_t size, io_type_t );
    int update( io_id_t id, void* buffer, size_t size );

    // TODO put in private
    void m_create_symlink( io_state_id_t state_id );

  private:

    // CALLBACK FOR FTI HEADS
    static void callback();

    void pull( io_state_id_t state_id );
    void copy( io_state_id_t state_id, io_level_t from, io_level_t to );

    void m_push_weight_to_server( const Message & m );

    void m_load_head( io_state_id_t state );
    void m_load_user( io_state_id_t state );

    void m_pull_head( io_state_id_t state_id );
    void m_pull_user( io_state_id_t state_id );

    void m_store_head( io_state_id_t state_id );
    void m_store_user( io_state_id_t state_id );

    void m_request_fini();

    // (1) state request from user to worker
    void m_request_load();

    // (2) state request from peer to worker
    void m_request_peer();

    // (3) state info from user
    void m_request_post();

    // organize storage
    void m_request_push();
    void m_request_dump();
    void m_request_pull();

    // request state cache and peer info from server
    void m_query_server();

    bool m_trigger_query() { return (m_request_interval % m_request_counter++) == 0; }

//----------------------------------------------------------------------------------------
//  VARIABLES
//----------------------------------------------------------------------------------------

    IoController* m_io;
    PeerController* m_peer;
    MpiController* m_mpi;

    void* m_zmq_context;
    int m_runner_id;
    int m_cycle;

    std::map<io_id_t,io_state_id_t> m_ckpted_states;
    std::map<io_id_t,io_state_id_t> m_cached_states;

    bool m_worker_thread;
    size_t m_request_counter;

    // FTI sleeps 500 us each iteration. Thus, a request interval of 2
    // coresponds to a server info request each second.
    int m_request_interval;

    class StatePool {
      public:
        StatePool() : m_used_slots(0) {}
        void init( size_t capacity );
        size_t free() { return m_capacity - m_used_slots; }
        size_t size() { return m_used_slots; }
        size_t capacity() { return m_capacity; }
        StatePool& operator++();
        StatePool operator++(int);
        StatePool& operator--();
        StatePool operator--(int);
      private:
        size_t m_capacity;
        ssize_t m_used_slots;
        friend class StorageController;
    };

    friend class StatePool;

    StatePool state_pool;

  public:  // public so peer_controller can access it. feel free to friend!
//----------------------------------------------------------------------------------------
//  SERVER CONNECTION
//----------------------------------------------------------------------------------------

    class Server {
      public:
        void init();
        void prefetch_request( StorageController* storage );
        void delete_request( StorageController* storage );
        void fini();
        void* m_socket;
      //private:
        //friend class StorageController;
    };

    //friend class Server;
    Server server;

};

#endif // _STORAGE_CONTROLLER_H_
