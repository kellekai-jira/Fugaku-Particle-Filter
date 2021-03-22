#ifndef _HELPERS_H_
#define _HELPERS_H_ 

#include "../../../server-p2p/messages/cpp/control_messages.pb.h"
#include "ZeroMQ.h"

//void send_message(void * socket, const ::melissa_p2p::Message &m, int flags = 0) {
////    char buf[m.ByteSizeLong()];
////    m.SerializeToArray(buf, m.ByteSizeLong());
////    zmq::send_n(socket, buf, m.ByteSizeLong(), flags);
//}
//
//
//::melissa_p2p::Message receive_message(void * socket) {
////    auto resp = zmq::recv(socket);
////    ::melissa_p2p::Message m;
////    m.ParseFromArray(zmq::data(resp), zmq::size(resp));
////    return m;
//}
//
#endif // _HELPERS_H_
