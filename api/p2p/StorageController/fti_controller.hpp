#ifndef _FTI_CONTROLLER_H_
#define _FTI_CONTROLLER_H_

#include <fti.h>
#include "fti_kernel.hpp"
#include "io_controller.hpp"
#include "zip_controller.hpp"
#include <vector>
#include <map>
#include <cassert>
#include "mpi_controller_impl.hpp"
    
struct topo_t {
  topo_t() :
    nbProc(-1),
    nbNodes(-1),
    myRank(-1),
    splitRank(-1),
    nodeSize(-1),
    nbHeads(-1),
    nbApprocs(-1),
    groupSize(-1),
    sectorID(-1),
    nodeID(-1),
    groupID(-1),
    amIaHead(-1),
    headRank(-1),
    headRankNode(-1),
    nodeRank(-1),
    masterLocal(false),
    masterGlobal(false),
    groupRank(-1),
    right(-1),
    left(-1) {
    body.resize(FTI_BUFS);
  }
  int nbProc;
  int nbNodes;
  int myRank;
  int splitRank;
  int nodeSize;
  int nbHeads;
  int nbApprocs;
  int groupSize;
  int sectorID;
  int nodeID;
  int groupID;
  int amIaHead;
  int headRank;
  int headRankNode;
  int nodeRank;
  bool masterLocal;
  bool masterGlobal;
  int groupRank;
  int right;
  int left;
  std::vector<int> body;
};
 
struct exec_t {
  exec_t() :
    globalComm(MPI_COMM_NULL) {}
  MPI_Comm globalComm;
  std::string id;
};
 

inline bool operator==(const io_state_id_t& lhs, const io_state_id_t& rhs) {
    return lhs.t == rhs.t && lhs.id == rhs.id;
}

inline bool operator!=(const io_state_id_t& lhs, const io_state_id_t& rhs) {
    return !(lhs == rhs);
}

/*
 * state encoding and decoding
 * id: 32 bits, t:24 bits, param: 8 bits
 * max state_id: 4294967295, max t: 16777215, max p: 255
 *
 */
inline io_state_id_t to_state_id(const int64_t ckpt_id) {
  int64_t mask_param  = 0xFF;
  int64_t mask_t      = 0xFFFFFF;
  int64_t mask_id     = 0xFFFFFFFF;
  int64_t id          = ckpt_id & mask_id;
  int64_t t           = (ckpt_id >> 32) & mask_t;
  int64_t param       = (ckpt_id >> 56) & mask_param;
  return { t, id, param };
}

inline int64_t to_ckpt_id(io_state_id_t state_id) {
  int64_t ckpt_id = state_id.param;
  ckpt_id = (ckpt_id << 24) | state_id.t;
  ckpt_id = (ckpt_id << 32) | state_id.id;
  return ckpt_id;
}


class FtiController : public IoController {
  public:
    void init_io( int runner_id );
    void init_core();
    void fini();
    int protect( std::string name, void* buffer, size_t size, io_type_t type );
    bool is_adapt() { return m_zip_controller.is_adapt(); }
    bool is_validate() { return m_zip_controller.is_validate(); }
    int reprotect_all();
    bool load( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void store( io_state_id_t state_id, io_level_t level = IO_STORAGE_L1 );
    void remove( io_state_id_t state_id, io_level_t level );
    void stage( io_state_id_t state, io_level_t from, io_level_t to );
    void filelist_local( io_state_id_t ckpt_id, std::vector<std::string> & ckptfiles );
    void update_metadata( io_state_id_t ckpt_id, io_level_t level );

    bool is_local( io_state_id_t state_id );
    bool is_global( io_state_id_t state_id );

    void request( io_state_id_t state_id );

    bool probe( io_tag_t tag );
    bool to_validate() {return m_zip_controller.to_validate();}
    int get_parameter_id() {return m_zip_controller.get_parameter_id();}
    int get_num_parameters() { return m_zip_controller.get_num_parameters(); }
    void advance_validate() {m_zip_controller.advance_validate();}
    void sendrecv( const void* send_buffer, void* recv_buffer, int send_size, int recv_size, io_tag_t tag, io_msg_t message_type  );
    void send( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type  );
    void isend( const void* send_buffer, int size, io_tag_t tag, io_msg_t message_type, mpi_request_t & req  );
    void recv( void* recv_buffer, int size, io_tag_t tag, io_msg_t message_type  );
    void get_message_size( int* size, io_tag_t tag, io_msg_t message_type  );

    void register_callback( void (*f)(void) );
  private:
    
    void stage_l1l2( std::string L1_CKPT, std::string L1_META, std::string L2_TEMP, std::string L2_CKPT, io_state_id_t state_id );
    void stage_l2l1( std::string L2_CKPT, std::string L1_TEMP, std::string L1_META_TEMP,
      std::string L1_CKPT, std::string L1_META_CKPT, io_state_id_t state_id );
		void init_compression_parameter();
    
    std::map<io_level_t,FTIT_level> m_io_level_map;
    std::map<io_type_t,fti_id_t> m_io_type_map;
    //std::map<io_zip_type_t,FTIT_CPC_TYPE> m_io_zip_type_map;
    //std::map<io_zip_mode_t,FTIT_CPC_MODE> m_io_zip_mode_map;
    //std::map<FTIT_CPC_TYPE,io_zip_type_t> m_io_zip_type_inv_map;
    //std::map<FTIT_CPC_MODE,io_zip_mode_t> m_io_zip_mode_inv_map;
    std::map<io_msg_t,int> m_io_msg_map;
    std::map<io_tag_t,int> m_io_tag_map;
    std::map<std::string, io_var_t> m_var_id_map;
    //std::map<std::string, io_zip_t> m_var_zip_map;
    int m_next_garbage_coll;
    //io_id_t m_id_counter;  
    int m_last_cycle;
    FTI::Kernel m_kernel;
    ZipController m_zip_controller;
    int m_runner_id;
    topo_t m_topo;
    exec_t m_exec;
};

#endif // _FTI_CONTROLLER_H_
