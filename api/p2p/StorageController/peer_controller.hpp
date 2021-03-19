#ifndef _PEER_CONTROLLER_H_
#define _PEER_CONTROLLER_H_

#include <string>

class Peer {
  public:
    Peer();
};

class PeerController {
  public:

      PeerController( IoController* io ) : m_io(io) {}
      /// checks if somebody wants to load states from the disk
      void handle_requests();

      /// mirrors a state from another runner
      /// returns false if the state could not be found.
      bool mirror(io_id_t id);

private:

			std::string get_file_name_from_path( const std::string& path );
      std::string hostname;
      int port;
      void * state_server_socket;
      void * state_request_socket;
      
      IoController* m_io;

};

#endif // _PEER_CONTROLLER_H_
