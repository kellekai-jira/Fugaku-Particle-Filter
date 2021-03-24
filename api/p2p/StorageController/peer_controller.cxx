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

    }
}


PeerController::PeerController( IoController* io, void* zmq_context, MpiController* mpi )
{
    m_io = io;
    m_mpi = mpi;
    m_zmq_context = zmq_context; 
    port = 3131;
    char tmp[MPI_MAX_PROCESSOR_NAME];
    melissa_get_node_name(tmp, MPI_MAX_PROCESSOR_NAME);
    hostname = tmp;

    std::string addr = std::string(hostname) + ":" + std::to_string(port);

    state_server_socket = zmq_socket(m_zmq_context, ZMQ_REP);
    zmq_bind(state_server_socket, addr.c_str());

    state_request_socket = zmq_socket(m_zmq_context, ZMQ_REQ);
}

PeerController::~PeerController()
{
    zmq_close(state_server_socket);
    zmq_close(state_request_socket);
}


bool PeerController::mirror( io_state_id_t state_id )
{
    ::melissa_p2p::Message dns_req;
    // get friendly head rank of the same rank...
    dns_req.mutable_runner_request()->set_head_rank(m_mpi->rank());
    dns_req.mutable_runner_request()->mutable_socket()->set_node_name(hostname);
    dns_req.mutable_runner_request()->mutable_socket()->set_port(port);
    dns_req.set_runner_id(runner_id);
    send_message(state_request_socket, dns_req);

    auto dns_reply = receive_message(state_request_socket);

    ::melissa_p2p::Message state_request;
    state_request.set_runner_id(runner_id);
    state_request.mutable_state_request()->mutable_state_id()->set_t(state_id.t);
    state_request.mutable_state_request()->mutable_state_id()->set_id(state_id.id);

    bool found = false;
    // go through list of runners (that is shuffled/ put in a good order by the server)
    for (int i = 0; i < dns_reply.runner_response().sockets_size(); i++)
    {
        std::string addr = dns_reply.runner_response().sockets(i).node_name() + ':' +
            std::to_string(dns_reply.runner_response().sockets(i).port());

        std::string port_name = fix_port_name(addr.c_str());
        assert( zmq_connect(state_request_socket, port_name.c_str()) == 0 );

        send_message(state_request_socket, state_request);
        auto m = receive_message(state_request_socket);
        if (m.state_response().has_state_id())
        {
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

            break;
        }


        zmq_disconnect(state_request_socket, addr.c_str());
        if (found) {
            break;
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

