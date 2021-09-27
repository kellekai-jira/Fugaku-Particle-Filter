#include "app_core.h"
#include "StorageController/helpers.h"
#include "p2p.pb.h"

#include "api_common.h"
#include "StorageController/fti_controller.hpp"
#include "StorageController/mpi_controller.hpp"
#include "StorageController/storage_controller_impl.hpp"

#include "StorageController/helpers.h"

#include <unistd.h>

#include <cstdlib>
#include <memory>

#include "PythonInterface.h"

void* job_socket;

std::vector<INDEX_MAP_T> local_index_map;
std::vector<INDEX_MAP_T> local_index_map_hidden;

FtiController io;
MpiController mpi;
int fti_protect_id;

MPI_Fint melissa_comm_init_f(const MPI_Fint *old_comm_fortran)
{
    if (is_p2p()) {
        runner_id = atoi(getenv("MELISSA_DA_RUNNER_ID"));
        printf("Hello! I'm runner %d\n", runner_id);

        // TODO: only do this if is_p2p() !
        MPI_Comm comm_c = MPI_Comm_f2c(*old_comm_fortran);
        mpi.init( comm_c );
        storage.io_init( &mpi, &io );
        comm = mpi.comm();  // TODO: use mpi controller everywhere in api

        // To do good logging
        comm_rank = mpi.rank();
#ifdef REPORT_TIMING

#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
        try_init_timing();
        trigger(START_INIT, 0);
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
    fti_protect_id = storage.protect( NULL, local_vect_size, IO_BYTE );

    // open sockets to server on rank 0:
    if (mpi.rank() == 0)
    {
        char * melissa_server_master_node = getenv(
            "MELISSA_SERVER_MASTER_NODE");

        if (! melissa_server_master_node) {
            L(
                "you must set the MELISSA_SERVER_MASTER_NODE environment variable before running!");
            assert(false);
        }

        // Refactor: put all sockets in a class
        job_socket = zmq_socket(context, ZMQ_REQ);
        std::string port_name = fix_port_name(melissa_server_master_node);
        D("connect to job request server at %s", port_name.c_str());
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
        D("Waiting for Head rank!");
        req.wait();  // be sure that there is nothing else in the mpi send queue
        //if( io.m_dict_bool["master_local"] ) req.wait();  // be sure that there is nothing else in the mpi send queue
        //int dummy; io.recv( &dummy, sizeof(int), IO_TAG_POST, IO_MSG_ONE );
    }

    // Now we can change buf!
    ::melissa_p2p::Message m;
    m.set_runner_id(runner_id);
    m.mutable_weight()->mutable_state_id()->set_t(field.current_step);
    m.mutable_weight()->mutable_state_id()->set_id(field.current_state_id);
    m.mutable_weight()->set_weight(weight);

    D("Pushing weight message(size = %d) to fti head: %s", m.ByteSize(), m.DebugString().c_str());
    size_t bs = m.ByteSize();  // TODO: change bytesize to bytesize long

    if (bs > buf.size())
    {
        buf.resize(bs);
    }

    m.SerializeToArray(buf.data(), bs);
    io.isend( buf.data(), m.ByteSize(), IO_TAG_POST, IO_MSG_ONE, req );
    //io.send( buf.data(), m.ByteSize(), IO_TAG_POST, IO_MSG_ONE);  // even faster than isend on juwels!
    req.wait();  // be synchronous on juwels with wrf for now
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

int melissa_p2p_expose(VEC_T *values,
                   VEC_T *hidden_values)
{
    static bool is_first = true;
    if (is_first) {
        trigger(STOP_INIT, 0);
        is_first = false;
    }

#ifdef REPORT_TIMING
#ifndef REPORT_TIMING_ALL_RANKS
    if (comm_rank == 0)
#endif
    {
    timing->maybe_report();
    }
#endif

    // Update pointer
    storage.update( fti_protect_id, values, field.local_vect_size );

    // store to ram disk
    // TODO: call protect direkt in chunk based api

    if (field.current_step == 0) {
        // if checkpointing initial state, use runner_id as state id
        field.current_state_id = runner_id; // We are beginning like this...
    }

    io_state_id_t current_state = { field.current_step, field.current_state_id };
    trigger(START_STORE, to_ckpt_id(current_state));
    storage.store( current_state );
    trigger(STOP_STORE, to_ckpt_id(current_state));

    // 2. calculate weight and synchronize weight on rank 0
    trigger(START_CALC_WEIGHT, current_state.t);
    double weight = calculate_weight(values, hidden_values);
    trigger(STOP_CALC_WEIGHT, current_state.id);

    int nsteps = 0;

    io_state_id_t parent_state, next_state;

    // Rank 0:
    uint64_t loop_counter = 0;
    if (mpi.rank() == 0) {
        // 3. push weight to server (via the head who forwards as soon nas the state is checkpointed to the pfs)
        trigger(START_PUSH_WEIGHT_TO_HEAD, current_state.t);
        push_weight_to_head(weight);
        trigger(STOP_PUSH_WEIGHT_TO_HEAD, current_state.id);

        trigger(START_JOB_REQUEST, current_state.t);
        // 4. ask server for more work
        ::melissa_p2p::JobResponse job_response;
        bool entered_loop = false;
        do {
            job_response = request_work_from_server();
            if (!job_response.has_parent()) {
                if (!entered_loop)
                {
                    entered_loop = true;
                    D("Server does not have any good jobs for me. Retrying in 500 ms intervals");

                }
                usleep(500000); // retry after 500ms
                loop_counter++;
            }
        } while (!job_response.has_parent());
        D("Now  I work on %s", job_response.DebugString().c_str());
        parent_state.t = job_response.parent().t();
        parent_state.id = job_response.parent().id();
        next_state.t = job_response.job().t();
        next_state.id = job_response.job().id();
        nsteps = job_response.nsteps();
    }
    else
    {
        trigger(START_JOB_REQUEST, current_state.t);
    }

    // Broadcast to all ranks:
    MPI_Bcast(&parent_state.t, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&parent_state.id, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&next_state.t, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&next_state.id, 1, MPI_INT, 0, mpi.comm());
    MPI_Bcast(&nsteps, 1, MPI_INT, 0, mpi.comm());
    trigger(STOP_JOB_REQUEST, loop_counter);


    if (nsteps > 0) {
        trigger(START_LOAD, to_ckpt_id(parent_state));
        // called by every app core
        if ( current_state == parent_state ) {
            printf("Not performing a state load as good state already in memory");
            trigger(LOCAL_HIT, parent_state.id);
        } else {
            storage.load( parent_state );
        }


        field.current_step = next_state.t;
        field.current_state_id = next_state.id;

        trigger(STOP_LOAD, to_ckpt_id(parent_state));
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





