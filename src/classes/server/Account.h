#ifndef EPOLLCHAT_ACCOUNT_H
#define EPOLLCHAT_ACCOUNT_H

#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include "../general/Constants.h"



using namespace std;
using namespace src::classes::general;

namespace src::classes::server {
    class Client;
    class Account : enable_shared_from_this<Account>{
    public:
        string DisplayName;
        string Key;
        Hash ID;
        shared_ptr<Client> Connection;
        map<Hash,string> Rooms;

        Account();
        explicit Account(string dispName, string key);
        Account(const Account& other);

        void PushRoom(Hash id, string name);
        string RoomForID(Hash idIn);
        vector<Hash> RoomsForName(const string& nameIn);
        void SetConnection(const shared_ptr<Client>& connection);
    private:
        static Hash count;
        shared_ptr<mutex> m_Rooms;
        void Setup();
    };
} // server

#endif //EPOLLCHAT_ACCOUNT_H
