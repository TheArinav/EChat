#include "ClientResponse.h"

namespace src::classes::general {
    //template<typename... Args>
//    ClientResponse::ClientResponse(ClientActionType type, int fd, Args... args):
//            Type(type),TargetFD(fd)
//    {
//        stringstream ss{};
//        ((ss << args << " "), ...);
//        Data = ss.str();
//    }

    string ClientResponse::Serialize() const {
        stringstream result{};
        result << DELIMITER_START << " "
               << static_cast<int>(TargetFD) << " "
               << TargetFD << " "
               << DATA_START << " "
               << Data << " "
               << DATA_END << " "
               << DELIMITER_END;
        return result.str();
    }

//    template<typename... Args>
//    ClientResponse ClientResponse::Deserialize(const string& inp) {
//        stringstream input(inp);
//        string token;
//
//        input >> token;
//        if (token[0]!=DELIMITER_START)
//            return {};
//
//        int typeInt;
//        input >> typeInt;
//        auto type = static_cast<ClientActionType>(typeInt);
//
//        int fd;
//        input >> fd;
//
//        input >> token;
//        if(token[0]!=DATA_START)
//            return {};
//
//        string data;
//        getline(input, data, ' ');
//
//        if (data.find(DATA_END) != string::npos)
//            data.erase(data.find(DATA_END));
//
//        ClientResponse result(type, fd);
//        result.Data = data;
//
//        return result;
//    }

    ClientResponse::ClientResponse()=default;
} // general