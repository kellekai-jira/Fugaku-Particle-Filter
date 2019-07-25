#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <cassert>
#include <zmq.h>

#include <mpi.h>


//#ifdef __DEBUG
#define D(x ...) printf(x); printf(" (%s:%d)\n", __FILE__, __LINE__)
//#else
//#define D(...)
//#endif

#define ZMQ_CHECK(x) if (x == -1) { D("zmq error: %s" , zmq_strerror(zmq_errno())); }

enum Phase {
  PHASE_INIT,
  PHASE_SIMULATION,
  PHASE_FINAL
};

Phase phase = PHASE_INIT;

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

/**
 *******************************************************************************
 *
 * @ingroup melissa_utils
 *
 * Gets the name of the processus node
 *
 *******************************************************************************
 *
 * @param[out] *node_name
 * The node name
 *
 *******************************************************************************/
void melissa_get_node_name (char *node_name, size_t buf_len)
{
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    char   *addr;
    char ok = 0;

    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET)
        {
            sa = (struct sockaddr_in *) ifa->ifa_addr;
            addr = inet_ntoa(sa->sin_addr);
            if (strcmp (ifa->ifa_name, "ib0") == 0)
            {
            	  assert(strlen(node_name) <= buf_len);
                strcpy(node_name, addr);
                ok = 1;
                break;
            }
        }
    }
    if (ok == 0)
    {
      gethostname(node_name, buf_len);
    }
}
