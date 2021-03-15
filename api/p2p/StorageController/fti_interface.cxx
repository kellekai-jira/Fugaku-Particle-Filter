#include "fti_interface.hpp"
#include <fti.h>
#include <iostream>

io_id_t fti_ckpt_id(int cycle, int state_id) {
    // this should work for up to 10000 members!
    assert(state_id < 10000 && "too many state_ids!");
    return cycle*10000 + state_id;
}

int FtiController::protect( void* buffer, size_t size, io_type_t type ) {
  assert( m_io_type_map.count(type) != 0 && "invalid type" );
  FTI_Protect(m_id_counter, buffer, size, m_io_type_map[type]);
  m_id_counter++;
  return m_id_counter;
}

void FtiController::init_io( MpiController* mpi ) {
  FTI_Init("config.fti", mpi->comm() );
}
      
void FtiController::init_core( MpiController* mpi ) {
  mpi->register_comm( "fti_comm_world", FTI_COMM_WORLD );
  mpi->set_comm( "fti_comm_world" );
  const FTIT_topology* const FTI_Topo = FTI_GetTopo();
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_DOUBLE, FTI_DBLE ) );
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_BYTE, FTI_CHAR ) );
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_INT, FTI_INTG ) );
  m_io_level_map.insert( std::pair<io_level_t,FTIT_level>( IO_STORAGE_L1, FTI_L1 ) );
  m_io_level_map.insert( std::pair<io_level_t,FTIT_level>( IO_STORAGE_L2, FTI_L4 ) );
  m_io_msg_map.insert( std::pair<io_msg_t,int>( IO_MSG_ALL, FTI_HEAD_MODE_COLL ) );
  m_io_msg_map.insert( std::pair<io_msg_t,int>( IO_MSG_MASTER, FTI_HEAD_MODE_SING ) );
  m_io_msg_map.insert( std::pair<io_msg_t,int>( IO_MSG_SELF, FTI_HEAD_MODE_SELF ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_REQUEST, IO_TAG_REQUEST + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_MESSAGE, IO_TAG_MESSAGE + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_ERASE, IO_TAG_ERASE + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_PULL, IO_TAG_PULL + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_PUSH, IO_TAG_PUSH + 1000000 ) );
  m_dict_int.insert( std::pair<std::string,int>( "nodes", FTI_Topo->nbNodes ) );
  m_dict_int.insert( std::pair<std::string,int>( "procs_node", FTI_Topo->nodeSize ) );
  m_dict_int.insert( std::pair<std::string,int>( "procs_total", FTI_Topo->nbProc ) );
  m_dict_bool.insert( std::pair<std::string,bool>( "master_node", FTI_Topo->masterLocal ) );
  m_dict_bool.insert( std::pair<std::string,bool>( "master_global", FTI_Topo->masterGlobal ) );
  m_id_counter = 0;
}
      
void FtiController::sendrecv( void* send_buffer, void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadRecv( recv_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
    FTI_HeadSend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  } else {
    FTI_AppSend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
    FTI_AppRecv( recv_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  }
}

void FtiController::send( void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadSend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  } else {
    FTI_AppSend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  }
}
      
void FtiController::isend( void* send_buffer, int size, io_tag_t tag, io_msg_t message_type, mpi_request_t & req  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadIsend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type], &req.mpi_request );
  } else {
    FTI_AppIsend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type], &req.mpi_request );
  }
}
      
void FtiController::recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadRecv( recv_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  } else {
    FTI_AppRecv( recv_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  }
}
    
bool FtiController::probe( io_tag_t tag ) {
  if( tag == IO_TAG_PULL ) {
    return !m_state_pull_requests.empty();
  } else if( tag == IO_TAG_PUSH ) {
    return !m_state_push_requests.empty();
  } else {
    return FTI_HeadProbe( m_io_tag_map[tag] );  
  }
}

void FtiController::fini() {
  FTI_Finalize();
}

void FtiController::load( io_id_t state_id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  FTI_Load( state_id, m_io_level_map[level] ); 
}

void FtiController::store( io_id_t state_id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  FTI_Checkpoint( state_id, m_io_level_map[level] );
}

void FtiController::move( io_id_t state_id, io_level_t from, io_level_t to ) {
  FTI_Move( state_id, from, to );
}
      
void FtiController::remove( io_id_t state_id, io_level_t level ) {
  FTI_Remove( state_id, m_io_level_map[level] );
}

void FtiController::copy( io_id_t state_id, io_level_t from, io_level_t to ) {
  assert( m_io_level_map.count(from) != 0 && "invalid checkpoint level" );
  assert( m_io_level_map.count(to) != 0 && "invalid checkpoint level" );
  FTI_Copy( state_id, m_io_level_map[from], m_io_level_map[to] ); 
}

bool FtiController::is_local( io_id_t state_id ) {
  FTIT_stat st;
  FTI_Stat( state_id, &st );
  return FTI_ST_IS_LOCAL(st.level);
}

void FtiController::request( io_id_t state_id ) {

}

void FtiController::register_callback( void (*f)(void) ) {
  FTI_RegisterUserFunction( f );
}


