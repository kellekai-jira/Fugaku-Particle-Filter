#ifndef __MPIMANAGER__
#define __MPIMANAGER__

#include <mpi.h>
#include <string>
#include <map>

#define COMM_WORLD_KEY "comm_world"

#define MPI_COMM_DUP_AND_INSERT(NAME, COMM) \
  do { \
    MPI_Comm dup;   MPI_Comm_dup( COMM, &dup ); \
    int rank;       MPI_Comm_rank( dup, &rank ); \
    int size;       MPI_Comm_size( dup, &size ); \
    m_comms.insert( std::pair<std::string, mpi_comm_t>( NAME, { dup, size, rank } ) ); \
  } while(0)

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
        void register_comm( std::string, MPI_Comm );
        void set_comm( std::string key );
        const MPI_Comm & comm( std::string key = m_comm_set );
        const int & size( std::string key = m_comm_set );
        const int & rank( std::string key = m_comm_set );
        void barrier( std::string key = m_comm_set );
        void finalize();

        MPI_Fint fortranComm();

    private:
        static std::string m_comm_set;
        struct mpi_comm_t {
          MPI_Comm comm;
          int size;
          int rank;
        };
        std::map<std::string,mpi_comm_t> m_comms;

};

#endif // __MPIMANAGER__
