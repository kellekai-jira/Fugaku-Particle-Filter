#include "peer_controller.hpp"
#include "p2p.pb.h"

#include "helpers.h"

#include <fstream>


#include "utils.h"

#include "melissa_utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include "api_common.h"

#include "mpi_controller.hpp"

#include "storage_controller_impl.hpp"

#include <cassert>


void PeerController::handle_requests()
{
    // poll on state server socket
    zmq_pollitem_t items[1];
    items[0] = {state_server_socket, 0, ZMQ_POLLIN, 0};

    ZMQ_CHECK(zmq_poll(items, 1, 0));

    // answer only one request to not block for too long (otherwise this would be a while...)
    if (items[0].revents & ZMQ_POLLIN) {
        auto req = receive_message(state_server_socket);
        auto io_state_id = io_state_id_t(req.state_request().state_id().t(), req.state_request().state_id().id());

        trigger(START_COPY_STATE_TO_RUNNER, req.runner_id());

        ::melissa_p2p::Message reply;
        reply.mutable_state_response();
        if (!m_io->is_local(io_state_id)) {
            send_message(state_server_socket, reply);
            return;
        }

				std::vector<std::string> filenames;

        m_io->filelist_local( io_state_id, filenames );
        // Generate File list message.
        for (auto &it : filenames) {
            auto fn = reply.mutable_state_response()->add_filenames();
            *fn = get_file_name_from_path(it);
        }

        send_message(state_server_socket, reply, ZMQ_SNDMORE);

        // attach raw data
        for (auto it = filenames.begin(); it != filenames.end(); it++) {

            int flags = (it+1 == filenames.end() ? 0 : ZMQ_SNDMORE);
            std::ifstream infile (*it, std::ifstream::binary);

            // get size of file
            infile.seekg (0, infile.end);
            long size = infile.tellg();
            infile.seekg (0);

            // allocate msg for file content
            auto data_msg = zmq::msg_init(size);

            // read content of infile
            infile.read (zmq::data(*data_msg), size);

            zmq::send(*data_msg, state_server_socket, flags);
        }
        trigger(STOP_COPY_STATE_TO_RUNNER, req.runner_id());

    }
}


PeerController::PeerController( IoController* io, void* zmq_context, MpiController* mpi )
{
    m_io = io;
    m_mpi = mpi;
    m_zmq_context = zmq_context;
    char tmp[MPI_MAX_PROCESSOR_NAME];
    melissa_get_node_name(tmp, MPI_MAX_PROCESSOR_NAME);

    hostname = tmp;


    state_server_socket = zmq_socket(m_zmq_context, ZMQ_REP);
    char port_name[1024]; //make this sufficiently large.
                          //otherwise an error will be thrown because of invalid argument.

    ZMQ_CHECK(zmq_bind(state_server_socket, "tcp://*:*"));
    size_t size = sizeof(port_name);
    zmq_getsockopt( state_server_socket, ZMQ_LAST_ENDPOINT, &port_name, &size );

    int colon_pos = strlen(port_name)-1;
    for (; port_name[colon_pos] != ':' && colon_pos > 0; colon_pos--);
    assert(colon_pos > 0);
    port = atoi(port_name + colon_pos + 1);

    printf("Bound my state server to %s, publishing %s:%d\n", port_name,
            hostname.c_str(), port);

}

PeerController::~PeerController()
{
    zmq_close(state_server_socket);
}


