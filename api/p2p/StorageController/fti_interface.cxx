#include "fti_interface.hpp"
#include <fti.h>
#include <iostream>

//using namespace FTI;     
int FtiController::protect( void* buffer, size_t size, io_type_t type ) {
  switch (type) {
    case IO_DOUBLE:
      FTI_Protect(m_id_counter, buffer, size, FTI_DBLE);
      m_id_counter++;
      break;
    case IO_INT:
      FTI_Protect(m_id_counter, buffer, size, FTI_INTG);
      m_id_counter++;
      break;
    default:
      std::cout << "Invalid Type" << std::endl;
  }  
  return m_id_counter;
}

void FtiController::init( MpiController & mpi ) {
  FTI_Init("config.fti", mpi.comm() );
  m_id_counter = 0;
}

void FtiController::fini() {
  FTI_Finalize();
}

void FtiController::load( int id, io_level_t level ) {
  switch( level ) {
    case IO_STORAGE_L1:
      FTI_Load( id, FTI_L1 ); 
      break;
    case IO_STORAGE_L2:
      FTI_Load( id, FTI_L4 ); 
      break;
    default:
      std::cout << "invalid checkpoint level '" << level << "'" << std::endl;
  }
}

void FtiController::store( int id, io_level_t level ) {
  switch( level ) {
    case IO_STORAGE_L1:
      FTI_Checkpoint( id, FTI_L1 );
      break;
    case IO_STORAGE_L2:
      FTI_Checkpoint( id, FTI_L4 );
      break;
    default:
      std::cout << "invalid checkpoint level '" << level << "'" << std::endl;
  }
}

void FtiController::move( int id, io_level_t from, io_level_t to ) {
  FTI_Move( id, from, to );
}

void FtiController::copy( int id, io_level_t from, io_level_t to ) {

}

bool FtiController::is_local( int id ) {
  FTIT_stat st;
  FTI_Stat( id, &st );
  return FTI_ST_IS_LOCAL(st.level);
}

void FtiController::request( int id ) {

}

void FtiController::register_callback( void (*f)(void) ) {

}


