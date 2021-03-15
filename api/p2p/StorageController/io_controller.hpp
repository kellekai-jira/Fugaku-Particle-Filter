#ifndef _IO_CONTROLLER_H_
#define _IO_CONTROLLER_H_
#include <iostream>
#include <queue>

#include "mpi_controller.hpp"

enum io_level_t {
  IO_STORAGE_L1,
  IO_STORAGE_L2,
  IO_STORAGE_L3,
  IO_STORAGE_L4,
};

enum io_result_t {
  IO_SUCCESS,
  IO_FAILURE,
};

enum io_type_t {
  IO_DOUBLE,
  IO_INT,
};

enum io_msg_t {
  IO_MSG_ALL,
  IO_MSG_MASTER,
  IO_MSG_SELF
};
  
enum io_tag_t {
  IO_TAG_REQUEST,
  IO_TAG_MESSAGE,
  IO_TAG_FINAL,
  IO_TAG_ERASE,
  IO_TAG_PULL,
  IO_TAG_PUSH
};


class IoController {
   public:
      virtual void init_io( MpiController* mpi ) = 0;
      virtual void init_core( MpiController* mpi ) = 0;
      virtual void fini() = 0;
      virtual int protect( void* buffer, size_t size, io_type_t type ) = 0;
      virtual bool is_local( int id ) = 0;
      virtual void move( int id, io_level_t from, io_level_t to ) = 0;
      virtual void remove( int id, io_level_t level ) = 0;
      virtual void store( int id, io_level_t level = IO_STORAGE_L1 ) = 0;
      virtual void copy( int id, io_level_t from, io_level_t to ) = 0;
      virtual void load( int id, io_level_t level = IO_STORAGE_L1 ) = 0;
      virtual void request( int id ) = 0;
      virtual bool probe( io_tag_t tag ) = 0;
      virtual void register_callback( void (*f)(void) ) = 0;
      virtual void sendrecv( void* send_buffer, void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  ) = 0;
      virtual void send( void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  ) = 0;
      virtual void isend( void* send_buffer, int size, io_tag_t tag, io_msg_t message_type, mpi_request_t & req  ) = 0;
      virtual void recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  ) = 0;
      
      std::map<std::string,int> m_dict_int;
      std::map<std::string,bool> m_dict_bool;
      std::map<std::string,double> m_dict_double;
      std::map<std::string,std::string> m_dict_string;
    
      std::queue<int> m_state_pull_requests; 
      std::queue<int> m_state_push_requests; 
};

#endif // _IO_CONTROLLER_H_
