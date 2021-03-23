#ifndef _HELPERS_H_
#define _HELPERS_H_

#include "p2p.pb.h"
#include "ZeroMQ.h"

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

#endif // _HELPERS_H_
