#include "api_common.h"
#include <cassert>
#include "ZeroMQ.h"

#include "utils.h"

int getRunnerId() {
    assert(runner_id != -1); // must be inited
    return runner_id;
}

/**
 * Returns simulation's rank
 * is the same as the runner rank.
 */
int getCommRank() {
    int rank;
    MPI_Comm_rank(comm, &rank);
    return rank;
}

/**
 * Returns Simulations comm size
 * is the same as the runner comm size.
 */
int getCommSize() {
    int size;
    MPI_Comm_size(comm, &size);
    return size;
}


/// if node name = my nodename, replace by localhost!
std::string fix_port_name(const char* port_name_) {
    std::string port_name(port_name_);
    char my_host_name[MPI_MAX_PROCESSOR_NAME + 1] = {0};
    melissa_get_node_name(my_host_name, MPI_MAX_PROCESSOR_NAME);
    size_t found = port_name.find(my_host_name);
    // check if found and if hostname is between tcp://<nodename>:port
    if(found != std::string::npos && port_name[found - 1] == '/'
            && port_name[found + strlen(my_host_name)] == ':')
    {
        port_name = port_name.substr(0, found) + "127.0.0.1"
            + port_name.substr(found + strlen(my_host_name));
    }
    return port_name;
}


ServerRankConnection::ServerRankConnection(const char* addr_request) {
    data_request_socket = zmq_socket(context, ZMQ_REQ);
    assert(data_request_socket);
    std::string cleaned_addr = fix_port_name(addr_request);
    D("Data Request Connection to %s", cleaned_addr.c_str());
    // ZMQ_CHECK();
    int ret = zmq_connect(data_request_socket, cleaned_addr.c_str());
    assert(ret == 0);

    D("connect socket %p", data_request_socket);
}

ServerRankConnection::~ServerRankConnection() {
    D("closing socket %p", data_request_socket);
    zmq_close(data_request_socket);
}

void ServerRankConnection::send(
        VEC_T* values, const size_t bytes_to_send, VEC_T* values_hidden,
        const size_t bytes_to_send_hidden, const int current_state_id,
        const int current_step, const char* field_name) {
    // send simuid, rank, stateid, current_step, field_name next message:
    // doubles

    // current_step is incremented on the server side
    int header[] = {
        getRunnerId(), getCommRank(), current_state_id, current_step};
    auto msg_header =
        zmq::msg_init(sizeof(header) + MPI_MAX_PROCESSOR_NAME);

    std::memset(zmq::data(*msg_header), 0, zmq::size(*msg_header));
    std::memcpy(zmq::data(*msg_header), header, sizeof(header));
    std::strncpy(
            zmq::data(*msg_header) + sizeof(header),
            field_name, MPI_MAX_PROCESSOR_NAME);

    D("sending on socket %p", data_request_socket);

    zmq::send(*msg_header, data_request_socket, ZMQ_SNDMORE);

    D("-> Simulation runnerid %d, rank %d sending stateid %d "
            "current_step=%d fieldname=%s, %lu+%lu hidden bytes",
            getRunnerId(), getCommRank(), current_state_id, current_step,
            field_name, bytes_to_send, bytes_to_send_hidden);
    // D("values[0]  = %.3f", values[0]);
    // D("values[1]  = %.3f", values[1]);
    // D("values[2]  = %.3f", values[2]);
    // D("hidden values[0]  = %.3f", values_hidden[0]);
    // D("hidden values[1]  = %.3f", values_hidden[1]);
    // D("hidden values[2]  = %.3f", values_hidden[2]);
    // D("values[35] = %.3f", values[35]);
    int flag = (bytes_to_send_hidden > 0) ? ZMQ_SNDMORE : 0;

    // required for zmq::send_n in combination with a number of bytes
    // (instead of an element count)
    static_assert(std::is_same<VEC_T, char>::value, "");

    zmq::send_n(data_request_socket, values, bytes_to_send, flag);

    if(bytes_to_send_hidden > 0)
    {
        zmq::send_n(
                data_request_socket, values_hidden, bytes_to_send_hidden);
    }
}

