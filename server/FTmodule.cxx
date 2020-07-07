#ifdef WITH_FTI

#include "FTmodule.h"
#include <algorithm>
#include "MpiManager.h"
#include "Part.h"

void FTmodule::init( MpiManager & mpi, int & epoch_counter )
{
    m_id_var = 0;
    m_id_dataset = 0;
    m_protected = false;
    m_checkpointing = false;
    m_has_hidden = true;
    id_check.clear();
    FTI_Init( FTI_CONFIG, mpi.comm() );
#ifdef WITH_FTI_THREADS
    MPI_Comm_dup( FTI_COMM_WORLD, &m_fti_comm_dup );
    mpi.register_comm( "fti_comm_dup", m_fti_comm_dup );
    mpi.set_comm( "fti_comm_dup" );
    FTsched.init(1);
#else
    m_fti_comm = FTI_COMM_WORLD;
    mpi.register_comm( "fti_comm", m_fti_comm );
    mpi.set_comm( "fti_comm" );
#endif
    // protect global variables and set id_offset for subset ids
    FTIT_hsize_t dim = 1;
    FTIT_hsize_t offset = 0;
    FTIT_hsize_t count = 1;
    FTI_DefineGlobalDataset( 0, 1, &dim, "epoch_counter", NULL, FTI_INTG );
    FTI_Protect( 0, &epoch_counter, 1, FTI_INTG );
    FTI_AddSubset( 0, 1, &offset, &count, 0 );
    m_id_var++;
    m_id_dataset++;
    m_restart = static_cast<bool>(FTI_Status());
}

void FTmodule::protect_states( MpiManager & mpi, std::unique_ptr<Field> & field )
{
    protect_background( mpi, field );
    protect_hidden( mpi, field );
}

void FTmodule::protect_background( MpiManager & mpi, std::unique_ptr<Field> & field )
{
    // initialize some vars
    int myRank = mpi.rank();
    int comm_size_server = mpi.size();
    size_t local_vect_sizes_server[comm_size_server];
    size_t global_vect_size = field->globalVectSize();
    FTIT_hsize_t state_dim = static_cast<FTIT_hsize_t>(global_vect_size);
    int dataset_id = m_id_dataset; // init with current dataset id-counter
    
    calculate_local_vect_sizes_server(comm_size_server, global_vect_size, local_vect_sizes_server);
    
    // determine offset in file for current dataset
    FTIT_hsize_t offset_base = 0;
    for(int i=0; i<myRank; i++) { offset_base+=local_vect_sizes_server[i]; }
    
    for(auto it_ens=field->ensemble_members.begin(); it_ens!=field->ensemble_members.end(); it_ens++) {
        
        FTIT_hsize_t offset = offset_base;
        
        // dataset name '<fieldname>_background_<stateid>'
        std::string dataset_name(field->name);
        dataset_name += "_background_" + std::to_string( dataset_id-m_id_dataset );
        
        FTI_DefineGlobalDataset( dataset_id, 1, &state_dim, dataset_name.c_str(), NULL, FTI_DBLE );
        
        // iterate over all parts for own server_rank
        std::vector<Part>::iterator it_part = field->parts.begin();
        while( (it_part = std::find_if( it_part, field->parts.end(), [myRank]( Part & part ) {return myRank == part.rank_server;} )) != field->parts.end() ){
            
            // protect part
            void* ptr = it_ens->state_background.data() + it_part->local_offset_server;
            FTI_Protect( m_id_var, ptr, it_part->send_count, FTI_DBLE );
            
            // add part to global dataset
            offset += static_cast<FTIT_hsize_t>(it_part->local_offset_server);
            FTIT_hsize_t count = static_cast<FTIT_hsize_t>(it_part->send_count);
            FTI_AddSubset( m_id_var, 1, &offset, &count, dataset_id );
           
            // store subset id in keymap
            std::string subset_name(dataset_name);
            // subset name '<fieldname>_background_<stateid>_<runner_rank>'
            subset_name += "_" + std::to_string( it_part->rank_runner );
            id_map[subset_name] = m_id_var;
            
            m_id_var++;
            it_part++;
        
        }
        
        dataset_id++;
    
    }
    
    m_id_dataset += dataset_id;
    m_protected = true;
}

