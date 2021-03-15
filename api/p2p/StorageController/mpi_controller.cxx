#include "mpi_controller.hpp"
#include <cassert>
#include "utils.h"

bool mpi_request_t::test() {
  int flag = 0;
  if( mpi_request != MPI_REQUEST_NULL ) MPI_Test( &mpi_request, &flag, &mpi_status );
  if( flag == 1 ) {
    errval = mpi_status.MPI_ERROR;
    if( errval != MPI_SUCCESS ) {
      int len;
      MPI_Error_string(mpi_status.MPI_ERROR, errstr, &len);
      std::cerr << "[IO ERROR]: " << errstr << "STAT: " << mpi_status.MPI_ERROR << std::endl;
    }
  }
  return (flag == 1);
}

void mpi_request_t::wait() {
  int flag;
  MPI_Wait( &mpi_request, &mpi_status );
  errval = mpi_status.MPI_ERROR;
  if( errval != MPI_SUCCESS ) {
    int len;
    MPI_Error_string(mpi_status.MPI_ERROR, errstr, &len);
    std::cerr << "[IO ERROR]: " << errstr << "STAT: " << mpi_status.MPI_ERROR << std::endl;
  }
}

void mpi_request_t::free() {
  MPI_Request_free( &mpi_request );
}

MpiController::MpiController() :
    m_comm_key("mpi_comm_world")
{
    m_comms[m_comm_key] = MPI_COMM_NULL;
}

void MpiController::init()
{
#if defined WITH_FTI && FTI_THREADS
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    if( provided < MPI_THREAD_MULTIPLE ) {
        D( "thread level is not provided!" );
        MPI_Abort( MPI_COMM_WORLD, -1 );
    }
#else
    MPI_Init(NULL, NULL);
#endif
    m_comms[m_comm_key] = MPI_COMM_WORLD;
    MPI_Comm_size( m_comms[m_comm_key], &m_size );
    MPI_Comm_rank( m_comms[m_comm_key], &m_rank );
}

void MpiController::register_comm( std::string key, MPI_Comm & comm )
{
    m_comms[key] = std::move(comm);
}

void MpiController::set_comm( std::string key )
{
    assert( m_comms.count(key) > 0 && "invalid key for MPI communicator!");

    m_comm_key = key;

    MPI_Comm_size( m_comms[m_comm_key], &m_size );
    MPI_Comm_rank( m_comms[m_comm_key], &m_rank );
}

const MPI_Comm & MpiController::comm()
{
    return m_comms[m_comm_key];
}

const int & MpiController::size()
{
    return m_size;
}

const int & MpiController::rank()
{
    return m_rank;
}

void MpiController::barrier( std::string comm ) {
  MPI_Barrier(m_comms[m_comm_key]);
}

void MpiController::finalize()
{
    MPI_Finalize();
}


MPI_Fint MpiController::fortranComm()
{
    return MPI_Comm_c2f(comm());
}
