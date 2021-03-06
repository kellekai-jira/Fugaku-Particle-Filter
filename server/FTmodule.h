#ifndef __FTMODULE__
#define __FTMODULE__

#define FTI_CONFIG "config.fti"

#include <fti.h>
#include "Field.h"
#include <map>
#include <memory>
#if WITH_FTI_THREADS
#include "FTthreadManager.h"
#endif
#include "MpiManager.h"

class FTmodule {

    public:

        void init( MpiManager & mpi, int & epoch_counter );
        void protect_background( MpiManager & mpi, std::unique_ptr<Field> & field );
        void store_subset( std::unique_ptr<Field> & field, int dataset_id, int runner_rank );
        void initCP( int epoch ); 
        void finalizeCP( void ); 
        void flushCP( void ); 
        void recover( void ); 
        void finalize( void ); 

    private:
        
        MPI_Comm m_fti_comm;
        MPI_Comm m_fti_comm_dup;
#if WITH_FTI_THREADS
        FTthreadManager FTsched;
#endif
        bool m_checkpointing;
        std::map<std::string,int> id_map;
        bool m_restart;
        int m_id_offset;
        bool m_protected;
        std::set<std::string> id_check;
};

#endif // __FTMODULE__
