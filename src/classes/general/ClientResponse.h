#ifndef EPOLLCHAT_SERVERREQUEST_H
#define EPOLLCHAT_SERVERREQUEST_H

#include <string>
#include <cstring>
#include <sstream>

#include "./Constants.h"
#include "./Enums.h"

using namespace std;
using namespace src::classes::general;

namespace src::classes::general {
    struct ClientResponse {
    public:
        ClientActionType Type;
        int TargetFD;
        string Data;
        ClientResponse();
        template <typename... Args>
        ClientResponse(ClientActionType type, int fd, Args... args);
        [[nodiscard]] string Serialize() const;
        template<typename... Args>
        static ClientResponse Deserialize(const string&);
    };
} // src

#endif //EPOLLCHAT_SERVERREQUEST_H
