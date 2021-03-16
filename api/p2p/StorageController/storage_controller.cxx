#include "storage_controller.hpp"
#include "storage_controller_impl.hpp"

#include "utils.h"
#include <memory>
#include <numeric>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>

// TODO error checking!

//#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

void StorageController::init( MpiController* mpi, IoController* io ) { 
  m_peer = new PeerController();
  m_io = io;
  m_mpi = mpi;
  m_io->register_callback( StorageController::callback );
  m_comm_global_size = m_mpi->size();
  // heads dont return here!
  m_io->init_io(m_mpi);
  m_io->init_core(m_mpi);
  m_comm_worker_size = m_io->m_dict_int["nodes"]; 
  m_comm_runner_size = m_io->m_dict_int["nodes"] * (m_io->m_dict_int["procs_node"]-1);
  m_worker_thread = false;
}

void StorageController::fini() {
  delete m_peer;
  int dummy[2];
  m_io->sendrecv( &dummy[0], &dummy[1], sizeof(int), IO_TAG_FINAL, IO_MSG_MASTER );
  m_mpi->barrier("fti_comm_world");
  m_io->fini();
}

// Callback called in the FTI head loop:
void StorageController::callback() {
  
  //void* context;
  static bool init = false;
  if( !init ) {
    storage.m_io->init_core(storage.m_mpi);
    storage.m_comm_worker_size = storage.m_io->m_dict_int["nodes"]; 
    storage.m_comm_runner_size = storage.m_io->m_dict_int["nodes"] * (storage.m_io->m_dict_int["procs_node"]-1);
    init = true;
  }
  //static std::unique_ptr<StateServer> state_server(nullptr);
  
  //if (state_server.get() == nullptr) {
  //  // open all sockets necessary
  //  state_server = std::make_unique<StateServer>(context);
  //  storage.m_worker_thread = true;
  //}

  storage.m_finalize_worker();
  
  // query info from server
  // submit:
  // - remove requests
  // - load requests for prefetching
  storage.m_query_server();

  // has to have the highest priority!!!
  storage.m_state_info_user();

  // check and handle state request from Alice
  storage.m_state_request_user();

  // check and handle push request
  storage.m_state_request_push();

  // check and handle state request from Bob
  storage.m_state_request_peer();

  // check and handle delete request
  storage.m_state_request_remove();

  // check and handle delete request
  storage.m_state_request_load();
}

int StorageController::protect( void* buffer, size_t size, io_type_t type) {
  m_io->protect(buffer, size, type);  
}

int StorageController::update( io_id_t id, void* buffer, io_size_t size ) {
  m_io->update(id, buffer, size);  
}

void StorageController::load( io_id_t state_id ) {
  if( m_worker_thread ) {
    return m_load_head( state_id );
  } else {
    return m_load_user( state_id );
  }
}

void StorageController::store( io_id_t state_id ) {
  if( m_worker_thread ) {
    return m_store_head( state_id );
  } else {
    return m_store_user( state_id );
  }
}

void StorageController::copy( io_id_t state_id, io_level_t from, io_level_t to) {
  m_io->copy(state_id, from, to);  
}

void StorageController::copy_extern( io_id_t state_id ) {
  
  std::stringstream global;
  global << m_io->m_dict_string["global_dir"];
  global << "/";
  global << m_states[state_id].io_exec_id;
  global << "/l4/";
  global << std::to_string(state_id);
  
  std::stringstream extern_meta;
  extern_meta << m_io->m_dict_string["meta_dir"];
  extern_meta << "/";
  extern_meta << m_states[state_id].io_exec_id;
  extern_meta << "/l4/";
  extern_meta << std::to_string(state_id);
  
  std::stringstream meta;
  meta << m_io->m_dict_string["meta_dir"];
  meta << "/";
  meta << m_io->m_dict_string["exec_id"];
  meta << "/l1/";
  meta << std::to_string(state_id);
  
  std::stringstream meta_tmp_dir;
  meta_tmp_dir << m_io->m_dict_string["meta_dir"];
  meta_tmp_dir << "/tmp";
    
  std::stringstream local_tmp_dir;
  local_tmp_dir << m_io->m_dict_string["local_dir"];
  local_tmp_dir << "/tmp";
    
  std::stringstream local_tmp;
  local_tmp << local_tmp_dir.str();

  if( m_io->m_dict_bool["master_local"] ) {
    assert( mkdir( local_tmp_dir.str().c_str(), 0777 ) == 0 );  
  }

  if( m_io->m_dict_bool["master_global"] ) {
    assert( mkdir( meta_tmp_dir.str().c_str(), 0777 ) == 0 );
  }

  m_mpi->barrier();
 
  for(int i=0; i<m_io->m_dict_int["app_procs_node"]; i++) {
    int proc = m_fti_kernel.topo->body[i];

    std::stringstream filename;
    filename << "Ckpt" << state_id << "-Rank" << proc << ".fti";
    
    std::stringstream global_fn;
    global_fn << global.str(); 
    global_fn << "/";
    global_fn << filename.str(); 
    
    std::stringstream local_tmp_fn;
    local_tmp_fn << local_tmp_dir.str();
    local_tmp_fn << "/";
    local_tmp_fn << filename.str();

    m_fti_kernel.file_copy( global_fn.str(), local_tmp_fn.str() );

    if (m_fti_kernel.topo->groupRank == 0) {
      int groupId = i+1;
      std::stringstream metafilename_extern;
      metafilename_extern << extern_meta.str();
      metafilename_extern << "/";
      metafilename_extern << "sector" << m_fti_kernel.topo->sectorID << "-group" << groupId << ".fti";
      std::stringstream metafilename_tmp;
      metafilename_tmp << meta_tmp_dir.str();
      metafilename_tmp << "/";
      metafilename_tmp << "sector" << m_fti_kernel.topo->sectorID << "-group" << groupId << ".fti";
      m_fti_kernel.file_copy( metafilename_extern.str(), metafilename_tmp.str() );
    }

  }
  
  m_mpi->barrier();
  
  std::stringstream local;
  local << m_io->m_dict_string["local_dir"];
  local << "/";
  local << m_states[state_id].io_exec_id;
  local << "/l1/";
  local << std::to_string(state_id);

  if( m_io->m_dict_bool["master_local"] ) {
    assert( std::rename( local_tmp_dir.str().c_str(), local.str().c_str() ) == 0 ); 
  }
  if( m_io->m_dict_bool["master_global"] ) {
    assert( std::rename( meta_tmp_dir.str().c_str(), meta.str().c_str() ) == 0 ); 
    m_fti_kernel.update_ckpt_metadata( state_id, FTI_L1 );
  }

  m_mpi->barrier();

}

