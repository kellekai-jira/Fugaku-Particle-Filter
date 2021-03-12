#include "storage_controller.hpp"

// TODO implement exceptions for error handling
// and remove asserts!

int StorageController::protect( void* buffer, size_t size, io_type_t type) {
  m_io->protect(buffer, size, type);  
}

void StorageController::copy( int id, io_level_t from, io_level_t to) {
  m_io->copy(id, from, to);  
}

void StorageController::m_load_core( int state_id ) {
  if( !m_io->is_local( state_id ) ) {
    m_peer->request( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    m_io->copy( state_id, IO_STORAGE_L2, IO_STORAGE_L1 );
  }
}

void StorageController::m_load_user( int state_id ) {
  if( !m_io->is_local( state_id ) ) {
    m_io->request( state_id );
  }
  assert( m_io->is_local( state_id ) && "unable to load state to local storage" );
  m_io->load( state_id );
}
    
void StorageController::m_store_core( int state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_store_user( int state_id ) {
  m_io->store( state_id );
  assert( m_io->is_local( state_id ) && "unable to store state to local storage" );
}
 
