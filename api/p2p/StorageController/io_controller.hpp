#ifndef _IO_CONTROLLER_H_
#define _IO_CONTROLLER_H_
#include <iostream>
#include <queue>
#include <exception>

#include "mpi_controller.hpp"

#define IO_TRY( expr, result, msg ) do { \
  if( (expr) != (result) ) \
    throw IoException( (msg), __FILE__, __LINE__, __func__); \
  } while(0)

const size_t IO_TRANSFER_SIZE = 16*1024*1024; // 16 Mb

typedef int io_id_t;

struct io_var_t;

struct io_ckpt_t;

struct io_state_id_t;

enum io_status_t {
  IO_STATE_BUSY,
  IO_STATE_IDLE,
  IO_STATE_DONE
};

enum io_level_t {
  IO_STORAGE_L1,
  IO_STORAGE_L2,
  IO_STORAGE_L3,
  IO_STORAGE_L4
};

enum io_result_t {
  IO_SUCCESS,
  IO_FAILURE
};

enum io_type_t {
  IO_DOUBLE,
  IO_BYTE,
  IO_INT,
  IO_USER1,
  IO_USER2,
  IO_USER3
};

enum io_msg_t {
  IO_MSG_ALL,
  IO_MSG_ONE
};

enum io_tag_t {
  IO_TAG_LOCK,
  IO_TAG_FREE,
  IO_TAG_LOAD,
  IO_TAG_PEER,
  IO_TAG_PULL,
  IO_TAG_PUSH,
  IO_TAG_POST,
  IO_TAG_DUMP,
  IO_TAG_FINI
};

class IoController {
   public:
      virtual void init_io( MpiController* mpi ) = 0;
      virtual void init_core() = 0;
      virtual void fini() = 0;
      virtual io_id_t protect( void* buffer, size_t size, io_type_t type ) = 0;
      virtual void update( io_id_t, void* buffer, size_t size ) = 0;
      virtual bool is_local( io_state_id_t state_id ) = 0;
      virtual bool is_global( io_state_id_t state_id ) = 0;
      virtual void remove( io_state_id_t state_id, io_level_t level ) = 0;
      virtual void store( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 ) = 0;
      virtual void copy( io_state_id_t state, io_level_t from, io_level_t to ) = 0;
      virtual void copy_extern( io_state_id_t state, io_level_t from, io_level_t to ) = 0;
      virtual void load( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 ) = 0;
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
      std::queue<io_ckpt_t> m_state_dump_requests;
      MpiController* m_mpi;
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
