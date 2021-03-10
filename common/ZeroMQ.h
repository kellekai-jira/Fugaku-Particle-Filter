#ifndef ZeroMQ_H_
#define ZeroMQ_H_

#include <cstddef>
#include <memory>
#include <type_traits>

struct zmq_msg_t;


namespace zmq
{
namespace impl
{
void send(void* socket, const void* data, std::size_t size, int flags = 0);
}

using Message = zmq_msg_t;
using MessageRef = std::unique_ptr<zmq_msg_t, void (*)(zmq_msg_t*)>;
using FreeFn = void (*)(void*, void*);

char* data(Message& msg);
zmq::MessageRef msg_init();
zmq::MessageRef msg_init(std::size_t size);
zmq::MessageRef msg_init(
    void* data, std::size_t size, FreeFn free = nullptr, void* hints = nullptr);

template <typename T>
zmq::MessageRef msg_init_n(
    T* data, std::size_t count, FreeFn free = nullptr, void* hints = nullptr) {
    static_assert(std::is_trivial<T>::value, "");

    return msg_init(data, sizeof(T) * count, free, hints);
}


zmq::MessageRef recv(void* socket, int flags = 0);
void recv(Message& msg, void* socket, int flags = 0);
void send(Message& msg, void* socket, int flags = 0);
std::size_t size(Message& msg);

template <typename T>
void send(void* socket, const T* first, const T* last, int flags = 0) {
    static_assert(std::is_trivial<T>::value, "");

    assert(last >= first);

    auto count = last - first;

    impl::send(socket, first, count * sizeof(T), flags);
}

template <typename T>
void send_n(void* socket, const T* data, std::size_t count, int flags = 0) {
    static_assert(std::is_trivial<T>::value, "");

    impl::send(socket, data, count * sizeof(T), flags);
}

inline void send_empty(void* socket, int flags = 0) {
    impl::send(socket, nullptr, 0, flags);
}


}

#endif /* ZeroMQ_H_ */
