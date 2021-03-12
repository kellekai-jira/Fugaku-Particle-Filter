#ifndef _IO_CONTROLLER_H_
#define _IO_CONTROLLER_H_

#include "mpi_controller.hpp"

enum io_level_t {
  IO_STORAGE_L1,
  IO_STORAGE_L2,
  IO_STORAGE_L3,
  IO_STORAGE_L4,
};

enum io_type_t {
  IO_DOUBLE,
  IO_INT,
};


class IoController {
   public:
      virtual void init( MpiController & mpi ) = 0;
      virtual void fini() = 0;
      virtual int protect( void* buffer, size_t size, io_type_t type ) = 0;
      virtual bool is_local( int id ) = 0;
      virtual void move( int id, io_level_t from, io_level_t to ) = 0;
      virtual void store( int id, io_level_t level = IO_STORAGE_L1 ) = 0;
      virtual void copy( int id, io_level_t from, io_level_t to ) = 0;
      virtual void load( int id, io_level_t level = IO_STORAGE_L1 ) = 0;
      virtual void request( int id ) = 0;
      virtual void register_callback( void (*f)(void) ) = 0;
};

#endif // _IO_CONTROLLER_H_
