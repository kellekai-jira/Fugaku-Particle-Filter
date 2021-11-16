#include "fti_controller.hpp"
#include <fti.h>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <chrono>
#include <sys/mman.h>
#include <bitset>

#include <fstream>

#include <boost/filesystem.hpp>
#include "api_common.h"  // for timing
#include "helpers.hpp"

#include <limits.h>
#include <sys/stat.h>
#include <ftw.h>

/* Call unlink or rmdir on the path, as appropriate. */
// [FROM: https://stackoverflow.com/a/1149837/5073895]
int
rm(const char *path, const struct stat *s, int flag, struct FTW *f)
{
        int status;
        int (*rm_func)(const char *);
        (void)s;
        (void)f;
        rm_func = flag == FTW_DP ? rmdir : unlink;
        if( status = rm_func(path), status != 0 ){
                perror(path);
        } else if( getenv("VERBOSE") ){
                puts(path);
        }
        return status;
}



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

void FtiController::init_io( int runner_id ) {
  std::stringstream config_file;
  config_file << "config-" << std::setw(3) << std::setfill('0') << runner_id << ".fti";
  FTI_Init( config_file.str().c_str(), mpi.comm() );
  m_runner_id = runner_id;
  m_next_garbage_coll = time(NULL) + 10;
}

void FtiController::set_state_size_per_proc( std::vector<uint64_t> vec ) {
  m_state_sizes_per_rank = vec;
}

