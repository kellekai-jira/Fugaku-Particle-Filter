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

    const int LAUNCHER_TIMEOUT=60;  // seconds
    const int LAUNCHER_PING_INTERVAL=60;  // seconds

    int due_date_launcher;
    int next_message_date_to_launcher;

    void updateLauncherDueDate();
    void updateLauncherNextMessageDate();
public:
    LauncherConnection(void * context, std::string launcher_host);
    virtual ~LauncherConnection();

    inline void * getTextPuller() { return text_puller; }

    void receiveText();

    bool checkLauncherDueDate();
    void ping();
};

#endif /* LAUNCHERCONNECTION_H_ */
