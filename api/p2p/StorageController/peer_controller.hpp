#ifndef _PEER_CONTROLLER_H_
#define _PEER_CONTROLLER_H_

#include <string>
#include "io_controller.hpp"
#include "fti_controller.hpp"
#include "ZeroMQ.h"
#include "helpers.h"

class Peer {
  public:
    Peer();
};

class PeerController {
  public:

      PeerController( IoController* io, void* zmq_context, MpiController* mpi );
      ~PeerController();

      /// checks if somebody wants to load states from the disk
      void handle_requests();

      /// mirrors a state from another runner
      /// returns false if the state could not be found.
      bool mirror(io_state_id_t id);

private:

	    std::string get_file_name_from_path( const std::string& path );
      std::string hostname;
      int port;
      void* m_zmq_context;
      void * state_server_socket;

      IoController* m_io;
      MpiController* m_mpi;

};

#endif // _PEER_CONTROLLER_H_