bool PeerController::mirror( io_state_id_t state_id )
{
    printf("Runner %d Performing dns request\n", runner_id);

    trigger(START_REQ_RUNNER_LIST, runner_id);
    ::melissa_p2p::Message dns_req;
    // get friendly head rank of the same rank...
    dns_req.mutable_runner_request()->set_head_rank(m_mpi->rank());
    dns_req.mutable_runner_request()->mutable_socket()->set_node_name(hostname);
    dns_req.mutable_runner_request()->mutable_socket()->set_port(port);
    dns_req.mutable_runner_request()->mutable_socket()->set_runner_id(runner_id);  // could optimize this out but it is needed, at least for event triggering
    dns_req.mutable_runner_request()->mutable_searched_state_id()->set_t(state_id.t);
    dns_req.mutable_runner_request()->mutable_searched_state_id()->set_id(state_id.id);

    dns_req.set_runner_id(runner_id);
    send_message(storage.server.m_socket, dns_req);

    auto dns_reply = receive_message(storage.server.m_socket);
    trigger(STOP_REQ_RUNNER_LIST, runner_id);

    ::melissa_p2p::Message state_request;
    state_request.set_runner_id(runner_id);
    state_request.mutable_state_request()->mutable_state_id()->set_t(state_id.t);
    state_request.mutable_state_request()->mutable_state_id()->set_id(state_id.id);



    printf("retrieved %d peers to try from...\n", dns_reply.runner_response().sockets_size());

    bool found = false;
    // go through list of runners (that is shuffled/ put in a good order by the server)
    for (int i = 0; i < dns_reply.runner_response().sockets_size(); i++)
    {

        trigger(START_REQ_RUNNER, dns_reply.runner_response().sockets(i).runner_id());
        state_request_socket = zmq_socket(m_zmq_context, ZMQ_REQ);

        const int linger = 600;  // ms
        zmq_setsockopt (state_request_socket, ZMQ_LINGER, &linger, sizeof(int));

        std::string addr = std::string("tcp://") + dns_reply.runner_response().sockets(i).node_name() + ':' +
            std::to_string(dns_reply.runner_response().sockets(i).port());

        std::string port_name = fix_port_name(addr.c_str());
        printf("Try to retrieve State %d,%d at %s\n", state_id.t, state_id.id, port_name.c_str());

        IO_TRY( zmq_connect(state_request_socket, port_name.c_str()), 0, "unable to connect to zmq socket" );

        send_message(state_request_socket, state_request);


        zmq_pollitem_t items[1];
        items[0] = {state_request_socket, 0, ZMQ_POLLIN, 0};

        ZMQ_CHECK(zmq_poll(items, 1, 600000));  // wait 600 000 us = 600 ms for an event

        // answer only one request to not block for too long (otherwise this would be a while...)
        if (items[0].revents & ZMQ_POLLIN) {
            std::cout << "RECEIVED ANSWER FROM " << port_name << std::endl;
            auto m = receive_message(state_request_socket);
            if (m.state_response().has_state_id())
            {
                trigger(STOP_REQ_RUNNER, dns_reply.runner_response().sockets(i).runner_id());
                trigger(START_COPY_STATE_FROM_RUNNER, dns_reply.runner_response().sockets(i).runner_id());

                std::cout << "FOUND STATE" << std::endl;
                found = true;
                // FIXME: assert that stateid is the one requested!

                std::string local_tmp = m_io->m_dict_string["local_dir"] + "/tmp/";
                std::string local_dir = m_io->m_dict_string["local_dir"] + "/" +
                  m_io->m_dict_string["exec_id"] + "/l1/" + std::to_string(to_ckpt_id(state_id));

                mkdir( local_tmp.c_str(), 0777 );

                for (size_t j = 0; j < m.state_response().filenames_size(); j++)
                {

                    assert_more_zmq_messages(state_request_socket);

                    std::string ckpt_file = local_tmp + m.state_response().filenames(j);
                    std::ofstream outfile( ckpt_file, std::ofstream::binary );

                    // write to outfile
                    auto data_msg = zmq::recv(state_request_socket);

                    outfile.write (zmq::data(*data_msg), zmq::size(*data_msg));
                    outfile.close();  // close explicitly to create softlink

                }

                rename( local_tmp.c_str(), local_dir.c_str() );

                if( m_io->m_dict_bool["master_global"]) {
                  m_io->update_metadata( state_id, IO_STORAGE_L1 );
                }

                trigger(STOP_COPY_STATE_FROM_RUNNER, dns_reply.runner_response().sockets(i).runner_id());

            }
        }


        //zmq_disconnect(state_request_socket, port_name.c_str());

        zmq_close(state_request_socket);

        if (found) {
            trigger(PEER_HIT, dns_reply.runner_response().sockets(i).runner_id());
            break;
        } else {
            trigger(STOP_REQ_RUNNER, dns_reply.runner_response().sockets(i).runner_id());
            trigger(PEER_MISS, dns_reply.runner_response().sockets(i).runner_id());
        }
    }

    return found;
}

std::string PeerController::get_file_name_from_path( const std::string& path ) {

   char sep = '/';

   size_t i = path.rfind(sep, path.length());
   if (i != std::string::npos) {
      return(path.substr(i+1, path.length() - i));
   }

   return("");
}

