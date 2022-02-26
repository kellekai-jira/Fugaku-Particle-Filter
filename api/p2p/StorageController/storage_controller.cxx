#include "storage_controller.hpp"
#include "storage_controller_impl.hpp"

#include <sys/statvfs.h>
#include "utils.h"
#include <memory>
#include <numeric>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cmath>
#include <string>
#include <queue>
#include <fti.h>  // TODO: remove this from here!
#include <string>

#include "api_common.h"
// TODO error checking!

//dirty:
time_t next_try_delete = 0;

//======================================================================
//  STORAGE CONTROLLER INITIALIZATION
//======================================================================

// init MPI
// init IO

void StorageController::io_init( IoController* io, int runner_id ) {
  
  m_io = io;
  m_io->register_callback( StorageController::callback );

//  IO_TRY( std::getenv("MELISSA_DA_RUNNER_ID") != nullptr, true, "MELISSA_DA_RUNNER_ID variable is not set" );
//  m_runner_id = atoi(getenv("MELISSA_DA_RUNNER_ID"));
//  std::cout << "RUNNER_ID: " << m_runner_id << std::endl;
  m_runner_id = runner_id; 

  // heads dont return from init_io !!!
  m_io->init_io(runner_id);

  m_io->init_core();

}

void StorageController::init( size_t capacity, int64_t state_size_proc ) {
  // this is only called by app cores

  // propagate state size to head
  m_io->send( &capacity, sizeof(size_t), IO_TAG_POST, IO_MSG_ONE );
  m_io->send( &state_size_proc, sizeof(int64_t), IO_TAG_POST, IO_MSG_ALL );

  m_worker_thread = false;

}


void StorageController::fini() {
  printf("Finalizing storage controller\n");
  //delete m_peer;
  int dummy[2];
  m_io->sendrecv( &dummy[0], &dummy[1], sizeof(int), sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
  mpi.barrier("fti_comm_world");
  m_io->fini();

  zmq_ctx_destroy(m_zmq_context);
}



//======================================================================
//  STATIC CALLBACK FUNCTION
//======================================================================

// Callback called in the FTI head loop:
void StorageController::callback() {

  static bool init = false;
  if( !init ) {
    // To do good logging
    comm_rank = mpi.rank();
#ifdef REPORT_TIMING
#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
    try_init_timing();  // TODO actually we might call this even earlier for the heads just after initing the mpi
    M_TRIGGER(START_INIT, 0);
    }
#endif
    if ( comm_rank == 0 ) { 
      storage.m_zmq_context = zmq_ctx_new();  // TODO: simplify context handling
      storage.server.init();
    }
    storage.m_io->init_core();
    std::vector<std::string> files;
    storage.m_io->filelist_local( {0,1}, files );

    size_t capacity;
    storage.m_io->recv( &capacity, sizeof(size_t), IO_TAG_POST, IO_MSG_ONE );
    
    mpi.broadcast(capacity);

    std::vector<uint64_t> size_proc(storage.m_io->m_dict_int["app_procs_node"]), size_node;
    storage.m_io->recv( size_proc.data(), sizeof(int64_t), IO_TAG_POST, IO_MSG_ALL );

    uint64_t state_size_node = 0;
    for(int i=0; i<storage.m_io->m_dict_int["app_procs_node"]; i++) {
      state_size_node += size_proc[i];
    }

    // in slots
    capacity = capacity / state_size_node;

    // 2 model checkpoints
    // 1 for sending to peer (needs memory copy of ckpt file)
    size_t minimum_storage_requirement = 3;

    IO_TRY( minimum_storage_requirement > capacity, false, "Insufficiant storage capacity!" );

    // 1 model checkpoints
    // 1 for sending to peer (needs memory copy of ckpt file)
    size_t minimum_storage_reservation = 2;

    capacity -= minimum_storage_reservation;

    size_t prefetch_capacity = capacity - minimum_storage_reservation;

    prefetch_capacity = (prefetch_capacity<STORAGE_MAX_PREFETCH) ? prefetch_capacity : STORAGE_MAX_PREFETCH;

    storage.state_pool.init( prefetch_capacity );

    //storage.m_peer = new PeerController( storage.m_io, storage.m_zmq_context, storage.m_mpi );
    init = true;
  	
		struct statvfs buf;
  	statvfs(storage.m_io->m_dict_string["local_dir"].c_str(), &buf);
  	double avail_fs = static_cast<double>(buf.f_bsize) * buf.f_bavail;
  	double free_fs = static_cast<double>(buf.f_bsize) * buf.f_bfree;

    //if(storage.m_io->m_dict_bool["master_global"]) {
      std::cout << "storage capacity [# slots]: " << capacity << std::endl;
      std::cout << "state size per node [Mb]: " << ((double)state_size_node)/(1024*1024) << std::endl;
      std::cout << "prefetch capacity [# slots]: " << storage.state_pool.capacity() << std::endl;
      std::cout << "free [# slots]: " << storage.state_pool.free() << std::endl;
      std::cout << "free local fs: " << free_fs/(1024*1024*1024) << " GB" << std::endl;
      std::cout << "avail local fs: " << avail_fs/(1024*1024*1024) << " GB" << std::endl;
      std::cout << "avail local fs: " << avail_fs/(1024*1024*1024) << " GB" << std::endl;
 		  std::cout << "Total mem available: " << static_cast<double>(get_mem_total())/(1024*1024) << " GB" << std::endl;
    //}
    M_TRIGGER(STOP_INIT, 0);
  }

#ifdef REPORT_TIMING
#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
  timing->maybe_report();
    }
