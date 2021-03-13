#ifndef _FTI_CONTROLLER_H_
#define _FTI_CONTROLLER_H_

#include <fti.h>
#include "io_controller.hpp"
#include <vector>
#include <map>
#include <cassert>

class FtiController : public IoController {
  public:
    void init_io( MpiController* mpi );
    void init_core( MpiController* mpi );
    void fini();
    int protect( void* buffer, size_t size, io_type_t type );
    void load( int id, io_level_t level = IO_STORAGE_L1 );
    void store( int id, io_level_t level = IO_STORAGE_L1 );
    void move( int id, io_level_t from, io_level_t to );
    void remove( int id, io_level_t level );
    void copy( int id, io_level_t from, io_level_t to );

    bool is_local( int id );

    void request( int id );

    bool probe( io_tag_t tag );
    void sendrecv( void* send_buffer, void* recv_buffer, int n, io_tag_t tag, io_msg_t message_type  );
    void send( void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  );
    void recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  );

    void register_callback( void (*f)(void) );
  private:
    std::map<io_level_t,FTIT_level> m_io_level_map;
    std::map<io_type_t,fti_id_t> m_io_type_map;
    std::map<io_msg_t,int> m_io_msg_map;
    std::map<io_tag_t,int> m_io_tag_map;
    int m_id_counter;
};

#endif // _FTI_CONTROLLER_H_
