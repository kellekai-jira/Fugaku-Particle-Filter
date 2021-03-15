#include "storage_controller.hpp"

#include "utils.h"
#include <memory>
#include <numeric>

// TODO error checking!

//#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

// (1) state request from user to worker
void StorageController::m_state_request_user() {
  if( m_io->probe( IO_TAG_REQUEST ) ) {
    io_id_t *state_id = new io_id_t[m_io->m_dict_int["procs_app"]];
    int result=(int)IO_SUCCESS; 
    m_io->recv( state_id, sizeof(io_id_t), IO_TAG_REQUEST, IO_MSG_ALL );
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
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received INFORMATION request" << std::endl;
    state_info_t new_state_info;
    io_id_t info[2]; // [0]->active [1]->finished state id
    m_io->recv( &info, sizeof(io_id_t)*2, IO_TAG_MESSAGE, IO_MSG_MASTER );
    if( m_states.count( info[0] ) == 0 ) {
      new_state_info.status = MELISSA_STATE_BUSY;
      m_states.insert( std::pair<io_id_t, state_info_t>( info[0], new_state_info ) );
    } else {
      m_states[info[0]].status = MELISSA_STATE_BUSY;
    }
    if( m_states.count( info[1] ) == 0 ) {
      new_state_info.status = MELISSA_STATE_IDLE;
      m_states.insert( std::pair<io_id_t, state_info_t>( info[1], new_state_info ) );
    } else {
      m_states[info[1]].status = MELISSA_STATE_IDLE;
    }
    m_io->m_state_push_requests.push(info[1] );
  }
}

// organize storage
void StorageController::m_state_request_push() {
  if( m_io->probe( IO_TAG_PUSH ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PUSH request: " << std::endl;
    m_io->copy( m_io->m_state_push_requests.front(), IO_STORAGE_L1, IO_STORAGE_L2 );
    m_io->m_state_push_requests.pop();
  }
}

void StorageController::m_finalize_worker() {
  if( m_io->probe( IO_TAG_FINAL ) ) {
    int dummy;
    m_io->recv( &dummy, sizeof(int), IO_TAG_FINAL, IO_MSG_MASTER );
    m_state_info_user();
    while( m_io->probe( IO_TAG_PUSH ) ) {
      if(m_io->m_dict_bool["master_global"]) std::cout << "head received PUSH request: " << std::endl;
      m_io->copy( m_io->m_state_push_requests.front(), IO_STORAGE_L1, IO_STORAGE_L2 );
      m_io->m_state_push_requests.pop();
    }
    while( m_io->probe( IO_TAG_PULL ) ) {
      if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
      load( m_io->m_state_pull_requests.front() );
      m_io->m_state_pull_requests.pop();
    }
    m_io->send( &dummy, sizeof(int), IO_TAG_FINAL, IO_MSG_MASTER );
  }
}

void StorageController::m_state_request_remove() {

}

void StorageController::m_state_request_load() {
  if( m_io->probe( IO_TAG_PULL ) ) {
    if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
    load( m_io->m_state_pull_requests.front() );
    m_io->m_state_pull_requests.pop();
  }
}

void StorageController::m_query_server() {
  // update peers 
  static int count = 0; 
  // update states
  // [dummy impl zum testen]
  if(count == 30) {
    for(int i=2; i<11; i++) {
      m_io->m_state_pull_requests.push( i );
    }
  }
  count++;
}
