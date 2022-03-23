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
#include <algorithm>
#include <cctype>

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

int64_t elegantPair( int64_t x, int64_t y ) {
    return (x >= y) ? (x * x + x + y) : (y * y + x);
}

std::pair<int64_t,int64_t> elegantUnpair(int64_t z) {
    int64_t sqrtz = static_cast<int64_t>(sqrt(z));
    int64_t sqz = sqrtz * sqrtz;
    std::pair<int64_t,int64_t> state;
    if ((z - sqz) >= sqrtz) {
        state.first = sqrtz;
        state.second = z - sqz - sqrtz; 
    } else {
        state.first = z - sqz;
        state.second = sqrtz;
    }
    return state;
  //return ((z - sqz) >= sqrtz) ? [sqrtz, z - sqz - sqrtz] : [z - sqz, sqrtz];
}

int FtiController::reprotect_all() {
  //for(int m=1; m<get_num_parameters(); m++) {
  for(auto const& var : m_var_id_map) {
    protect( var.first, var.second.data, var.second.size, var.second.type );
  }
  //}
}

int FtiController::protect( std::string name, void* buffer, size_t size, io_type_t type ) {
  
  assert( m_io_type_map.count(type) != 0 && "invalid type" );

  //io_zip_t zip;

  //if ( m_var_zip_map.count(name) != 0 ) {   
  //  zip.mode = m_var_zip_map[name].mode;
  //  zip.parameter = m_var_zip_map[name].parameter;
  //  zip.type = m_var_zip_map[name].type;
  //}

  io_var_t variable;

  variable.data = buffer;
  variable.size = size; 
  variable.type = type; 
  //variable.zip = zip;
  
  if(m_var_id_map.find(name) == m_var_id_map.end()) { 
    variable.id = FTI_setIDFromString( name.c_str() ); 
    m_var_id_map.insert( std::pair<std::string,io_var_t>( name, variable ) );
  } else {
    m_var_id_map[name].data = variable.data;
    m_var_id_map[name].size = variable.size;
    m_var_id_map[name].type = variable.type;
    //m_var_id_map[name].zip.mode = variable.zip.mode;
    //m_var_id_map[name].zip.parameter = variable.zip.type;
    //m_var_id_map[name].zip.type = variable.zip.type;
  }
  
  FTI_Protect(
      m_var_id_map[name].id, 
      m_var_id_map[name].data, 
      m_var_id_map[name].size, 
      m_io_type_map[m_var_id_map[name].type]);
  
  FTI::data_t data = {0};
  data.ptr = (void*) buffer;
  data.count = size;
  data.size = size * sizeof(double);
	
  m_zip_controller.adaptParameter( &data, name );
  
  std::cout << "== best parameters ( variable: "<<name<<" ) ==" << std::endl;
  std::cout << "== mode: " << data.compression.mode << std::endl;
  std::cout << "== parameter: " << data.compression.parameter << std::endl;
  std::cout << "== type: " << data.compression.type << std::endl;
	fflush(stdout);	

  FTI_SetCompression( 
      m_var_id_map[name].id, 
      data.compression.mode, 
      data.compression.parameter, 
      data.compression.type);
  
  //FTI_SetCompression( 
  //    m_var_id_map[name].id, 
  //    m_io_zip_mode_map[m_var_id_map[name].zip.mode], 
  //    m_var_id_map[name].zip.parameter, 
  //    m_io_zip_type_map[m_var_id_map[name].zip.type]);
  
  return m_var_id_map[name].id;

}

