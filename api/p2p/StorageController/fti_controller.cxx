#include "fti_controller.hpp"
#include <fti.h>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

int FtiController::protect( void* buffer, size_t size, io_type_t type ) {
  assert( m_io_type_map.count(type) != 0 && "invalid type" );
  FTI_Protect(m_id_counter, buffer, size, m_io_type_map[type]);
  io_var_t variable = { buffer, size, type };
  m_var_id_map.insert( std::pair<io_id_t,io_var_t>( m_id_counter, variable ) );
  return m_id_counter++;
}

void FtiController::update( io_id_t id, void* buffer, size_t size ) {
  assert( m_var_id_map.count(id) != 0 && "invalid type" );
  FTI_Protect(id, buffer, size, m_var_id_map[id].type);
}

void FtiController::init_io( MpiController* mpi ) {
  m_mpi = mpi;
  FTI_Init("config.fti", mpi->comm() );
}

void FtiController::init_core() {
  // FTI_COMM_WORLD is the MPI_COMM_WORLD replacement
  // so FTI_COMM_WORLD contains all app cores if you are on an app core
  // and containes all FTI headranks if you are on an head rank
  m_mpi->register_comm( "fti_comm_world", FTI_COMM_WORLD );
  m_mpi->set_comm( "fti_comm_world" );
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_DOUBLE, FTI_DBLE ) );
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_BYTE, FTI_CHAR ) );
  m_io_type_map.insert( std::pair<io_type_t,fti_id_t>( IO_INT, FTI_INTG ) );
  m_io_level_map.insert( std::pair<io_level_t,FTIT_level>( IO_STORAGE_L1, FTI_L1 ) );
  m_io_level_map.insert( std::pair<io_level_t,FTIT_level>( IO_STORAGE_L2, FTI_L4 ) );
  m_io_msg_map.insert( std::pair<io_msg_t,int>( IO_MSG_ALL, FTI_HEAD_MODE_COLL ) );
  m_io_msg_map.insert( std::pair<io_msg_t,int>( IO_MSG_ONE, FTI_HEAD_MODE_SING ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_LOAD, IO_TAG_LOAD + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_POST, IO_TAG_POST + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_DUMP, IO_TAG_DUMP + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_PULL, IO_TAG_PULL + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_PUSH, IO_TAG_PUSH + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_FINI, IO_TAG_FINI + 1000000 ) );
  m_dict_int.insert( std::pair<std::string,int>( "nodes", m_kernel.topo->nbNodes ) );
  m_dict_int.insert( std::pair<std::string,int>( "procs_node", m_kernel.topo->nodeSize ) );
  int app_procs_node = m_kernel.topo->nodeSize - m_kernel.topo->nbHeads;
  m_dict_int.insert( std::pair<std::string,int>( "app_procs_node", app_procs_node ) );
  m_dict_int.insert( std::pair<std::string,int>( "procs_total", m_kernel.topo->nbProc ) );
  m_dict_bool.insert( std::pair<std::string,bool>( "master_local", m_kernel.topo->masterLocal ) );
  m_dict_bool.insert( std::pair<std::string,bool>( "master_global", m_kernel.topo->masterGlobal ) );
  auto strip_id = [](std::string path, std::string id) {
    auto start_position_to_erase = path.find(std::string("/"+id));
    path.erase(start_position_to_erase, path.size());
    return path;
  };
  m_dict_string.insert( std::pair<std::string,std::string>( "global_dir", strip_id(m_kernel.conf->glbalDir,m_kernel.exec->id) ) );
  m_dict_string.insert( std::pair<std::string,std::string>( "local_dir", strip_id(m_kernel.conf->localDir,m_kernel.exec->id) ) );
  m_dict_string.insert( std::pair<std::string,std::string>( "meta_dir", strip_id(m_kernel.conf->metadDir,m_kernel.exec->id) ) );
  m_dict_string.insert( std::pair<std::string,std::string>( "exec_id", m_kernel.exec->id ) );
  m_id_counter = 0;
}