void StorageController::m_load_head( io_id_t state_id ) {
  if( !m_io->is_local( state_id ) ) {
    m_peer->request( state_id );
  }
  if( !m_io->is_local( state_id ) ) {
    sleep(2);
    if( m_io->is_global( state_id ) ) {
      //m_io->copy( state_id, IO_STORAGE_L2, IO_STORAGE_L1 );
      copy_extern( state_id );
    } else {
      copy_extern( state_id );
    }
  }
}

void StorageController::m_load_user( io_id_t state_id ) {
  if( !m_io->is_local( state_id ) ) {
    sleep(1);
    int status;
    m_io->sendrecv( &state_id, &status, sizeof(io_id_t), IO_TAG_REQUEST, IO_MSG_ALL );
    // TODO check status
  }
  assert( m_io->is_local( state_id ) && "unable to load state to local storage" );
  m_io->load( state_id );
}
    
void StorageController::m_store_head( io_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_store_user( io_id_t state_id ) {
  m_io->store( state_id );
  assert( m_io->is_local( state_id ) && "unable to store state to local storage" );
}

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
    io_id_t info[4];
    // [0]->cycle_active, [1]->parent_id_active, [2]->cycle_finished, [3]->parent_id_finished
    m_io->recv( &info, sizeof(io_id_t)*4, IO_TAG_MESSAGE, IO_MSG_MASTER );
    state_info_t active_state_info;
    state_info_t finished_state_info;
    io_id_t active_state_id = generate_state_id( info[0], info[1] ); 
    io_id_t finished_state_id = generate_state_id( info[2], info[3] ); 
    if( m_states.count( active_state_id ) == 0 ) {
      active_state_info.status = MELISSA_STATE_BUSY;
      active_state_info.io_exec_id = m_io->m_dict_string["exec_id"]; 
      active_state_info.runner_id = m_runner_id;
      active_state_info.cycle = info[0];
      active_state_info.parent_id = info[1];
      m_states.insert( std::pair<io_id_t, state_info_t>( active_state_id, active_state_info ) );
    } else {
      m_states[active_state_id].status = MELISSA_STATE_BUSY;
    }
    finished_state_info.status = MELISSA_STATE_IDLE;
    finished_state_info.io_exec_id = m_io->m_dict_string["exec_id"]; 
    finished_state_info.runner_id = m_runner_id;
    finished_state_info.cycle = info[2];
    finished_state_info.parent_id = info[3];
    m_states.insert( std::pair<io_id_t, state_info_t>( finished_state_id, finished_state_info ) );
    m_io->m_state_push_requests.push(finished_state_id);
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
      state_info_t fake_state_info;
      fake_state_info.status = MELISSA_STATE_IDLE;
      fake_state_info.io_exec_id = m_io->m_dict_string["exec_id"]; 
      fake_state_info.runner_id = m_runner_id;
      fake_state_info.cycle = 0;
      fake_state_info.parent_id = i;
      io_id_t fake_state_id = generate_state_id( 0, i );
      m_states.insert( std::pair<io_id_t, state_info_t>( fake_state_id, fake_state_info ) );
      m_io->m_state_pull_requests.push( i );
    }
  }
  count++;
}

