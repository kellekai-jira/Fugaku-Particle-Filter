#include "app_core.h"
#include "StorageController/helpers.h"
#include "p2p.pb.h"

#include "api_common.h"
#include "StorageController/fti_controller.hpp"
#include "StorageController/storage_controller_impl.hpp"
#include "StorageController/mpi_controller_impl.hpp"

#include "StorageController/helpers.h"

#include <unistd.h>
#include <limits.h>

#include <cstdlib>
#include <memory>

#include "PythonInterface.h"

#include <mpi.h>
#include <mpi-ext.h>
#include <sstream>

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void melissa_da_signal_handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

void print_local_hostname ( const MPI_Comm & comm, const std::string & filename ) {
  const std::string delim(", ");
  std::string hostname;
  std::string ip;
  struct ifaddrs *ifap;
  int ndim, shape[3], coords[6] = {0,0,0,0,0,0};
  
  // hostname
  try {
    char temp[HOST_NAME_MAX];
    if (gethostname(temp,HOST_NAME_MAX) == -1) {
      hostname = "undefined"; 
    } else {
      temp[HOST_NAME_MAX-1] = '\0';
      hostname = temp; 
    }
  } catch (...) {
    hostname = "undefined"; 
  }
 
  // topology
  FJMPI_Topology_get_dimension( &ndim );
  FJMPI_Topology_get_shape( &shape[0], &shape[1], &shape[2] );
  FJMPI_Topology_get_coords( comm, 0, FJMPI_LOGICAL, ndim, coords );
  
  if(getifaddrs (&ifap) < 0) {
    ip = "undefined";
  } else {
    for (struct ifaddrs* ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET)
      {
        const struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
        const char* addr = inet_ntoa(sa->sin_addr);
        if (strcmp (ifa->ifa_name, "tofu1") == 0)
        {
          ip = addr;
          break;
        }
      }
    }
    freeifaddrs(ifap);
  }
  
  std::cout << "[Writing host info into file: " << filename << "]" << std::endl;
  std::ofstream fs( filename );
  fs << "hostname" << delim << "ip" << delim << "coord" << delim << "dims" << delim << "shape" << std::endl;
  fs << hostname << delim;
  fs << ip << delim;
  fs << "(" << coords[0]; 
  for (int i=1; i<ndim; i++) fs << "," << coords[i];
  fs << ")" << delim;
  fs << ndim << delim;
  fs << "(" << shape[0] << "," << shape[1] << "," << shape[2] << ")" << std::endl;
  fs.close();
} 

void* job_socket;

std::vector<INDEX_MAP_T> local_index_map;
std::vector<INDEX_MAP_T> local_index_map_hidden;

FtiController io;
int fti_protect_id;

