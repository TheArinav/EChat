#ifndef EPOLLCHAT_SERVER_H
#define EPOLLCHAT_SERVER_H

#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <map>
#include <tuple>
#include <queue>
#include <atomic>
#include <memory>
#include <sys/time.h>
#include <iostream>
#include <fcntl.h>
#include <thread>
#include <limits>
#include <unistd.h>
#include <mutex>

#include "./Account.h"
#include "./ChatRoom.h"
#include "./Client.h"
#include "../general/ClientResponse.h"

using namespace std;
using namespace src::classes::general;
using namespace src::classes::server;

typedef struct addrinfo AddressInfo;
typedef struct epoll_event EpollEvent;

const int MAX_EVENTS= numeric_limits<int>::max();

namespace src::classes::general {
    struct ServerRequest {
    public:
        ServerActionType Type;
        int TargetFD;
        string Data;

        ServerRequest();

        template<typename... Args>
        ServerRequest(ServerActionType type, int fd, Args... args);

        [[nodiscard]] string Serialize() const;

        template<typename... Args>
        static ServerRequest Deserialize(const string &);
    };
}

namespace src::classes::server {
    class Server {
        vector<shared_ptr<Client>> Connections;
        vector<shared_ptr<Account>> Accounts;
        vector<shared_ptr<ChatRoom>> Rooms;
        vector<string> ServerLog;
        map<Hash,tuple<Hash,Hash,string>> Messages;
        queue<tuple<Hash,shared_ptr<ClientResponse>>> Responses;
        queue<tuple<Hash,shared_ptr<ServerRequest>>> Requests;

        int FileDescriptor;
        int EpollFD;
        string ServerName;
        thread *ServerThread;

        Server();
        ~Server();
        explicit Server(string  name);

        void Start();
        void Stop();
    private:
        atomic<Hash> msgCount;
        shared_ptr<atomic<bool>> Status;
        shared_ptr<mutex> m_Connections;
        shared_ptr<mutex> m_Accounts;
        shared_ptr<mutex> m_Rooms;
        shared_ptr<mutex> m_Log;
        shared_ptr<mutex> m_Messages;
        shared_ptr<mutex> m_Responses;
        shared_ptr<mutex> m_Requests;

        void PushConnection(shared_ptr<Client> client);
        shared_ptr<Client> GetConnection(long long i);

        void PushAccount(const shared_ptr<Account>& account);
        shared_ptr<Account> GetAccount(long long i);
        long long FindAccount(Hash id);

        void PushRoom(const shared_ptr<ChatRoom>& room);
        shared_ptr<ChatRoom> GetRoom(long long i);
        long long FindRoom(Hash id);

        void PushLog(string msg);
        string GetLog(unsigned long i);

        void EmplaceMessage(Hash h, tuple<Hash,Hash,string> cont);
        tuple<Hash,Hash,string> KGetMessage(Hash id);
        vector<tuple<Hash,Hash,string>> V1GetMessage(Hash room);
        vector<tuple<Hash,Hash,string>> V2GetMessage(Hash sender);
        vector<tuple<Hash,Hash,string>> V3GetMessage(string content);

        void PushRequest(Hash id, const shared_ptr<ServerRequest>& req);
        tuple<Hash,shared_ptr<ServerRequest>> PopRequest();

        void PushResponse(Hash id, const shared_ptr<ClientResponse>& resp);
        tuple<Hash, shared_ptr<ClientResponse>> PopResponse();

        void Setup();
        void EnactRespond();
        void LogMessage(const string& msg);
    };
} // server

#endif //EPOLLCHAT_SERVER_H
