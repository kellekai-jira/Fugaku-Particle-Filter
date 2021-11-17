#ifndef __MPIMANAGER__
#define __MPIMANAGER__

#include <mpi.h>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <type_traits>
#include "utils.h"
#include "io_controller_defs.hpp"

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
        void broadcast( std::vector<io_state_id_t> & buffer, std::string key = m_comm_set, int root = 0 );
        void broadcast( std::string & buffer, std::string key = m_comm_set, int root = 0 );
        void broadcast( std::vector<char> & buffer, std::string key = m_comm_set, int root = 0 );
        template<class T>  
				void broadcast( T & value, std::string key = m_comm_set, int root = 0 );
        void finalize();

        MPI_Fint fortranComm();

    private:
        static std::string m_comm_set;
        static int bcast_counter_val;
        static int bcast_counter_int;
        static int bcast_counter_char;
        struct mpi_comm_t {
          MPI_Comm comm;
          int size;
          int rank;
        };
        std::map<std::string,mpi_comm_t> m_comms;

};

template<class T>
void MpiController::broadcast( T & value, std::string key, int root ) {
  int bcast_counter_val_root;
  if ( m_comms[key].rank == root ) {
    bcast_counter_val_root = bcast_counter_val;
  }
  MPI_Bcast( &bcast_counter_val_root, 1, MPI_INT, root, m_comms[key].comm );
  int elem_size = sizeof(T);
  MPI_Bcast( &value, elem_size, MPI_BYTE, root, m_comms[key].comm );
  MDBG("MpiController::broadcast(value) (root count: %d, my count: %d)", bcast_counter_val_root, bcast_counter_val);
  bcast_counter_val++;
}

#endif // __MPIMANAGER__