void FtiController::init_io( int runner_id ) {
  std::stringstream config_file;
  config_file << "config-" << std::setw(3) << std::setfill('0') << runner_id << ".fti";
  FTI_Init( config_file.str().c_str(), mpi.comm() );
  m_runner_id = runner_id;
  m_next_garbage_coll = time(NULL) + 10;
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
  //m_io_zip_type_map.insert( std::pair<io_zip_type_t,FTIT_CPC_TYPE>( IO_ZIP_TYPE_DEFAULT, FTI_CPC_TYPE_NONE ) );
  //m_io_zip_type_map.insert( std::pair<io_zip_type_t,FTIT_CPC_TYPE>( IO_ZIP_TYPE_A, FTI_CPC_ACCURACY ) );
  //m_io_zip_type_map.insert( std::pair<io_zip_type_t,FTIT_CPC_TYPE>( IO_ZIP_TYPE_B, FTI_CPC_PRECISION ) );
  //m_io_zip_mode_map.insert( std::pair<io_zip_mode_t,FTIT_CPC_MODE>( IO_ZIP_MODE_DEFAULT, FTI_CPC_MODE_NONE ) );
  //m_io_zip_mode_map.insert( std::pair<io_zip_mode_t,FTIT_CPC_MODE>( IO_ZIP_MODE_A, FTI_CPC_FPZIP ) );
  //m_io_zip_mode_map.insert( std::pair<io_zip_mode_t,FTIT_CPC_MODE>( IO_ZIP_MODE_B, FTI_CPC_ZFP ) );
  //m_io_zip_mode_map.insert( std::pair<io_zip_mode_t,FTIT_CPC_MODE>( IO_ZIP_MODE_C, FTI_CPC_SINGLE ) );
  //m_io_zip_mode_map.insert( std::pair<io_zip_mode_t,FTIT_CPC_MODE>( IO_ZIP_MODE_D, FTI_CPC_HALF ) );
  //m_io_zip_type_inv_map.insert( std::pair<FTIT_CPC_TYPE,io_zip_type_t>( FTI_CPC_TYPE_NONE, IO_ZIP_TYPE_DEFAULT ) );
  //m_io_zip_type_inv_map.insert( std::pair<FTIT_CPC_TYPE,io_zip_type_t>( FTI_CPC_ACCURACY, IO_ZIP_TYPE_A ) );
  //m_io_zip_type_inv_map.insert( std::pair<FTIT_CPC_TYPE,io_zip_type_t>( FTI_CPC_PRECISION, IO_ZIP_TYPE_B ) );
  //m_io_zip_mode_inv_map.insert( std::pair<FTIT_CPC_MODE,io_zip_mode_t>( FTI_CPC_MODE_NONE, IO_ZIP_MODE_DEFAULT ) );
  //m_io_zip_mode_inv_map.insert( std::pair<FTIT_CPC_MODE,io_zip_mode_t>( FTI_CPC_FPZIP, IO_ZIP_MODE_A ) );
  //m_io_zip_mode_inv_map.insert( std::pair<FTIT_CPC_MODE,io_zip_mode_t>( FTI_CPC_ZFP, IO_ZIP_MODE_B ) );
  //m_io_zip_mode_inv_map.insert( std::pair<FTIT_CPC_MODE,io_zip_mode_t>( FTI_CPC_SINGLE, IO_ZIP_MODE_C ) );
  //m_io_zip_mode_inv_map.insert( std::pair<FTIT_CPC_MODE,io_zip_mode_t>( FTI_CPC_HALF, IO_ZIP_MODE_D ) );
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
  m_zip_controller.init();
  MDBG("[num_parameters:%d] FTI CONTROLLER", get_num_parameters());
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
        MDBG("head slave got notified TAG : %d", tag);
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
  MDBG("try to load ckpt id: %ld", to_ckpt_id(state_id));
  bool res = FTI_Load( to_ckpt_id(state_id), m_io_level_map[level] ) == FTI_SCES;
  M_TRIGGER(STOP_FTI_LOAD, state_id.param);
  return res; 
}

void FtiController::store( io_state_id_t state_id, io_level_t level ) {
  assert( m_io_level_map.count(level) != 0 && "invalid checkpoint level" );
  MDBG("storing state {id:%d | t:%d | p:%d}", state_id.id, state_id.t, get_parameter_id());
  M_TRIGGER(START_FTI_STORE, state_id.t);
  FTI_Checkpoint( to_ckpt_id(state_id), m_io_level_map[level] );
  M_TRIGGER(STOP_FTI_STORE, state_id.param);
  //if( (state_id.t == 0) && m_zip_controller.is_validate() ) {
  //  for(int m=1; m<m_zip_controller.num_parameters(); m++) {
  //    for(auto const& var : m_var_id_map) {
  //      protect( var.first, var.second.data, var.second.size, var.second.type );
  //    }
  //    FTI_Checkpoint( to_ckpt_id(state_id, m), m_io_level_map[level] );
  //    m_zip_controller.advance_validate();
  //  }
  //} else if ( m_zip_controller.is_validate() ) {
  //  m_zip_controller.advance_validate();
  //}
  //advance_validate();
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
      int64_t ckpt_id = to_ckpt_id( state_id );
      char ckpt_id_c_str[256];
      snprintf(ckpt_id_c_str, 256, "%ld", ckpt_id);
      std::string ckpt_id_str(ckpt_id_c_str);
      std::string g_path_string = m_dict_string["global_dir"] + "/" + ckpt_id_str;
      MDBG("path to delete -> '%s'", g_path_string.c_str());
			if( nftw(g_path_string.c_str(), rm, FOPEN_MAX, FTW_DEPTH) == -1 ){ 
        MERR("failed to remove checkpoint path '%s'", g_path_string.c_str());
      }
      m_kernel.remove_ckpt_metadata( to_ckpt_id( state_id ), m_io_level_map[level] );
      mpi.barrier();
    } else {
      m_kernel.remove_ckpt_metadata( to_ckpt_id( state_id ), m_io_level_map[level] );
      mpi.barrier();
    }
    M_TRIGGER(STOP_DELETE_PFS,0);
  }
}

