#include "peer_controller.hpp"
#include "../../../server-p2p/messages/cpp/control_messages.pb.h"
#include "ZeroMQ.h"
#include "helpers.h"

void PeerController::handle_requests()
{
    // Do a poll...

    zmq_pollitem_t items[1];
    items[0] = {state_server_socket, 0, ZMQ_POLLIN, 0};

    ZMQ_CHECK(zmq_poll(items, items_to_poll, 0));

    // answer requests
    if (items[0].revents & ZMQ_POLLIN) {
        auto req = receive_message(state_server_socket);
        io_fti_t id = to_fti_id(req.state_request.state_id().t(), req.state_request().state_id().id());

        // TODO Ask fti for file list!
        // TODO Generate File list message.
        // TODO send back this message.

    }
}


PeerController::PeerController(void * context)
{

    port = 3131;
    char tmp[MPI_MAX_PROCESSOR_NAME];
    melissa_get_node_name(tmp, MPI_MAX_PROCESSOR_NAME);
    hostname = tmp;

    std::string addr = hostname + ':' + port;

    state_server_socket = zmq_socket(context, ZMQ_REP);
    zmq_bind(state_server_socket, addr.c_str());

    state_request_socket = zmq_socket(context, ZMQ_REQ);


    // Start ftp thread in background!
}

PeerController::~PeerController()
{
    zmq_close(state_server_socket);
    zmq_close(state_request_socket);
}


bool PeerController::mirror(io_id_t id)
{
    ::melissa_p2p::Message m;
    // get friendly head rank of the same rank...
    m.mutable_runner_request()->set_head_rank(mpi.my_head_rank());
    m.mutable_runner_request()->mutable_socket->set_node_name(hostname);
    m.mutable_runner_request()->mutable_socket->set_port(port);
    m.set_runner_id(runner_id);


    send_message(state_request_socket, m);

    auto reply = receive_message(state_request_socket);

    ::melissa_p2p::Message state_request;
    int t, id;
    from_fti_id(id, &t, &id);
    state_request.set_runner_id(runner_id);
    state_request.mutable_state_request()->mutable_state_id()->set_t(t);
    state_request.mutable_state_request()->mutable_state_id()->set_id(id);

    bool found = false;
    // go through list of runners (that is shuffled/ put in a good order by the server)
    for (int i = 0; i < reply.runner_response().runners_size(); i++)
    {
        std::string addr = reply.runner_response().socket().node_name() + ':' +
            reply.runner_response().socket().port()

        zmq_connect(state_request_socket, addr.c_str());

        send_message(state_request_socket, state_request);
        m = receive_message(state_request);
        if (m.state_response.has_state_id())
        {
            found = true;
            // FIXME: assert that stateid is the one requested!

            // TODO: shuffle
            for (size_t j = 0; j < m.state_response.filenames_size(); j++)
            {
                std::string cmd = "echo ftp get " + addr + state_response.filenames(j);
                // TODO: need to install an ftp client!
                system(cmd.c_str()); // sec issue: when renaming filenames wroongly one can get the id...

                // TODO: if got the state: nice otherwise go to next
            }
        }


        zmq_disconnect(state_request_socket, addr.c_str());
        if (found) {
            break;
        }
    }

    return found;
}
