#include "MpiManager.h"
#include <cassert>
#include "utils.h"

MpiManager::MpiManager() :
    m_comm_key("mpi_comm_world")
{
    m_comms[m_comm_key] = MPI_COMM_NULL;
}

void MpiManager::init()
{
#if defined(WITH_FTI) && defined(WITH_FTI_THREADS)
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

void MpiManager::register_comm( std::string key, MPI_Comm & comm )
{
    m_comms[key] = std::move(comm);
}

void MpiManager::set_comm( std::string key )
{
    assert( m_comms.count(key) > 0 && "invalid key for MPI communicator!");

    m_comm_key = key;

    MPI_Comm_size( m_comms[m_comm_key], &m_size );
    MPI_Comm_rank( m_comms[m_comm_key], &m_rank );
}

const MPI_Comm & MpiManager::comm()
{
    return m_comms[m_comm_key];
}

const int & MpiManager::size()
{
    return m_size;
}

const int & MpiManager::rank()
{
    return m_rank;
}

void MpiManager::finalize()
{
    MPI_Finalize();
}


MPI_Fint MpiManager::fortranComm()
{
    return MPI_Comm_c2f(comm());
}
