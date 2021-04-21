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
#include <chrono>
using boost::asio::ip::udp;

void PeerController::handle_avail_request()
{
    // Check for udp state avail requests:
    boost::array<char, 1024> buf;
    udp::endpoint remote_endpoint;
    boost::system::error_code error;
    size_t numbytes = m_udp_socket.receive_from(boost::asio::buffer(buf),
            remote_endpoint, 0, error);


    if (error) {
        if (error == boost::asio::error::would_block) {
            // nothing received
        } else if (error != boost::asio::error::message_size) {
            throw boost::system::system_error(error);
        }
    } else {
        // received something

        ::melissa_p2p::Message req;
        req.ParseFromArray(buf.data(), numbytes);


        ::melissa_p2p::Message reply;
        reply.mutable_state_avail_response()->set_available(false);

        if (req.has_state_avail_request()) {

            D("We do have the requested state!");
            // respond to state avail request
            auto io_state_id = io_state_id_t(req.state_avail_request().state_id().t(), req.state_avail_request().state_id().id());

            if (m_io->is_local(io_state_id)) {
                reply.mutable_state_avail_response()->mutable_state_id()->CopyFrom(req.state_avail_request().state_id());
                reply.mutable_state_avail_response()->mutable_socket()->set_node_name(hostname);
                reply.mutable_state_avail_response()->mutable_socket()->set_port(port);
                reply.mutable_state_avail_response()->mutable_socket()->set_runner_id(runner_id);
                reply.mutable_state_avail_response()->set_available(true);

                reply.set_runner_id(runner_id);
            }
            size_t s = reply.ByteSize();
            reply.SerializeToArray(buf.data(), s);
            boost::system::error_code ignored_error;
            m_udp_socket.send_to(boost::asio::buffer(buf, s),
                    remote_endpoint, 0, ignored_error);
        }
    }
}




void PeerController::handle_state_request()
{
    if (has_msg(state_server_socket)) {
        auto req = receive_message(state_server_socket);
        // respond to state request

        D("got state req: %s", req.state_request().DebugString().c_str());

        auto io_state_id = io_state_id_t(req.state_request().state_id().t(), req.state_request().state_id().id());

        trigger(START_COPY_STATE_TO_RUNNER, req.runner_id());

        ::melissa_p2p::Message reply;
        reply.mutable_state_response();
        if (!m_io->is_local(io_state_id)) {
            D("but this state is not available locally");
            send_message(state_server_socket, reply);
            return;
        }

        D("sending state...");

        std::vector<std::string> filenames;

        m_io->filelist_local( io_state_id, filenames );
        assert(filenames.size() > 0);

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
            D("send part with flags: %d", flags);
        }
        D("sent full state message");
        trigger(STOP_COPY_STATE_TO_RUNNER, req.runner_id());
    }
}

void PeerController::handle_requests()
{
    handle_avail_request();
    handle_state_request();
}


PeerController::PeerController( IoController* io, void* zmq_context, MpiController* mpi )
    : m_udp_socket(m_io_service, udp::endpoint(udp::v4(), 0))
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
    const int linger = 0;
    zmq_setsockopt (state_server_socket, ZMQ_LINGER, &linger, sizeof(int));



    ZMQ_CHECK(zmq_bind(state_server_socket, "tcp://*:*"));
    size_t size = sizeof(port_name);
    zmq_getsockopt( state_server_socket, ZMQ_LAST_ENDPOINT, &port_name, &size );

    int colon_pos = strlen(port_name)-1;
    for (; port_name[colon_pos] != ':' && colon_pos > 0; colon_pos--);
    port = atoi(port_name + colon_pos + 1);

    port_udp = m_udp_socket.local_endpoint().port();
    m_udp_socket.non_blocking(true);

    D("Bound my state server to %s, publishing %s:%d, udp :%d", port_name,
            hostname.c_str(), port, port_udp);
}

PeerController::~PeerController()
{
    zmq_close(state_server_socket);
}

