#ifndef _FTI_CONTROLLER_H_
#define _FTI_CONTROLLER_H_

#include <fti.h>
#include "fti_kernel.hpp"
#include "io_controller.hpp"
#include "zip_controller.hpp"
#include <vector>
#include <map>
#include <cassert>
#include "mpi_controller_impl.hpp"

inline bool operator==(const io_state_id_t& lhs, const io_state_id_t& rhs) {
    return lhs.t == rhs.t && lhs.id == rhs.id;
}

inline bool operator!=(const io_state_id_t& lhs, const io_state_id_t& rhs) {
    return !(lhs == rhs);
}

int64_t elegantPair( int64_t x, int64_t y );

std::pair<int64_t,int64_t> elegantUnpair(int64_t);

inline int64_t to_ckpt_id(io_state_id_t state_id, int mode) {
  // this should work for up to 100000 members!
  return elegantPair( mode, elegantPair( state_id.id, state_id.t ) );
}

inline io_state_id_t to_state_id(const int64_t ckpt_id) {
  // this should work for up to 100000 members!
  std::pair<int64_t,int64_t> m_idt = elegantUnpair( ckpt_id );
  std::pair<int64_t,int64_t> idt = elegantUnpair( m_idt.second );
  return { idt.first, idt.second };
}

inline int64_t to_ckpt_id(io_state_id_t state_id) {
  // this should work for up to 100000 members!
  int64_t hash = elegantPair( 0, elegantPair( state_id.id, state_id.t ) );
  io_state_id_t state = to_state_id( hash );
  MDBG("[TO_CKPT_ID] id: %d, t: %d [before]", state_id.id, state_id.t);
  MDBG("[TO_CKPT_ID] id: %d, t: %d [after]", state.id, state.t);
  return elegantPair( 0, elegantPair( state_id.id, state_id.t ) );
}


class FtiController : public IoController {
  public:
    void init_io( int runner_id );
    void init_core();
    void fini();
    int protect( std::string name, void* buffer, size_t size, io_type_t type );
    bool load( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void store( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void remove( io_state_id_t state_id, io_level_t level );
    void stage( io_state_id_t state, io_level_t from, io_level_t to );
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
    
    void stage_l1l2( std::string L1_CKPT, std::string L1_META, std::string L2_TEMP, std::string L2_CKPT, io_state_id_t state_id );
    void stage_l2l1( std::string L2_CKPT, std::string L1_TEMP, std::string L1_META_TEMP,
      std::string L1_CKPT, std::string L1_META_CKPT, io_state_id_t state_id );
		void init_compression_parameter();
    
    std::map<io_level_t,FTIT_level> m_io_level_map;
    std::map<io_type_t,fti_id_t> m_io_type_map;
    //std::map<io_zip_type_t,FTIT_CPC_TYPE> m_io_zip_type_map;
    //std::map<io_zip_mode_t,FTIT_CPC_MODE> m_io_zip_mode_map;
    //std::map<FTIT_CPC_TYPE,io_zip_type_t> m_io_zip_type_inv_map;
    //std::map<FTIT_CPC_MODE,io_zip_mode_t> m_io_zip_mode_inv_map;
    std::map<io_msg_t,int> m_io_msg_map;
    std::map<io_tag_t,int> m_io_tag_map;
    std::map<std::string, io_var_t> m_var_id_map;
    //std::map<std::string, io_zip_t> m_var_zip_map;
    int m_next_garbage_coll;
    io_id_t m_id_counter;  
    int m_last_cycle;
    FTI::Kernel m_kernel;
    ZipController m_zip_controller;
    int m_runner_id;
};

#endif // _FTI_CONTROLLER_H_
