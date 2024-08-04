#include "Server.h"

#include <utility>
#include <functional>

namespace src::classes::server {
    Server::Server() {
        Setup();
    }

    Server::~Server() {
        Stop();
        if (ServerThread && ServerThread->joinable())
            ServerThread->join();
        delete ServerThread;
    }

    Server::Server(string name) :
            ServerName(move(name)) {
        Setup();
    }

    void Server::Start() {
        Status->store(true);
        ServerThread = new thread([this]() -> void {
            vector<EpollEvent> events(MAX_EVENTS);
            while (Status->load()) {
                int n = epoll_wait(EpollFD, events.data(), MAX_EVENTS, -1);
                if (n == -1) {
                    perror("epoll_wait");
                    exit(EXIT_FAILURE);
                }

                for (int i = 0; i < n; ++i) {
                    if (events[i].data.fd == FileDescriptor) {
                        // Handle new connection
                        sockaddr_storage addr{};
                        socklen_t addr_len = sizeof(addr);
                        int new_fd = accept(FileDescriptor, (sockaddr *) &addr, &addr_len);
                        if (new_fd == -1) {
                            perror("accept");
                            continue;
                        }

                        // Set non-blocking
                        int flags = fcntl(new_fd, F_GETFL, 0);
                        if (flags == -1) {
                            perror("fcntl");
                            close(new_fd);
                            continue;
                        }
                        flags |= O_NONBLOCK;
                        if (fcntl(new_fd, F_SETFL, flags) == -1) {
                            perror("fcntl");
                            close(new_fd);
                            continue;
                        }

                        // Add new client to epoll
                        EpollEvent event;
                        event.data.fd = new_fd;
                        event.events = EPOLLIN | EPOLLET;
                        if (epoll_ctl(EpollFD, EPOLL_CTL_ADD, new_fd, &event) == -1) {
                            perror("epoll_ctl");
                            close(new_fd);
                            continue;
                        }

                        auto client = make_shared<Client>(new_fd, addr, true);
                        Connections.push_back(move(client));
                    } else {
                        // Handle client data
                        for (auto &client: Connections) {
                            if (client->FileDescriptor == events[i].data.fd) {
                                ssize_t bytes_read = client->Read();
                                if (bytes_read <= 0) {
                                    if (bytes_read == 0) {
                                        // Connection closed
                                        close(client->FileDescriptor);
                                    } else {
                                        string buff = string(client->ReadBuffer.begin(), client->ReadBuffer.end());
                                        auto curReq = make_shared<ServerRequest>(ServerRequest::Deserialize(buff));
                                        auto requesterID = client->ID;
                                        PushRequest(requesterID, curReq);
                                    }
                                    continue;
                                }
                            }
                        }
                    }
                }
                EnactRespond();
            }
        });
        ServerThread->detach();
    }