void FTmodule::protect_hidden( MpiManager & mpi, std::unique_ptr<Field> & field )
{
    // initialize some vars
    int myRank = mpi.rank();
    int comm_size_server = mpi.size();
    size_t local_vect_sizes_server[comm_size_server];
    size_t global_vect_size = field->globalVectSizeHidden();
    FTIT_hsize_t state_dim = static_cast<FTIT_hsize_t>(global_vect_size);
    int dataset_id = m_id_dataset; // init with current dataset id-counter
    
    calculate_local_vect_sizes_server(comm_size_server, global_vect_size, local_vect_sizes_server);
    
    // hidden states can be smaller than server_comm -> have no hidden state parts
    if( local_vect_sizes_server[myRank] == 0 ) {
        m_has_hidden = false;
        return;
    }
    
    // determine offset in file for current dataset
    FTIT_hsize_t offset_base = 0;
    for(int i=0; i<myRank; i++) { offset_base+=local_vect_sizes_server[i]; }
    
    for(auto it_ens=field->ensemble_members.begin(); it_ens!=field->ensemble_members.end(); it_ens++) {
        
        FTIT_hsize_t offset = offset_base;
        
        // dataset name '<fieldname>_hidden_<stateid>'
        std::string dataset_name(field->name);
        dataset_name += "_hidden_" + std::to_string( dataset_id-m_id_dataset );
        
        FTI_DefineGlobalDataset( dataset_id, 1, &state_dim, dataset_name.c_str(), NULL, FTI_DBLE );
        
        // iterate over all parts for own server_rank
        std::vector<Part>::iterator it_part = field->parts_hidden.begin();
        while( (it_part = std::find_if( it_part, field->parts_hidden.end(), [myRank]( Part & part ) {return myRank == part.rank_server;} )) != field->parts_hidden.end() ){
            
            // protect part
            void* ptr = it_ens->state_background.data() + it_part->local_offset_server;
            FTI_Protect( m_id_var, ptr, it_part->send_count, FTI_DBLE );
            
            // add part to global dataset
            offset += static_cast<FTIT_hsize_t>(it_part->local_offset_server);
            FTIT_hsize_t count = static_cast<FTIT_hsize_t>(it_part->send_count);
            FTI_AddSubset( m_id_var, 1, &offset, &count, dataset_id );
           
            // store subset id in keymap
            std::string subset_name(dataset_name);
            // subset name '<fieldname>_hidden_<stateid>_<runner_rank>'
            subset_name += "_" + std::to_string( it_part->rank_runner );
            id_map[subset_name] = m_id_var;
            
            m_id_var++;
            it_part++;
        
        }
        
        dataset_id++;
    
    }
    
    m_id_dataset += dataset_id;
    m_protected = true;
}

void FTmodule::store_subset( std::unique_ptr<Field> & field, int state_id, int runner_rank, FTtype type )
{

    if( m_checkpointing ) {

        std::string key( field->name + "_" + m_FTtype[type] + "_" + std::to_string( state_id ) + "_" + std::to_string( runner_rank ));
        
        if( id_check.find(key) == id_check.end() ) {
#ifdef WITH_FTI_THREADS
            FTsched.submit( FTI_AddVarICP, id_map[key] );
#else
            FTI_AddVarICP( id_map[key] );
#endif
            id_check.insert(key);
        }
    
    }

}

void FTmodule::initCP( int epoch )
{
    if( !m_checkpointing ) {
        id_check.clear();
        m_checkpointing = true;
        FTI_InitICP( epoch, FTI_L4_H5_SINGLE, 1 );
        // add epoch counter 
        FTI_AddVarICP( 0 );
    }
}

void FTmodule::flushCP( void )
{
#ifdef WITH_FTI_THREADS
    if( m_checkpointing ) {
        FTsched.synchronize();
        FTsched.submit( FTI_FinalizeICP );
    }
#endif
}

void FTmodule::finalizeCP( void )
{
    if( m_checkpointing ) {
#ifdef WITH_FTI_THREADS
        FTsched.synchronize();
#else
        FTI_FinalizeICP();
#endif
        m_checkpointing = false;
    }
}

void FTmodule::recover( void )
{
    if( m_restart && m_protected ) {
        FTI_Recover();
        m_restart = false;
    }
}

void FTmodule::finalize( void )
{
    FTI_Finalize();
}

#endif //WITH_FTI
