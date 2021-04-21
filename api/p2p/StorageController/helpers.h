#ifndef _HELPERS_H_
#define _HELPERS_H_

#include "p2p.pb.h"
#include "ZeroMQ.h"

#include <zmq.h>
#include "utils.h"

inline void send_message(void * socket, const ::melissa_p2p::Message &m, int flags = 0) {
    char buf[m.ByteSize()];
    m.SerializeToArray(buf, m.ByteSize());
    zmq::send_n(socket, buf, m.ByteSize(), flags);
}


inline ::melissa_p2p::Message receive_message(void * socket) {
    auto resp = zmq::recv(socket);
    ::melissa_p2p::Message m;
    m.ParseFromArray(zmq::data(*resp), zmq::size(*resp));
    return m;
}

inline bool has_msg(void * socket, int timeout_ms=0) {
    // poll on state server socket now
    zmq_pollitem_t items[1];
    items[0] = {socket, 0, ZMQ_POLLIN, 0};

    //D("polling on state server socket");
    ZMQ_CHECK(zmq_poll(items, 1, timeout_ms));

    // answer only one request to not block for too long (otherwise this would be a while...)
    return (items[0].revents & ZMQ_POLLIN);
}

#endif // _HELPERS_H_