#endif

  // EVENT
  // message from alice, sent after completion of the forecast
  // containing (1) forecast state id and (2) WEIGHT.
  IO_PROBE( IO_TAG_POST, storage.m_request_post() );

  // EVENT
  // Alice cannot find the state locally. She sends a message
  // containing the state id.
  IO_PROBE( IO_TAG_LOAD, storage.m_request_load() );

  // EVENT
  // Bob check and serve peer requests
  // storage.m_request_peer();

  // QUERY (every x seconds)
  // request -> send (1) cached state list and (2) number of free slots to server
  // response -> receive up to 2 states to prefetch and up to 2 states to delete if neccessary
  // storage.m_query_server();

  // EVENT
  // this event is triggered, when the storage controller gets a
  // prefetch instruction from the weight server [requested in 'm_query_server']
  IO_PROBE( IO_TAG_PULL, storage.m_request_pull() );

  // app core may tell the head to finish application. This will end in a call to fti_finalize on all cores (app and fti head cores) to ensure that the last checkpoint finished writing
  IO_PROBE( IO_TAG_FINI, storage.m_request_fini() );

  IO_PROBE( IO_TAG_DUMP, storage.m_request_dump() );
  
  fflush(stdout);
}

//======================================================================
//  STORAGE CONTROLLER API
//======================================================================

int StorageController::protect( std::string name, void* buffer, size_t size, io_type_t type) {
  m_io->protect(name, buffer, size, type);
}

void StorageController::load( io_state_id_t state_id ) {
  if( m_worker_thread ) {
    return m_load_head( state_id );
  } else {
    return m_load_user( state_id );
  }
}

void StorageController::pull( io_state_id_t state_id ) {
	struct statvfs buf;
  statvfs(storage.m_io->m_dict_string["local_dir"].c_str(), &buf);
  double avail_fs = static_cast<double>(buf.f_bsize) * buf.f_bavail;
  double free_fs = static_cast<double>(buf.f_bsize) * buf.f_bfree;
  std::cout << "free local fs: " << free_fs/(1024*1024*1024) << " GB" << std::endl;
  std::cout << "avail local fs: " << avail_fs/(1024*1024*1024) << " GB" << std::endl;
 	std::cout << "Total mem available: " << static_cast<double>(get_mem_total())/(1024*1024) << " GB" << std::endl;
  if( m_worker_thread ) {
    return m_pull_head( state_id );
  } else {
    return m_pull_user( state_id );
  }
}

void StorageController::store( io_state_id_t state_id ) {
	struct statvfs buf;
  statvfs(storage.m_io->m_dict_string["local_dir"].c_str(), &buf);
  double avail_fs = static_cast<double>(buf.f_bsize) * buf.f_bavail;
  double free_fs = static_cast<double>(buf.f_bsize) * buf.f_bfree;
  std::cout << "free local fs: " << free_fs/(1024*1024*1024) << " GB" << std::endl;
  std::cout << "avail local fs: " << avail_fs/(1024*1024*1024) << " GB" << std::endl;
 	std::cout << "Total mem available: " << static_cast<double>(get_mem_total())/(1024*1024) << " GB" << std::endl;
  if( m_worker_thread ) {
    return m_store_head( state_id );
  } else {
    return m_store_user( state_id );
  }
}

