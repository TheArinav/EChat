#include "ChatRoom.h"

namespace src::classes::server {
    Hash ChatRoom::count=1;
    ChatRoom::ChatRoom():
    DisplayName("UnnamedRoom"),Host(nullptr){
        Setup();
    }

    ChatRoom::ChatRoom(string dispName, shared_ptr<Account> *p_hostPtr):
    DisplayName(move(dispName)),Host(p_hostPtr){
        Setup();
    }

    void ChatRoom::PushMessage(string *p_msg) {
        {
            lock_guard<mutex> guard(*m_Messages);
            Messages.push_back(p_msg);
        }
    }

    void ChatRoom::PushMember(shared_ptr<Account> *p_member) {
        {
            lock_guard<mutex> guard(*m_Members);
            Members.push_back(p_member);
        }
    }

    void ChatRoom::Setup() {
        this->ID=count++;
        this->m_Members= make_unique<mutex>();
        this->m_Messages= make_unique<mutex>();
    }

    string *ChatRoom::GetMessage(int i) {
        {
            lock_guard<mutex> guard(*m_Messages);
            return Messages[i];
        }
    }

    shared_ptr<Account> *ChatRoom::GetMember(int i) {
        {
            lock_guard<mutex> guard(*m_Members);
            return Members[i];
        }
    }
} // server