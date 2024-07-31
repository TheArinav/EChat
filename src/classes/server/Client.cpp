//
// Created by IMOE001 on 7/31/2024.
//

#include "Client.h"

namespace src::classes::server {
    Client::Client(int fd, sockaddr_storage addr,bool guest):
    FileDescriptor(fd), Address(addr), IsGuest(guest){

    }

    Client::~Client() {

    }

    ssize_t Client::Read() {
        return 0;
    }

    ssize_t Client::Write() {
        return 0;
    }

    void Client::EnqueueResponse(const string &s_resp) {

    }

    Client::Client() =default;
} // server