int ServerRankConnection::receive(
        VEC_T* out_values, size_t bytes_expected, VEC_T* out_values_hidden,
        size_t bytes_expected_hidden, int* out_current_state_id,
        int* out_current_step) {
    // receive a first message that is 1 if we want to change the state,
    // otherwise 0 or 2 if we want to quit. the first message also contains
    // out_current_state_id and out_current_step the 2nd message just
    // consists of bytes that will be put into out_values

    auto msg = zmq::recv(data_request_socket);
    D("Received message size = %lu", zmq::size(*msg));
    assert(zmq::size(*msg) == 4 * sizeof(int));

    int state[4] = {0};

    std::memcpy(state, zmq::data(*msg), sizeof(state));

    *out_current_state_id = state[0];
    *out_current_step = state[1];
    int type = state[2];
    int nsteps = state[3];

    if(type == CHANGE_STATE)
    {
        assert_more_zmq_messages(data_request_socket);

        // zero copy is for sending only!
        msg = zmq::recv(data_request_socket);

        D("<- Simulation got %lu bytes, expected %lu + %lu hidden bytes... "
                "for state %d, current_step=%d, nsteps=%d (socket=%p)",
                zmq::size(*msg), bytes_expected, bytes_expected_hidden,
                *out_current_state_id, *out_current_step, nsteps,
                data_request_socket);

        assert(zmq::size(*msg) == bytes_expected);

        std::memcpy(out_values, zmq::data(*msg), bytes_expected);

        // print_vector(std::vector<double>(out_values,
        //                                 out_values +
        //                                 doubles_expected));

        if(bytes_expected_hidden > 0)
        {
            assert_more_zmq_messages(data_request_socket);

            // zero copy is for sending only!
            msg = zmq::recv(data_request_socket);

            assert(zmq::size(*msg) == bytes_expected_hidden);
            std::memcpy(out_values_hidden, zmq::data(*msg),
                    bytes_expected_hidden);

            // print_vector(std::vector<double>(out_values_hidden,
            // out_values_hidden +
            // doubles_expected_hidden));
        }

        assert_no_more_zmq_messages(data_request_socket);
    }
    else if(type == END_RUNNER)
    {
        // TODO: is this really an error?
        printf("Error: Server decided to end this runner now.\n");
        // calculate 0 steps now.
        nsteps = 0;  // caller will call melissa_finalize()
    }
    else if(type == KILL_RUNNER)
    {
        printf("Error: Server decided that this Runner crashed. So killing "
                "it now.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        exit(1);
    }
    else
    {
        assert(type == KEEP_STATE);
        // TODO: unimplemented
        assert(false);
    }
    return nsteps;
}


void Field::initConnections(
        const std::vector<size_t>& local_vect_sizes,
        const std::vector<size_t>& local_hidden_vect_sizes,
        const int bytes_per_element, const int bytes_per_element_hidden) {
    std::vector<Part> parts = calculate_n_to_m(
            server.comm_size, local_vect_sizes, bytes_per_element);
    std::vector<Part> parts_hidden = calculate_n_to_m(
            server.comm_size, local_hidden_vect_sizes,
            bytes_per_element_hidden);

    assert(parts_hidden.size() == 0 || parts_hidden.size() == parts.size());
    // for this to hold true the hidden state should be at least as big as
    // server.comm_size

    auto hidden_part = parts_hidden.begin();
    for(auto part = parts.begin(); part != parts.end(); ++part)
    {

        if(part->rank_runner == getCommRank())
        {
            size_t hidden_sendcount = 0;
            size_t hidden_local_offest_runner = 0;
            if(parts_hidden.size() > 0)
            {
                // assume the same parts (just different sizes) exist for
                // the hidden state
                assert(
                        hidden_part->rank_runner == getCommRank()); // Same
                // part...

                hidden_sendcount = hidden_part->send_count;
                hidden_local_offest_runner =
                    hidden_part->local_offset_runner;
            }

            connected_server_ranks.push_back(
                    {part->send_count, part->local_offset_runner,
                    hidden_sendcount, hidden_local_offest_runner,
                    ServerRanks::get(part->rank_server)});
        }
        if(parts_hidden.size() > 0) // doing the hidden parts thing...
        {
            hidden_part++;
        }
    }
}

// TODO: change MPI_INT to size_t where possible, also in server.cxx
// TODO: this will crash if there are more than two fields? maybe use dealer
// socket that supports send send recv recv scheme.
void Field::putState(VEC_T* values, VEC_T* hidden_values, const char* field_name) {
    // send every state part to the right server rank
    for(auto csr = connected_server_ranks.begin();
            csr != connected_server_ranks.end(); ++csr)
    {
        D("put state, local offset: %lu, send count: %lu",
                csr->local_vector_offset, csr->send_count);
        csr->server_rank.send(
                &values[csr->local_vector_offset], csr->send_count,
                &hidden_values[csr->local_vector_offset_hidden],
                csr->send_count_hidden, current_state_id, current_step,
                field_name);
    }

    current_state_id = -1;
}

int Field::getState(VEC_T* values, VEC_T* values_hidden) {
    int nsteps = -1;
    // TODO: an optimization would be to poll instead of receiving directly.
    // this way we receive first whoever comes first. but as we need to
    // synchronize after it probably does not matter a lot?
    for(auto csr = connected_server_ranks.begin();
            csr != connected_server_ranks.end(); ++csr)
    {
        // receive state parts from every serverrank.
        D("get state, local offset: %lu, send count: %lu",
                csr->local_vector_offset, csr->send_count);

        // do not try to receive if we are finalizeing already. Even check
        // if the last receive might have started finalization.
        if(phase == PHASE_FINAL)
        {
            return 0;
        }

        int nnsteps = csr->server_rank.receive(
                &values[csr->local_vector_offset], csr->send_count,
                &values_hidden[csr->local_vector_offset_hidden],
                csr->send_count_hidden, &current_state_id, &current_step);
        assert(nsteps == -1 || nsteps == nnsteps); // be sure that all send
        // back the same
        // nsteps...
        nsteps = nnsteps;
    }
    return nsteps;
}



ServerRankConnection& ServerRanks::get(int server_rank) {
    auto found = ranks.find(server_rank);
    if(found == ranks.end())
    {
        // connect to this server rank
        // we use unique_ptr's as other wise we would create a ServerRank
        // locally, we than would copy all its values in the ranks map and
        // then we would destroy it. unfortunately this also closes the zmq
        // connection !
        auto res = ranks.emplace(
                server_rank,
                std::unique_ptr<ServerRankConnection>(new ServerRankConnection(
                        server.port_names.data()
                        + server_rank *
                        MPI_MAX_PROCESSOR_NAME)));
        return *res.first->second;
    }
    else
    {
        return *(found->second);
    }
}
