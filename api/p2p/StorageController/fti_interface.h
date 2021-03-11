#ifndef _FTI_CONTROLLER_H_
#define _FTI_CONTROLLER_H_

#include <fti.h>
#include "io_controller.h"

class FtiController : IoController {
   public:
      bool is_local( int id );
      void move( int id, int device_from, int device_to );
      void store( int id, int device );
      void stage( int id, int device );
      void load( int id, int device );
      void register_callback( void (*f)(void) );
};

#endif // _FTI_CONTROLLER_H_
