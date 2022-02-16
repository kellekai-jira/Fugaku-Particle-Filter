#ifndef _FTI_KERNEL_H_
#define _FTI_KERNEL_H_

#include <string>
#include <fti.h>

class FtiController;
class ZipController;
class StorageController;

namespace FTI {
  
  typedef FTIT_dataset data_t;

  class Kernel {
    friend class ::FtiController;
    friend class ::ZipController;
    friend class ::StorageController;
    private:

      // API
      void remove_ckpt_metadata( int ckptId, int level );
      void update_ckpt_metadata( int ckptId, int level );
      void load_ckpt_meta_proc( int ckptId, int proc, int64_t* fs, std::string mfn );
      void file_copy( std::string from,  std::string to );
      void print( std::string msg, int priority );
      
      void transform( FTIT_dataset* );
      
      // FTI Core
      FTIT_topology* topo = FTI_GetTopo();
      FTIT_configuration* conf = FTI_GetConf();
      FTIT_execution* exec = FTI_GetExec();
      FTIT_checkpoint* ckpt = FTI_GetCkpt();

  };

}

extern "C" {

void FTI_Print(const char* msg, int priority);

int FTI_UpdateCkptMetaData(FTIT_configuration* FTI_Conf,
    FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo,
    int ckptId, int level, bool dcp, bool elastic, int deviceId );

int FTI_LoadCkptMetaProc( FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt, 
    const char* mfn, int id, int proc, int64_t* fs);

int FTI_FileCopy(const char* from, const char *to, size_t buffersize, 
    size_t* offset, size_t count, bool overwrite);

int FTI_RemoveCkptMetaData(FTIT_topology* FTI_Topo, FTIT_configuration* FTI_Conf, int ckptId, int level );
  
int FTI_Transform( FTIT_dataset* data );

}


#endif // _FTI_KERNEL_H_
