#ifndef __MPIMANAGER__
#define __MPIMANAGER__

#include <mpi.h>
#include <string>
#include <map>

class MpiManager 
{
    
    public:
        
        MpiManager();
        void init();
        void register_comm( std::string, MPI_Comm & );
        void set_comm( std::string );
        const MPI_Comm & comm();
        const int & size();
        const int & rank();
        void finalize();
            
    private:
   
        std::string m_comm_key;
        std::map<std::string,MPI_Comm> m_comms;
        int m_size;
        int m_rank;

};

#endif // __MPIMANAGER__
