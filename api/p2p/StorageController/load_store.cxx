#include "storage_controller.h"

// TODO implement exceptions for error handling
// and remove asserts!

void StorageController::m_load_core( int state_id ) {
  if( !m_io->is_local( state_id ) ) {
    m_peer->request( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    m_io->copy( state_id, IO_STORAGE_L2, IO_STORAGE_L1 );
  }
  assert( m_io->is_local( state_id ) && "unable to load state to local storage" );
}

void StorageController::m_load_user( int state_id ) {
  if( !m_io->is_local( state_id ) ) {
    m_io->request( state_id );
  }
  m_io->load( state_id );
}
    
void StorageController::m_store_core( int state_id ) {
  m_io->store( state_id );
  assert( m_io->is_local( state_id ) && "unable to store state to local storage" );
}

void StorageController::m_store_user( int state_id ) {
  
}
 
