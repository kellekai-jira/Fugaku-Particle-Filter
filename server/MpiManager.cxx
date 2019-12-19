#include "MpiManager.h"
#include <cassert>

void MpiManager::init()
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
    m_comm = MPI_COMM_WORLD;
    MPI_Comm_size( m_comm, &m_size );
    MPI_Comm_rank( m_comm, &m_rank );

    m_valid = true;
}


void MpiManager::update_comm( MPI_Comm & comm )
{
    m_comm = comm;
    MPI_Comm_size( m_comm, &m_size );
    MPI_Comm_rank( m_comm, &m_rank );
    m_valid = true;
}

const MPI_Comm & MpiManager::comm()
{
    assert( m_valid && "not a valid MPI environment!" );
    return m_comm;
}

const int & MpiManager::size()
{
    assert( m_valid && "not a valid MPI environment!" );
    return m_size;
}

const int & MpiManager::rank()
{
    assert( m_valid && "not a valid MPI environment!" );
    return m_rank;
}

void MpiManager::finalize()
{
    MPI_Finalize();
}
