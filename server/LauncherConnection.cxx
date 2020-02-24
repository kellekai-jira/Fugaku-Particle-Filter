/*
 * LauncherConnection.cxx
 *
 *  Created on: Feb 24, 2020
 *      Author: friese
 */

#include "LauncherConnection.h"

#include <zmq.h>
#include "utils.h"

LauncherConnection::LauncherConnection(void * context, const std::string &launcher_host, const int comm_rank) {

    L("Establishing connection to launcher on %s:3000-3002", launcher_host.c_str());

    text_puller = zmq_socket (context, ZMQ_SUB);
    text_pusher = zmq_socket (context, ZMQ_PUSH);
    text_requester = zmq_socket (context, ZMQ_REQ);

    const int linger = 10000;
    std::string launcher_addr = "tcp://" + launcher_host + ":3000";  // TODO: make as option
    zmq_setsockopt (text_pusher, ZMQ_LINGER, &linger, sizeof(int));
    ZMQ_CHECK(zmq_connect (text_pusher, launcher_addr.c_str()));

    launcher_addr = "tcp://" + launcher_host + ":3001";  // TODO: make as option
    zmq_setsockopt(text_puller, ZMQ_SUBSCRIBE, "", 0);
    zmq_setsockopt (text_puller, ZMQ_LINGER, &linger, sizeof(int));
    ZMQ_CHECK(zmq_connect (text_puller, launcher_addr.c_str()));

    // === opent req-rep port  === //
    launcher_addr = "tcp://" + launcher_host + ":3002";  // TODO: make as option
    zmq_setsockopt (text_requester, ZMQ_LINGER, &linger, sizeof(int));
    const int recv_timeout = 100000; // recv timeout
    zmq_setsockopt (text_requester, ZMQ_RCVTIMEO, &recv_timeout, sizeof(int));
    ZMQ_CHECK(zmq_connect (text_requester, launcher_addr.c_str()));


    // Send the first message

    send_message_server_name(node_name, comm_rank, text_pusher, 0);

}

LauncherConnection::~LauncherConnection() {
    zmq_close (text_pusher);
    zmq_close (text_puller);
    zmq_close (text_requester);
}

