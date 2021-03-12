#ifndef _PEER_CONTROLLER_H_
#define _PEER_CONTROLLER_H_

#include <map>

class Peer {
  public:
    Peer();
};

class PeerController {
  public:
    void request( int state_id );
    bool query( int state_id, int* peer_id );
    bool probe( int* state_id, int* peer_id );
    int transfer( int state_id, int state_rank, int peer_id );
    void response( int state_id, int peer_id, bool available );
  private:
    std::map<int,Peer> peers;
};

#endif // _PEER_CONTROLLER_H_
