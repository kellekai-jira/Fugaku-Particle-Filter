#include "utils.h"
#include <cassert>
#include "zmq.h"
#include <cstring>

int get_cpu_id()
{
    /* Get the the current process' stat file from the proc filesystem */
    FILE* procfile = fopen("/proc/self/stat", "r");
    long to_read = 8192;
    char buffer[to_read];
    int read = fread(buffer, sizeof(char), to_read, procfile);
    fclose(procfile);

    // Field with index 38 (zero-based counting) is the one we want
    char* line = strtok(buffer, " ");
    for (int i = 1; i < 38; i++)
    {
        line = strtok(NULL, " ");
    }

    line = strtok(NULL, " ");
    int cpu_id = atoi(line);
    return cpu_id;
}


void check_data_types() {
    // check that the size_t datatype is the same on the server and on the client side! otherwise the communication might fail.
    // for sure this could be done more intelligently in future!
    D("sizeof(size_t)=%lu", sizeof(size_t));
    assert(sizeof(size_t) == 8);
}

int comm_rank (-1);
int comm_size (-1);
Phase phase (PHASE_INIT);


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

