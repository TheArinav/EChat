#ifndef EPOLLCHAT_SERVERCONNECTION_H
#define EPOLLCHAT_SERVERCONNECTION_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>

#include "../../classes/client/AccountInfo.h"
#include "../../classes/client/ClientInfo.h"
#include "../../classes/client/ChatRoomInfo.h"
#include "../../classes/general/ClientResponse.h"
#include "../../classes/general/Constants.h"
#include "../../classes/general/Enums.h"
#include "../../classes/server/Server.h"

using namespace std;
using namespace src::classes::client;

namespace src::front::IO {

    class ServerConnection {
    public:
        string HostAddr;
        string DisplayName;
        int FDConnection;

        ServerConnection();
        ~ServerConnection();
        explicit ServerConnection(string addr);

        void Start(); // Starts connection ASYNC
        void Stop(); // Sends stop signal through the atomic flag
        ClientResponse Request(ServerRequest&& req);

        function<bool()> isLoggedIn;
        function<void(Hash,string)> event_JoinedRoom;
        function<void(Hash,string)> event_LeftRoom;
        function<void(Hash, Hash,string)> event_GotMessage;

        thread* t_Sender;
        thread* t_Receiver;
        thread* t_Background;
        shared_ptr<atomic<bool>> f_Stop;
        shared_ptr<atomic<int>> f_AwaitStatus;
        shared_ptr<atomic<bool>> f_EmptyPop;

        shared_ptr<ClientInfo> ConnectionInfo;
        shared_ptr<AccountInfo> UserInfo;
        shared_ptr<vector<ChatRoomInfo>> RoomsInfo;
        shared_ptr<vector<shared_ptr<ClientResponse>>> IngoingResponses;
        shared_ptr<vector<int>> IngoingPopOrder;
        shared_ptr<vector<ServerRequest>> OutgoingRequests;

        shared_ptr<mutex> m_ConnectionInfo;
        shared_ptr<mutex> m_UserInfo;
        shared_ptr<mutex> m_RoomsInfo;
        shared_ptr<mutex> m_IngoingResponses;
        shared_ptr<mutex> m_IngoingPopOrder;
        shared_ptr<mutex> m_OutgoingRequests;

        void Setup();
        shared_ptr<ClientResponse> AwaitResponse(int type);

        void PushResp(shared_ptr<ClientResponse> response, int order);
        shared_ptr<ClientResponse> PopResp();
        shared_ptr<ClientResponse> PopMessage();
        void PushReq(const ServerRequest& request);
        ServerRequest PopReq();
    };

} // IO

#endif //EPOLLCHAT_SERVERCONNECTION_H
