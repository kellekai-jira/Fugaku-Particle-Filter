#ifndef _PEER_CONTROLLER_H_
#define _PEER_CONTROLLER_H_

#include <string>
#include "io_controller.hpp"
#include "fti_controller.hpp"
#include "ZeroMQ.h"
#include "helpers.h"

#define BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_SYSTEM_NO_DEPRECATED
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/system/error_code.hpp>

class Peer {
  public:
    Peer();
};

class PeerController {
public:
    PeerController( IoController* io, void* zmq_context, MpiController* mpi );
    ~PeerController();

    /// checks if somebody wants to load states from the disk
    void handle_requests();

    /// mirrors a state from another runner
    /// returns false if the state could not be found.
    bool mirror(io_state_id_t state_id);

private:

    std::string get_file_name_from_path( const std::string& path );
    std::string hostname;
    int port;
    int port_udp;
    void* m_zmq_context;
    void * state_server_socket;

    IoController* m_io;
    MpiController* m_mpi;


    void handle_state_request();
    void handle_avail_request();
    ::melissa_p2p::Message dns_req(const io_state_id_t & state_id );
    bool get_state_from_peer(const io_state_id_t & state_id,
            const std::string & port_name, const int peer_runner_id);
    bool flush_out_state_avail_reqs(boost::asio::ip::udp::socket & socket,
        const io_state_id_t & state_id );

    boost::asio::io_service m_io_service;
    boost::asio::ip::udp::socket m_udp_socket;
};

#endif // _PEER_CONTROLLER_H_
