#ifndef __FTMODULE__
#define __FTMODULE__

#define FTI_CONFIG "config.fti"

#include <fti.h>
#include <hdf5.h>
#include "Field.h"
#include <map>
#include <memory>
#ifdef WITH_FTI_THREADS
#include "FTthreadManager.h"
#endif
#include "MpiManager.h"

enum FTtype { FT_BACKGROUND=0, FT_HIDDEN };

class FTmodule {

    public:

        void init( MpiManager & mpi, int & epoch_counter );
        void protect_states( MpiManager & mpi, std::unique_ptr<Field> & field );
        void store_subset( std::unique_ptr<Field> & field, int dataset_id, int runner_rank, FTtype type );
        void initCP( int epoch ); 
        void finalizeCP( void ); 
        void flushCP( void ); 
        void recover( void ); 
        void finalize( void ); 

    private:
        
        void protect_background( MpiManager & mpi, std::unique_ptr<Field> & field );
        void protect_hidden( MpiManager & mpi, std::unique_ptr<Field> & field );
        MPI_Comm m_fti_comm;
        MPI_Comm m_fti_comm_dup;
#ifdef WITH_FTI_THREADS
        FTthreadManager FTsched;
#endif
        bool m_checkpointing;
        std::map<std::string,int> id_map;
        bool m_restart;
        int m_id_var;
        int m_id_dataset;
        bool m_protected;
        bool m_has_hidden;
        std::set<std::string> id_check;
        const std::string m_FTtype[2] = { "background", "hidden" }; 
};

#endif // __FTMODULE__
