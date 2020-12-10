#ifdef WITH_FTI

#include "FTmodule.h"
#include <algorithm>
#include "MpiManager.h"

void FTmodule::init( MpiManager & mpi, int & epoch_counter )
{
    m_checkpointing = false;
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
    hsize_t dim = 1;
    hsize_t offset = 0;
    hsize_t count = 1;
    FTI_DefineGlobalDataset( m_id_offset, 1, &dim, "epoch_counter", NULL, FTI_INTG );
    FTI_Protect( m_id_offset, &epoch_counter, 1, FTI_INTG );
    FTI_AddSubset( m_id_offset, 1, &offset, &count, m_id_offset );
    m_id_offset++;
    m_restart = static_cast<bool>(FTI_Status());
    m_protected = false;
}

void FTmodule::protect_background( MpiManager & mpi, std::unique_ptr<Field> & field )
{
    int comm_size_server = mpi.size();
    int comm_size_runner = field->local_vect_sizes_runner.size();
    size_t local_vect_sizes_server[comm_size_server];
    size_t global_vect_size = field->globalVectSize();
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
    int myRank = mpi.rank();
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
        FTI_DefineGlobalDataset( dataset_id, dataset_rank, &state_dim, dataset_name.c_str(), NULL, FTI_CHAR );
        std::vector<Part>::iterator it_part = field->parts.begin();
        while( (it_part = std::find_if( it_part, field->parts.end(), [myRank]( Part & part ) {return myRank == part.rank_server;} )) != field->parts.end() ){
            offset += static_cast<hsize_t>(it_part->local_offset_server);
            hsize_t count = static_cast<hsize_t>(it_part->send_count);
            void* ptr = it_ens->state_background.data() + it_part->local_offset_server;
            FTI_Protect( subset_id, ptr, it_part->send_count, FTI_CHAR );
            count_tot += count;
            FTI_AddSubset( subset_id, 1, &offset, &count, dataset_id );
            std::string subset_name(dataset_name);
            subset_name += "_" + std::to_string( it_part->rank_runner );
            id_map[subset_name] = subset_id;
            subset_id++;
            it_part++;
        }
        dataset_id++;
    }
    m_protected = true;
}

void FTmodule::store_subset( std::unique_ptr<Field> & field, int state_id, int runner_rank )
{
    if( m_checkpointing ) {
        std::string key(field->name);
        key += "_" + std::to_string( state_id ) + "_" + std::to_string( runner_rank );
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
        //FTI_InitICP( epoch, FTI_L4_H5_SINGLE, 1 );
        FTI_InitICP( epoch, 4, 1 );
        for(int id=0; id<m_id_offset; id++) {
            FTI_AddVarICP( id );
        }
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
