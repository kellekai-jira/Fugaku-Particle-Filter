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

    // answer only one request to not block for too long (otherwise this would be a while...)
    if (items[0].revents & ZMQ_POLLIN) {
        auto req = receive_message(state_server_socket);
        io_fti_t id = to_fti_id(req.state_request.state_id().t(), req.state_request().state_id().id());

        ::melissa_p2p::Message reply;
        reply.mutable_state_response();
        if (!has_state_cached(req.state_request().state_id())) {  // TODO: kai we need a function that checks if the state is in the ram disk cache!
            send_message(state_server_socket);

            return;
        }
        // TODO Ask fti for file list! kai
        std::vector<std::string> filenames;

        // Generate File list message.
        for (auto &it : filenames) {
            reply.mutable_state_response()->add_filenames() = *it;
        }

        send_message(state_server_socket, reply, ZMQ_SNDMORE);

        // attach raw data
        for (auto &it : filenames) {
            reply.mutable_state_response()->add_filenames() = *it;

            int flags = (it == filenames.end() ? 0 : ZMQ_SNDMORE);
            std::ifstream infile (*it ,std::ifstream::binary);

            // get size of file
            infile.seekg (0, infile.end);
            long size = infile.tellg();
            infile.seekg (0);

            // allocate msg for file content
            auto data_msg = zmq::msg_init(size);

            // read content of infile
            infile.read (zmq::data(data_msg), size);

            zmq::send(data_msg, state_server_socket, flags);
        }

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


    send_message(state_reqest_socket, m);

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

            for (size_t j = 0; j < m.state_response().filenames_size(); j++)
            {

                assert_more_zmq_messages(state_request_socket);
                std::string exec_dir = ...; // TODO: kai: get path from where checkpoints can be loaded
                std::ofstream outfile( exec_dir/m.state_response().filenames(j),
                        std::ofstream::binary );

                // write to outfile
                auto data_msg = zmq::recv(state_request_socket);

                outfile.write (zmq::data(data_msg), zmq::size(data_msg));
                outfile.close();  // close explicitly to create softlink
                // TODO: create soft link to global dir...
                // TODO: send file per file ....
            }
            break;
        }


        zmq_disconnect(state_request_socket, addr.c_str());
        if (found) {
            break;
        }
    }

    return found;
}
