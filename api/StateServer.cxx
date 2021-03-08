#include "StateServer.h"

#include "utils.h"
#include <memory>

// Callback called in the FTI head loop:
void melissa_server_states() {
    static std::unique_ptr<StateServer> state_server(nullptr);
    if (state_server.get() == nullptr) {
        // open all sockets necessary
        state_server = std::make_unique<StateServer>(context);
    }


    // 1. Check if application rank 0 has a request
    //      if so forward it to the other FTI_head ranks and try to get the state.
    //      then do an mpi send back.
    //      TODO: use scp to actual get the states from the ramdisk -> easier to implement and only blocking on requester side..., maybe not even...
    // 2. Check if other states have a request, respond it
    state_server->respond_state_request

    // 3. Garbage collection, ask server which states to remove
    // 4. Maybe perform a prefetch request and do some prefetching.
    // 5. maybe dump states to deeper level
}