    void Server::Setup() {
        Status = make_shared<atomic<bool>>();
        Status->store(false);
        FileDescriptor = -1;
        ServerThread = nullptr;
        msgCount=0;

        EpollFD = epoll_create1(0);
        if (EpollFD == -1) {
            cerr << "Error in epoll_create1:\n\t" << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }

        AddressInfo hints{}, *server_inf, *p;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int rv = getaddrinfo(nullptr, SERVER_PORT, &hints, &server_inf);
        if (rv) {
            cerr << "Error in getaddrinfo():\n\t" << gai_strerror(rv) << endl;
            exit(EXIT_FAILURE);
        }

        int p_errno = -1;
        for (p = server_inf; p != nullptr; p = p->ai_next) {
            FileDescriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (FileDescriptor == -1) {
                perror("socket()");
                p_errno = errno;
                continue;
            }
            int yes = 1;
            if (setsockopt(FileDescriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                cerr << "Error in setsockopt():\n\t" << strerror(errno) << endl;
                exit(EXIT_FAILURE);
            }
            if (bind(FileDescriptor, p->ai_addr, p->ai_addrlen) == -1) {
                perror("bind()");
                p_errno = errno;
                continue;
            }
            break;
        }
        freeaddrinfo(server_inf);

        if (!p) {
            cerr << "Bind failure, cause:\n\t" << strerror(p_errno) << endl;
            exit(EXIT_FAILURE);
        }

        if (listen(FileDescriptor, 10) == -1) {
            cerr << "Listen failure, cause:\n\t" << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }

        int flags = fcntl(FileDescriptor, F_GETFL, 0);
        if (flags == -1) {
            cerr << "Error in fcntl:\n\t" << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }
        flags |= O_NONBLOCK;
        if (fcntl(FileDescriptor, F_SETFL, flags) == -1) {
            cerr << "Error in fcntl:\n\t" << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }

        EpollEvent event{};
        event.data.fd = FileDescriptor;
        event.events = EPOLLIN;
        if (epoll_ctl(EpollFD, EPOLL_CTL_ADD, FileDescriptor, &event) == -1) {
            cerr << "Error in epoll_ctl:\n\t" << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }
    }

    void Server::EnactRespond() {
        function<bool()> isRequestsEmpty = [this]() -> bool {
            {
                lock_guard<mutex> guard(*m_Requests);
                return Requests.empty();
            }
        };
        while (!isRequestsEmpty()) {
            //region Setup
            auto current = PopRequest();
            Hash ConnectionID = get<0>(current);
            shared_ptr<Account> requester = nullptr;
            bool isGuest = false;
            shared_ptr<Client> connection = nullptr;
            function<unsigned long()> ConnectionsSize = [this]() -> unsigned long {
                lock_guard<mutex> guard(*m_Connections);
                return Connections.size();
            };
            function<unsigned long()> AccountSize = [this]() -> unsigned long {
                lock_guard<mutex> guard(*m_Accounts);
                return Accounts.size();
            };

            {
                bool f_break = false;
                for (unsigned long i = 0; i < ConnectionsSize() && !f_break; i++) {
                    if ((connection = GetConnection(i))) {
                        {
                            lock_guard<mutex> guard(*m_Connections);
                            if (connection->ID == get<0>(current))
                                f_break = true;
                            isGuest = connection->IsGuest;
                        }
                    }
                }
            }

            if (!isGuest)
                requester = connection->Owner;

            shared_ptr<ServerRequest> request = get<1>(current);
            //endregion

            if (request == nullptr)
                continue;

            //region Enact/Respond:
            stringstream ss_response{};
            stringstream ss_log{};
            stringstream ss_data(request->Data);
            ClientActionType responseType;
            function<bool(shared_ptr<Account>, Hash, string)> verifyIdentity = [this](
                    const shared_ptr<Account> &accIn,
                    Hash _id, const string &_key) -> bool {
                {
                    lock_guard<mutex> guard(*m_Accounts);
                    return accIn->ID == _id && accIn->Key == _key;
                }
            };
            //region Enact
            switch (request->Type) {
                case ServerActionType::NONE: {
                    cerr << "Server has attempted to enact a NULL request";
                    continue;
                }
                case ServerActionType::LoginAccount: {
                    if (!isGuest) { //Check if already logged in
                        ss_response << "'Nothing to do, you are already logged in. To switch accounts, "
                                       "have to logout first.'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") has requested to re-login. Request denied; Aborted.";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //format: '{id} [key]'
                    //region Unpack data
                    Hash id;
                    string key;
                    ss_data >> id;
                    getline(ss_data, key);
                    //endregion

                    shared_ptr<Account> targetAccount = nullptr;
                    bool f_break = false;

                    for (unsigned long i = 0; i < AccountSize() && !f_break; i++)
                        if (verifyIdentity((targetAccount = GetAccount(i)), id, key))
                            f_break = true;
                    if (!f_break) { //Account was not found
                        ss_response << "'Login failed, provided credentials were found to be invalid'";
                        ss_log << "Guest has requested to login into an account with id (#"
                               << id
                               << ") using key '"
                               << key
                               << "'. The provided credentials did not match internal record. Request Denied; Aborted.";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //region Move the connection to the target account:
                    targetAccount->Connection = connection;
                    connection->SetOwner(make_shared<Account>(*targetAccount));
                    //endregion

                    ss_response << ServerName << " "
                                << "You were successfully logged in";
                    ss_log << "User ("
                           << targetAccount->DisplayName
                           << "#"
                           << targetAccount->ID
                           << ") Has requested to login. Request Approved; User has logged in.";
                    responseType = general::ClientActionType::InformSuccess;

                    break;
                }
                case ServerActionType::LogoutAccount: {
                    if (isGuest) { //Check if guest
                        ss_response << "Nothing to do; a guest cannot logout.";
                        ss_log << "Guest with connection ID '#"
                               << connection->ID
                               << "' has requested to logout. Request Denied; Aborted.";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //format {id} [key]
                    //region Unpack data
                    Hash id;
                    string key;
                    ss_data >> id;
                    getline(ss_data, key);
                    //endregion

                    if (!verifyIdentity(requester, id, key)) {
                        ss_response << "'Logout failed, provided credentials were found to be invalid'";
                        ss_log << "Guest has requested to logout from an account with id (#"
                               << id
                               << ") using key '"
                               << key
                               << "'. The provided credentials did not match internal record. Request Denied; Aborted.";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //region Disconnect client from the account
                    requester->Connection = nullptr;
                    connection->SetOwner(nullptr);
                    //endregion

                    ss_response << "'You were successfully logged out of the server.'";
                    ss_log << "User ("
                           << requester->DisplayName
                           << "#"
                           << requester->ID
                           << ") has requested to logout. Request Approved; Client was logged out.";
                    responseType = general::ClientActionType::InformSuccess;
                    break;
                }
                case ServerActionType::RegisterAccount: {
                    if (!isGuest) {
                        ss_response << "'To register a new account, you must first logout of the current one'";
                        ss_log << "Logged-in user ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") has requested to register a new account. Request denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //format [name] | [key]
                    //region Unpack data
                    string name, key;
                    {   //Make sure temporary vars go out of scope asap.
                        string::size_type separator = request->Data.find('|');
                        name = request->Data.substr(0, separator);
                        key = request->Data.substr(separator + 1);
                    }
                    //endregion

                    auto newCl = make_shared<Account>(name, key);

                    PushAccount(newCl);
                    shared_ptr<Account> target = GetAccount(-1);
                    auto ID = target->ID;
                    ss_response << ID
                                << " 'Account was successfully registered'";
                    ss_log << "Guest had requested to register a new account. Request Approved, "
                              "New account registered: ("
                           << target->DisplayName
                           << "#"
                           << ID
                           << ")";
                    responseType = general::ClientActionType::InformSuccess;
                    break;
                }
                case ServerActionType::CreateRoom: {
                    if (isGuest) {
                        ss_response << "'You must be logged in in order to create a new chatroom on the server'";
                        ss_log << "Guest with ID (#"
                               << connection->ID
                               << ") Had requested the creation of a new chatroom. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //format {id} [key] | [name]
                    //region Unpack data
                    Hash cID;
                    string name;
                    string cKey;
                    ss_data >> cID;
                    { //Make sure temporary vars go out of scope asap.
                        string reminder;
                        getline(ss_data, reminder);
                        string::size_type separator = reminder.find('|');
                        name = reminder.substr(0, separator);
                        cKey = reminder.substr(separator + 1);
                    }
                    //endregion

                    if (!verifyIdentity(requester, cID, cKey)) {
                        ss_response << "'Authentication failed, your provided credentials did not match the internal"
                                       "records. Aborted'";
                        ss_log << "Warning: Authentication failure => requester("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ":"
                               << requester->Key
                               << ") Requested access with credentials {id="
                               << cID
                               << " ,Key = "
                               << cKey
                               << "}. Request details {type='Create chatroom', RoomName = '"
                               << name
                               << "'}. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }


                    shared_ptr<ChatRoom> target;
                    {
                        auto newRoom = make_shared<ChatRoom>(name, requester);
                        PushRoom(newRoom);
                        target = GetRoom(-1);
                    }

                    ss_response << target->DisplayName
                                << "#"
                                << target->ID
                                << " "
                                << "'The chatroom was created successfully.'";
                    ss_log << "User ("
                           << requester->DisplayName
                           << "#"
                           << requester->ID
                           << ") Had requested the creation of a new chatroom. Request Approved, chatroom ["
                           << target->DisplayName
                           << "#"
                           << target->ID
                           << "] Was created.";
                    responseType = general::ClientActionType::InformSuccess;
                    break;
                }
                case ServerActionType::AddMember: {
                    if (isGuest) {
                        ss_response << "'You must be logged-in to a Host user of a chatroom in order to add"
                                       "a new member. You are currently NOT logged-in to ANY account. Aborted";
                        ss_log << "Guest user has requested to add a new member to a chatroom. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //format {reqID} {roomID} {memID} [reqKey]
                    //region Unpack data
                    Hash reqID, roomID, memID;
                    string reqKey;
                    ss_data >> reqID >> roomID >> memID;
                    getline(ss_data, reqKey);
                    //endregion

                    if (!verifyIdentity(requester, reqID, reqKey)) {
                        ss_response << "'Authentication failed, your provided credentials did not match the internal"
                                       "records. Aborted'";
                        ss_log << "Warning: Authentication failure => requester("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ":"
                               << requester->Key
                               << ") Requested access with credentials {id=" << reqID
                               << " ,Key = "
                               << reqKey
                               << "}. Request details {type='Add member to room', RoomID = '"
                               << roomID
                               << "MemberAccountID= "
                               << memID
                               << "'}. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    long long roomIndex;

                    if ((roomIndex = FindRoom(roomID)) == -1) {
                        ss_response << "'Referred chatroom was not found. Aborted'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to add a new member to a non-existent chatroom. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    auto targetRoom = GetRoom(roomIndex);
                    if (targetRoom->Host->ID != reqID) {
                        ss_response << "'You must be the host of a chatroom to add a new member to it. Aborted'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to add a new member to a room they are not the host of. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    shared_ptr<Account> targetAccount = nullptr;
                    { // Find user account to add as member
                        long long accInd;
                        if ((accInd = FindAccount(memID)) != -1)
                            targetAccount = GetAccount(accInd);
                    }
                    if (targetAccount == nullptr) {
                        ss_response
                                << "'The client ID you provided was invalid. Failed to add new member to chatroom. Aborted";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to add a non-existent account as a member of a chatroom. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }
                    targetRoom->PushMember(targetAccount);
                    ss_response << "'Member was successfully added to the chatroom'";
                    ss_log << "User ("
                           << requester->DisplayName
                           << "#"
                           << requester->ID
                           << ") had requested to add a new member to a chatroom. Request Approved; A new member ("
                           << targetAccount->DisplayName
                           << "#"
                           << targetAccount->ID
                           << ") Was added to room ["
                           << targetRoom->DisplayName
                           << "#"
                           << targetRoom->ID
                           << "]";
                    responseType = general::ClientActionType::InformSuccess;
                    break;
                }
                case ServerActionType::RemoveMember: {
                    if (isGuest) {
                        ss_response << "'You must be logged-in to a Host user of a chatroom or the user themselves"
                                       " in order to remove a chatroom member. You are currently NOT logged-in to ANY"
                                       " account. Aborted";
                        ss_log << "Guest user has requested to remove a member from a chatroom. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //format {reqID} {roomID} {memID} [reqKey]
                    //region Unpack data
                    Hash reqID, roomID, memID;
                    string reqKey;
                    ss_data >> reqID >> roomID >> memID;
                    getline(ss_data, reqKey);
                    //endregion

                    if (!verifyIdentity(requester, reqID, reqKey)) {
                        ss_response << "'Authentication failed, your provided credentials did not match the internal"
                                       "records. Aborted'";
                        ss_log << "Warning: Authentication failure => requester("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ":"
                               << requester->Key
                               << ") Requested access with credentials {id=" << reqID
                               << " ,Key = "
                               << reqKey
                               << "}. Request details {type='Remove member from room', RoomID = '"
                               << roomID
                               << "MemberAccountID= "
                               << memID
                               << "'}. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    long long roomIndex;

                    if ((roomIndex = FindRoom(roomID)) == -1) {
                        ss_response << "'Referred chatroom was not found. Aborted'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to remove a member from a non-existent chatroom. Request Denied;"
                                  " Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    auto targetRoom = GetRoom(roomIndex);

                    if(!targetRoom->FindMember(memID)){
                        ss_response << "'Failed to find member with provided ID in the chatroom. Aborted'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to kick a user that isn't a member of the room they referred to. "
                                  "Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    if (targetRoom->Host->ID != reqID && memID != requester->ID) {
                        ss_response << "'You must be the host of a chatroom or the member themselves to kick them from"
                                       " the room. Aborted'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to kick a member from a room they are not the host of or the member "
                                  "themselves. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    shared_ptr<Account> targetAccount = nullptr;
                    { // Find user account to add as member
                        long long accInd;
                        if ((accInd = FindAccount(memID)) != -1)
                            targetAccount = GetAccount(accInd);
                    }
                    if (targetAccount == nullptr) {
                        ss_response
                                << "'The client ID you provided was invalid. Failed to kick member from the chatroom."
                                   " Aborted";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to kick a non-existent account from a chatroom. Request Denied;"
                                  " Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }
                    targetRoom->EraseMember(targetAccount->ID);
                    ss_response << "'Member was successfully removed from the chatroom'";
                    ss_log << "User ("
                           << requester->DisplayName
                           << "#"
                           << requester->ID
                           << ") had requested to add a remove a member from a chatroom. Request Approved; Member ("
                           << targetAccount->DisplayName
                           << "#"
                           << targetAccount->ID
                           << ") Was removed from room ["
                           << targetRoom->DisplayName
                           << "#"
                           << targetRoom->ID
                           << "]";
                    responseType = general::ClientActionType::InformSuccess;
                    break;
                }
                case ServerActionType::SendMessage:
                    if (isGuest) {
                        ss_response << "'You must be logged-in to a member user of a chatroom in order to send a message."
                                       " You are currently NOT logged-in to ANY account. Aborted";
                        ss_log << "Guest user has requested to send a message in a chatroom. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    //format {cID} {rID} [cKey] | [msg]
                    //region Unpack data
                    Hash cID, rID;
                    string cKey,msg;
                    ss_data >> cID >> rID;
                    {
                        string reminder;
                        getline(ss_data, reminder);
                        string::size_type separator = reminder.find('|');
                        cKey = reminder.substr(0, separator);
                        msg = reminder.substr(separator + 1);
                    }
                    //endregion
                    if (!verifyIdentity(requester, cID, cKey)) {
                        ss_response << "'Authentication failed, your provided credentials did not match the internal"
                                       "records. Aborted'";
                        ss_log << "Warning: Authentication failure => requester("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ":"
                               << requester->Key
                               << ") Requested access with credentials {id=" << cID
                               << " ,Key = "
                               << cKey
                               << "}. Request details {type='Send message in room', RoomID = '"
                               << rID
                               << "Message= "
                               << msg
                               << "'}. Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }
                    long long roomIndex;

                    if ((roomIndex = FindRoom(rID)) == -1) {
                        ss_response << "'Referred chatroom was not found. Aborted'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to send a message in a non-existent chatroom. Request Denied;"
                                  " Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }

                    auto targetRoom = GetRoom(roomIndex);

                    if(!targetRoom->FindMember(cID)){
                        ss_response << "'You must be a member of a chatroom to send a message in it. Aborted'";
                        ss_log << "User ("
                               << requester->DisplayName
                               << "#"
                               << requester->ID
                               << ") had requested to send a message in a room they are not a member of. "
                                  "Request Denied; Aborted";
                        responseType = general::ClientActionType::InformFailure;
                        goto Respond;
                    }
                    msgCount.store(msgCount.load()+1);
                    EmplaceMessage(msgCount.load(),tuple<Hash,Hash,string>(rID,Hash(cID),string(msg)));
                    targetRoom->PushMessage(cID,string(msg));

                    ss_response << msgCount.load()
                                << "'Message sent'";
                    ss_log << "User ("
                           << requester->DisplayName
                           << "#"
                           << requester->ID
                           << ") had requested to send a message in a chatroom. Request Approved; Message {"
                           << msgCount.load()
                           << ">"
                           << msg
                           << ") Was sent in room ["
                           << targetRoom->DisplayName
                           << "#"
                           << targetRoom->ID
                           << "]";
                    responseType = general::ClientActionType::InformSuccess;
                    break;
            }
            //endregion
            Respond:
            {
                LogMessage(ss_log.str());
                auto s_resp = ClientResponse(responseType, connection->FileDescriptor, ss_response.str()).Serialize();
                connection->EnqueueResponse(s_resp);
            }
            //endregion
        }
    }

    void Server::Stop() {
        Status->store(false);
    }

    void Server::LogMessage(const string &msg) {
        stringstream ss;
        string s_time;
        {
            char fmt[64];
            char buf[64];
            struct timeval tv{};
            struct tm *tm;

            gettimeofday(&tv, nullptr);
            tm = localtime(&tv.tv_sec);
            strftime(fmt, sizeof(fmt), "%H:%M:%S:%%06u", tm);
            snprintf(buf, sizeof(buf), fmt, tv.tv_usec);
            s_time = string(buf);
        }
        ss << "[LOG(" << ServerLog.size() + 1 << ")]:" << "[" << s_time << "]=" << msg;
        ServerLog.push_back(ss.str());
    }

    void Server::PushConnection(shared_ptr<Client> client) {
        {
            lock_guard<mutex> guard(*m_Connections);
            Connections.push_back(client);
        }
    }

    shared_ptr<Client> Server::GetConnection(long long i) {
        {
            lock_guard<mutex> guard(*m_Connections);
            if (i < 0)
                i = (long) Connections.size() - i;
            return Connections[i];
        }
    }

    void Server::PushAccount(const shared_ptr<Account> &account) {
        {
            lock_guard<mutex> guard(*m_Accounts);
            Accounts.push_back(account);
        }
    }

    shared_ptr<Account> Server::GetAccount(long long i) {
        {
            lock_guard<mutex> guard(*m_Accounts);
            if (i < 0)
                i = (long) Accounts.size() - i;
            return Accounts[i];
        }
    }

    void Server::PushRoom(const shared_ptr<ChatRoom> &room) {
        {
            lock_guard<mutex> guard(*m_Rooms);
            Rooms.push_back(room);
        }
    }

    shared_ptr<ChatRoom> Server::GetRoom(long long i) {
        {
            lock_guard<mutex> guard(*m_Rooms);
            if (i < 0)
                i = (long) Rooms.size() - i;
            return Rooms[i];
        }
    }

    void Server::PushLog(string msg) {
        {
            lock_guard<mutex> guard(*m_Log);
            ServerLog.push_back(move(msg));
        }
    }

    string Server::GetLog(unsigned long i) {
        {
            lock_guard<mutex> guard(*m_Log);
            if (i < 0)
                i = ServerLog.size() - i;
            return ServerLog[i];
        }
    }

    void Server::EmplaceMessage(Hash h, tuple<Hash, Hash, string> cont) {
        {
            lock_guard<mutex> guard(*m_Messages);
            Messages.emplace(h, move(cont));
        }
    }

    tuple<Hash, Hash, string> Server::KGetMessage(Hash id) {
        {
            lock_guard<mutex> guard(*m_Messages);
            return Messages[id];
        }
    }

    vector<tuple<Hash, Hash, string>> Server::V1GetMessage(Hash room) {
        {
            lock_guard<mutex> guard(*m_Messages);
            vector<tuple<Hash, Hash, string>> res;
            for (auto &cur: Messages)
                if (get<0>(cur.second) == room)
                    res.push_back(cur.second);
            return res;
        }
    }

    vector<tuple<Hash, Hash, string>> Server::V2GetMessage(Hash sender) {
        {
            lock_guard<mutex> guard(*m_Messages);
            vector<tuple<Hash, Hash, string>> res;
            for (auto &cur: Messages)
                if (get<1>(cur.second) == sender)
                    res.push_back(cur.second);
            return res;
        }
    }

    vector<tuple<Hash, Hash, string>> Server::V3GetMessage(string content) {
        {
            lock_guard<mutex> guard(*m_Messages);
            vector<tuple<Hash, Hash, string>> res;
            for (auto &cur: Messages)
                if (get<2>(cur.second) == content)
                    res.push_back(cur.second);
            return res;
        }
    }

    void Server::PushRequest(Hash id, const shared_ptr<ServerRequest> &req) {
        {
            lock_guard<mutex> guard(*m_Requests);
            Requests.emplace(id, req);
        }
    }

    tuple<Hash, shared_ptr<ServerRequest>> Server::PopRequest() {
        {
            lock_guard<mutex> guard(*m_Requests);
            auto tmp = Requests.front();
            Requests.pop();
            return tmp;
        }
    }

    void Server::PushResponse(Hash id, const shared_ptr<ClientResponse> &resp) {
        {
            lock_guard<mutex> guard(*m_Responses);
            Responses.emplace(id, resp);
        }
    }

    tuple<Hash, shared_ptr<ClientResponse>> Server::PopResponse() {
        {
            lock_guard<mutex> guard(*m_Responses);
            auto tmp = Responses.front();
            Responses.pop();
            return tmp;
        }
    }

    long long Server::FindRoom(Hash id) {
        function<unsigned long()> roomsSize = [this]() -> unsigned long {
            {
                lock_guard<mutex> guard(*m_Rooms);
                return Rooms.size();
            }
        };
        for (unsigned long i = 0; i < roomsSize(); i++)
            if (GetRoom((long long) i)->ID == id)
                return (long long) i;
        return -1;
    }

    long long Server::FindAccount(Hash id) {
        function<unsigned long()> accountsSize = [this]() -> unsigned long {
            {
                lock_guard<mutex> guard(*m_Accounts);
                return Accounts.size();
            }
        };
        for (unsigned long i = 0; i < accountsSize(); i++)
            if (GetAccount((long long) i)->ID == id)
                return (long long) i;
        return -1;
    }
} // server

namespace src::classes::general {
    template<typename... Args>
    ServerRequest::ServerRequest(ServerActionType type, int fd, Args... args):
            Type(type), TargetFD(fd) {
        stringstream ss{};
        ((ss << args << " "), ...);
        Data = ss.str();
    }

    string ServerRequest::Serialize() const {
        stringstream result{};
        result << DELIMITER_START << " "
               << static_cast<int>(TargetFD) << " "
               << TargetFD << " "
               << DATA_START << " "
               << Data << " "
               << DATA_END << " "
               << DELIMITER_END;
        return result.str();
    }

    template<typename... Args>
    ServerRequest ServerRequest::Deserialize(const string &inp) {
        stringstream input(inp);
        string token;

        input >> token;
        if (token[0] != DELIMITER_START)
            return {};

        int typeInt;
        input >> typeInt;
        auto type = static_cast<ServerActionType>(typeInt);

        int fd;
        input >> fd;

        input >> token;
        if (token[0] != DATA_START)
            return {};

        string data;
        getline(input, data, ' ');

        if (data.find(DATA_END) != string::npos)
            data.erase(data.find(DATA_END));

        ServerRequest result(type, fd);
        result.Data = data;

        return result;
    }

    ServerRequest::ServerRequest() = default;

}