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

#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

#define IO_PROBE( tag, func ) do { if( storage.m_io->probe( tag ) ) {func; return;} } while(0)

#define PROTOBUF_MAX_SIZE 512

using namespace melissa_p2p;

static MpiController mpi_controller_null;

class StorageController {

  public:
    StorageController() :
      m_worker_thread(true),
      m_request_counter(1),
      m_request_interval(10000) {}

    void init( MpiController* mpi, IoController* io,
      size_t capacity, size_t state_size );
    void fini();

    // CALLBACK FOR FTI HEADS
    static void callback();

    // API
    void load( io_state_t state );
    void pull( io_id_t state_id );
    int free() { return m_free; }
    void store( io_id_t state_id );
    void copy( io_id_t state_id, io_level_t from, io_level_t to );
    int protect( void* buffer, size_t size, io_type_t );
    int update( io_id_t id, void* buffer, io_size_t size );

    // TODO put in private
    void m_create_symlink( io_id_t ckpt_id );

  private:

    void m_push_weight_to_server( io_state_t state_info, double weight );

    template<typename T> void m_serialize( T& message, char* buffer );
    template<typename T> void m_deserialize( T& message, char* buffer, int size );

    void m_load_head( io_state_t state );
    void m_load_user( io_state_t state );

    void m_pull_head( io_id_t state_id );
    void m_pull_user( io_id_t state_id );

    void m_store_head( io_id_t state_id );
    void m_store_user( io_id_t state_id );

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

    int m_runner_id;
    int m_cycle;

    std::map<io_id_t,io_state_t> m_known_states;
    std::map<io_id_t,io_state_t> m_cached_states;

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
    size_t m_state_size_node;

    // number of states
    int m_prefetch_capacity;
    int m_free;

//----------------------------------------------------------------------------------------
//  SERVER CONNECTION
//----------------------------------------------------------------------------------------

    class Server {
      public:
        void init();
        void prefetch_request( StorageController* storage );
        void fini();
      private:
        void* m_socket;
    };

    friend class Server;

    Server server;

};

#endif // _STORAGE_CONTROLLER_H_
