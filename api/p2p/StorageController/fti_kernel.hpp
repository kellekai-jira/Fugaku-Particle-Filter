#ifndef _FTI_KERNEL_H_
#define _FTI_KERNEL_H_

#include <string>
#include <fti.h>

class FtiController;
class StorageController;

namespace FTI {
  
  class Kernel {
    friend class ::FtiController;
    friend class ::StorageController;
    private:

      // API
      void update_ckpt_metadata( int ckptId, int level );
      void file_copy( std::string from,  std::string to );
      
      // FTI Core
      FTIT_topology* topo = FTI_GetTopo();
      FTIT_configuration* conf = FTI_GetConf();
      FTIT_execution* exec = FTI_GetExec();

  };

}

extern "C" {

int FTI_UpdateCkptMetaData(FTIT_configuration* FTI_Conf,
    FTIT_execution* FTI_Exec, FTIT_topology* FTI_Topo,
    int ckptId, int level, bool dcp, bool elastic, int deviceId );

int FTI_FileCopy(const char* from, const char *to, size_t buffersize, 
    size_t* offset, size_t count, bool overwrite);

}


#endif // _FTI_KERNEL_H_
