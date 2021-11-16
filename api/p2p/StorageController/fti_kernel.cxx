#include "fti_kernel.hpp"
#include "io_controller.hpp"

void FTI::Kernel::update_ckpt_metadata( int ckptId, int level ) {
  IO_TRY( FTI_UpdateCkptMetaData(conf, exec, topo, ckptId, level, 
        false, false, 0 ), FTI_SCES, "failed to update checkpoint metadata");
}

void FTI::Kernel::remove_ckpt_metadata( int ckptId, int level ) {
  // TODO make FTI_RemoveCkptMetaData collective and blocking
  IO_TRY( FTI_RemoveCkptMetaData(topo, conf, ckptId, level ), FTI_SCES, "failed to remove checkpoint metadata");
}

void FTI::Kernel::file_copy( std::string from, std::string to ) {
  IO_TRY( FTI_FileCopy(from.c_str(), to.c_str(), IO_TRANSFER_SIZE, 
        NULL, -1, false), FTI_SCES, "failed to copy file" ); 
}

void FTI::Kernel::print( std::string msg, int priority ) {
  FTI_Print( msg.c_str(), priority );
}

