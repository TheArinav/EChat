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
        template<typename... Args>
        ClientResponse(ClientActionType type, int fd, Args... args) :
                Type(type), TargetFD(fd)
        {
            stringstream ss{};
            ((ss << args << " "), ...);
            Data = ss.str();
        }
        [[nodiscard]] string Serialize() const;
        template<typename... Args>
        static ClientResponse Deserialize(const string& inp) {
            stringstream input(inp);
            string token;

            input >> token;
            if (token[0] != DELIMITER_START)
                return {};

            int typeInt;
            input >> typeInt;
            auto type = static_cast<ClientActionType>(typeInt);

            int fd;
            input >> fd;

            input >> token;
            if (token[0] != DATA_START)
                return {};

            string data;
            getline(input, data);

            if (data.find(DATA_END) != string::npos)
                data.erase(data.find(DATA_END));

            ClientResponse result(type, fd);
            result.Data = data;

            return result;
        }
    };
} // src

#endif //EPOLLCHAT_SERVERREQUEST_H
