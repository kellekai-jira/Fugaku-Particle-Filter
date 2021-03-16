#ifndef _FTI_CONTROLLER_H_
#define _FTI_CONTROLLER_H_

#include <fti.h>
#include "fti_kernel.hpp"
#include "io_controller.hpp"
#include <vector>
#include <map>
#include <cassert>

class FtiController : public IoController {
  public:
    void init_io( MpiController* mpi );
    void init_core( MpiController* mpi );
    void fini();
    int protect( void* buffer, io_size_t size, io_type_t type );
    void update( io_id_t id, void* buffer, io_size_t size );
    void load( io_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void store( io_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void move( io_id_t state_id, io_level_t from, io_level_t to );
    void remove( io_id_t state_id, io_level_t level );
    void copy( io_id_t state_id, io_level_t from, io_level_t to );

    bool is_local( io_id_t state_id );
    bool is_global( io_id_t state_id );

    void request( io_id_t state_id );

    bool probe( io_tag_t tag );
    void sendrecv( void* send_buffer, void* recv_buffer, int n, io_tag_t tag, io_msg_t message_type  );
    void send( void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  );
    void isend( void* send_buffer, int size, io_tag_t tag, io_msg_t message_type, mpi_request_t & req  );
    void recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  );

    void register_callback( void (*f)(void) );
  private:
    std::map<io_level_t,FTIT_level> m_io_level_map;
    std::map<io_type_t,fti_id_t> m_io_type_map;
    std::map<io_msg_t,int> m_io_msg_map;
    std::map<io_tag_t,int> m_io_tag_map;
    std::map<io_id_t,io_var_t> m_var_id_map;
    io_id_t m_id_counter;
    FTI::Kernel m_kernel;
};

#endif // _FTI_CONTROLLER_H_
