#include "mpi_controller.hpp"
#include <cassert>
#include "utils.h"
    
std::string MpiController::m_comm_set = "global_comm";

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
  if( mpi_request == MPI_REQUEST_NULL ) return;
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

MpiController::MpiController()
{
    m_comms.insert( std::pair<std::string, mpi_comm_t>( "global_comm", { MPI_COMM_NULL, -1, -1 } ) );
}

void MpiController::init( MPI_Comm & comm )
{
  // TODO init mit global comm
    m_comms[m_comm_set].comm = comm;
    MPI_Comm_size( m_comms[m_comm_set].comm, &m_comms[m_comm_set].size );
    MPI_Comm_rank( m_comms[m_comm_set].comm, &m_comms[m_comm_set].rank );
}

void MpiController::register_comm( std::string key, MPI_Comm & comm )
{
    int size; MPI_Comm_size( comm, &size );
    int rank; MPI_Comm_rank( comm, &rank );
    mpi_comm_t mpi_comm = { comm, size, rank }; 
    m_comms.insert( std::pair<std::string, mpi_comm_t>( key, mpi_comm ) );
}

void MpiController::set_comm( std::string key )
{
    assert( m_comms.count(key) > 0 && "invalid key for MPI communicator!");

    m_comm_set = key;
}

const MPI_Comm & MpiController::comm( std::string key )
{
    return m_comms[key].comm;
}

const int & MpiController::size( std::string key )
{
    return m_comms[key].size;
}

const int & MpiController::rank( std::string key )
{
    return m_comms[key].rank;
}

void MpiController::barrier( std::string key ) {
  MPI_Barrier(m_comms[key].comm);
}

void MpiController::finalize()
{
    MPI_Finalize();
}


MPI_Fint MpiController::fortranComm()
{
    return MPI_Comm_c2f(comm());
}
