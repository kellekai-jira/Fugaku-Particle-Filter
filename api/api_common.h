#ifndef _API_COMMON_H_
#define _API_COMMON_H_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "mpi.h"

#include "melissa_da_api.h"
#include "ApiTiming.h"
#include "Part.h"
#include "ZeroMQ.h"
#include "melissa_da_config.h"
#include "melissa_utils.h"
#include "messages.h"
#include "utils.h"
#include "melissa_da_stype.h"


//
// zmq context:
// TODO: push into Singleton or something comparable
extern void* context;

extern void* job_req_socket;
extern void* weight_push_socket;

extern MPI_Comm comm;

extern int runner_id;

extern std::vector<INDEX_MAP_T> local_index_map;
extern std::vector<INDEX_MAP_T> local_index_map_hidden;


inline bool is_p2p()
{
    if (getenv("MELISSA_DA_IS_P2P")) {
        return true;
    } else {
        return false;
    }
}


struct ConnectedServerRank;

int getCommRank();
int getRunnerId();
int getCommSize();
std::string fix_port_name(const char* port_name_);

// One of these exists to abstract the connection to every server rank that
// needs to be connected with this model task runner rank
struct ServerRankConnection
{
    void* data_request_socket;

    ServerRankConnection(const char* addr_request);

    ~ServerRankConnection();

    void send(
        VEC_T* values, const size_t bytes_to_send, VEC_T* values_hidden,
        const size_t bytes_to_send_hidden, const int current_state_id,
        const int current_step, const char* field_name);

    int receive(
        VEC_T* out_values, size_t bytes_expected, VEC_T* out_values_hidden,
        size_t bytes_expected_hidden, int* out_current_state_id,
        int* out_current_step);
};

struct ServerRanks
{
    static std::map<int, std::unique_ptr<ServerRankConnection> > ranks;
    static ServerRankConnection& get(int server_rank);
};


struct ConnectedServerRank
{
    size_t send_count;
    size_t local_vector_offset;

    size_t send_count_hidden;
    size_t local_vector_offset_hidden;

    ServerRankConnection& server_rank;
};


struct Server
{
    int comm_size = 0;
    std::vector<char> port_names;
};


struct Field
{
    std::string name;
    int current_state_id;
    int current_step;
    size_t local_vect_size;
    size_t local_hidden_vect_size;
    std::vector<ConnectedServerRank> connected_server_ranks;
    void initConnections(
        const std::vector<size_t>& local_vect_sizes,
        const std::vector<size_t>& local_hidden_vect_sizes,
        const int bytes_per_element, const int bytes_per_element_hidden);

    void putState(VEC_T* values, VEC_T* hidden_values, const char* field_name);

    int getState(VEC_T* values, VEC_T* values_hidden);
};


extern Field field;
struct ServerRanks;
extern Server server;


#ifdef REPORT_TIMING
extern std::unique_ptr<ApiTiming> timing;

inline void try_init_timing() {
    if (!timing) {
#ifndef REPORT_TIMING_ALL_RANKS
        if(getCommRank() == 0)
#endif
        {
            printf("initing timing!\n");
            timing = std::make_unique<ApiTiming>();
        }
    }
}
#endif

#endif
