//
// Created by IMOE001 on 7/31/2024.
//

#ifndef EPOLLCHAT_CHATROOMINFO_H
#define EPOLLCHAT_CHATROOMINFO_H

#include "../general/Enums.h"
#include "../general/Constants.h"

#include <string>
#include <vector>
#include <tuple>

using namespace std;
using namespace src::classes::general;

namespace src::classes::client {
    class ChatRoomInfo {
    public:
        Hash ID{};
        string DisplayName;
        vector<tuple<Hash,string>> Messages;
        ChatRoomInfo()=default;
        ChatRoomInfo(Hash id, string dn):ID(id),DisplayName(move(dn)){}
    };

} // client

#endif //EPOLLCHAT_CHATROOMINFO_H
