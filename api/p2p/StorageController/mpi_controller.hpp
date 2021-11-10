#ifndef __MPIMANAGER__
#define __MPIMANAGER__

#include <mpi.h>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <type_traits>
#include "utils.h"

struct mpi_comm_t {
  MPI_Comm comm;
  int size;
  int rank;
};

extern std::string m_comm_set;
extern std::map<std::string,mpi_comm_t> m_comms;

struct mpi_request_t {
    MPI_Request mpi_request;
    char errstr[MPI_MAX_ERROR_STRING];
    int errval;
    bool test();
    void wait();
    void free();
    mpi_request_t() : mpi_request(MPI_REQUEST_NULL) {}
  private:
    MPI_Status mpi_status{0};
};


class MpiController
{

    public:

        MpiController();
        void init( MPI_Comm & comm );
        void register_comm( std::string, MPI_Comm & );
        void set_comm( std::string key );
        const MPI_Comm & comm( std::string key = m_comm_set );
        const int & size( std::string key = m_comm_set );
        const int & rank( std::string key = m_comm_set );
        void barrier( std::string key = m_comm_set );
        template<class T>
        void broadcast( std::vector<T> & buffer, std::string key = m_comm_set, int root = 0 );
        template<class T>
        void broadcast( T & value, std::string key = m_comm_set, int root = 0 );
        void finalize();

        MPI_Fint fortranComm();

    private:

};
        
template<class T>
void MpiController::broadcast( std::vector<T> & buffer, std::string key, int root ) {
  int byte_count; 
  int elem_size = sizeof(T);
  int rank = m_comms[key].rank;
  MPI_Comm comm = m_comms[key].comm;
  if ( rank == root ) {
    byte_count = buffer.size() * elem_size;
  }
  MPI_Bcast( &byte_count, 1, MPI_INT, root, comm );
  if ( rank != root ) { 
    assert( ((byte_count % elem_size) == 0) && "the unthinkable happened!" );
    int elem_count = byte_count / elem_size;
    buffer.resize( elem_count );
  }
  MPI_Bcast( buffer.data(), byte_count, MPI_BYTE, root, comm );
}

template<class T>
void MpiController::broadcast( T & value, std::string key, int root ) {
  static_assert(std::is_integral<T>::value, "Integral required.");
  int elem_size = sizeof(T);
  int rank = m_comms[key].rank;
  MPI_Comm comm = m_comms[key].comm;
  MPI_Bcast( &value, elem_size, MPI_BYTE, root, comm );
}

#endif // __MPIMANAGER__
