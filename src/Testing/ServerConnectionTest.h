//
// Created by ariel on 8/5/2024.
//

#ifndef EPOLLCHAT_SERVERCONNECTIONTEST_H
#define EPOLLCHAT_SERVERCONNECTIONTEST_H

#include <memory>

#include "../classes/server/Server.h"

namespace src::Testing {

        class ServerConnectionTest {
        public:
            static shared_ptr<Server> p_Server;
            static thread* t_Server;
            static thread* t_Client;
            static shared_ptr<atomic<bool>> Stop;
            static shared_ptr<atomic<bool>> b_finished;
            static void BuildServer();
            static void TestClient();
            static void Free();
        };

    } // Testing

#endif //EPOLLCHAT_SERVERCONNECTIONTEST_H
