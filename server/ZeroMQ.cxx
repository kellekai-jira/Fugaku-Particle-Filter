#include "ZeroMQ.h"
#include <utils.h>

#include <cassert>
#include <cstdio>
#include <new>

#include <zmq.h>


namespace impl {
std::unique_ptr<zmq_msg_t, void (*)(zmq_msg_t*)>
make_uninitialized_zmq_message() {
    auto f = [](zmq_msg_t* p) {
        zmq_msg_close(p);
        delete p;
    };
    auto p_raw = new zmq_msg_t;

    return std::unique_ptr<zmq_msg_t, decltype(f)>(p_raw, f);
}
}


void* zmq::msg_data(Message* p_msg) {
    assert(p_msg);

    return zmq_msg_data(p_msg);
}


zmq::MessageRef zmq::msg_init() {
    auto p = impl::make_uninitialized_zmq_message();

    zmq_msg_init(p.get());

    return p;
}


zmq::MessageRef zmq::msg_init(std::size_t size) {
    auto p = impl::make_uninitialized_zmq_message();

    if (zmq_msg_init_size(p.get(), size) < 0) {
        throw std::bad_alloc();
    }

    return p;
}


zmq::MessageRef
zmq::msg_init(char* data, std::size_t size, zmq::FreeFn free, void* hints) {
    auto p = impl::make_uninitialized_zmq_message();

    if (zmq_msg_init_data(p.get(), data, size, free, hints) < 0) {
        throw std::bad_alloc();
    }

    return p;
}


zmq::MessageRef zmq::msg_recv(void* socket, int flags) {
    auto msg = zmq::msg_init();

    msg_recv(msg.get(), socket, flags);

    return msg;
}


void zmq::msg_recv(zmq::Message* p_msg, void* socket, int flags) {
    assert(p_msg);
    assert(socket);

    ZMQ_CHECK(zmq_msg_recv(p_msg, socket, flags));
}


void zmq::msg_send(zmq::Message* p_msg, void* p, int n) {
    assert(p_msg);
    assert(p);
    ZMQ_CHECK(zmq_msg_send(p_msg, p, n));
}


std::size_t zmq::msg_size(zmq::Message* p_msg) {
    assert(p_msg);
    return zmq_msg_size(p_msg);
}
