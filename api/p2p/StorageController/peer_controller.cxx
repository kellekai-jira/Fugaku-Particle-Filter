#include "peer_controller.hpp"
    
bool PeerController::query( int state_id, int* peer_id ) {
  bool success = false; 
  for( auto peer : peers ) {
    // ask peers for part
  }
  return success;
}

int PeerController::transfer( int state_id, int state_rank, int peer_id ) {
}

void PeerController::request( int state_id ) {
}

void PeerController::response( int state_id, int peer_id, bool available ) {
}

bool PeerController::probe( int* state_id, int* peer_id ) {
}
