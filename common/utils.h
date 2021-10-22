#ifndef UTILS_H_
#define UTILS_H_

#include "melissa_da_stype.h"

#include <cassert>
#include <climits>
#include <csignal>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>
#include <sstream>

#include <arpa/inet.h>
#include <execinfo.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>

#include <mpi.h>
#include <zmq.h>

#include <streambuf>
#include <cassert>

class AddTimeStamp : public std::streambuf
{
public:
    AddTimeStamp( std::basic_ios< char >& out )
        : out_( out )
        , sink_()
        , newline_( true )
    {
        sink_ = out_.rdbuf( this );
        assert( sink_ );
    }
    ~AddTimeStamp()
    {
        out_.rdbuf( sink_ );
    }
protected:
    int_type overflow( int_type m = traits_type::eof() )
    {
        if( traits_type::eq_int_type( m, traits_type::eof() ) )
            return sink_->pubsync() == -1 ? m: traits_type::not_eof(m);
        if( newline_ )
        {   // --   add timestamp here
            std::ostream str( sink_ );
            if( !(str << "[" << getTimestamp() << " ms] ") ) // add perhaps a seperator " "
                return traits_type::eof(); // Error
        }
        newline_ = traits_type::to_char_type( m ) == '\n';
        return sink_->sputc( m );
    }
private:
    long getTimestamp() {
    	auto now = std::chrono::system_clock::now();
    	auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    	auto value = now_ms.time_since_epoch();
    	long duration = value.count();
    }
    AddTimeStamp( const AddTimeStamp& );
    AddTimeStamp& operator=( const AddTimeStamp& ); // not copyable
    // --   Members
    std::basic_ios< char >& out_;
    std::streambuf* sink_;
    bool newline_;
};

inline void print_stack_trace() {
    void* array[10];
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
#define MDBG(...) \
    do { \
    } while(false)
#else
#define MDBG(x ...) \
    do { \
        if(comm_rank == 0) { \
            char str[1024]; \
            std::snprintf(str, 1024, x); \
            std::stringstream ss; \
            ss << str << " (" << __FILE__ << ":" <<  __LINE__ << ")"; \
            std::cout << ss.str() << std::endl; \
            fflush(stdout); \
        } \
    } while(false)
#endif

// normal logging:
#define MPRT(x ...) \
    do { \
        if(comm_rank == 0) { \
            char str[1024]; \
            std::snprintf(str, 1024, x); \
            std::stringstream ss; \
            ss << "[rank:" << comm_rank << "] " << str << std::endl; \
            std::cout << ss.str() << std::endl; \
            fflush(stdout); \
        } \
    } while(false)

#define MERR(x ...) \
    do { \
        if(comm_rank == 0) { \
            std::printf("[%d] ", comm_rank); \
            std::printf(x); \
            std::printf("\n"); \
            std::fprintf(stderr, "[melissa_da_server,%d] ", comm_rank); \
            std::fprintf(stderr, x); \
            std::fprintf(stderr, "\n"); \
            std::raise(SIGINT); \
            std::exit(EXIT_FAILURE); \
        } \
    } while(false)


#define ZMQ_CHECK(x) \
    do { \
        if((x) == -1) { \
            int err2 = errno; \
            int err = zmq_errno(); \
            MERR("zmq error(%d, errno=%d): %s", err, err2, zmq_strerror(err)); \
            std::raise(SIGINT); \
        } \
    } while(false)

// https://stackoverflow.com/questions/40807833/sending-size-t-type-data-with-mpi
// :
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

extern MPI_Datatype MPI_MY_INDEX_MAP_T;
void create_MPI_INDEX_MAP_T();


/// sums over a vector
template <class T>
T sum_vec(const std::vector<T>& vec) {
    return std::accumulate(vec.cbegin(), vec.cend(), T{0});
}


// inline Functions:
template <typename T>
inline void print_vector(const std::vector<T>& vec) {
    printf("[");
    for(auto it = vec.begin(); it != vec.end(); it++)
    {
        // printf("%.3f,", *it);
        std::cout << *it << ", ";
    }
    std::cout.flush();

    printf("]\n");
}

inline void assert_more_zmq_messages(void* socket) {
    int more;
    size_t more_size = sizeof(more);
    zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &more_size);
    assert(more);
}

inline void assert_no_more_zmq_messages(void* socket) {
    int more;
    size_t more_size = sizeof(more);
    zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &more_size);
    assert(more == 0);
}

inline std::ostream& operator<<(std::ostream& os, const INDEX_MAP_T& rhs) {
    return os << rhs.varid << ": " << rhs.index;
}


// timing:
typedef std::chrono::time_point<std::chrono::system_clock> TimePoint;
inline double diff_to_millis(const TimePoint& lhs, const TimePoint& rhs) {
    return std::chrono::duration<double, std::milli>(lhs - rhs).count();
}

// Globals:
extern int comm_rank;
extern int comm_size;
extern Phase phase;


// init
inline void init_utils() {
    check_data_types();
    create_MPI_INDEX_MAP_T();
}


void slow_MPI_Scatterv(const void *sendbuf, const size_t *sendcounts, const
                       size_t *displs,
                       MPI_Datatype sendtype, void *recvbuf, size_t recvcount,
                       MPI_Datatype recvtype,
                       int root, MPI_Comm comm);
void slow_MPI_Gatherv(const void *sendbuf, size_t sendcount, MPI_Datatype
                      sendtype,
                      void *recvbuf, const size_t *recvcounts, const
                      size_t *displs,
                      MPI_Datatype recvtype, int root, MPI_Comm comm);

#endif