MPI_Fint melissa_comm_init_f(const MPI_Fint *old_comm_fortran)
{
		signal(SIGSEGV, melissa_da_signal_handler);
    // preceed all std::cout calls with a timestamp
    static AddTimeStamp ats( std::cout );
    if (is_p2p()) {
        
        char* envstr = getenv("MELISSA_DA_RUNNER_ID");
        assert( envstr != NULL );
        std::stringstream ss(envstr);
        std::vector<int> runner_ids;
        while( ss.good() ) {
          std::string substr;
          std::getline(ss, substr, ',');
          assert( substr.find_first_not_of("0123456789") == std::string::npos && "runner id is not an integer!" );
          runner_ids.push_back( std::stoi( substr ) );
        }

        MPI_Comm comm_split;
        MPI_Comm comm_universe = MPI_Comm_f2c(*old_comm_fortran);
        int runner_group_size = runner_ids.size();
        int runner_group = 0;
        if( runner_group_size > 1 ) { 
          int comm_universe_size; MPI_Comm_size(comm_universe, &comm_universe_size);
          int comm_universe_rank; MPI_Comm_rank(comm_universe, &comm_universe_rank);
          assert( comm_universe_size%runner_group_size == 0 && "comm-size not multiple of runner group-size!" );
          int comm_split_size = comm_universe_size / runner_group_size;
          runner_group = comm_universe_rank / comm_split_size;
          MPI_Comm_split( comm_universe, runner_group, comm_universe_rank, &comm_split );
        } else {
          comm_split = comm_universe;
        }
        runner_id = runner_ids[runner_group];
				
        printf("Hello! I'm runner %d\n", runner_id);
        
        char c_cwd[PATH_MAX];
   			if (getcwd(c_cwd, sizeof(c_cwd)) != NULL) {
   			    printf("Current working dir: %s\n", c_cwd);
   			} else {
   			    perror("getcwd() error");
   			    return 1;
   			}

        MDBG( "my current working directory is '%s'", c_cwd );

        std::stringstream runner_dir;
        runner_dir << c_cwd << "/runner-"  << std::setw(3) << std::setfill('0') << runner_id;
   			
        MDBG( "now changing into directory '%s'", runner_dir.str().c_str() );
        
        if (chdir( runner_dir.str().c_str() ) < 0) {
   			    perror("chdir() error");
   			    return 1;
   			}
        
   			if (getcwd(c_cwd, sizeof(c_cwd)) != NULL) {
   			    printf("Current working dir: %s\n", c_cwd);
   			} else {
   			    perror("getcwd() error");
   			    return 1;
   			}

        MDBG( "my current working directory is '%s'", c_cwd );


        // TODO: only do this if is_p2p() !
        mpi.init( comm_split );
        storage.io_init( &io, runner_id );
        comm = mpi.comm();  // TODO: use mpi controller everywhere in api

        // To do good logging
        comm_rank = mpi.rank();
#ifdef REPORT_TIMING

#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
        try_init_timing();
        M_TRIGGER(START_INIT, 0);
    }
#endif

        return MPI_Comm_c2f(comm);
    } else {
        return *old_comm_fortran;
    }
}

void melissa_p2p_init(const char *field_name,
                  const size_t local_vect_size,
                  const size_t local_hidden_vect_size,
                  const int bytes_per_element,
                  const int bytes_per_element_hidden,
                  const INDEX_MAP_T local_index_map_[],
                  const INDEX_MAP_T local_index_map_hidden_[]
                  )
{

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    field.current_state_id = runner_id;  // Start like this!
    if (local_index_map_ != nullptr) {
    // store local_index_map to reuse in weight calculation
        local_index_map.resize(local_vect_size);
        local_index_map_hidden.resize(local_hidden_vect_size);
        std::copy(local_index_map_, local_index_map_ + local_vect_size,  local_index_map.data());
    }
    if (local_index_map_hidden_ != nullptr) {
        std::copy(local_index_map_hidden_, local_index_map_hidden_ + local_hidden_vect_size,
                local_index_map_hidden.data());
    }

    // there is no real PHASE_INIT in p2p since no configuration messages need to be
    // exchanged at the beginning
    phase = PHASE_SIMULATION;

    // TODO move storage.init in constructor and use smartpointer instead
    assert(local_hidden_vect_size == 0 && "Melissa-P2P does not yet work if there is a hidden state");
    storage.init( 50L*1024L*1024L*1024L, local_vect_size);  // 50GB storage for now

    // open sockets to server on rank 0:
    if (mpi.rank() == 0)
    {
        char * melissa_server_master_node = getenv(
            "MELISSA_SERVER_MASTER_NODE");

        if (! melissa_server_master_node) {
            MPRT(
                "you must set the MELISSA_SERVER_MASTER_NODE environment variable before running!");
            assert(false);
        }

        // Refactor: put all sockets in a class
        job_socket = zmq_socket(context, ZMQ_REQ);
        std::string port_name = fix_port_name(melissa_server_master_node);
        MDBG("connect to job request server at %s", port_name.c_str());
        int req = zmq_connect(job_socket, port_name.c_str());
        assert(req == 0);
    }
}


double calculate_weight(VEC_T *values, VEC_T *hidden_values)
{
#define USE_PYTHON_CALCULATE_WEIGHT
#ifdef USE_PYTHON_CALCULATE_WEIGHT
    // Warning returns correct weight only on rank 0! other ranks return -1.0
    // use python interface, export index map
    static std::unique_ptr<PythonInterface> pi;  // gets destroyed when static scope is left!
    static bool is_first_time = true;
    if (is_first_time) {
        pi = std::unique_ptr<PythonInterface>(new PythonInterface());
        is_first_time = false;
    }
    
    return pi->calculate_weight(field.current_step, field.current_state_id, values,
            hidden_values);
#else

    return 0.44;
#endif
}