void FtiController::stage( io_state_id_t state_id, io_level_t from, io_level_t to ) {
  
  MDBG("staging state {id:%d | t:%d | p:%d}", state_id.id, state_id.t, get_parameter_id());
  
  assert( m_io_level_map.count(from) != 0 && "invalid checkpoint level" );
  assert( m_io_level_map.count(to) != 0 && "invalid checkpoint level" );
  assert( m_kernel.topo->amIaHead == 1 && "copy for application threads not implemented for extern" );
  
  std::string l2_temp_dir, l2_ckpt_dir;
  std::string l1_temp_dir, l1_ckpt_dir, l1_meta_temp_dir, l1_meta_dir;
  
  l2_ckpt_dir = m_dict_string["global_dir"] + "/" + std::to_string(to_ckpt_id(state_id));
  l1_ckpt_dir = m_dict_string["local_dir"] + "/" + m_dict_string["exec_id"] + "/l1/" + std::to_string(to_ckpt_id(state_id));
  l1_meta_dir = m_dict_string["meta_dir"] + "/" + m_dict_string["exec_id"] + "/l1/" + std::to_string(to_ckpt_id(state_id)); 
  l1_temp_dir = m_dict_string["local_dir"] + "/" + m_dict_string["exec_id"] + "/" + melissa::helpers::make_uuid();
  
  if( m_dict_bool["master_global"] ) {
    l2_temp_dir = m_dict_string["global_dir"] + "/" + melissa::helpers::make_uuid();
    l1_meta_temp_dir = m_dict_string["meta_dir"] + "/" + m_dict_string["exec_id"] + "/" + melissa::helpers::make_uuid();
  }
  mpi.broadcast(l2_temp_dir);
  mpi.broadcast(l1_meta_temp_dir);

  MDBG("l2_ckpt_dir: %s", l2_ckpt_dir.c_str());
  MDBG("l1_ckpt_dir: %s", l1_ckpt_dir.c_str());
  MDBG("l1_meta_dir: %s", l1_meta_dir.c_str());
  MDBG("l1_temp_dir: %s", l1_temp_dir.c_str());
  MDBG("l2_temp_dir: %s", l2_temp_dir.c_str());
  MDBG("l1_meta_temp_dir: %s", l1_meta_temp_dir.c_str());

  if( from == IO_STORAGE_L1 ) {
    stage_l1l2( l1_ckpt_dir, l1_meta_dir, l2_temp_dir, l2_ckpt_dir, state_id  );
  }
  else {
    stage_l2l1( l2_ckpt_dir, l1_temp_dir, l1_meta_temp_dir, l1_ckpt_dir, l1_meta_dir, state_id  );
  }

  mpi.barrier();
  
}

