#ifndef _FTI_CONTROLLER_H_
#define _FTI_CONTROLLER_H_

#include <fti.h>
#include "fti_kernel.hpp"
#include "io_controller.hpp"
#include <vector>
#include <map>
#include <cassert>

struct io_state_id_t {
  io_state_id_t( io_id_t _t, io_id_t _id ) : t(_t), id(_id) {}
  io_state_id_t() : t(0), id(0) {}
  io_id_t t;
  io_id_t id;
};

struct io_var_t {
  void* data;
  size_t size;
  io_type_t type;
};

struct io_ckpt_t {
  io_state_id_t state_id;
  io_level_t level;
};

inline io_id_t to_ckpt_id(io_state_id_t state_id) {
  // this should work for up to 10000 members!
  assert(state_id.id < 10000 && "too many state_ids!");
  return state_id.t*10000 + state_id.id;
}

inline io_state_id_t to_state_id(const io_id_t ckpt_id) {
  // this should work for up to 10000 members!
  return { ckpt_id / 10000, ckpt_id % 10000 };
}

class FtiController : public IoController {
  public:
    void init_io( MpiController* mpi );
    void init_core();
    void fini();
    io_id_t protect( void* buffer, size_t size, io_type_t type );
    void update( io_id_t varid, void* buffer, size_t size );
    void load( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void store( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void remove( io_state_id_t state_id, io_level_t level );
    void copy( io_state_id_t state, io_level_t from, io_level_t to );
    void copy_extern( io_state_id_t state, io_level_t from, io_level_t to );
    void filelist_local( io_state_id_t ckpt_id, std::vector<std::string> & ckptfiles );
    void update_metadata( io_state_id_t ckpt_id, io_level_t level );

    bool is_local( io_state_id_t state_id );
    bool is_global( io_state_id_t state_id );

    void request( io_state_id_t state_id );

    bool probe( io_tag_t tag );
    void sendrecv( const void* send_buffer, void* recv_buffer, int send_size, int recv_size, io_tag_t tag, io_msg_t message_type  );
    void send( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  );
    void isend( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type, mpi_request_t & req  );
    void recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  );
    void get_message_size( int* size, io_tag_t tag, io_msg_t message_type  );

    void register_callback( void (*f)(void) );
  private:
    std::map<io_level_t,FTIT_level> m_io_level_map;
    std::map<io_type_t,fti_id_t> m_io_type_map;
    std::map<io_msg_t,int> m_io_msg_map;
    std::map<io_tag_t,int> m_io_tag_map;
    std::map<io_id_t,io_var_t> m_var_id_map;
    io_id_t m_id_counter;
    FTI::Kernel m_kernel;
    MpiController* m_mpi;
};

#endif // _FTI_CONTROLLER_H_