void push_weight_to_head(double weight)
{
    static mpi_request_t req;
    static std::vector<char> buf = {0};  // Isend wants us to not change this if still waiting!

    assert(comm_rank == 0);

    static bool wait = false;
    if( wait ) {
        MDBG("start waiting for Head rank!");
        fflush(stdout);
        req.wait();  // be sure that there is nothing else in the mpi send queue
        MDBG("finished waiting for Head rank!");
        fflush(stdout);
        //if( io.m_dict_bool["master_local"] ) req.wait();  // be sure that there is nothing else in the mpi send queue
        //int dummy; io.recv( &dummy, sizeof(int), IO_TAG_POST, IO_MSG_MST );
    }

    // Now we can change buf!
    ::melissa_p2p::Message m;
    m.set_runner_id(runner_id);
    m.mutable_weight()->mutable_state_id()->set_t(field.current_step);
    m.mutable_weight()->mutable_state_id()->set_id(field.current_state_id);
    m.mutable_weight()->set_weight(weight);

    MDBG("start Pushing weight message(size = %d) to fti head: %s", m.ByteSize(), m.DebugString().c_str());
    fflush(stdout);
    size_t bs = m.ByteSize();  // TODO: change bytesize to bytesize long

    if (bs > buf.size())
    {
        buf.resize(bs);
    }

    m.SerializeToArray(buf.data(), bs);
    io.isend( buf.data(), m.ByteSize(), IO_TAG_POST, IO_MSG_MST, req );
    //io.send( buf.data(), m.ByteSize(), IO_TAG_POST, IO_MSG_MST);  // even faster than isend on juwels!
    req.wait();  // be synchronous on juwels with wrf for now
    MDBG("finished Pushing weight message(size = %d) to fti head: %s", m.ByteSize(), m.DebugString().c_str());
    fflush(stdout);
    wait = true;
}

::melissa_p2p::JobResponse request_work_from_server() {
    ::melissa_p2p::Message m;
    m.set_runner_id(runner_id);
    m.mutable_job_request();  // create job request

    send_message(job_socket, m);


    do
    {
#ifdef REPORT_TIMING
#ifndef REPORT_TIMING_ALL_RANKS
                    if (comm_rank == 0)
#endif
                    {
                    timing->maybe_report();
                    }
#endif
    } while (!has_msg(job_socket, 10));
    auto r = receive_message(job_socket);

    return r.job_response();
}

int melissa_p2p_expose(const char* field_name, VEC_T *values, int64_t size, io_type_t io_type,
                   VEC_T *hidden_values, int64_t size_hidden, io_type_t io_type_hidden, MELISSA_EXPOSE_MODE mode)
{
    static bool is_first = true;
    
    // Update pointer
    storage.protect( std::string(field_name), values, size, io_type );
    MDBG("I am good");
    // return immediately if just field to expose
    if( mode == MELISSA_MODE_EXPOSE ) return 0;
    MDBG("I am good");
    
    assert(  calculateWeight != NULL && "no function registered for weight calculation (call 'melissa_register_weight_function' after melissa_init)" );
    
    if ( is_first ) {
        M_TRIGGER(STOP_INIT, 0);
        is_first = false;
    }
    MDBG("I am good");

#ifdef REPORT_TIMING
#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
    timing->maybe_report();
    }
