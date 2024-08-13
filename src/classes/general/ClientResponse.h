#ifndef EPOLLCHAT_SERVERREQUEST_H
#define EPOLLCHAT_SERVERREQUEST_H

#include <string>
#include <cstring>
#include <sstream>
#include <iostream>

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
            if (token.empty() || token[0] != DELIMITER_START) {
                cerr << "Error: Invalid start delimiter." << endl;
                return {};
            }

            int typeInt;
            input >> typeInt;
            if (typeInt < 0 || typeInt >= static_cast<int>(ClientActionType::LeaveRoom)) {
                cerr << "Error: Invalid ClientActionType value." << endl;
                return {};
            }
            auto type = static_cast<ClientActionType>(typeInt);

            int fd;
            input >> fd;

            input >> token;
            if (token.empty() || token[0] != DATA_START) {
                cerr << "Error: Invalid data start delimiter." << endl;
                return {};
            }

            string data;
            getline(input, data);

            auto endPos = data.find(DATA_END);
            if (endPos == string::npos) {
                cerr << "Error: Missing data end delimiter." << endl;
                return {};
            }
            data.erase(endPos);  // Remove the data end delimiter

            ClientResponse result(type, fd);
            result.Data = data;

            return result;
        }
    };
} // src

#endif //EPOLLCHAT_SERVERREQUEST_H
