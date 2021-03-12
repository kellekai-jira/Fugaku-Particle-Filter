#include "fti_interface.hpp"
#include <fti.h>
#include <iostream>

//using namespace FTI;     
int FtiController::protect( void* buffer, size_t size, io_type_t type ) {
  assert( m_io_type_map.count(type) != 0 && "invalid type" );
  FTI_Protect(m_id_counter, buffer, size, m_io_type_map[type]);
  m_id_counter++;
  return m_id_counter;
}

void FtiController::init( MpiController & mpi ) {
  FTI_Init("config.fti", mpi.comm() );
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_DOUBLE, FTI_DBLE ) );
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_INT, FTI_INTG ) );
  m_io_level_map.insert( std::pair<io_level_t,FTIT_level>( IO_STORAGE_L1, FTI_L1 ) );
  m_io_level_map.insert( std::pair<io_level_t,FTIT_level>( IO_STORAGE_L2, FTI_L4 ) );
  m_id_counter = 0;
}

void FtiController::fini() {
  FTI_Finalize();
}

void FtiController::load( int id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  FTI_Load( id, m_io_level_map[level] ); 
}

void FtiController::store( int id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  FTI_Checkpoint( id, m_io_level_map[level] );
}

void FtiController::move( int id, io_level_t from, io_level_t to ) {
  FTI_Move( id, from, to );
}

void FtiController::copy( int id, io_level_t from, io_level_t to ) {
  assert( m_io_level_map.count(from) != 0 && "invalid checkpoint level" );
  assert( m_io_level_map.count(to) != 0 && "invalid checkpoint level" );
  FTI_Copy( id, m_io_level_map[from], m_io_level_map[to] ); 
}

bool FtiController::is_local( int id ) {
  FTIT_stat st;
  FTI_Stat( id, &st );
  return FTI_ST_IS_LOCAL(st.level);
}

void FtiController::request( int id ) {

}

void FtiController::register_callback( void (*f)(void) ) {
  FTI_RegisterUserFunction( f );
}