void FtiController::sendrecv( const void* send_buffer, void* recv_buffer, int send_size, int recv_size, io_tag_t tag, io_msg_t message_type  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadRecv( recv_buffer, recv_size, m_io_tag_map[tag], m_io_msg_map[message_type] );
    FTI_HeadSend( send_buffer, send_size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  } else {
    FTI_AppSend( send_buffer, send_size, m_io_tag_map[tag], m_io_msg_map[message_type] );
    FTI_AppRecv( recv_buffer, recv_size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  }
}

void FtiController::send( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadSend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  } else {
    FTI_AppSend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  }
}

void FtiController::isend( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type, mpi_request_t & req  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadIsend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type], &req.mpi_request );
  } else {
    FTI_AppIsend( send_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type], &req.mpi_request );
  }
}

void FtiController::recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  ) {
  static int count = 0;
  if( FTI_AmIaHead() ) {
    std::cout << "called : " << count << " times ["<<tag<<"]" << std::endl;
    FTI_HeadRecv( recv_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
    count++;
  } else {
    FTI_AppRecv( recv_buffer, size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  }
}

bool FtiController::probe( io_tag_t tag ) {
  if( tag == IO_TAG_PULL ) {
    return !m_state_pull_requests.empty();
  } else if( tag == IO_TAG_PUSH ) {
    return !m_state_push_requests.empty();
  } else if( tag == IO_TAG_DUMP ) {
    return !m_state_dump_requests.empty();
  } else {
    return FTI_HeadProbe( m_io_tag_map[tag] );
  }
}

void FtiController::get_message_size( int* size, io_tag_t tag, io_msg_t message_type  ) {
  if( FTI_AmIaHead() ) {
    FTI_HeadGetMessageSize( size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  } else {
    FTI_AppGetMessageSize( size, m_io_tag_map[tag], m_io_msg_map[message_type] );
  }
}

void FtiController::fini() {
  FTI_Finalize();
}

void FtiController::load( io_state_id_t state_id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  FTI_Load( to_ckpt_id(state_id), m_io_level_map[level] );
}

void FtiController::store( io_state_id_t state_id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  FTI_Checkpoint( to_ckpt_id(state_id), m_io_level_map[level] );
}

void FtiController::remove( io_state_id_t state_id, io_level_t level ) {
  IO_TRY( FTI_Remove( to_ckpt_id(state_id), m_io_level_map[level] ),
      FTI_SCES, "failed to remove file" );
}

void FtiController::copy( io_state_id_t state_id, io_level_t from, io_level_t to ) {
  assert( m_io_level_map.count(from) != 0 && "invalid checkpoint level" );
  assert( m_io_level_map.count(to) != 0 && "invalid checkpoint level" );
  if( from == IO_STORAGE_L1 ) {
    FTI_Copy( to_ckpt_id(state_id), m_io_level_map[from], m_io_level_map[to] );
  } else {
    copy_extern( state_id, from, to );
  }
}

void FtiController::copy_extern( io_state_id_t state_id, io_level_t from, io_level_t to ) {

  assert( m_kernel.topo->amIaHead == 1 && "copy for application threads not implemented for extern" );
  assert( from == IO_STORAGE_L2 && to == IO_STORAGE_L1 && "copy from level 1 to level 2 not implemented for extern" );

  std::stringstream global;
  global << m_dict_string["global_dir"];
  global << "/";
  global << std::to_string(to_ckpt_id(state_id));

  std::stringstream extern_meta;
  extern_meta << m_dict_string["meta_dir"];
  extern_meta << "/";
  extern_meta << std::to_string(to_ckpt_id(state_id));

  std::stringstream meta;
  meta << m_dict_string["meta_dir"];
  meta << "/";
  meta << m_dict_string["exec_id"];
  meta << "/l1/";
  meta << std::to_string(to_ckpt_id(state_id));

  std::stringstream meta_tmp_dir;
  meta_tmp_dir << m_dict_string["meta_dir"];
  meta_tmp_dir << "/tmp";

  std::stringstream local_tmp_dir;
  local_tmp_dir << m_dict_string["local_dir"];
  local_tmp_dir << "/tmp";

  std::stringstream local_tmp;
  local_tmp << local_tmp_dir.str();

  if( m_dict_bool["master_local"] ) {
    assert( mkdir( local_tmp_dir.str().c_str(), 0777 ) == 0 );
  }

  if( m_dict_bool["master_global"] ) {
    assert( mkdir( meta_tmp_dir.str().c_str(), 0777 ) == 0 );
  }

  m_mpi->barrier();

  for(int i=0; i<m_dict_int["app_procs_node"]; i++) {
    int proc = m_kernel.topo->body[i];

    std::stringstream filename;
    filename << "Ckpt" << to_ckpt_id(state_id) << "-Rank" << proc << ".fti";

    std::stringstream global_fn;
    global_fn << global.str();
    global_fn << "/";
    global_fn << filename.str();

    std::stringstream local_tmp_fn;
    local_tmp_fn << local_tmp_dir.str();
    local_tmp_fn << "/";
    local_tmp_fn << filename.str();

    m_kernel.file_copy( global_fn.str(), local_tmp_fn.str() );

    if (m_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::stringstream metafilename_extern;
      metafilename_extern << extern_meta.str();
      metafilename_extern << "/";
      metafilename_extern << "sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      std::stringstream metafilename_tmp;
      metafilename_tmp << meta_tmp_dir.str();
      metafilename_tmp << "/";
      metafilename_tmp << "sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      m_kernel.file_copy( metafilename_extern.str(), metafilename_tmp.str() );
    }

  }

  m_mpi->barrier();

  std::stringstream local;
  local << m_dict_string["local_dir"];
  local << "/";
  local << m_dict_string["exec_id"];
  local << "/l1/";
  local << std::to_string(to_ckpt_id(state_id));

  if( m_dict_bool["master_local"] ) {
    assert( std::rename( local_tmp_dir.str().c_str(), local.str().c_str() ) == 0 );
  }
  if( m_dict_bool["master_global"] ) {
    assert( std::rename( meta_tmp_dir.str().c_str(), meta.str().c_str() ) == 0 );
    m_kernel.update_ckpt_metadata( to_ckpt_id(state_id), FTI_L1 );
  }

  std::stringstream msg;
  msg << "Conversion of Ckpt." << to_ckpt_id(state_id) << "from level '" << 4 << "' to '" << 1 << "' was successful";
  m_kernel.print(msg.str(), FTI_INFO);

  m_mpi->barrier();

}

bool FtiController::is_local( io_state_id_t state_id ) {
  FTIT_stat st;
  FTI_Stat( to_ckpt_id(state_id), &st );
  return FTI_ST_IS_LOCAL(st.level);
}

bool FtiController::is_global( io_state_id_t state_id ) {
  FTIT_stat st;
  FTI_Stat( to_ckpt_id(state_id), &st );
  return FTI_ST_IS_GLOBAL(st.level);
}

void FtiController::request( io_state_id_t state_id ) {

}

void FtiController::register_callback( void (*f)(void) ) {
  FTI_RegisterUserFunction( f );
}

void FtiController::filelist_local( io_state_id_t state_id, std::vector<std::string> & ckptfiles ) {
  ckptfiles.clear();
  std::string directory = m_dict_string["local_dir"] + "/" +
    m_dict_string["exec_id"] + "/l1/" + std::to_string(to_ckpt_id(state_id));
  for(int i; i<m_dict_int["app_procs_node"]; i++) {
    std::string filepath = directory + "/" + "Ckpt" + std::to_string(to_ckpt_id(state_id)) +
      "-Rank" + std::to_string(m_kernel.topo->body[i]) + "." + m_kernel.conf->suffix;
    ckptfiles.push_back(filepath);
    std::cout << filepath << std::endl;
  }
}

void FtiController::update_metadata( io_state_id_t state_id, io_level_t level ) {
  m_kernel.update_ckpt_metadata( to_ckpt_id(state_id), m_io_level_map[level] );
}