#endif
    MDBG("I am good");

    // store to ram disk
    // TODO: call protect direkt in chunk based api

    if (field.current_step == 0) {
        // if checkpointing initial state, use runner_id as state id
        field.current_state_id = runner_id; // We are beginning like this...
    }
    MDBG("I am good");

    io_state_id_t current_state = { field.current_step, field.current_state_id };
    MDBG("I am good");
    M_TRIGGER(START_STORE, to_ckpt_id(current_state));
    MDBG("start storing state as L1 checkpoint");
    storage.store( current_state );
    MDBG("finished storing state as L1 checkpoint");
    fflush(stdout);
    M_TRIGGER(STOP_STORE, to_ckpt_id(current_state));

    // 2. calculate weight and synchronize weight on rank 0
    M_TRIGGER(START_CALC_WEIGHT, current_state.t);
    MDBG("start calculating weight for state");
    fflush(stdout);
    int __comm_size;MPI_Comm_size(comm, &__comm_size);
    MDBG("mpi.size(): %d, comm size: %d", mpi.size(), __comm_size);
    fflush(stdout);
    double weight = calculateWeight();
    MDBG("finished calculating weight for state");
    fflush(stdout);
    M_TRIGGER(STOP_CALC_WEIGHT, current_state.id);

    int nsteps = 0;

    io_state_id_t parent_state, next_state;

    // Rank 0:
    uint64_t loop_counter = 0;
    if (mpi.rank() == 0) {
        // 3. push weight to server (via the head who forwards as soon nas the state is checkpointed to the pfs)
        M_TRIGGER(START_PUSH_WEIGHT_TO_HEAD, current_state.t);
        MDBG("start pushing weight to head");
        fflush(stdout);
        push_weight_to_head(weight);
        MDBG("finished pushing weight to head");
        fflush(stdout);
        M_TRIGGER(STOP_PUSH_WEIGHT_TO_HEAD, current_state.id);

        M_TRIGGER(START_JOB_REQUEST, current_state.t);
        // 4. ask server for more work
        ::melissa_p2p::JobResponse job_response;
        bool entered_loop = false;
        do {
            job_response = request_work_from_server();
            if (!job_response.has_parent()) {
                if (!entered_loop)
                {
                    entered_loop = true;
                    MDBG("Server does not have any good jobs for me. Retrying in 500 ms intervals");

                }
                usleep(500000); // retry after 500ms
                loop_counter++;
            }
        } while (!job_response.has_parent());
        MDBG("Now  I work on %s", job_response.DebugString().c_str());
        fflush(stdout);
        parent_state.t = job_response.parent().t();
        parent_state.id = job_response.parent().id();
        next_state.t = job_response.job().t();
        next_state.id = job_response.job().id();
        nsteps = job_response.nsteps();
    }
    else
    {
        M_TRIGGER(START_JOB_REQUEST, current_state.t);
    }

    // Broadcast to all ranks:
    MPI_Bcast(&parent_state.t, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&parent_state.id, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&next_state.t, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&next_state.id, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&nsteps, 1, MPI_INT, 0, mpi.comm());
    M_TRIGGER(STOP_JOB_REQUEST, loop_counter);


    if (nsteps > 0) {
        M_TRIGGER(START_LOAD, to_ckpt_id(parent_state));
        // called by every app core
        if ( current_state == parent_state ) {
            printf("Not performing a state load as good state already in memory");
            M_TRIGGER(LOCAL_HIT, parent_state.id);
        } else {
            storage.load( parent_state );
        }


        field.current_step = next_state.t;
        field.current_state_id = next_state.id;

        M_TRIGGER(STOP_LOAD, to_ckpt_id(parent_state));
    }
    else
    {
        zmq_close(job_socket);

        storage.fini();

        field.current_step = -1;
        field.current_state_id = -1;
    }

    // Do some fancy logging:

    if (comm_rank == 0) {
        long pages = sysconf(_SC_PHYS_PAGES);
        long av_pages = sysconf(_SC_AVPHYS_PAGES);
        long page_size = sysconf(_SC_PAGE_SIZE);
        long long total = pages * page_size;
        long long avail = av_pages * page_size;
        printf("Total mem available: %llu / %llu MiB\n", avail/1024/1024, total/1024/1024);
        fflush(stdout);
    }

    return nsteps;
}

