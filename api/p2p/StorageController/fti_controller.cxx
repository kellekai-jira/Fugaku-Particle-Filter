#include "fti_controller.hpp"
#include <fti.h>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

#include "api_common.h"  // for timing
#include "helpers.hpp"

int FtiController::protect( void* buffer, size_t size, io_type_t type ) {
  assert( m_io_type_map.count(type) != 0 && "invalid type" );
  FTI_Protect(m_id_counter, buffer, size, m_io_type_map[type]);
  io_var_t variable = { buffer, size, type };
  m_var_id_map.insert( std::pair<io_id_t,io_var_t>( m_id_counter, variable ) );
  return m_id_counter++;
}

void FtiController::update( io_id_t id, void* buffer, size_t size ) {
  assert( m_var_id_map.count(id) != 0 && "invalid type" );
  FTI_Protect(id, buffer, size, m_io_type_map[m_var_id_map[id].type]);
}

void FtiController::init_io( MpiController* mpi, int runner_id ) {
  this->m_mpi = mpi;
  std::stringstream config_file;
  config_file << "config-" << std::setw(3) << std::setfill('0') << runner_id << ".fti";
  FTI_Init( config_file.str().c_str(), m_mpi->comm() );
  m_runner_id = runner_id;
}

void FtiController::set_state_size_per_proc( std::vector<uint64_t> vec ) {
  m_state_sizes_per_rank = vec;
}

