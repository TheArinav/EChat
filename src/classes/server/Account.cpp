//
// Created by IMOE001 on 7/31/2024.
//

#include "Account.h"
#include "Client.h"

namespace src::classes::server {
    Hash Account::count =1;
    Account::Account():
    DisplayName("Anonymous"), Key(""){
        Setup();
    }

    Account::Account(string dispName, string key):
    DisplayName(move(dispName)), Key(move(key)){
        Setup();
    }

    void Account::Setup() {
        this->ID=count++;
        this->m_Rooms= make_shared<mutex>();
    }

    void Account::PushRoom(Hash id, string name) {
        {
            lock_guard<mutex> guard(*m_Rooms);
            Rooms.emplace(id,move(name));
        }
    }

    string Account::RoomForID(Hash idIn) {
        {
            lock_guard<mutex> guard(*m_Rooms);
            return Rooms[idIn];
        }
    }

    vector<Hash> Account::RoomsForName(const string& nameIn) {
        {
            lock_guard<mutex> guard(*m_Rooms);
            vector<Hash> matches{};
            for(auto& cur :Rooms)
                if(cur.second==nameIn)
                    matches.push_back(cur.first);
            return matches;
        }
    }

    void Account::SetConnection(const shared_ptr<Client>& connection) {
        connection->SetOwner(shared_from_this());
    }

    Account::Account(const Account &other): Account(other.DisplayName,other.Key) {
        ID = other.ID;
        count--;
        for(const auto& cur: other.Rooms)
            Rooms.emplace(cur.first,cur.second);
        m_Rooms= other.m_Rooms;
        Connection = other.Connection;
    }
} // server