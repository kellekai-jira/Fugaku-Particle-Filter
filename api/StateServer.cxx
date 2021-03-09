#include "StateServer.h"

#include "utils.h"
#include <memory>


#include "../../server-p2p/messages/cpp/control_messages.pb.h"


//FIXME: double cheeck all MPI_COMM_WORLD, especially in melissa_p2p_api.cxx
//
void handle_application_state_request() {
    if (getCommRank() == 0) {
        // wait for messages from application rank 0
        MPI_IProbe...();

        // send which state to retrieve to other ranks
        MPI_Send() to other ranks whcih state to get
    } else {
        // probe for requests from head rank 0
        // retrieve host name list from it and try to retrieve state... Then MPI_allgather in head rank 0
    }
}


void handle_state_server_requests() {
        if state_server.recv.......
            see if state is there and send to state server...
                // !! important to check again if state can be retrieved locally. (it might be possible that the state server was about to prefetch tehe state anyway ;))



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
    handle_application_state_request();

    // 2. Check if other states have a request, respond it
    handle_state_server_requests();

    // 3. Garbage collection, ask server which states to remove
    // 4. Maybe perform a prefetch request and do some prefetching.
    // 5. maybe dump states to deeper level
}