void FtiController::init_core() {
  // FTI_COMM_WORLD is the MPI_COMM_WORLD replacement
  // so FTI_COMM_WORLD contains all app cores if you are on an app core
  // and containes all FTI headranks if you are on an head rank
  m_mpi->register_comm( "fti_comm_world", FTI_COMM_WORLD );
  m_mpi->set_comm( "fti_comm_world" );
  char tmp[FTI_BUFS];
  if( FTI_AmIaHead() ) {
    snprintf(tmp, FTI_BUFS, "%s.head", m_kernel.conf->mTmpDir);
  } else {
    snprintf(tmp, FTI_BUFS, "%s.app", m_kernel.conf->mTmpDir);
  }
  strncpy(m_kernel.conf->mTmpDir, tmp, FTI_BUFS);
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
    if (start_position_to_erase != std::string::npos) {
        path.erase(start_position_to_erase, path.size());
    }
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
  if( FTI_AmIaHead() ) {
    if( tag == IO_TAG_PULL ) {
      return !m_state_pull_requests.empty();
    } else if( tag == IO_TAG_PUSH ) {
      return !m_state_push_requests.empty();
    } else if( tag == IO_TAG_DUMP ) {
      return !m_state_dump_requests.empty();
    } else {
      return FTI_HeadProbe( m_io_tag_map[tag] );
    }
  } else {
    return FTI_AppProbe( m_io_tag_map[tag] );
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

bool FtiController::load( io_state_id_t state_id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  M_TRIGGER(START_FTI_LOAD, state_id.t);
  bool res = FTI_Load( to_ckpt_id(state_id), m_io_level_map[level] ) == FTI_SCES;
  M_TRIGGER(STOP_FTI_LOAD, state_id.id);
  return res; 
}

void FtiController::store( io_state_id_t state_id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  FTI_Checkpoint( to_ckpt_id(state_id), m_io_level_map[level] );
  m_mpi->barrier();
}

void FtiController::remove( io_state_id_t state_id, io_level_t level ) {
  if( level == IO_STORAGE_L1 ) {
    M_TRIGGER(START_DELETE_LOCAL,0);
  } else if ( level == IO_STORAGE_L2 ) {
    M_TRIGGER(START_DELETE_PFS,0);
  }
  IO_TRY( FTI_Remove( to_ckpt_id(state_id), m_io_level_map[level] ),
      FTI_SCES, "failed to remove file" );
  if( level == IO_STORAGE_L1 ) {
    M_TRIGGER(STOP_DELETE_LOCAL,0);
  } else if ( level == IO_STORAGE_L2 ) {
    M_TRIGGER(STOP_DELETE_PFS,0);
  }
}

void FtiController::stage( io_state_id_t state_id, io_level_t from, io_level_t to ) {
  
  assert( m_io_level_map.count(from) != 0 && "invalid checkpoint level" );
  assert( m_io_level_map.count(to) != 0 && "invalid checkpoint level" );

  assert( m_kernel.topo->amIaHead == 1 && "copy for application threads not implemented for extern" );
  //assert( from == IO_STORAGE_L2 && to == IO_STORAGE_L1 && "copy from level 2 to level 1 not implemented for extern" );
  
  if( from == IO_STORAGE_L1 ) M_TRIGGER(START_PUSH_STATE_TO_PFS,0);
  else M_TRIGGER(START_COPY_STATE_FROM_PFS,0);

  // LEVEL 2 DIRECTORIES
  
  // Checkpoint
  std::stringstream L2_BASE;
  L2_BASE << m_dict_string["global_dir"];
  std::stringstream L2_TEMP;
  L2_TEMP << L2_BASE.str() << "/" << melissa::helpers::make_uuid();
  std::stringstream L2_CKPT;
  L2_CKPT << L2_BASE.str() << "/" << std::to_string(to_ckpt_id(state_id));
  
  // Metadata
  std::stringstream L2_META_BASE;
  L2_META_BASE << m_dict_string["meta_dir"];
  std::stringstream L2_META_TEMP;
  L2_META_TEMP << L2_META_BASE.str() << "/" << melissa::helpers::make_uuid();
  std::stringstream L2_META_CKPT;
  L2_META_CKPT << L2_META_BASE.str() << "/" << std::to_string(to_ckpt_id(state_id));
  
  MDBG("stage %d -> %d | L2 | %s, %s, %s, %s", from, to, L2_TEMP.str().c_str(), L2_CKPT.str().c_str(), L2_META_TEMP.str().c_str(), L2_META_CKPT.str().c_str());
  
  // LEVEL 1 DIRECTORIES
  
  // Checkpoint
  std::stringstream L1_BASE;
  L1_BASE << m_dict_string["local_dir"] << "/" << m_dict_string["exec_id"];
  std::stringstream L1_TEMP;
  L1_TEMP << L1_BASE.str() << "/" << melissa::helpers::make_uuid();
  std::stringstream L1_CKPT;
  L1_CKPT << L1_BASE.str() << "/l1/" << std::to_string(to_ckpt_id(state_id));
  
  // Metadata
  std::stringstream L1_META_BASE;
  L1_META_BASE << m_dict_string["meta_dir"] << "/" << m_dict_string["exec_id"];
  std::stringstream L1_META_TEMP;
  L1_META_TEMP << L1_META_BASE.str() << "/" << melissa::helpers::make_uuid();
  std::stringstream L1_META_CKPT;
  L1_META_CKPT << L1_META_BASE.str() << "/l1/" << std::to_string(to_ckpt_id(state_id));
  
  MDBG("stage %d -> %d | L1 | %s, %s, %s, %s", from, to, L1_TEMP.str().c_str(), L1_CKPT.str().c_str(), L1_META_TEMP.str().c_str(), L1_META_CKPT.str().c_str());
 
  m_mpi->barrier();
  
  if( from == IO_STORAGE_L1 ) {
    stage_l1l2( L1_CKPT.str(), L1_META_CKPT.str(), L2_TEMP.str(), L2_META_TEMP.str(), 
        L2_CKPT.str(), L2_META_CKPT.str(), state_id  );
  }
  else {
    stage_l2l1( L1_CKPT.str(), L1_META_CKPT.str(), L2_TEMP.str(), L2_META_TEMP.str(), 
        L2_CKPT.str(), L2_META_CKPT.str(), state_id  );
  }

  m_mpi->barrier();
  
  if( from == IO_STORAGE_L1 ) M_TRIGGER(START_PUSH_STATE_TO_PFS,0);
  else M_TRIGGER(STOP_COPY_STATE_FROM_PFS,0);

}

void FtiController::stage_l1l2( std::string L1_CKPT, std::string L1_META_CKPT, std::string L2_TEMP, std::string L2_META_TEMP,
    std::string L2_CKPT, std::string L2_META_CKPT, io_state_id_t state_id  ) {

  struct stat info;
  IO_TRY( stat( L2_CKPT.c_str(), &info ), -1, "the global checkpoint directory already exists!" );
  
  if( m_dict_bool["master_global"] ) {
    IO_TRY( mkdir( L2_META_TEMP.c_str(), 0777 ), 0, "unable to create directory" );
    IO_TRY( mkdir( L2_TEMP.c_str(), 0777 ), 0, "unable to create directory" );
  }

  std::stringstream L2_CKPT_FN;
  L2_CKPT_FN << L2_TEMP << "/Ckpt" << to_ckpt_id(state_id) << "-mpiio.fti";
  std::string gfn = L2_CKPT_FN.str();
  
  MPI_File mpifd;
  MPI_File_open(m_mpi->comm(), gfn.c_str(), MPI_MODE_WRONLY|MPI_MODE_CREATE, MPI_INFO_NULL, &mpifd);
  
  MPI_Offset global_file_offset = 0;
  for(int i=0; i<m_dict_int["app_procs_node"]; i++) {
    int proc = m_kernel.topo->body[i];

    std::stringstream L1_CKPT_FN;
    L1_CKPT_FN << L1_CKPT << "/Ckpt" << to_ckpt_id(state_id) << "-Rank" << proc << ".fti";
    std::string lfn = L1_CKPT_FN.str();
    
    int64_t local_file_size = m_state_sizes_per_rank[i];
    MDBG("stage 0 -> 1 | STATE SIZE [%d] | %ld", i, local_file_size);

    std::ifstream localfd( lfn, std::ios::binary );

    size_t pos = 0;
    while (pos < local_file_size) {
      size_t bSize = IO_TRANSFER_SIZE;
      if ((local_file_size - pos) < IO_TRANSFER_SIZE) {
        bSize = local_file_size - pos;
      }

      std::vector<char> buffer(bSize, 0);
      localfd.read(buffer.data(), buffer.size());

      MPI_Datatype dType;
      MPI_Type_contiguous( bSize, MPI_BYTE, &dType );
      MPI_Type_commit( &dType );

      int err = MPI_File_write_at( mpifd, global_file_offset, buffer.data(), 1, dType, MPI_STATUS_IGNORE );
      // check if successful
      if (err != 0) {
        errno = 0;
        int reslen;
        char str[FTI_BUFS], mpi_err[FTI_BUFS];
        MPI_Error_string(err, mpi_err, &reslen);
        snprintf(str, FTI_BUFS,
            "unable to create file [MPI ERROR - %i] %s", err, mpi_err);
        MDBG(str);
        return;
      }


      MPI_Type_free(&dType);
      global_file_offset += bSize;
      pos = pos + bSize;
    }

    localfd.close();

    if (m_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::stringstream L1_META_CKPT_FN;
      L1_META_CKPT_FN << L1_META_CKPT;
      L1_META_CKPT_FN << "/sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      std::stringstream L2_META_TEMP_FN;
      L2_META_TEMP_FN << L2_META_TEMP;
      L2_META_TEMP_FN << "/sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      m_kernel.file_copy( L1_META_CKPT_FN.str(), L2_META_TEMP_FN.str() );
    }
  }

  MPI_File_close(&mpifd);

  if( m_dict_bool["master_global"] ) {
    IO_TRY( std::rename( L2_META_TEMP.c_str(), L2_META_CKPT.c_str() ), 0, "unable to rename local directory" );
    IO_TRY( std::rename( L2_TEMP.c_str(), L2_CKPT.c_str() ), 0, "unable to rename local_meta directory" );
    update_metadata( state_id, IO_STORAGE_L2 );
  }

  std::stringstream msg;
  msg << "Conversion of Ckpt." << to_ckpt_id(state_id) << "from level '" << 1 << "' to '" << 4 << "' was successful";
  m_kernel.print(msg.str(), FTI_INFO);

}

void FtiController::stage_l2l1( std::string L2_CKPT, std::string L2_META_CKPT, std::string L1_TEMP, std::string L1_META_TEMP,
    std::string L1_CKPT, std::string L1_META_CKPT, io_state_id_t state_id ) {

  struct stat info;
  IO_TRY( stat( L1_CKPT.c_str(), &info ), -1, "the local checkpoint directory already exists!" );
  
  if( m_dict_bool["master_global"] ) {
    IO_TRY( mkdir( L1_META_TEMP.c_str(), 0777 ), 0, "unable to create directory" );
    IO_TRY( mkdir( L1_TEMP.c_str(), 0777 ), 0, "unable to create directory" );
  }
  
  std::stringstream L2_CKPT_FN;
  L2_CKPT_FN << L2_CKPT << "/Ckpt" << to_ckpt_id(state_id) << "-mpiio.fti";
  std::string gfn = L2_CKPT_FN.str();

  MPI_File mpifd;
  MPI_File_open(m_mpi->comm(), gfn.c_str(), MPI_MODE_RDWR, MPI_INFO_NULL, &mpifd);
  
  MPI_Offset global_file_offset = 0;
  for(int i=0; i<m_dict_int["app_procs_node"]; i++) {
    int proc = m_kernel.topo->body[i];

    std::stringstream L1_CKPT_FN;
    L1_CKPT_FN << L1_TEMP << "/Ckpt" << to_ckpt_id(state_id) << "-Rank" << proc << ".fti";
    std::string lfn = L1_CKPT_FN.str();
    
    int64_t local_file_size = m_state_sizes_per_rank[i];
    MDBG("stage 1 -> 0 | STATE SIZE [%d] | %ld", i, local_file_size);

    std::ofstream localfd( lfn, std::ios::binary );

    size_t pos = 0;
    while (pos < local_file_size) {
      size_t bSize = IO_TRANSFER_SIZE;
      if ((local_file_size - pos) < IO_TRANSFER_SIZE) {
        bSize = local_file_size - pos;
      }

      std::vector<char> buffer(bSize, 0);

      MPI_Datatype dType;
      MPI_Type_contiguous( bSize, MPI_BYTE, &dType );
      MPI_Type_commit( &dType );

      int err = MPI_File_read_at( mpifd, global_file_offset, buffer.data(), 1, dType, MPI_STATUS_IGNORE );
      // check if successful
      if (err != 0) {
        errno = 0;
        int reslen;
        char str[FTI_BUFS], mpi_err[FTI_BUFS];
        MPI_Error_string(err, mpi_err, &reslen);
        snprintf(str, FTI_BUFS,
            "unable to create file [MPI ERROR - %i] %s", err, mpi_err);
        MDBG(str);
        return;
      }
      
      localfd.write(buffer.data(), buffer.size());

      MPI_Type_free(&dType);
      global_file_offset += bSize;
      pos = pos + bSize;
    }

    localfd.close();

    if (m_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::stringstream L2_META_CKPT_FN;
      L2_META_CKPT_FN << L2_META_CKPT;
      L2_META_CKPT_FN << "/sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      std::stringstream L1_META_TEMP_FN;
      L1_META_TEMP_FN << L1_META_TEMP;
      L1_META_TEMP_FN << "/sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      m_kernel.file_copy( L2_META_CKPT_FN.str(), L1_META_TEMP_FN.str() );
    }
  }

  MPI_File_close(&mpifd);

  if( m_dict_bool["master_global"] ) {
    IO_TRY( std::rename( L1_META_TEMP.c_str(), L1_META_CKPT.c_str() ), 0, "unable to rename local directory" );
    IO_TRY( std::rename( L1_TEMP.c_str(), L1_CKPT.c_str() ), 0, "unable to rename local_meta directory" );
    update_metadata( state_id, IO_STORAGE_L1 );
  }

  std::stringstream msg;
  msg << "Conversion of Ckpt." << to_ckpt_id(state_id) << "from level '" << 4 << "' to '" << 1 << "' was successful";
  m_kernel.print(msg.str(), FTI_INFO);


}

bool FtiController::is_local( io_state_id_t state_id ) {
  M_TRIGGER(START_CHECK_LOCAL, state_id.t);
  FTIT_stat st;
  FTI_Stat( to_ckpt_id(state_id), &st );
  bool res = FTI_ST_IS_LOCAL(st.level);
  M_TRIGGER(STOP_CHECK_LOCAL, state_id.id);
  return res;
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
  for(int i = 0; i<m_dict_int["app_procs_node"]; i++) {
    std::string filepath = directory + "/" + "Ckpt" + std::to_string(to_ckpt_id(state_id)) +
      "-Rank" + std::to_string(m_kernel.topo->body[i]) + "." + m_kernel.conf->suffix;
    ckptfiles.push_back(filepath);
  }
}

void FtiController::update_metadata( io_state_id_t state_id, io_level_t level ) {
  m_kernel.update_ckpt_metadata( to_ckpt_id(state_id), m_io_level_map[level] );
}


