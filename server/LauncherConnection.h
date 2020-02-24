/*
 * LauncherConnection.h
 *
 *  Created on: Feb 24, 2020
 *      Author: friese
 */

#ifndef LAUNCHERCONNECTION_H_
#define LAUNCHERCONNECTION_H_

#include <string>

class LauncherConnection {
private:
    void * text_pusher = nullptr;
    void * text_puller = nullptr;
    void * text_requester = nullptr;
public:
    LauncherConnection(void * context, const std::string &launcher_host, const int comm_rank);
    virtual ~LauncherConnection();
};

#endif /* LAUNCHERCONNECTION_H_ */
