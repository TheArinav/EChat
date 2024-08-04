#ifndef EPOLLCHAT_CHATROOM_H
#define EPOLLCHAT_CHATROOM_H

#include <string>
#include <memory>
#include <mutex>
#include <vector>

#include "../general/Constants.h"
#include "Account.h"

using namespace std;
using namespace src::classes::general;

namespace src::classes::server {

    class ChatRoom {
    public :
        Hash ID;
        string DisplayName;
        vector<tuple<Hash,string>> Messages;
        vector<shared_ptr<Account>> Members;
        shared_ptr<Account> Host;

        ChatRoom();
        explicit ChatRoom(string dispName, const shared_ptr<Account>& p_hostPtr);
        void PushMessage(Hash sID, const string& p_msg);
        void PushMember(const shared_ptr<Account>& p_member);
        tuple<Hash,string> GetMessage(int i);
        shared_ptr<Account> GetMember(int i);
        void EraseMember(Hash id);
        bool FindMember(Hash id);
        bool FindMessage(unsigned long i);
    private:
        static Hash count;
        unique_ptr<mutex> m_Messages;
        unique_ptr<mutex> m_Members;
        void Setup();
    };

} // server

#endif //EPOLLCHAT_CHATROOM_H