void FtiController::stage_l1l2( std::string l1_ckpt_dir, std::string l1_meta_dir, std::string l2_temp_dir,
    std::string l2_ckpt_dir, io_state_id_t state_id  ) {

  M_TRIGGER(START_PUSH_STATE_TO_PFS,state_id.t);
  
  if( m_dict_bool["master_global"] ) {
    struct stat info;
    IO_TRY( stat( l2_ckpt_dir.c_str(), &info ), -1, "the global checkpoint directory already exists!" );
    IO_TRY( stat( l2_temp_dir.c_str(), &info ), -1, "the global checkpoint directory already exists!" );
    IO_TRY( mkdir( l2_temp_dir.c_str(), 0777 ), 0, "unable to create directory" );
  }
  
  mpi.barrier();

  std::string l2_ckpt_fn, l2_meta_fn;

  l2_ckpt_fn = l2_temp_dir + "/Ckpt" + std::to_string(to_ckpt_id(state_id)) + "-worker" + std::to_string(m_kernel.topo->splitRank) + "-serialized.fti";
  l2_meta_fn = l2_temp_dir + "/Meta" + std::to_string(to_ckpt_id(state_id)) + "-worker" + std::to_string(m_kernel.topo->splitRank) + "-serialized.fti";
  
  int fd = open( l2_ckpt_fn.c_str(), O_WRONLY|O_CREAT, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR );
  
  std::ofstream metafs( l2_meta_fn );
    
  uint64_t t_open_local=0, t_read_local=0, t_write_global=0, t_close_local=0, t_meta=0; 

  for(int i=0; i<m_dict_int["app_procs_node"]; i++) {
    
    int proc = m_kernel.topo->body[i];
    
    std::chrono::system_clock::time_point t1, t2;
    
    t1 = std::chrono::system_clock::now();
    // FIXME this only takes into account group size of 1!!!
    //if (m_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::string l1_meta_fn = l1_meta_dir + "/sector" + std::to_string(m_kernel.topo->sectorID) + "-group" + std::to_string(groupId) + ".fti";
      std::ifstream tmp_metafs( l1_meta_fn );
      std::string str( std::istreambuf_iterator<char>(tmp_metafs), (std::istreambuf_iterator<char>()) );
      size_t count_lines = std::count_if( str.begin(), str.end(), []( char c ){return c =='\n';});
      metafs << count_lines << std::endl << str << std::flush;
      tmp_metafs.close();
    //}
    t2 = std::chrono::system_clock::now();
    t_meta += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    int64_t local_file_size;
    m_kernel.load_ckpt_meta_proc( to_ckpt_id(state_id), proc, &local_file_size, l1_meta_fn.c_str() );
    
    std::string l1_ckpt_fn = l1_ckpt_dir + "/Ckpt" + std::to_string(to_ckpt_id(state_id)) + "-Rank" + std::to_string(proc) + ".fti";
    
    
    t1 = std::chrono::system_clock::now();
    int lfd = open(l1_ckpt_fn.c_str(), O_RDONLY, 0);
    std::unique_ptr<char[]> buffer(new char[IO_TRANSFER_SIZE]);
    t2 = std::chrono::system_clock::now();
    t_open_local += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

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
      t_read_local += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
      
      // check if successful
      if (check != bSize) {
        MERR("unable to read '%lu' bytes from file '%s'", bSize, l1_ckpt_fn.c_str());
      }

      t1 = std::chrono::system_clock::now();
      check = write( fd, buffer.get(), bSize );
      t2 = std::chrono::system_clock::now();
      t_write_global += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
      
      // check if successful
      if (check != bSize) {
        MERR("unable to write '%lu' bytes into file '%s'", bSize, l2_ckpt_fn.c_str());
      }

      pos = pos + bSize;
    }
    
    t1 = std::chrono::system_clock::now();
    close(lfd);
    t2 = std::chrono::system_clock::now();
    t_close_local += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

  }
    
  MDBG("Performance -> [ol: %lu ms|rl: %lu ms|wg: %lu ms|cl: %lu ms|m: %lu ms]", t_open_local, t_read_local, t_write_global, t_close_local, t_meta);

  fsync(fd);
  close(fd);
  metafs.flush();
  metafs.close();

  mpi.barrier();

  if( m_dict_bool["master_global"] ) {
    IO_TRY( std::rename( l2_temp_dir.c_str(), l2_ckpt_dir.c_str() ), 0, "unable to rename local_meta directory" );
  }
  
  mpi.barrier();

  update_metadata( state_id, IO_STORAGE_L2 );
    
  std::stringstream msg;
  msg << "Conversion of Ckpt." << to_ckpt_id(state_id) << "from level '" << 1 << "' to '" << 4 << "' was successful";
  m_kernel.print(msg.str(), FTI_INFO);
  
  M_TRIGGER(STOP_PUSH_STATE_TO_PFS,state_id.param);

}

