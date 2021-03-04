#ifndef ZeroMQ_H_
#define ZeroMQ_H_

#include <cstddef>
#include <memory>

struct zmq_msg_t;


namespace zmq {
using Message = zmq_msg_t;
using MessageRef = std::unique_ptr<zmq_msg_t, void (*)(zmq_msg_t*)>;
using FreeFn = void (*)(void*, void*);

void* msg_data(Message* p_msg);
zmq::MessageRef msg_init();
zmq::MessageRef msg_init(std::size_t size);
zmq::MessageRef msg_init(
    char* data, std::size_t size, FreeFn free = nullptr, void* hints = nullptr);
zmq::MessageRef msg_recv(void* socket, int flags = 0);
void msg_recv(Message* p_msg, void* socket, int flags = 0);
void msg_send(Message* p_msg, void* socket, int flags = 0);
std::size_t msg_size(Message* p_msg);
}

#endif /* ZeroMQ_H_ */
