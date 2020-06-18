/*
 * LauncherConnection.cxx
 *
 *  Created on: Feb 24, 2020
 *      Author: friese
 */

#include "LauncherConnection.h"

#include <zmq.h>
#include "utils.h"

#include "melissa_utils.h"  // melissa_utils from melissa-sa for melissa_get_node_name
#include "melissa_messages.h"

LauncherConnection::LauncherConnection(void * context, std::string launcher_host)
{
    updateLauncherDueDate();

    char hostname[MPI_MAX_PROCESSOR_NAME];
    melissa_get_node_name(hostname, MPI_MAX_PROCESSOR_NAME);

    if (launcher_host == hostname)
    {
        // conecting to localhost
        launcher_host = "127.0.0.1";
    }

    L("Establishing connection to launcher on %s:3000-3002", launcher_host.c_str());

    text_puller = zmq_socket (context, ZMQ_SUB);
    text_pusher = zmq_socket (context, ZMQ_PUSH);
    text_requester = zmq_socket (context, ZMQ_REQ);

    const int linger = 10000;
    std::string launcher_addr = "tcp://" + launcher_host + ":5555";  // TODO: make as option
    zmq_setsockopt (text_pusher, ZMQ_LINGER, &linger, sizeof(int));
    ZMQ_CHECK(zmq_connect (text_pusher, launcher_addr.c_str()));

    launcher_addr = "tcp://" + launcher_host + ":5556";  // TODO: make as option
    zmq_setsockopt(text_puller, ZMQ_SUBSCRIBE, "", 0);
    zmq_setsockopt (text_puller, ZMQ_LINGER, &linger, sizeof(int));
    ZMQ_CHECK(zmq_connect (text_puller, launcher_addr.c_str()));

    // === opent req-rep port  === //
    launcher_addr = "tcp://" + launcher_host + ":5554";  // TODO: make as option
    zmq_setsockopt (text_requester, ZMQ_LINGER, &linger, sizeof(int));
    const int recv_timeout = 100000; // recv timeout
    zmq_setsockopt (text_requester, ZMQ_RCVTIMEO, &recv_timeout, sizeof(int));
    ZMQ_CHECK(zmq_connect (text_requester, launcher_addr.c_str()));


    // Send the first message
    send_message_server_name(hostname, comm_rank, text_pusher, 0);
    updateLauncherNextMessageDate();
    D("Successful connected to launcher");
}

LauncherConnection::~LauncherConnection()
{
    send_message_stop(text_pusher, 0);

    zmq_close (text_pusher);
    zmq_close (text_puller);
    zmq_close (text_requester);
}

void LauncherConnection::updateLauncherDueDate()
{
    due_date_launcher = time(NULL) + LAUNCHER_TIMEOUT * 1000;
}

bool LauncherConnection::checkLauncherDueDate()
{
    return time(NULL) < due_date_launcher;
}

void LauncherConnection::receiveText()
{
    zmq_msg_t msg;
    zmq_msg_init (&msg);
    zmq_msg_recv (&msg, text_puller, 0);
    D("Launcher message recieved %s", zmq_msg_data (&msg));
    updateLauncherDueDate();
    // ATM We do not care what the launcher sends us. We only check if it is still alive
    //process_launcher_message(zmq_msg_data (&msg), server_ptr);

    // TODO: Loadbalancing
    // -: if it is  a message requesting the workload return the percentage.
    // Then the launcher will decide if it tries to start more runners or not.
    // -: check if it is a message that means that some runners may be killed.
    // then the server kills them as this will produce no waiting times.
    // REM: The launcher may not send load balancing messages too often as it
    // always takes some time for runner kills to execute and for the system to
    // stabilize again!

    zmq_msg_close (&msg);
}

void LauncherConnection::updateLauncherNextMessageDate()
{
    next_message_date_to_launcher = time(NULL) + LAUNCHER_PING_INTERVAL * 1000;
}

void LauncherConnection::ping()
{
    if (time(NULL) > next_message_date_to_launcher)
    {
        send_message_hello(text_pusher, 0);
        updateLauncherNextMessageDate();
    }
}