void FtiController::stage_l2l1( std::string l2_ckpt_dir, std::string l1_temp_dir, std::string l1_meta_temp_dir,
    std::string l1_ckpt_dir, std::string l1_meta_dir, io_state_id_t state_id ) {

  M_TRIGGER(START_COPY_STATE_FROM_PFS,state_id.t);
  
  MDBG("pulling state_id '{t: %d, id: %d}'", state_id.t, state_id.id);

  struct stat info;
  IO_TRY( stat( l1_ckpt_dir.c_str(), &info ), -1, "the local checkpoint directory already exists!" );
  IO_TRY( stat( l1_temp_dir.c_str(), &info ), -1, "the local checkpoint directory already exists!" );
  
  if( m_dict_bool["master_global"] ) {
    IO_TRY( stat( l1_meta_temp_dir.c_str(), &info ), -1, "the local checkpoint directory already exists!" );
    IO_TRY( mkdir( l1_meta_temp_dir.c_str(), 0777 ), 0, "unable to create directory" );
  }
  
  mpi.barrier(); 

  IO_TRY( mkdir( l1_temp_dir.c_str(), 0777 ), 0, "unable to create directory" );
  
  std::string l2_ckpt_fn, l2_meta_fn;
  l2_ckpt_fn = l2_ckpt_dir + "/Ckpt" + std::to_string(to_ckpt_id(state_id)) + "-worker" + std::to_string(m_kernel.topo->splitRank) + "-serialized.fti";
  l2_meta_fn = l2_ckpt_dir + "/Meta" + std::to_string(to_ckpt_id(state_id)) + "-worker" + std::to_string(m_kernel.topo->splitRank) + "-serialized.fti";

  int fd = open( l2_ckpt_fn.c_str(), O_RDWR );
  if( fd < 0 ) {
    MERR("unable to read from file '%s'", l2_ckpt_fn.c_str());
  }
  //if (m_kernel.topo->groupRank == 0) {
    std::ifstream metafs( l2_meta_fn );
    std::string metastr( (std::istreambuf_iterator<char>(metafs) ),
                         (std::istreambuf_iterator<char>()    ) );
    metafs.close();
    std::istringstream metaiss(metastr);
  //}
  
  for(int i=0; i<m_dict_int["app_procs_node"]; i++) {
    
    int proc = m_kernel.topo->body[i];
    
    /****************************************************************************
     * 
     *    transfer meta data
     *
     ***************************************************************************/
    
    //if (m_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::string l1_meta_temp_fn = l1_meta_temp_dir + "/sector" + std::to_string(m_kernel.topo->sectorID) + "-group" + std::to_string(groupId) + ".fti";
      std::ofstream tmp_metafs(l1_meta_temp_fn);
      std::string count_str;
      std::getline( metaiss, count_str );
      size_t count;
      sscanf(count_str.c_str(), "%lu", &count);
      MDBG("count: %lu", count);
      for(size_t i=0; i<count; i++) {
        std::string line;
        std::getline( metaiss, line );
        tmp_metafs << line << std::endl << std::flush;
      }
      tmp_metafs.close();
    //}
    
    /****************************************************************************
     * 
     *    transfer ckpt data
     *
     ***************************************************************************/

    std::string l1_ckpt_fn = l1_temp_dir + "/Ckpt" + std::to_string(to_ckpt_id(state_id)) + "-Rank" + std::to_string(proc) + ".fti";
      
    MDBG( "trying to transfer to: '%s'", l1_ckpt_fn.c_str() );
    
    int64_t local_file_size;
    m_kernel.load_ckpt_meta_proc( to_ckpt_id(state_id), proc, &local_file_size, l1_meta_temp_fn );
    
    MDBG( "number of bytes to transfer: '%ld'", local_file_size );
    
    int lfd = open( l1_ckpt_fn.c_str(), O_WRONLY|O_CREAT, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR );
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
        MERR("unable to read '%lu' from file '%s'", bSize, l2_ckpt_fn.c_str());
        return;
      }
      
      check = write(lfd, buffer.get(), bSize);
      // check if successful
      if (check != bSize) {
        MERR("unable to write '%lu' into file '%s'", bSize, l1_ckpt_fn.c_str());
        return;
      }
      pos = pos + bSize;
    }
    
    fsync(lfd);
    close(lfd);

  }

  close(fd);
  
  mpi.barrier();
  
  if( m_dict_bool["master_global"] ) {
    IO_TRY( std::rename( l1_meta_temp_dir.c_str(), l1_meta_dir.c_str() ), 0, "unable to rename local directory" );
  }
  IO_TRY( std::rename( l1_temp_dir.c_str(), l1_ckpt_dir.c_str() ), 0, "unable to rename local_meta directory" );

  update_metadata( state_id, IO_STORAGE_L1 );

  mpi.barrier();

  std::stringstream msg;
  msg << "Conversion of Ckpt." << to_ckpt_id(state_id) << "from level '" << 4 << "' to '" << 1 << "' was successful";
  m_kernel.print(msg.str(), FTI_INFO);

  M_TRIGGER(STOP_COPY_STATE_FROM_PFS,state_id.param);

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

