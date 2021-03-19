#include "../../../server-p2p/messages/cpp/control_messages.pb.h"

void send_message(void * socket, const ::melissa_p2p::Message &m, flags = 0) {
    zmq::send_n(socket, m.SerializeToArray(m.ByteSize()), m.ByteSize(), flags);
}


::melissa_p2p::Message receive_message(void * socket) {
    auto resp = zmq::recv(socket);
    ::melissa_p2p::Message m;
    m.ParseFromArray(zmq::data(resp), zmq::size(resp));
    return m;
}


