//
// Created by IMOE001 on 7/31/2024.
//

#include "../general/Constants.h"

#include "Client.h"
#include "Account.h"
#include <unistd.h>

namespace src::classes::server {
    Hash Client::count=0;
    Client::Client(int fd, sockaddr_storage addr, bool guest):
    FileDescriptor(fd), Address(addr), IsGuest(guest){
        Setup();
    }

    Client::~Client() {
        close(FileDescriptor);
    }

    ssize_t Client::Read() {
        return 0;
    }

    ssize_t Client::Write() {
        return 0;
    }

    void Client::EnqueueResponse(const string &s_resp) {

    }

    void Client::Setup() {
        ReadBuffer=vector<char>(BUFFER_SIZE);
        WriteBuffer=vector<char>(BUFFER_SIZE);
        Owner=nullptr;
        ID=count++;
    }

    void Client::SetOwner(shared_ptr<src::classes::server::Account> owner) {
        this->Owner=move(owner);
    }

    Client::Client() =default;
} // server