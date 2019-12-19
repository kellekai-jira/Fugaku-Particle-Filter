#ifndef __MPIMANAGER__
#define __MPIMANAGER__

#include <mpi.h>

class MpiManager 
{
    
    public:
        
        MpiManager() : m_valid(false) {}
         
        void init();
        void update_comm( MPI_Comm & comm );
        const MPI_Comm & comm();
        const int & size();
        const int & rank();
        void finalize();
            
    private:
        
        MPI_Comm m_comm;
        int m_size;
        int m_rank;

        bool m_valid;
};

#endif // __MPIMANAGER__
