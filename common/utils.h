#ifndef UTILS_H_
#define UTILS_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <cassert>
#include <csignal>
#include <zmq.h>

#include <mpi.h>

#include <iostream>

#include <stdint.h>
#include <limits.h>
#include <vector>

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


inline void print_stack_trace() {
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    printf("Stack trace requested:\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

enum Phase
{
    PHASE_INIT,
    PHASE_SIMULATION,
    PHASE_FINAL
};

// debug logs:
#ifdef NDEBUG
// release mode
#define D(...)
#else
// debug mode
// #define D(x ...) printf(x); printf(" (%s:%d)\n", __FILE__, __LINE__)
#define D(x ...) if(comm_rank == 0) {printf(x); printf(" (%s:%d)\n", __FILE__, \
                                                       __LINE__);}
#endif

// normal logging:
#define L(x ...) if (comm_rank == 0) {printf("[%d] ", comm_rank); printf(x); \
                                      printf("\n");}


#define ZMQ_CHECK(x) if (x == -1) { int err2 = errno; int err = zmq_errno(); D( \
                                        "zmq error(%d, errno=%d): %s", err, \
                                        err2, zmq_strerror(err)); \
                                    std::raise(SIGINT); }

// https://stackoverflow.com/questions/40807833/sending-size-t-type-data-with-mpi  :
#if SIZE_MAX == UCHAR_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_CHAR
#elif SIZE_MAX == USHRT_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_SHORT
#elif SIZE_MAX == UINT_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED
#elif SIZE_MAX == ULONG_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_LONG
#elif SIZE_MAX == ULLONG_MAX
   #define my_MPI_SIZE_T MPI_UNSIGNED_LONG_LONG
#else
   #error "what is happening here?"
#endif



// Functions:
void check_data_types();

template <typename T>
inline void print_vector (const std::vector<T> &vec)
{
    printf("[");
    for (auto it = vec.begin(); it != vec.end(); it++)
    {
        // printf("%.3f,", *it);
        std::cout << *it << ", ";
    }
    std::cout.flush();

    printf("]\n");
}

/// sums over a vector
template <class T>
T sum_vec(const std::vector<T> &vec) {
    T res = 0;
    for (auto &it : vec) {
        res += it;
    }
    return res;
}


// inline Functions:
inline void assert_more_zmq_messages(void * socket)
{
    int more;
    size_t more_size = sizeof (more);
    zmq_getsockopt (socket, ZMQ_RCVMORE, &more, &more_size);
    assert(more);
}

inline void assert_no_more_zmq_messages(void * socket)
{
    int more;
    size_t more_size = sizeof (more);
    zmq_getsockopt (socket, ZMQ_RCVMORE, &more, &more_size);
    assert(more == 0);
}

// Globals:
extern int comm_rank;
extern int comm_size;
extern Phase phase;

#endif