void FtiController::init_core() {
  // FTI_COMM_WORLD is the MPI_COMM_WORLD replacement
  // so FTI_COMM_WORLD contains all app cores if you are on an app core
  // and containes all FTI headranks if you are on an head rank
  mpi.register_comm( "fti_comm_world", FTI_COMM_WORLD );
  mpi.set_comm( "fti_comm_world" );
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
  m_io_msg_map.insert( std::pair<io_msg_t,int>( IO_MSG_MST, FTI_HEAD_MODE_ROOT ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_LOAD, IO_TAG_LOAD + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_POST, IO_TAG_POST + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_DUMP, IO_TAG_DUMP + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_PULL, IO_TAG_PULL + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_PUSH, IO_TAG_PUSH + 1000000 ) );
  m_io_tag_map.insert( std::pair<io_tag_t,int>( IO_TAG_FINI, IO_TAG_FINI + 1000000 ) );
  m_dict_int.insert( std::pair<std::string,int>( "current_cycle", 0 ) );
  m_dict_int.insert( std::pair<std::string,int>( "nodes", m_kernel.topo->nbNodes ) );
  m_dict_int.insert( std::pair<std::string,int>( "procs_node", m_kernel.topo->nodeSize ) );
  int app_procs_node = m_kernel.topo->nodeSize - m_kernel.topo->nbHeads;
  m_dict_int.insert( std::pair<std::string,int>( "app_procs_node", app_procs_node ) );
  m_dict_int.insert( std::pair<std::string,int>( "procs_total", m_kernel.topo->nbProc ) );
  // node master process. One for each FTI-comm on each node. Heads are automatically node master.
  m_dict_bool.insert( std::pair<std::string,bool>( "master_local", m_kernel.topo->masterLocal ) );
  // FTI-comm master processes. One app and one worker.
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
    if( m_dict_bool["master_global"] ) { 
      if( tag == IO_TAG_PULL ) {
        return !m_state_pull_requests.empty();
      } else if( tag == IO_TAG_PUSH ) {
        return !m_state_push_requests.empty();
      } else if( tag == IO_TAG_DUMP ) {
        if (time(NULL) > m_next_garbage_coll) {
            m_next_garbage_coll = time(NULL) + 10;
            return true;
        }
        return false;
      } else {
        return FTI_HeadProbe( m_io_tag_map[tag] );
      }
    } else {
      int flag; MPI_Iprobe( 0, tag, mpi.comm(), &flag, MPI_STATUS_IGNORE ); 
      if( flag ) {
        MPI_Recv( NULL, 0, MPI_BYTE, 0, tag, mpi.comm(), MPI_STATUS_IGNORE ); 
        MPI_Send( NULL, 0, MPI_BYTE, 0, tag, mpi.comm() ); 
      }
      return (bool)flag;
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

// called from app processes to load checkpoint with certain ID
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
  mpi.barrier();
}

void FtiController::remove( io_state_id_t state_id, io_level_t level ) {
  if( level == IO_STORAGE_L1 ) {
    M_TRIGGER(START_DELETE_LOCAL,0);
    // collective and blocking
    IO_TRY( FTI_Remove( to_ckpt_id(state_id), m_io_level_map[level] ), FTI_SCES, "failed to remove file" );
    M_TRIGGER(STOP_DELETE_LOCAL,0);
  } else if ( level == IO_STORAGE_L2 ) {
    M_TRIGGER(START_DELETE_PFS,0);
    if( m_dict_bool["master_global"] ) {
      int ckpt_id = to_ckpt_id( state_id );
      char ckpt_id_c_str[256];
      snprintf(ckpt_id_c_str, 256, "%d", ckpt_id);
      std::string ckpt_id_str(ckpt_id_c_str);
      std::string g_path_string = m_dict_string["global_dir"] + "/" + ckpt_id_str;
      MDBG("path to delete -> '%s'", g_path_string.c_str());
			if( nftw(g_path_string.c_str(), rm, FOPEN_MAX, FTW_DEPTH) == -1 ){ 
        MERR("failed to remove checkpoint path ''", g_path_string.c_str());
      }
      m_kernel.remove_ckpt_metadata( to_ckpt_id( state_id ), m_io_level_map[level] );
      mpi.barrier();
    } else {
      mpi.barrier();
    }
    M_TRIGGER(STOP_DELETE_PFS,0);
  }
}

void FtiController::stage( io_state_id_t state_id, io_level_t from, io_level_t to ) {
  
  assert( m_io_level_map.count(from) != 0 && "invalid checkpoint level" );
  assert( m_io_level_map.count(to) != 0 && "invalid checkpoint level" );
  assert( m_kernel.topo->amIaHead == 1 && "copy for application threads not implemented for extern" );
  
  // LEVEL 2 DIRECTORIES
  
  // Checkpoint + Metadata
  std::stringstream L2_BASE;
  L2_BASE << m_dict_string["global_dir"];
  std::stringstream L2_TEMP;
  L2_TEMP << L2_BASE.str() << "/" << melissa::helpers::make_uuid();
  std::stringstream L2_CKPT;
  L2_CKPT << L2_BASE.str() << "/" << std::to_string(to_ckpt_id(state_id));
  
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
 
  mpi.barrier();
  
  if( from == IO_STORAGE_L1 ) {
    stage_l1l2( L1_CKPT.str(), L1_META_CKPT.str(), L2_TEMP.str(), L2_CKPT.str(), state_id  );
  }
  else {
    stage_l2l1( L2_CKPT.str(), L1_TEMP.str(), L1_META_TEMP.str(), L1_CKPT.str(), L1_META_CKPT.str(), state_id  );
  }

  mpi.barrier();
  
}

void FtiController::stage_l1l2( std::string L1_CKPT, std::string L1_META_CKPT, std::string L2_TEMP,
    std::string L2_CKPT, io_state_id_t state_id  ) {

  M_TRIGGER(START_PUSH_STATE_TO_PFS,0);
  std::vector<char> l2_temp_vec;
  
  if( m_dict_bool["master_global"] ) {
    struct stat info;
    IO_TRY( stat( L2_CKPT.c_str(), &info ), -1, "the global checkpoint directory already exists!" );
    IO_TRY( stat( L2_TEMP.c_str(), &info ), -1, "the global checkpoint directory already exists!" );
    IO_TRY( mkdir( L2_TEMP.c_str(), 0777 ), 0, "unable to create directory" );
    std::copy(L2_TEMP.begin(), L2_TEMP.end(), std::back_inserter(l2_temp_vec));
  }
  
  mpi.broadcast(l2_temp_vec);

  std::string l2_temp(l2_temp_vec.begin(), l2_temp_vec.end());
  

  std::stringstream L2_CKPT_FN;
  L2_CKPT_FN << l2_temp << "/Ckpt" << to_ckpt_id(state_id) << "-worker" << m_kernel.topo->splitRank << "-serialized.fti";
  std::string gfn = L2_CKPT_FN.str();
  
  std::stringstream L2_META_FN;
  L2_META_FN << l2_temp << "/Meta" << to_ckpt_id(state_id) << "-worker" << m_kernel.topo->splitRank << "-serialized.fti";
  std::string mfn = L2_META_FN.str();
  
  MDBG("ckpt file '%s'", gfn.c_str());
  MDBG("meta file '%s'", mfn.c_str());
  int fd = open( gfn.c_str(), O_WRONLY|O_CREAT, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR );
  
  std::ofstream metafs(mfn);

  for(int i=0; i<m_dict_int["app_procs_node"]; i++) {
    int proc = m_kernel.topo->body[i];

    std::stringstream L1_CKPT_FN;
    L1_CKPT_FN << L1_CKPT << "/Ckpt" << to_ckpt_id(state_id) << "-Rank" << proc << ".fti";
    std::string lfn = L1_CKPT_FN.str();
    
    int64_t local_file_size = m_state_sizes_per_rank[i];
    
    uint64_t t_open_local, t_read_local, t_write_global, t_close_local, t_meta; 
    std::chrono::system_clock::time_point t1, t2;
    
    t1 = std::chrono::system_clock::now();
    int lfd = open(lfn.c_str(), O_RDONLY, 0);
    std::unique_ptr<char[]> buffer(new char[IO_TRANSFER_SIZE]);
    t2 = std::chrono::system_clock::now();
    t_open_local = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

    size_t pos = 0;
    while (pos < local_file_size) {
      size_t bSize = IO_TRANSFER_SIZE;
      if ((local_file_size - pos) < IO_TRANSFER_SIZE) {
        bSize = local_file_size - pos;
      }
      
      ssize_t check;
      
      t1 = std::chrono::system_clock::now();
      check = read(lfd, buffer.get(), bSize); 
      t2 = std::chrono::system_clock::now();
      t_read_local = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
      
      // check if successful
      if (check != bSize) {
        MERR("unable to read '%lu' bytes from file '%s'", bSize, lfn.c_str());
      }

      t1 = std::chrono::system_clock::now();
      check = write( fd, buffer.get(), bSize );
      t2 = std::chrono::system_clock::now();
      t_write_global = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
      
      // check if successful
      if (check != bSize) {
        MERR("unable to write '%lu' bytes into file '%s'", bSize, gfn.c_str());
      }

      pos = pos + bSize;
    }
    
    t1 = std::chrono::system_clock::now();
    close(lfd);
    t2 = std::chrono::system_clock::now();
    t_close_local = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

    t1 = std::chrono::system_clock::now();
    // FIXME this only takes into account group size of 1!!!
    //if (m_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::stringstream L1_META_CKPT_FN;
      L1_META_CKPT_FN << L1_META_CKPT;
      L1_META_CKPT_FN << "/sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      std::ifstream tmp_metafs(L1_META_CKPT_FN.str());
      std::string str(std::istreambuf_iterator<char>{tmp_metafs}, {});
      size_t count_lines = std::count_if( str.begin(), str.end(), []( char c ){return c =='\n';});
      metafs << count_lines << std::endl << str;
      tmp_metafs.close();
    //}
    t2 = std::chrono::system_clock::now();
    t_meta = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
  }

  fsync(fd);
  close(fd);
  metafs.flush();
  metafs.close();

  mpi.barrier();

  if( m_dict_bool["master_global"] ) {
    IO_TRY( std::rename( l2_temp.c_str(), L2_CKPT.c_str() ), 0, "unable to rename local_meta directory" );
  }
  
  update_metadata( state_id, IO_STORAGE_L2 );

  std::stringstream msg;
  msg << "Conversion of Ckpt." << to_ckpt_id(state_id) << "from level '" << 1 << "' to '" << 4 << "' was successful";
  m_kernel.print(msg.str(), FTI_INFO);
  
  M_TRIGGER(STOP_PUSH_STATE_TO_PFS,0);

}

void FtiController::stage_l2l1( std::string L2_CKPT, std::string L1_TEMP, std::string L1_META_TEMP,
    std::string L1_CKPT, std::string L1_META_CKPT, io_state_id_t state_id ) {

  M_TRIGGER(START_COPY_STATE_FROM_PFS,0);
  
  MDBG("pulling state_id '{t: %d, id: %d}'", state_id.id, state_id.id);

  struct stat info;
  IO_TRY( stat( L1_CKPT.c_str(), &info ), -1, "the local checkpoint directory already exists!" );
  IO_TRY( stat( L1_META_TEMP.c_str(), &info ), -1, "the local checkpoint directory already exists!" );
  IO_TRY( stat( L1_TEMP.c_str(), &info ), -1, "the local checkpoint directory already exists!" );
  
  IO_TRY( mkdir( L1_META_TEMP.c_str(), 0777 ), 0, "unable to create directory" );
  IO_TRY( mkdir( L1_TEMP.c_str(), 0777 ), 0, "unable to create directory" );

  std::stringstream L2_CKPT_FN;
  L2_CKPT_FN << L2_CKPT << "/Ckpt" << to_ckpt_id(state_id) << "-worker" << m_kernel.topo->splitRank << "-serialized.fti";
  std::string gfn = L2_CKPT_FN.str();
  
  std::stringstream L2_META_FN;
  L2_META_FN << L2_CKPT << "/Meta" << to_ckpt_id(state_id) << "-worker" << m_kernel.topo->splitRank << "-serialized.fti";
  std::string mfn = L2_META_FN.str();
  
  MDBG("global ckpt file: %s", gfn.c_str());
  MDBG("global meta file: %s", mfn.c_str());

  int fd = open( gfn.c_str(), O_RDWR );
  if( fd < 0 ) {
    MERR("unable to read from file '%s'", gfn.c_str());
  }
  //if (m_kernel.topo->groupRank == 0) {
    std::ifstream metafs( mfn );
    std::string metastr(std::istreambuf_iterator<char>{metafs}, {});
    metafs.close();
  //}
  
  for(int i=0; i<m_dict_int["app_procs_node"]; i++) {
    int proc = m_kernel.topo->body[i];

    std::stringstream L1_CKPT_FN;
    L1_CKPT_FN << L1_TEMP << "/Ckpt" << to_ckpt_id(state_id) << "-Rank" << proc << ".fti";
    std::string lfn = L1_CKPT_FN.str();
    
    MDBG("local ckpt file: %s", lfn.c_str());
    
    int64_t local_file_size = m_state_sizes_per_rank[i];
    
    MDBG("local ckpt file size: %ld", local_file_size);
    int lfd = open( lfn.c_str(), O_WRONLY|O_CREAT, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR );
    std::unique_ptr<char[]> buffer(new char[IO_TRANSFER_SIZE]);

    size_t pos = 0;
    while (pos < local_file_size) {
      size_t bSize = IO_TRANSFER_SIZE;
      if ((local_file_size - pos) < IO_TRANSFER_SIZE) {
        bSize = local_file_size - pos;
      }

      ssize_t check;

      check = read( fd, buffer.get(), bSize );
      // check if successful
      if (check != bSize) {
        MERR("unable to read '%lu' from file '%s'", bSize, gfn.c_str());
        return;
      }
      
      check = write(lfd, buffer.get(), bSize);
      // check if successful
      if (check != bSize) {
        MERR("unable to write '%lu' into file '%s'", bSize, lfn.c_str());
        return;
      }

      pos = pos + bSize;
    }
    
    fsync(lfd);
    close(lfd);

    //if (m_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::stringstream L1_META_TEMP_FN;
      L1_META_TEMP_FN << L1_META_TEMP;
      L1_META_TEMP_FN << "/sector" << m_kernel.topo->sectorID << "-group" << groupId << ".fti";
      std::ofstream tmp_metafs(L1_META_TEMP_FN.str());
      std::string count_str;
      std::getline( metafs, count_str );
      size_t count;
      sscanf(count_str.c_str(), "%zu", &count);
      for(size_t i=0; i<count; i++) {
        std::string str;
        std::getline( metafs, str );
        tmp_metafs << str << std::endl;
      }
      tmp_metafs.close();
    //}
  }

  close(fd);
  
  mpi.barrier();
  
  if( m_dict_bool["master_global"] ) {
    IO_TRY( std::rename( L1_META_TEMP.c_str(), L1_META_CKPT.c_str() ), 0, "unable to rename local directory" );
  }
  IO_TRY( std::rename( L1_TEMP.c_str(), L1_CKPT.c_str() ), 0, "unable to rename local_meta directory" );

  update_metadata( state_id, IO_STORAGE_L1 );

  mpi.barrier();

  std::stringstream msg;
  msg << "Conversion of Ckpt." << to_ckpt_id(state_id) << "from level '" << 4 << "' to '" << 1 << "' was successful";
  m_kernel.print(msg.str(), FTI_INFO);

  M_TRIGGER(STOP_COPY_STATE_FROM_PFS,0);

}

bool FtiController::is_local( io_state_id_t state_id ) {
  M_TRIGGER(START_CHECK_LOCAL, state_id.t);
  FTIT_stat st;
  FTI_Stat( to_ckpt_id(state_id), &st );
  std::bitset<8> x_level(st.level);
  std::bitset<8> x_io(st.io);
  std::bitset<8> x_elastic(st.elastic);
  std::bitset<8> x_dcp(st.dcp);
  std::bitset<16> x_device(st.device);
  std::stringstream ss;
  ss << "id: " << to_ckpt_id(state_id) << std::endl;
  ss << "level: " << x_level << std::endl;
  ss << "io: " << x_level << std::endl;
  ss << "elastic: " << x_level << std::endl;
  ss << "dcp: " << x_level << std::endl;
  ss << "device: " << x_level << std::endl;
  MDBG("%s", ss.str().c_str());
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


