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
/**
 * This function initializes a new 0MQ message with the data given by the
 * first argument.
 *
 * If free is not NULL, then 0MQ assumes ownership over the data referenced by
 * the first argument and as soon as the message was sent,
 * it will free the resources occupied the data by calling free(data, hints).
 *
 * @attention This function does *not* copy the data. Instead, the storage
 * referenced by the first argument must not be modified until the message was
 * sent.
 *
 * @param[in] data Contents to be sent
 * @param[in] size Message size in bytes
 * @param[in] free Reference to a function freeing resources referenced by data
 * @param[in] hints Value to be passed verbatim to the function free
 */
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
void send_n(void* socket, const T* data, std::size_t count, int flags = 0) {
    static_assert(std::is_trivial<T>::value, "");

    impl::send(socket, data, count * sizeof(T), flags);
}

inline void send_empty(void* socket, int flags = 0) {
    impl::send(socket, nullptr, 0, flags);
}


}

#endif /* ZeroMQ_H_ */