void ApiTiming::maybe_report() {
    /// should be called once in a while to check if it is time to write the timing info now!
    //if (runner_id != 0) {
        //return;
    //}
    if (is_time_to_write()) {
#ifdef REPORT_TIMING  // TODO: clean up defines with all ranks too!
        if(comm_rank == 0 )
        {
            if (!FTI_AmIaHead()) {
            report(
                    getCommSize(), field.local_vect_size + field.local_hidden_vect_size,
                    runner_id, false);
            }

            char fname[256];
            if (FTI_AmIaHead()) {
                sprintf(fname, "runner-%03d-head", runner_id);
            } else {
                sprintf(fname, "runner-%03d-app", runner_id);
            }
            print_events(fname, comm_rank);
            std::stringstream hostinfo_file;
            hostinfo_file << "runner-" << std::setfill('0') << std::setw(5) << runner_id << ".hostinfo.csv";
            print_local_hostname( mpi.comm(), hostinfo_file.str() ); 

            const std::array<EventTypeTranslation, 32> event_type_translations = {{
                {START_ITERATION, STOP_ITERATION, "Iteration"},
                    {START_PROPAGATE_STATE, STOP_PROPAGATE_STATE, "Propagation"},
                    {START_IDLE_RUNNER, STOP_IDLE_RUNNER, "Runner idle"},
                    {START_INIT, STOP_INIT, "_INIT"},
                    {START_JOB_REQUEST, STOP_JOB_REQUEST, "_JOB_REQUEST"},
                    {START_LOAD, STOP_LOAD, "_LOAD"},
                    {START_FTI_LOAD, STOP_FTI_LOAD, "_FTI_LOAD"},
                    {START_MODEL_MESSAGE, STOP_MODEL_MESSAGE, "_MODEL_MESSAGE"},
                    {START_M_LOAD_USER, STOP_M_LOAD_USER, "_M_LOAD_USER"},
                    {START_CHECK_LOCAL, STOP_CHECK_LOCAL, "_CHECK_LOCAL"},
                    {START_WAIT_HEAD, STOP_WAIT_HEAD, "_WAIT_HEAD"},
                    {START_CALC_WEIGHT, STOP_CALC_WEIGHT, "_CALC_WEIGHT"},
                    {START_LOAD_OBS, STOP_LOAD_OBS, "_LOAD_OBS"},
                    {START_PUSH_WEIGHT_TO_HEAD, STOP_PUSH_WEIGHT_TO_HEAD, "_PUSH_WEIGHT_TO_HEAD"},
                    {START_STORE, STOP_STORE, "_STORE"},
                    {START_IDLE_FTI_HEAD, STOP_IDLE_FTI_HEAD, "_IDLE_FTI_HEAD"},
                    {START_PUSH_STATE_TO_PFS, STOP_PUSH_STATE_TO_PFS, "_PUSH_STATE_TO_PFS"},
                    {START_PREFETCH, STOP_PREFETCH, "_PREFETCH"},
                    {START_PREFETCH_REQ, STOP_PREFETCH_REQ, "_PREFETCH_REQ"},
                    {START_REQ_RUNNER, STOP_REQ_RUNNER, "_REQ_RUNNER"},
                    {START_COPY_STATE_FROM_RUNNER, STOP_COPY_STATE_FROM_RUNNER, "_COPY_STATE_FROM_RUNNER"},
                    {START_COPY_STATE_TO_RUNNER, STOP_COPY_STATE_TO_RUNNER, "_COPY_STATE_TO_RUNNER"},
                    {START_COPY_STATE_FROM_PFS, STOP_COPY_STATE_FROM_PFS, "_COPY_STATE_FROM_PFS"},
                    {START_DELETE, STOP_DELETE, "_DELETE"},
                    {START_DELETE_REQ, STOP_DELETE_REQ, "_DELETE_REQ"},
                    {START_DELETE_LOCAL, STOP_DELETE_LOCAL, "_DELETE_LOCAL"},
                    {START_DELETE_PFS, STOP_DELETE_PFS, "_DELETE_PFS"},
                    {START_REQ_RUNNER_LIST, STOP_REQ_RUNNER_LIST, "_REQ_RUNNER_LIST"},
                    {START_REQ_STATE_FROM_RUNNER, STOP_REQ_STATE_FROM_RUNNER, "_REQ_STATE_FROM_RUNNER"},
                    {START_HANDLE_AVAIL_REQ, STOP_HANDLE_AVAIL_REQ, "_HANDLE_AVAIL_REQ"},
                    {START_HANDLE_STATE_REQ, STOP_HANDLE_STATE_REQ, "_HANDLE_STATE_REQ"},
                    {START_PUSH_WEIGHT_TO_SERVER, STOP_PUSH_WEIGHT_TO_SERVER, "_PUSH_WEIGHT_TO_SERVER"}
            }};

            bool close_different_parameter = is_p2p(); // In p2p api we allow to close regions even with different parameters in start and stop event
            write_region_csv(event_type_translations, fname, comm_rank, close_different_parameter);
        }
#ifdef REPORT_TIMING_ALL_RANKS
#endif
#endif

    }
}





