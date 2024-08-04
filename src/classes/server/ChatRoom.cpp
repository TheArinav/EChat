#include "ChatRoom.h"
#include "Client.h"
#include "../general/ClientResponse.h"

using namespace src::classes::general;

namespace src::classes::server {
    Hash ChatRoom::count=1;
    ChatRoom::ChatRoom():
    DisplayName("UnnamedRoom"),Host(nullptr){
        Setup();
    }

    ChatRoom::ChatRoom(string dispName,const shared_ptr<Account>& p_hostPtr):
    DisplayName(move(dispName)),Host(p_hostPtr){
        Setup();
    }


    void ChatRoom::PushMember(const shared_ptr<Account>& p_member) {
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

    tuple<Hash,string> ChatRoom::GetMessage(int i) {
        {
            lock_guard<mutex> guard(*m_Messages);
            return Messages[i];
        }
    }

    shared_ptr<Account> ChatRoom::GetMember(int i) {
        {
            lock_guard<mutex> guard(*m_Members);
            return Members[i];
        }
    }

    bool ChatRoom::FindMember(Hash id) {
        {
            lock_guard<mutex> guard(*m_Members);
            for(auto& cur : Members)
                if(cur->ID==id)
                    return true;
        }
        return false;
    }

    bool ChatRoom::FindMessage(unsigned long i) {
        {
            lock_guard<mutex> guard(*m_Messages);
            if (i>Messages.size())
                return false;
        }
        return true;
    }

    void ChatRoom::EraseMember(Hash id) {
        {
            int index =0;
            lock_guard<mutex> guard(*m_Members);
            for(auto& cur: Members)
            {
                if (cur->ID==id)
                    break;
                else
                    index++;
            }
            if(index==Members.size())
                return;
            Members.erase(Members.begin() + index);
        }
    }

    void ChatRoom::PushMessage(Hash sID, const string &p_msg) {
        {
            lock_guard<mutex> guard(*m_Messages);
            Messages.emplace_back(sID,p_msg);
        }
        {
            lock_guard<mutex> guard(*m_Members);
            for(auto& cur : Members)
                if(cur->ID!=sID)
                    cur->Connection->EnqueueResponse(ClientResponse(
                            ClientActionType::MessageIn,
                            cur->Connection->FileDescriptor,
                            p_msg).Serialize());
        }
    }
} // server