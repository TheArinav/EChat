//
// Created by ariel on 8/5/2024.
//

#include "ServerConnectionTest.h"

using namespace std;

namespace src::Testing {
    shared_ptr<atomic<bool>> ServerConnectionTest::Stop = nullptr;
    thread *ServerConnectionTest::t_Client = nullptr;
    thread *ServerConnectionTest::t_Server = nullptr;
    shared_ptr<Server> ServerConnectionTest::p_Server = nullptr;
    shared_ptr<atomic<bool>> ServerConnectionTest::b_finished = nullptr;

    void ServerConnectionTest::BuildServer() {
        Stop = make_shared<atomic<bool>>(false);
        b_finished = make_shared<atomic<bool>>(false);
        p_Server = make_shared<Server>("TestServer");
        p_Server->Start();

        // Small delay to ensure the server is ready before client attempts to connect
        this_thread::sleep_for(chrono::milliseconds(500));

        b_finished->store(true);
    }

    void ServerConnectionTest::TestClient() {
        addrinfo hints{}, *res = nullptr, *p = nullptr;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int stat = getaddrinfo("localhost", SERVER_PORT, &hints, &res);
        if (stat != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(stat));
            return;
        }

        int FD = -1;
        for (p = res; p; p = p->ai_next) {
            FD = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (FD == -1) {
                perror("socket");
                continue;
            }
            if (connect(FD, p->ai_addr, p->ai_addrlen) == -1) {
                perror("connect");
                close(FD);
                FD = -1;
                continue;
            }
            break;
        }

        if (FD == -1) {
            cerr << "Failed to connect to any address" << endl;
            freeaddrinfo(res);
            return;
        }

        freeaddrinfo(res);

        // Send a test request to the server to simulate activity
        string request_data = ServerRequest(ServerActionType::RegisterAccount,3,"ariel | 1").Serialize();
        if (send(FD, request_data.c_str(), request_data.length(), 0) == -1) {
            cerr << "Error in send(): " << strerror(errno) << endl;
            close(FD);
            return;
        } else {
            cout << "Sent test request to server." << endl;
        }

        // Wait for server response
        char buff[1024];
        ssize_t b_rec = recv(FD, buff, sizeof(buff) - 1, 0);
        if (b_rec < 0) {
            cerr << "Error in recv(): " << strerror(errno) << endl;
            close(FD);  // Close the socket on error
            return;
        } else if (b_rec == 0) {
            // Connection closed by the peer
            cerr << "Connection closed by the server." << endl;
            close(FD);
            return;
        }

        buff[b_rec] = '\0';
        string s_resp(buff);
        auto resp = ClientResponse::Deserialize(s_resp);
        stringstream ss(resp.Data);
        Hash id;
        ss >> id;
        cout << "Assigned id=" << id << endl;

        // Now send the termination request to the server
        string terminate_request_data = "Terminate connection";
        ServerRequest terminate_request(ServerActionType::TerminateConnection, FD, terminate_request_data);
        string serialized_terminate_request = terminate_request.Serialize();

        if (send(FD, serialized_terminate_request.c_str(), serialized_terminate_request.length(), 0) == -1) {
            cerr << "Error in sending terminate request: " << strerror(errno) << endl;
        } else {
            cout << "Sent termination request to server." << endl;
        }

        // Wait for a moment to allow the server to process the termination
        this_thread::sleep_for(chrono::seconds(1));
        close(FD);
    }

    void ServerConnectionTest::Free() {
        Stop->store(true);
        if (t_Server && t_Server->joinable())
            t_Server->join();
        if (t_Client && t_Client->joinable())
            t_Client->join();
        delete t_Server;
        delete t_Client;
    }
} // Testing