#ifndef EPOLLCHAT_ACCOUNTINFO_H
#define EPOLLCHAT_ACCOUNTINFO_H

#include <string>
#include <vector>

#include "ChatRoomInfo.h"
#include "ClientInfo.h"
#include "../general/Enums.h"
#include "../general/Constants.h"

using namespace std;
using namespace src::classes::general;

namespace src::classes::client {
    class AccountInfo {
    public:
        Hash ID;
        string Name;
        string Key;
        vector<ChatRoomInfo> Rooms;
        ClientInfo ConnectionInfo;
    };
} // client
#endif //EPOLLCHAT_ACCOUNTINFO_H
