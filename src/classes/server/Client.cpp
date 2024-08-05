#include "../general/Constants.h"
#include "Client.h"
#include "Account.h"
#include <unistd.h>
#include <memory>
#include <cstring>

namespace src::classes::server {
    Hash Client::count = 0;

    Client::Client(int fd, sockaddr_storage addr, bool guest)
            : FileDescriptor(fd), Address(addr), IsGuest(guest) {
        Setup();
    }

    Client::~Client() {
        close(FileDescriptor);
    }

    ssize_t Client::Read() {
        std::lock_guard<std::mutex> guard(*ReadMutex);
        ssize_t bytesRead = 0;
        size_t totalBytesRead = 0;
        const int buffer_size = 1024;
        char tmp_r_buff[buffer_size]={};
        memset(&tmp_r_buff,0,sizeof tmp_r_buff);
        bytesRead = recv(FileDescriptor, &tmp_r_buff, BUFFER_SIZE - totalBytesRead, 0);

        //Transfer data to temp buffer
        int i=0;
        ReadBuffer.clear();
        while(tmp_r_buff[i]!='\000' && i< buffer_size) {
            ReadBuffer.push_back(tmp_r_buff[i++]);
            totalBytesRead++;
        }
        return totalBytesRead;
    }


    ssize_t Client::Write() {
        lock_guard<mutex> guard(*WriteMutex);
        ssize_t bytesWritten = write(FileDescriptor, WriteBuffer.data(), WriteBuffer.size());
        if (bytesWritten > 0) {
            WriteBuffer.clear();  // Clear buffer after writing
        }
        return bytesWritten;
    }

    void Client::EnqueueResponse(const string &s_resp) {
        lock_guard<mutex> guard(*WriteMutex);
        WriteBuffer.insert(WriteBuffer.end(), s_resp.begin(), s_resp.end());
    }

    void Client::Setup() {
        WriteMutex = make_unique<mutex>();
        ReadMutex = make_unique<mutex>();
        OwnerMutex = make_unique<mutex>();
        ReadBuffer.reserve(BUFFER_SIZE);
        WriteBuffer.reserve(BUFFER_SIZE);
        Owner = nullptr;
        ID = count++;
    }

    void Client::SetOwner(shared_ptr<src::classes::server::Account> owner) {
        lock_guard<mutex> guard(*OwnerMutex);
        this->Owner = move(owner);
    }

    Client::Client() = default;
} // namespace server
