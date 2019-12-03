#include "FTmodule.h"
#include <algorithm>

void* pt_fti_add_var( void* arg )
{
    FTI_AddVarICP( *(int*) arg );
}


void FTmodule::init( const MPI_Comm & comm, void * epoch_counter ) 
{
    m_checkpointing = false;
    id_check.clear();
    FTI_Init( FTI_CONFIG, comm );
    // protect global variables and set id_offset for subset ids
    hsize_t dim = 1;
    hsize_t offset = 0;
    hsize_t count = 1;
    FTI_DefineGlobalDataset( m_id_offset, 1, &dim, "epoch_counter", NULL, FTI_INTG );
    FTI_Protect( m_id_offset, epoch_counter, 1, FTI_INTG );
    FTI_AddSubset( m_id_offset, 1, &offset, &count, m_id_offset ); 
    m_id_offset++;
    m_restart = static_cast<bool>(FTI_Status());
    m_protected = false;
    FTsched.init(1);
}

void FTmodule::protect_background( std::unique_ptr<Field> & field )
{
    int comm_size_server; 
    int comm_size_runner = field->local_vect_sizes_runner.size();
    MPI_Comm_size( FTI_COMM_WORLD, &comm_size_server );
    size_t local_vect_sizes_server[comm_size_server];
    size_t global_vect_size = 0;
    for (size_t i = 0; i < comm_size_runner; ++i)
    {
        global_vect_size += field->local_vect_sizes_runner[i];
    }
    for (int i = 0; i < comm_size_server; ++i)
    {
        // every server rank gets the same amount
        local_vect_sizes_server[i] = global_vect_size /
            comm_size_server;

        // let n be the rest of this division
        // the first n server ranks get one more to split the rest fair up...
        size_t n_rest = global_vect_size - size_t(global_vect_size /
                comm_size_server) *
            comm_size_server;
        if (size_t(i) < n_rest)
        {
            local_vect_sizes_server[i]++;
        }
    }
    int myRank; MPI_Comm_rank(FTI_COMM_WORLD, &myRank);
    int dataset_rank = 1;
    hsize_t state_dim = field->globalVectSize();    
    int dataset_id = 0 + m_id_offset; // equals state_id
    int subset_id = 0 + m_id_offset;
    int N_e = field->ensemble_members.size();
    hsize_t offset_base = 0;
    for(int i=0; i<myRank; i++) { offset_base+=local_vect_sizes_server[i]; }
    for(auto it_ens=field->ensemble_members.begin(); it_ens!=field->ensemble_members.end(); it_ens++) {
        size_t count_tot = 0;
        hsize_t offset = offset_base;
        std::string dataset_name(field->name);
        dataset_name += "_" + std::to_string( dataset_id-m_id_offset );
        FTI_DefineGlobalDataset( dataset_id, dataset_rank, &state_dim, dataset_name.c_str(), NULL, FTI_DBLE );
        std::vector<Part>::iterator it_part = field->parts.begin();
        while( (it_part = std::find_if( it_part, field->parts.end(), [myRank]( Part & part ) {return myRank == part.rank_server;} )) != field->parts.end() ){
            offset += static_cast<hsize_t>(it_part->local_offset_server);
            hsize_t count = static_cast<hsize_t>(it_part->send_count);
            void* ptr = it_ens->state_background.data() + it_part->local_offset_server;
            FTI_Protect( subset_id, ptr, it_part->send_count, FTI_DBLE );
            count_tot += count;
            FTI_AddSubset( subset_id, 1, &offset, &count, dataset_id ); 
            std::string subset_name(dataset_name);
            subset_name += "_" + std::to_string( it_part->rank_runner );
            //std::cout << "[" << myRank << "] fti_dbg -> protect_var ";
            //std::cout << subset_name << " ptr: " << ptr;
            //std::cout << " offset: " << offset << " count: " << count;
            //std::cout << " state_dim: " << state_dim << " did: " << dataset_id << std::endl;
            id_map[subset_name] = subset_id;
            subset_id++;
            it_part++;
        }
        //std::cout << "[" << myRank << "] fti_dbg -> dataset_count: id " << dataset_id << " count " << count_tot << "dataset size "  << state_dim << " offset base " << offset_base << std::endl;
        dataset_id++;
    }
    m_protected = true;
}

void FTmodule::store_subset( std::unique_ptr<Field> & field, int state_id, int runner_rank )
{
    int myRank; MPI_Comm_rank(FTI_COMM_WORLD, &myRank);
    if( m_checkpointing ) {
        std::string key(field->name);
        key += "_" + std::to_string( state_id ) + "_" + std::to_string( runner_rank );
        if( id_check.find(key) == id_check.end() ) {
            //FTI_AddVarICP( id_map[key] );
            FTsched.submit( FTI_AddVarICP, id_map[key] );
            std::cout << "[" << myRank << "] fti_dbg -> add_var " << key << std::endl;
            id_check.insert(key);
        }
    }
}

void FTmodule::initCP( int epoch ) 
{   
    int myRank; MPI_Comm_rank(FTI_COMM_WORLD, &myRank);
    if( !m_checkpointing ) {
        std::cout << "[" << myRank << "] fti_dbg -> init_icp" << std::endl;
        id_check.clear();
        m_checkpointing = true;
        FTI_InitICP( epoch, FTI_L4_DCP, 1 );
        //FTI_InitICP( epoch, 1, 1 );
        for(int id=0; id<m_id_offset; id++) {
            FTI_AddVarICP( id );
        }
    }
}

void FTmodule::flushCP( void ) 
{
    int myRank; MPI_Comm_rank(FTI_COMM_WORLD, &myRank);
    if( m_checkpointing ) {
        std::cout << "[" << myRank << "] fti_dbg -> flush_icp" << std::endl;
        FTsched.synchronize();
        FTsched.submit( FTI_FinalizeICP );
    }
}

void FTmodule::finalizeCP( void ) 
{
    int myRank; MPI_Comm_rank(FTI_COMM_WORLD, &myRank);
    if( m_checkpointing ) {
        std::cout << "[" << myRank << "] fti_dbg -> finalize_icp" << std::endl;
        FTsched.synchronize();
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
