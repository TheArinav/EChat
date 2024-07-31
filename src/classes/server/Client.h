//
// Created by IMOE001 on 7/31/2024.
//

#ifndef EPOLLCHAT_CLIENT_H
#define EPOLLCHAT_CLIENT_H

#include <string>
#include <vector>
#include <sys/socket.h>

using namespace std;

namespace src::classes::server {
    class Client {
    public:
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

    };
}// server

#endif //EPOLLCHAT_CLIENT_H
