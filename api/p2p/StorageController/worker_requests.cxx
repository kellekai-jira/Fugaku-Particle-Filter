#include "storage_controller.hpp"

#include "utils.h"
#include <memory>
#include <numeric>

// TODO error checking!

#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

// (1) state request from user to worker
void StorageController::m_state_request_user() {
  if( m_io->probe( IO_TAG_REQUEST ) ) {
    int *state_id = new int[m_io->m_dict_int["procs_app"]];
    int result=(int)IO_SUCCESS; 
    m_io->recv( state_id, sizeof(int), IO_TAG_REQUEST, IO_MSG_ALL );
    load( state_id[0] );
    m_io->send( &result, sizeof(int), IO_TAG_REQUEST, IO_MSG_ALL );
    delete[] state_id;
  }
}

// (2) state request from peer to worker
void StorageController::m_state_request_peer() {
}

// (3) state info from user
void StorageController::m_state_info_user() {
  if( m_io->probe( IO_TAG_MESSAGE ) ) {
    state_info_t new_state_info;
    int info[2]; // [0]->active [1]->finished state id
    m_io->recv( &info, sizeof(int)*2, IO_TAG_MESSAGE, IO_MSG_MASTER );
    if( m_states.count( info[0] ) == 0 ) {
      new_state_info.status = MELISSA_STATE_BUSY;
      m_states.insert( std::pair<int, state_info_t>( info[0], new_state_info ) );
    } else {
      m_states[info[0]].status = MELISSA_STATE_BUSY;
    }
    if( info[1] != -1 ) { // on initialization finished state is -1
      if( m_states.count( info[1] ) == 0 ) {
        new_state_info.status = MELISSA_STATE_IDLE;
        m_states.insert( std::pair<int, state_info_t>( info[1], new_state_info ) );
      } else {
        m_states[info[1]].status = MELISSA_STATE_IDLE;
      }
    }
  }
}

// organize storage
void StorageController::m_prefetch() {

}
void StorageController::m_erase() {

}
void StorageController::m_move() {

}
void StorageController::m_push() {

}

void StorageController::m_query_server() {

}
