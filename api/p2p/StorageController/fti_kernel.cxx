#include "fti_kernel.hpp"
#include "io_controller.hpp"

void FTI::Kernel::update_ckpt_metadata( int ckptId, int level ) {
  if( FTI_UpdateCkptMetaData(conf, exec, topo,
      ckptId, level, false, false, 0 ) != FTI_SCES ) {
    throw IoException("failed to update checkpoint metadata", 
        __FILE__, __LINE__, __func__);
  }
}

void FTI::Kernel::file_copy( std::string from, std::string to ) {
  if( FTI_FileCopy(from.c_str(), to.c_str(), IO_TRANSFER_SIZE, 
      NULL, -1, false) != FTI_SCES ) {
    throw IoException("failed to copy file", 
        __FILE__, __LINE__, __func__);
  }
}

