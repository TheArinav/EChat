#include "ServerConnection.h"

using namespace std;

namespace src::front::IO {

    ServerConnection::ServerConnection() {
        Setup();
    }

    ServerConnection::~ServerConnection() {
        Stop();

        if (t_Receiver && t_Receiver->joinable())
            t_Receiver->join();
        delete t_Receiver;

        if (t_Sender && t_Sender->joinable())
            t_Sender->join();
        delete t_Sender;

        if (t_Background && t_Background->joinable())
            t_Background->join();
        delete t_Background;

        if (RoomsInfo) {
            RoomsInfo->clear();
            RoomsInfo->shrink_to_fit();
        }
        if (IngoingResponses) {
            IngoingResponses->clear();
            IngoingResponses->shrink_to_fit();
        }
        if (OutgoingRequests) {
            OutgoingRequests->clear();
            OutgoingRequests->shrink_to_fit();
        }
    }


    ServerConnection::ServerConnection(string addr)
            : HostAddr(move(addr)) {
        Setup();
    }

    void ServerConnection::Start() {
        t_Sender = new thread([this]() {
            while (!f_Stop->load()) {
                auto cur = PopReq();
                if (f_EmptyPop && f_EmptyPop->load() || (cur.Data.empty()))  // Prevent sending if empty
                    continue;

                string serialized = cur.Serialize();
                const char *buf = serialized.c_str();
                ssize_t sent_bytes = send(FDConnection, buf, serialized.size(), 0);

                if (sent_bytes == -1) {
                    perror("send");
                }
            }
        });

        t_Receiver = new thread([this]() {
            while (!f_Stop->load()) {
                char buf[1024] = {0};
                auto st_recv = recv(FDConnection, buf, sizeof(buf) - 1, 0);
                if (st_recv > 0) {
                    buf[st_recv] = '\0';
                    string received_data(buf);

                    // Attempt to deserialize the response
                    auto deserialized = make_shared<ClientResponse>(ClientResponse::Deserialize(received_data));

                    if (f_AwaitStatus && f_AwaitStatus->load() == 0) {
                        PushResp(deserialized, -1);
                    } else if (deserialized->Type == classes::general::ClientActionType::InformSuccess ||
                               deserialized->Type == classes::general::ClientActionType::InformFailure) {
                        PushResp(deserialized, 0);
                        f_AwaitStatus->store(-1);
                    } else {
                        if (!f_Stop || f_Stop->load())
                            return;
                        PushResp(deserialized, 1);
                    }
                } else if (st_recv == 0) {
                    Stop();
                    return;
                } else {
                    perror("recv");
                    Stop();
                    return;
                }
            }
        });

        t_Background = new thread([this](){
            while(!f_Stop->load()){
                if(f_AwaitStatus->load()==0 && isLoggedIn){
                    auto current = PopResp();
                    if(!current || current->Type == classes::general::ClientActionType::NONE)
                        continue;

                    stringstream ss_data{current->Data};
                    if(current->Type == classes::general::ClientActionType::JoinRoom) {
                        if (event_JoinedRoom) {
                            Hash rID;
                            string rName;
                            ss_data >> rID;
                            getline(ss_data,rName);
                            rName=rName.substr(1,rName.size()-2);
                            event_JoinedRoom(rID,rName);
                        }
                    }
                    if(current->Type == classes::general::ClientActionType::LeaveRoom) {
                        if (event_LeftRoom) {
                            Hash rID;
                            string rName;
                            ss_data >> rID;
                            getline(ss_data,rName);
                            rName=rName.substr(1,rName.size()-2);
                            event_LeftRoom(rID,rName);
                        }
                    }
                    if(current->Type == classes::general::ClientActionType::MessageIn) {
                        if (event_GotMessage) {
                            Hash sID;
                            Hash rID;
                            string msg;
                            ss_data >> rID >> sID;
                            getline(ss_data,msg);
                            msg=msg.substr(1,msg.size()-2);
                            event_GotMessage(sID,rID,msg);
                        }
                    }
                }
            }
        });

    }


    void ServerConnection::Stop() {
        f_Stop->store(true);

        if (FDConnection != -1) {
            // Send the termination request first
            ServerRequest terminationRequest(ServerActionType::TerminateConnection, FDConnection, "");
            string s_ter = terminationRequest.Serialize();
            const char *buf_ter = s_ter.c_str();
            ssize_t sent_bytes = send(FDConnection, buf_ter, s_ter.size(), 0);

            if (sent_bytes == -1) {
                perror("send");
            }

            // Gracefully shutdown the socket after sending the termination request
            shutdown(FDConnection, SHUT_RDWR);

            // Clear any remaining data in the socket
            char buf[1024];
            while (recv(FDConnection, buf, sizeof(buf), MSG_DONTWAIT) > 0) {
                // Discard the data
            }

            // Close the socket
            close(FDConnection);
            FDConnection = -1;
        }
    }


    ClientResponse ServerConnection::Request(ServerRequest&& req) {
        PushReq(req);
        auto resp = AwaitResponse(1);
        return *resp;
    }


