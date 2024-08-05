//
// Created by IMOE001 on 7/31/2024.
//

#ifndef EPOLLCHAT_CLIENT_H
#define EPOLLCHAT_CLIENT_H

#include <string>
#include <vector>
#include <sys/socket.h>
#include <memory>
#include <mutex>

#include "../general/Constants.h"

using namespace std;
using namespace src::classes::general;

namespace src::classes::server {
    class Account;
    class Client {
    public:
        Hash ID;
        shared_ptr<Account> Owner;
        bool IsGuest;
        int FileDescriptor;
        sockaddr_storage Address;
        vector<char> ReadBuffer;
        vector<char> WriteBuffer;

        Client();
        explicit Client(int fd, sockaddr_storage addr, bool guest);

        ~Client();

        ssize_t Read();

        ssize_t Write();

        void EnqueueResponse(const string &s_resp);
        void SetOwner(shared_ptr<Account> owner);

    private:
        static Hash count;
        unique_ptr<mutex> WriteMutex;
        unique_ptr<mutex> ReadMutex;
        unique_ptr<mutex> OwnerMutex;
        void Setup();
    };
}// server

#endif //EPOLLCHAT_CLIENT_H
