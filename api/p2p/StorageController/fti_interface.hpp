#ifndef _FTI_CONTROLLER_H_
#define _FTI_CONTROLLER_H_

#include <fti.h>
#include "io_controller.hpp"
#include <vector>

//namespace FTI {

  static const int MPI_TAG_OFFSET = 1000000;

  enum mpi_tag_t {
    REQUEST = MPI_TAG_OFFSET,
    MESSAGE,
    ERASE,
    LOAD,
    COPY
  };

  class FtiController : public IoController {
    public:
      void init( MpiController & mpi );
      void fini();
      int protect( void* buffer, size_t size, io_type_t type );
      void load( int id, io_level_t level = IO_STORAGE_L1 );
      void store( int id, io_level_t level = IO_STORAGE_L1 );
      void move( int id, io_level_t from, io_level_t to );
      void copy( int id, io_level_t from, io_level_t to );

      bool is_local( int id );

      void request( int id );

      void register_callback( void (*f)(void) );
    private:
      int m_id_counter;
  };

//}
#endif // _FTI_CONTROLLER_H_
