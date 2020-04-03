#include "utils.h"
#include <cassert>
#include "zmq.h"
#include <cstring>


void check_data_types() {
    // check that the size_t datatype is the same on the server and on the client side! otherwise the communication might fail.
    // for sure this could be done more intelligently in future!
    D("sizeof(size_t)=%lu", sizeof(size_t));
    assert(sizeof(size_t) == 8);
}

int comm_rank (-1);
int comm_size (-1);
Phase phase (PHASE_INIT);