    void ServerConnection::Setup() {
        f_Stop = make_shared<atomic<bool>>(false);
        f_AwaitStatus = make_shared<atomic<int>>(0);
        f_EmptyPop = make_shared<atomic<bool>>(true);

        m_ConnectionInfo = make_shared<mutex>();
        m_UserInfo = make_shared<mutex>();
        m_IngoingResponses = make_shared<mutex>();
        m_IngoingPopOrder = make_shared<mutex>();
        m_OutgoingRequests = make_shared<mutex>();
        m_RoomsInfo = make_shared<mutex>();

        ConnectionInfo = make_shared<ClientInfo>();
        UserInfo = make_shared<AccountInfo>();
        RoomsInfo = make_shared<vector<ChatRoomInfo>>();
        IngoingResponses = make_shared<vector<shared_ptr<ClientResponse>>>();
        IngoingPopOrder = make_shared<vector<int>>();
        OutgoingRequests = make_shared<vector<ServerRequest>>();

        t_Sender = nullptr;
        t_Receiver = nullptr;
        t_Background = nullptr;

        isLoggedIn = {};
        event_LeftRoom= {};
        event_JoinedRoom = {};
        event_GotMessage = {};

        FDConnection = -1;
        if (HostAddr.empty())
            HostAddr = "localhost";

        AddressInfo hints{}, *res = nullptr, *p = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int stat = getaddrinfo(HostAddr.c_str(), SERVER_PORT, &hints, &res);
        if (stat != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(stat));
            return;
        }

        for (p = res; p; p = p->ai_next) {
            FDConnection = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (FDConnection == -1) {
                perror("socket");
                continue;
            }
            if (connect(FDConnection, p->ai_addr, p->ai_addrlen) == -1) {
                perror("connect");
                close(FDConnection);
                FDConnection = -1;
                continue;
            }
            break;
        }

        if (FDConnection == -1) {
            freeaddrinfo(res);
            return;
        }

        freeaddrinfo(res);

        // Clear any remaining data in the socket
        char buf[1024];
        while (recv(FDConnection, buf, sizeof(buf), MSG_DONTWAIT) > 0) {
            // Discard the data
        }
    }

    shared_ptr<ClientResponse> ServerConnection::AwaitResponse(int type) {
        if (type == 0)
            return {};
        if (type >= 2 || type < 0) {
            return {};
        }
        f_AwaitStatus->store(type);

        while (f_AwaitStatus->load() != -1);

        auto r = PopResp();
        if (!r || r->Type == classes::general::ClientActionType::NONE) {
            return {};
        }
        f_AwaitStatus->store(0);
        return r;
    }

    void ServerConnection::PushResp(shared_ptr<ClientResponse> response, int order) {
        int assignedIndex = -1;
        {
            lock_guard<mutex> responsesGuard(*m_IngoingResponses);

            // Check if response already exists
            auto it = find_if(IngoingResponses->begin(), IngoingResponses->end(),
                              [&](const shared_ptr<ClientResponse>& existingResp) {
                                  return existingResp->Type == response->Type &&
                                         existingResp->Data == response->Data;
                              });

            if (it == IngoingResponses->end()) {
                // Add only if the response does not exist
                IngoingResponses->push_back(response);
                assignedIndex = static_cast<int>(IngoingResponses->size()) - 1;
            } else {
                return; // Do not push if the response is already in the queue
            }
        }
        {
            lock_guard<mutex> orderGuard(*m_IngoingPopOrder);
            if (order < 0)
                order = static_cast<int>(IngoingPopOrder->size()) + order;
            if (order < 0)
                order = 0;
            else if (order > static_cast<int>(IngoingPopOrder->size()))
                order = static_cast<int>(IngoingPopOrder->size());

            IngoingPopOrder->insert(IngoingPopOrder->begin() + order, assignedIndex);
        }
    }

    shared_ptr<ClientResponse> ServerConnection::PopResp() {
        int index;
        f_EmptyPop->store(true);  // Initially set to true

        {
            lock_guard<mutex> orderGuard(*m_IngoingPopOrder);
            if (IngoingPopOrder->empty()) {
                return {};
            }
            index = IngoingPopOrder->front();
            IngoingPopOrder->erase(IngoingPopOrder->begin());
        }

        shared_ptr<ClientResponse> res = {};
        {
            lock_guard<mutex> responsesGuard(*m_IngoingResponses);
            if (index < 0 || index >= IngoingResponses->size()) {
                return {};
            }
            res = IngoingResponses->at(index);
            IngoingResponses->erase(IngoingResponses->begin() + index);
        }

        // Ensure this is correctly being set
        f_EmptyPop->store(false);

        {
            lock_guard<mutex> orderGuard(*m_IngoingPopOrder);
            for (int &cur : *IngoingPopOrder) {
                if (cur >= index) --cur;
            }
        }

        return res;
    }

    shared_ptr<ClientResponse> ServerConnection::PopMessage() {
        f_EmptyPop->store(true);
        lock_guard<mutex> guard(*m_IngoingResponses);
        shared_ptr<ClientResponse> msgResp{};
        int i = 0;
        for (auto& cur : *IngoingResponses) {
            if (cur->Type == classes::general::ClientActionType::MessageIn) {
                msgResp = cur;
                f_EmptyPop->store(false);
                break;
            }
            i++;
        }
        if (!f_EmptyPop->load())
            IngoingResponses->erase(IngoingResponses->begin() + i);
        return msgResp;
    }

    void ServerConnection::PushReq(const ServerRequest& request) {
        if (request.Data.empty()) {
            // Do not push empty requests to the queue
            return;
        }
        lock_guard<mutex> guard(*m_OutgoingRequests);
        OutgoingRequests->push_back(request);
    }


    ServerRequest ServerConnection::PopReq() {
        f_EmptyPop->store(true);
        lock_guard<mutex> guard(*m_OutgoingRequests);
        if (OutgoingRequests->empty())
            return {};  // Return an empty ServerRequest if the queue is empty

        ServerRequest res = OutgoingRequests->front();
        if (res.Data.empty()) {
            OutgoingRequests->erase(OutgoingRequests->begin());  // Ensure to remove it
            return {};  // Return an empty ServerRequest if the data is empty
        }

        OutgoingRequests->erase(OutgoingRequests->begin());
        f_EmptyPop->store(false);
        return res;
    }


} // namespace src::front::IO
