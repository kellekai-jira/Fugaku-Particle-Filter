#include "storage_controller.h"

// TODO error checking!

void StorageController::m_load_core( int state_id ) {

}

void StorageController::m_load_user( int state_id ) {
  if( m_io->is_local( state_id ) ) {
    m_io->load( state_id, IO_STORAGE_L1 );
  }

}
    
void StorageController::m_store_core( int state_id ) {

}

void StorageController::m_store_user( int state_id ) {

}
 