void StorageController::copy( io_state_id_t state_id, io_level_t from, io_level_t to) {
  if( to == IO_STORAGE_L1 && state_pool.free() == 0 ) {
    server.delete_request(this);
  }
  m_io->stage(state_id, from, to);
  if( to == IO_STORAGE_L1 ) {
    state_pool++;
  }
}

void StorageController::m_pull_head( io_state_id_t state_id ) {
  if( m_io->is_local( state_id ) ) return;
  while( state_pool.free() <= 1  ) {
    server.delete_request(this);
  }
  //if( !m_io->is_local( state_id ) ) {
    //m_peer->mirror( state_id );
  //}
  if( !m_io->is_local( state_id ) ) {
    int id, t;
    M_TRIGGER(PFS_PULL, to_ckpt_id(state_id) );
    m_io->stage( state_id, IO_STORAGE_L2, IO_STORAGE_L1 );
  }
  m_cached_states.insert( std::pair<io_id_t,io_state_id_t>( to_ckpt_id(state_id), state_id ) );
  assert( m_io->is_local( state_id ) && "state should be local now!" );
  state_pool++;
  M_TRIGGER(STATE_LOCAL_CREATE, to_ckpt_id(state_id));
}

void StorageController::m_pull_user( io_state_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_load_head( io_state_id_t state ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_load_user( io_state_id_t state ) {
  bool local_hit = m_io->is_local( state );
  M_TRIGGER(START_M_LOAD_USER, (local_hit)?1:0 );
  io_id_t state_id = to_ckpt_id( state );
  if( local_hit ) {
    M_TRIGGER(LOCAL_HIT, to_ckpt_id(state));
  } else {
    M_TRIGGER(LOCAL_MISS, to_ckpt_id(state));
    int status;
    M_TRIGGER(START_WAIT_HEAD, to_ckpt_id(state));
    m_io->sendrecv( &state_id, &status, sizeof(io_id_t), sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
    M_TRIGGER(STOP_WAIT_HEAD, to_ckpt_id(state));
    assert( m_io->is_local( state ) && "unable to load state to local storage" );
  }
  //try {
    while (!m_io->load( state )){
        MDBG("try again... (id: %d, t: %d)", state.id, state.t);
        int status;
        // FIXME: remove this and let server send to app cores where they need to find the state from!
        M_TRIGGER(START_WAIT_HEAD, to_ckpt_id(state));
            m_io->sendrecv( &state_id, &status, sizeof(io_id_t), sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
            M_TRIGGER(DIRTY_LOAD, to_ckpt_id(state));
        M_TRIGGER(STOP_WAIT_HEAD, to_ckpt_id(state));
    }
  M_TRIGGER(STOP_M_LOAD_USER, to_ckpt_id(state) );
}

void StorageController::m_store_head( io_state_id_t state_id ) {
  assert( 0 && "not implemented" );
}

void StorageController::m_store_user( io_state_id_t state_id ) {
  int dummy;
  m_io->store( state_id );
}

//======================================================================
//  SERVER QUERY
//======================================================================

void StorageController::m_query_server() {
  if( m_trigger_query() ) server.prefetch_request( this );
}

//======================================================================
//  REQUESTS
//======================================================================

void StorageController::m_communicate( io_tag_t tag ) {
  assert( m_io->m_dict_bool["master_global"] && "must only be called from head master!" ); 
  if(mpi.size() == 1) return;
  std::vector<MPI_Request> send_request(mpi.size()-1);
  std::vector<MPI_Request> recv_request(mpi.size()-1);
  std::vector<MPI_Status> send_status(mpi.size()-1);
  std::vector<MPI_Status> recv_status(mpi.size()-1);
  for(int i=1; i<mpi.size(); i++) {
    MDBG("head master notifies slave TAG : %d", tag);
    MPI_Isend( NULL, 0, MPI_BYTE, i, tag, mpi.comm(), &send_request[i-1] );
    MPI_Irecv( NULL, 0, MPI_BYTE, i, tag, mpi.comm(), &recv_request[i-1] );
  }
  MPI_Waitall( mpi.size()-1, &send_request[0], &send_status[0] );
  MPI_Waitall( mpi.size()-1, &recv_request[0], &recv_status[0] );
}

void StorageController::reprotect() {
  m_io->reprotect_all();
}

void StorageController::m_request_post() {
  M_TRIGGER(START_MODEL_MESSAGE,0);
  if(m_io->m_dict_bool["master_global"]) {
    std::cout << "head received INFORMATION request" << std::endl;
    fflush(stdout);
    // forward request to other head ranks
    m_communicate( IO_TAG_POST );
  }
  
  //static mpi_request_t req;
  //req.wait();  // be sure that there is nothing else in the mpi send queue

  Message weight_message;
  int msg_size;
  m_io->get_message_size( &msg_size, IO_TAG_POST, IO_MSG_MST );
  std::vector<char> buffer(msg_size);
  m_io->recv( buffer.data(), msg_size, IO_TAG_POST, IO_MSG_MST );
  
  mpi.broadcast(msg_size);
  mpi.broadcast(buffer);

  weight_message.ParseFromArray( buffer.data(), msg_size );

  assert( weight_message.has_weight() && "m does not contain a weight" );

  io_state_id_t state_id( weight_message.weight().state_id().t(), weight_message.weight().state_id().id(), storage.m_io->get_parameter_id() );
  io_id_t ckpt_id = to_ckpt_id( state_id );

  m_io->update_metadata( state_id, IO_STORAGE_L1 );
  assert( m_io->is_local(state_id) && "state should be local");
  m_io->stage( state_id, IO_STORAGE_L1, IO_STORAGE_L2 );

  //m_ckpted_states.insert( std::pair<io_id_t, io_state_id_t>( ckpt_id, state_id ) );
   
  if( storage.m_io->get_parameter_id() > 0 ) {
    storage.advance_validate();
    return;
  }
  storage.advance_validate();
  
  m_push_weight_to_server( weight_message );
  
  mpi.barrier();

  m_cached_states.insert( std::pair<io_id_t, io_state_id_t>( ckpt_id, state_id ) );
  state_pool++;
  std::cout << "FREE SLOTS: " << state_pool.free() << std::endl;
  // if slot free, keep checkpoint. If not ask to delete a state
  if( state_pool.free() == 0 ) {
    server.delete_request(this);
  }

  //int dummy;
  //m_io->send( &dummy, sizeof(int), IO_TAG_POST, IO_MSG_ONE );

  // ask if something to prefetch
  server.prefetch_request( this );


  
  if (state_id.t > m_io->m_dict_int["current_cycle"]) {  // what assimilation cycle are we working on? important for auto removing later
    m_io->m_dict_int["current_cycle"] = state_id.t;
  }

  M_TRIGGER(STOP_MODEL_MESSAGE, ckpt_id);
  M_TRIGGER(STATE_LOCAL_CREATE, ckpt_id);
}

// (1) state request from user to worker
void StorageController::m_request_load() {
  if(m_io->m_dict_bool["master_global"]) {
    // forward request to other head ranks
    m_communicate( IO_TAG_LOAD );
  }
  std::vector<io_id_t> state_id(m_io->m_dict_int["app_procs_node"]);
  int result=(int)IO_SUCCESS;
  m_io->recv( state_id.data(), sizeof(io_id_t), IO_TAG_LOAD, IO_MSG_ALL );
  //if(m_io->m_dict_bool["master_global"]) {
    std::cout << "head received LOAD request (ckpt_id: "<<state_id[0]<<")" << std::endl;
  //}
  pull( to_state_id(state_id[0]) );
  m_io->send( &result, sizeof(int), IO_TAG_LOAD, IO_MSG_ALL );
}

// (2) state request from peer to worker
void StorageController::m_request_peer() {
  //m_peer->handle_requests();
}

void StorageController::m_request_pull() {
  io_state_id_t state_id;
  if(m_io->m_dict_bool["master_global"]) {
    std::cout << "head received PULL request" << std::endl;
    // forward request to other head ranks
    assert(!m_io->m_state_pull_requests.empty() && "no state to pull!");
    m_communicate( IO_TAG_PULL );
    state_id = m_io->m_state_pull_requests.front();
    m_io->m_state_pull_requests.pop();
  }
  mpi.broadcast(state_id);
  if(!m_io->is_local(state_id)) {
    pull( state_id );
  }
}

void StorageController::m_request_dump() {
  io_state_id_t to_remove;
  if(m_io->m_dict_bool["master_global"]) {
    std::cout << "head received DUMP request" << std::endl;
    // forward request to other head ranks
    while (m_io->m_state_dump_requests.size() > 0 && m_io->m_state_dump_requests.front().t < m_io->m_dict_int["current_cycle"]-2) {
      m_communicate( IO_TAG_DUMP );
      to_remove = m_io->m_state_dump_requests.front();
      MDBG("Automatically removing the state t=%d, id=%d from the pfs", to_remove.t, to_remove.id);
      mpi.broadcast(to_remove);
      storage.m_io->remove( to_remove, IO_STORAGE_L2 ); 
      m_io->m_state_dump_requests.pop();
    }
  } else {
    mpi.broadcast(to_remove);
    MDBG("Automatically removing the state t=%d, id=%d from the pfs", to_remove.t, to_remove.id);
    storage.m_io->remove( to_remove, IO_STORAGE_L2 );
  }
}


//======================================================================
//  FINALIZE REQUEST
//======================================================================

void StorageController::m_request_fini() {
  if(m_io->m_dict_bool["master_global"]) std::cout << "head received FINALIZATION request" << std::endl;
  /// Request handled to end the Runner
  int dummy;
  m_io->recv( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
  //m_request_post();
  //while( m_io->probe( IO_TAG_PUSH ) ) {
  //  if(m_io->m_dict_bool["master_global"]) std::cout << "head received PUSH request: " << std::endl;
  //  m_io->stage( m_io->m_state_push_requests.front(), IO_STORAGE_L1, IO_STORAGE_L2 );
  //  m_io->m_state_push_requests.pop();
  //}
  //while( m_io->probe( IO_TAG_PULL ) ) {
  //  if(m_io->m_dict_bool["master_global"]) std::cout << "head received PULL request" << std::endl;
  //  pull( m_io->m_state_pull_requests.front() );
  //  m_io->m_state_pull_requests.pop();
  //}
  server.fini();
  m_io->send( &dummy, sizeof(int), IO_TAG_FINI, IO_MSG_ONE );
}

//======================================================================
//  State Pool
//======================================================================

void StorageController::StatePool::init( size_t capacity ) {
  m_capacity = capacity;
}

// prefix increment
StorageController::StatePool& StorageController::StatePool::operator++() {
  m_used_slots++;
  assert( m_used_slots <= m_capacity && "state pool overflow!" );
  return *this;
}

// postfix increment
StorageController::StatePool StorageController::StatePool::operator++(int) {
  StorageController::StatePool tmp = *this;
  ++*this;
  assert( m_used_slots <= m_capacity && "state pool overflow!" );
  return tmp;
}

// prefix decrement
StorageController::StatePool& StorageController::StatePool::operator--() {
  m_used_slots--;
  assert( m_used_slots >= 0 && "state pool underflow!" );
  return *this;
}

// postfix decrement
StorageController::StatePool StorageController::StatePool::operator--(int) {
  StorageController::StatePool tmp = *this;
  --*this;
  assert( m_used_slots >= 0 && "state pool underflow!" );
  return tmp;
}


//======================================================================
//  SERVER REQUESTS
//======================================================================

void StorageController::Server::init() { // FIXME: why not simply using constructor and destructor instead of init and fini ?
  if( mpi.rank() != 0 ) return;
  char * melissa_server_master_gp_node = getenv(
      "MELISSA_SERVER_MASTER_GP_NODE");
  if (melissa_server_master_gp_node == nullptr)
  {
    MPRT(
        "you must set the MELISSA_SERVER_MASTER_GP_NODE environment variable before running!");
    assert(false);
  }
  
  m_socket = zmq_socket(storage.m_zmq_context, ZMQ_REQ);
  std::string port_name = fix_port_name(melissa_server_master_gp_node);
  IO_TRY( zmq_connect(m_socket, port_name.c_str()), 0, "unable to connect to zmq socket" );
}

void StorageController::Server::fini() {
  if( mpi.rank() != 0 ) return;
  zmq_close(m_socket);
}

void StorageController::Server::prefetch_request( StorageController* storage ) {

  M_TRIGGER(START_PREFETCH,0);
  
  std::vector<io_state_id_t> dump;

  if( mpi.rank() == 0 ) {

    // TODO server needs to assure that the minimum storage requirements
    // are fullfilled (minimum 2 slots for ckeckpoints and other peer requests).
    Message request;

    for( const auto& pair : storage->m_cached_states ) {
      auto state = request.mutable_prefetch_request()->add_cached_states();
      state->set_t(pair.second.t);
      state->set_id(pair.second.id);
    }
    request.mutable_prefetch_request()->set_capacity(storage->state_pool.capacity());
    request.mutable_prefetch_request()->set_free(storage->state_pool.free());
    request.set_runner_id(storage->m_runner_id);

    M_TRIGGER(START_PREFETCH_REQ,0);
    send_message(m_socket, request);
    auto response = receive_message(m_socket);
    M_TRIGGER(STOP_PREFETCH_REQ,0);
  
    auto pull_states = response.prefetch_response().pull_states();
    for(auto it=pull_states.begin(); it!=pull_states.end(); it++) {
      io_state_id_t state = { it->t(), it->id() };
      storage->m_io->m_state_pull_requests.push( state );
    }

    auto dump_states = response.prefetch_response().dump_states();
    for(auto it=dump_states.begin(); it!=dump_states.end(); it++) {
      io_state_id_t state = { it->t(), it->id() };
      dump.push_back(state);
    }
  
  }
    
  mpi.broadcast(dump);
  for(auto & x : dump) {
    io_state_id_t state = { x.t, x.id };
    storage->m_io->remove( state, IO_STORAGE_L1 );
    storage->state_pool--;
  }
  
  mpi.barrier();

  M_TRIGGER(STOP_PREFETCH,0);

}

void StorageController::Server::delete_request( StorageController* storage ) {

  M_TRIGGER(START_DELETE,0);
  
  int t, id;
  
  if( mpi.rank() == 0 ) {
    
    Message request;

    for( const auto& pair : storage->m_cached_states ) {
      auto state = request.mutable_delete_request()->add_cached_states();
      state->set_t(pair.second.t);
      state->set_id(pair.second.id);
    }
    request.set_runner_id(storage->m_runner_id);

    // measure delete request
    M_TRIGGER(START_DELETE_REQ,0);
    send_message(m_socket, request);
    Message response = receive_message(m_socket);
    M_TRIGGER(STOP_DELETE_REQ,0);

    auto pull_state = response.delete_response().to_delete();
    t = pull_state.t();
    id = pull_state.id();


    std::cout << "HEAD IS REQUESTED TO DELETE (t: "<<t<<", id: "<<id<<")" <<  std::endl;

  }
  
  mpi.broadcast( t );
  mpi.broadcast( id );
  
  io_state_id_t state_id( t, id );

  storage->m_io->remove( state_id, IO_STORAGE_L1 );

  std::stringstream local;
  local << storage->m_io->m_dict_string["local_dir"];
  local << "/";
  local << storage->m_io->m_dict_string["exec_id"];
  local << "/l1/";
  local << std::to_string(to_ckpt_id(state_id));


  struct stat info;
  IO_TRY( stat( local.str().c_str(), &info ), -1, "the local checkpoint directory has not been deleted!" );
  MDBG("delete_request -> {t: %d, id: %d}", state_id.t, state_id.id);
  assert(!storage->m_io->is_local(state_id));
  
  storage->state_pool--;
  std::cout << "free: " << storage->state_pool.free() << std::endl;
  storage->m_cached_states.erase(to_ckpt_id(state_id));

  mpi.barrier();

  M_TRIGGER(STOP_DELETE,to_ckpt_id(state_id));
  M_TRIGGER(STATE_LOCAL_DELETE,to_ckpt_id(state_id));

}


//======================================================================
//  HELPER FUNCTIONS
//======================================================================

void StorageController::m_push_weight_to_server(const Message & m ) {
   
  if(!m_io->m_dict_bool["master_global"]) return;
  
  int t = m.weight().state_id().t();
  int id = m.weight().state_id().id();
    
  M_TRIGGER(START_PUSH_WEIGHT_TO_SERVER, t);
  
  send_message(server.m_socket, m);
  MDBG("Pushing weight message to weight server: %s", m.DebugString().c_str());
  zmq::recv(server.m_socket);  // receive ack
  // its good, we can give it free for delete...
  m_io->m_state_dump_requests.push(io_state_id_t(t, id));

  M_TRIGGER(STOP_PUSH_WEIGHT_TO_SERVER, id);
}

