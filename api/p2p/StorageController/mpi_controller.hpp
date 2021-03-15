#ifndef __MPIMANAGER__
#define __MPIMANAGER__

#include <mpi.h>
#include <string>
#include <map>

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
        void init();
        void register_comm( std::string, MPI_Comm & );
        void set_comm( std::string );
        const MPI_Comm & comm();
        const int & size();
        const int & rank();
        void barrier(std::string comm);
        void finalize();

        MPI_Fint fortranComm();

    private:

        std::string m_comm_key;
        std::map<std::string,MPI_Comm> m_comms;
        int m_size;
        int m_rank;

};

#endif // __MPIMANAGER__
