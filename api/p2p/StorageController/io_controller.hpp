#ifndef _IO_CONTROLLER_H_
#define _IO_CONTROLLER_H_
#include <iostream>
#include <queue>
#include <exception>
#include "io_controller_defs.hpp"
#include "mpi_controller.hpp"
#include "mpi_controller_impl.hpp"

class IoController {
   public:
      virtual void init_io( int runner_id ) = 0;
      virtual void init_core() = 0;
      virtual void fini() = 0;
      virtual void set_state_size_per_proc( std::vector<uint64_t> vec ) = 0;
      virtual io_id_t protect( std::string name, void* buffer, size_t size, io_type_t type ) = 0;
      virtual bool is_local( io_state_id_t state_id ) = 0;
      virtual bool is_global( io_state_id_t state_id ) = 0;
      virtual void remove( io_state_id_t state_id, io_level_t level ) = 0;
      virtual void store( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 ) = 0;
      virtual void stage( io_state_id_t state, io_level_t from, io_level_t to ) = 0;
      virtual bool load( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 ) = 0;
      virtual void request( io_state_id_t state_id ) = 0;
      virtual bool probe( io_tag_t tag ) = 0;
      virtual void register_callback( void (*f)(void) ) = 0;
      virtual void sendrecv( const void* send_buffer, void* recv_buffer, int send_size, int recv_size, io_tag_t tag, io_msg_t message_type  ) = 0;
      virtual void send( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  ) = 0;
      virtual void isend( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type, mpi_request_t & req  ) = 0;
      virtual void recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  ) = 0;
      virtual void get_message_size( int* size, io_tag_t tag, io_msg_t message_type  ) = 0;
      virtual void filelist_local( io_state_id_t ckpt_id, std::vector<std::string> & ckptfiles ) = 0;
      virtual void update_metadata( io_state_id_t ckpt_id, io_level_t level ) = 0;

      std::map<std::string,int> m_dict_int;
      std::map<std::string,bool> m_dict_bool;
      std::map<std::string,double> m_dict_double;
      std::map<std::string,std::string> m_dict_string;

      std::queue<io_state_id_t> m_state_pull_requests;
      std::queue<io_state_id_t> m_state_push_requests;
      std::queue<io_state_id_t> m_state_dump_requests;

};

class IoException : public std::runtime_error {
  const char* file;
  int line;
  const char* func;
  const char* info;

  public:
  IoException(const char* msg, const char* file_, int line_, const char* func_, const char* info_ = "") : std::runtime_error(msg),
    file (file_),
    line (line_),
    func (func_),
    info (info_)
    {
    }

  const char* get_file() const { return file; }
  int get_line() const { return line; }
  const char* get_func() const { return func; }
  const char* get_info() const { return info; }
};

#endif // _IO_CONTROLLER_H_
