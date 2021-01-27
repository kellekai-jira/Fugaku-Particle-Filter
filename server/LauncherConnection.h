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
    const int LAUNCHER_PING_INTERVAL=8;  // seconds

    time_t due_date_launcher;  // seconds
    time_t next_message_date_to_launcher;  // seconds

    void updateLauncherNextMessageDate();
public:
    LauncherConnection(void * context, std::string launcher_host);
    virtual ~LauncherConnection();

    inline void * getTextPuller() { return text_puller; }

    void receiveText();

    bool checkLauncherDueDate();
    void ping();
    void notify(const int runner_id, const int status);

    void updateLauncherDueDate();
};

#endif /* LAUNCHERCONNECTION_H_ */
