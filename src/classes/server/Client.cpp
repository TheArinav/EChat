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
        WriteBuffer.clear();
        WriteBuffer.resize(0);
        ReadBuffer.clear();
        ReadBuffer.resize(0);
        Owner = nullptr;
        close(FileDescriptor);

        // No need to manually delete the unique_ptr mutexes, they are automatically cleaned up.
    }

    ssize_t Client::Read() {
        std::lock_guard<std::mutex> guard(*ReadMutex);
        ssize_t bytesRead = 0;
        size_t totalBytesRead = 0;
        const int buffer_size = 1024;
        char tmp_r_buff[buffer_size] = {};
        memset(&tmp_r_buff, 0, sizeof tmp_r_buff);
        bytesRead = recv(FileDescriptor, &tmp_r_buff, buffer_size - totalBytesRead, 0);

        // Transfer data to ReadBuffer
        ReadBuffer.clear();
        for (int i = 0; i < buffer_size && tmp_r_buff[i] != '\0'; ++i) {
            ReadBuffer.push_back(tmp_r_buff[i]);
            totalBytesRead++;
        }
        return totalBytesRead;
    }

    ssize_t Client::Write() {
        std::lock_guard<std::mutex> guard(*WriteMutex);
        ssize_t bytesWritten = write(FileDescriptor, WriteBuffer.data(), WriteBuffer.size());
        if (bytesWritten > 0) {
            WriteBuffer.clear();  // Clear buffer after writing
        }
        return bytesWritten;
    }

    void Client::EnqueueResponse(const std::string &s_resp) {
        std::lock_guard<std::mutex> guard(*WriteMutex);
        WriteBuffer.insert(WriteBuffer.end(), s_resp.begin(), s_resp.end());
    }

    void Client::Setup() {
        WriteMutex = std::make_unique<std::mutex>();
        ReadMutex = std::make_unique<std::mutex>();
        OwnerMutex = std::make_unique<std::mutex>();
        ReadBuffer.reserve(BUFFER_SIZE);
        WriteBuffer.reserve(BUFFER_SIZE);
        Owner = nullptr;
        ID = count++;
    }

    void Client::SetOwner(std::shared_ptr<src::classes::server::Account> owner) {
        std::lock_guard<std::mutex> guard(*OwnerMutex);
        this->Owner = std::move(owner);
    }

    Client::Client() = default;
} // namespace server