long long now_ms()
{
    return std::chrono::duration_cast< std::chrono::milliseconds >(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}


::melissa_p2p::Message PeerController::dns_req(const io_state_id_t & state_id )
{
    trigger(START_REQ_RUNNER_LIST, runner_id);

    ::melissa_p2p::Message req;
    // get friendly head rank of the same rank...
    req.mutable_runner_request()->set_head_rank(m_mpi->rank());
    req.mutable_runner_request()->mutable_socket()->set_node_name(hostname);
    req.mutable_runner_request()->mutable_socket()->set_port(port);
    req.mutable_runner_request()->mutable_socket()->set_port_udp(port_udp);
    req.mutable_runner_request()->mutable_socket()->set_runner_id(runner_id);  // could optimize this out but it is needed, at least for event triggering
    req.mutable_runner_request()->mutable_searched_state_id()->set_t(state_id.t);
    req.mutable_runner_request()->mutable_searched_state_id()->set_id(state_id.id);

    req.set_runner_id(runner_id);
    send_message(storage.server.m_socket, req);

    auto res = receive_message(storage.server.m_socket);
    trigger(STOP_REQ_RUNNER_LIST, runner_id);
    return res;
}

bool PeerController::flush_out_state_avail_reqs(udp::socket & socket,
        const io_state_id_t & state_id )
{
    ::melissa_p2p::Message state_avail_request;
    state_avail_request.set_runner_id(runner_id);
    state_avail_request.mutable_state_avail_request()->mutable_state_id()->set_t(state_id.t);
    state_avail_request.mutable_state_avail_request()->mutable_state_id()->set_id(state_id.id);
    boost::array<char, 1024> state_avail_send_buf;
    size_t s = state_avail_request.ByteSize();
    state_avail_request.SerializeToArray(state_avail_send_buf.data(), s);

    auto dns_reply = dns_req(state_id);
    int peer_count = dns_reply.runner_response().sockets_size();
    D("retrieved %d peers to try from...", peer_count);


    if (peer_count == 0) {
        return false;
    }

    for (int i = 0; i < peer_count; ++i) {
        std::string addr = dns_reply.runner_response().sockets(i).node_name();
        int port = dns_reply.runner_response().sockets(i).port_udp();

        std::string fixed_host_name = fix_port_name(addr.c_str());
        D("ask %s:%d for state", fixed_host_name.c_str(), port);

        udp::resolver resolver(m_io_service);
        boost::system::error_code error;
        udp::resolver::query query(fixed_host_name, std::to_string(port), udp::resolver::query::numeric_service);
        auto res = resolver.resolve(query, error);
        if (error) {
            if (error == boost::asio::error::host_not_found) {
                D("host not found: %s:", fixed_host_name.c_str());
                continue;
            } else {
                throw boost::system::system_error(error);
            }
        }

        udp::endpoint receiver_endpoint = *res;

        socket.send_to(boost::asio::buffer(state_avail_send_buf, s), receiver_endpoint);
    }
    return true;
}

bool PeerController::get_state_from_peer(const io_state_id_t & state_id,
        const std::string & port_name, const int peer_runner_id)
{

    const int linger = 0;  // ms
    bool found = false;

    ::melissa_p2p::Message state_request;
    state_request.set_runner_id(runner_id);
    state_request.mutable_state_request()->mutable_state_id()->set_t(state_id.t);
    state_request.mutable_state_request()->mutable_state_id()->set_id(state_id.id);

    // Open a socket to the sender
    void * state_request_socket = zmq_socket(m_zmq_context, ZMQ_REQ);
    zmq_setsockopt(state_request_socket, ZMQ_LINGER, &linger, sizeof(int));
    IO_TRY( zmq_connect(state_request_socket, port_name.c_str()), 0, "unable to connect to zmq socket" );
    send_message(state_request_socket, state_request);

    int poll_ms = 2000 + rand() % 4000;
    int stop_poll_ms = now_ms() + poll_ms;
    D("%s has %d ms to send me state state %d,%d", port_name.c_str(), poll_ms, state_id.t, state_id.id);
    while (found == false && now_ms() < stop_poll_ms) {
        //handle_requests();  // don't block out other guys that ask for attention ;)
        if (has_msg(state_request_socket, 0)) {
            auto m = receive_message(state_request_socket);
            if (m.state_response().has_state_id())
            {
                trigger(START_COPY_STATE_FROM_RUNNER, peer_runner_id );
                trigger(PEER_HIT, peer_runner_id);
                D("Got it! Start copying state...");

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

                trigger(STOP_COPY_STATE_FROM_RUNNER, peer_runner_id);
            }
        }
    }
    zmq_close(state_request_socket);  // abandon the request of the state.

    return found;
}


bool PeerController::mirror( io_state_id_t state_id )
{
    trigger(START_REQ_RUNNER, 0);

    bool found = false;
    auto start = now_ms();
    unsigned long long next_flush_ms = 0;
    const int linger = 0;




    udp::socket socket(m_io_service);
    socket.open(udp::v4());
    socket.non_blocking(true);  // set nonblocking after open!

    boost::array<char, 1024> recv_buf;

    while (!found && (now_ms() - start < 5000l)) {
    //while (!found) {  // for debugging reasons we do not allow to load stuff from the pfs once it is on a runner...
        //handle_requests();  // don't block out other guys that ask for attention ;)
        if (now_ms() > next_flush_ms) {
            // repeat this every 5 seconds:
            next_flush_ms = now_ms() + 5000l;
            if (!flush_out_state_avail_reqs(socket, state_id)) {
                trigger(STOP_REQ_RUNNER, 0);
            }
        }

        // now wait for some avail responses arriving via udp....
        udp::endpoint sender_endpoint;
        boost::system::error_code error;
        size_t len = socket.receive_from(
                boost::asio::buffer(recv_buf), sender_endpoint, 0, error);

        if (error) {
            if (error == boost::asio::error::would_block) {
                // nothing received
                continue;
            } else {
                throw boost::system::system_error(error);
            }
        }

        ::melissa_p2p::Message m_a;
        m_a.ParseFromArray(recv_buf.data(), len);
        if (!(m_a.state_avail_response().available() && m_a.state_avail_response().state_id().t() == state_id.t
                    && m_a.state_avail_response().state_id().id() == state_id.id)) {
            D("Got an state_avail_response but of the wrong state");
            continue;
        }

        std::string addr = std::string("tcp://") + m_a.state_avail_response().socket().node_name() + ':' +
            std::to_string(m_a.state_avail_response().socket().port());
        std::string port_name = fix_port_name(addr.c_str());
        found = get_state_from_peer(state_id, port_name, m_a.runner_id());
    }

    trigger(STOP_REQ_RUNNER, found ? 1 : 0);
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

