#include "ZeroMQ.h"
#include <utils.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include <new>

#include <zmq.h>


namespace zmq {
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


    void send(void* socket, const void* data, std::size_t size, int flags) {
        assert(data || size == 0);

        ZMQ_CHECK(zmq_send(socket, data, size, flags));
    }
}
}


void* zmq::data(Message& msg) {
    return zmq_msg_data(&msg);
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
zmq::msg_init(void* data, std::size_t size, zmq::FreeFn free, void* hints) {
    auto p = impl::make_uninitialized_zmq_message();

    if (zmq_msg_init_data(p.get(), data, size, free, hints) < 0) {
        throw std::bad_alloc();
    }

    return p;
}


zmq::MessageRef zmq::recv(void* socket, int flags) {
    auto msg = zmq::msg_init();

    recv(*msg, socket, flags);

    return msg;
}


void zmq::recv(zmq::Message& msg, void* socket, int flags) {
    assert(socket);

    ZMQ_CHECK(zmq_msg_recv(&msg, socket, flags));
}


void zmq::send(zmq::Message& msg, void* p, int n) {
    assert(p);
    ZMQ_CHECK(zmq_msg_send(&msg, p, n));
}


std::size_t zmq::size(zmq::Message& msg) {
    return zmq_msg_size(&msg);
}
