#include "ClientResponse.h"

namespace src::classes::general {
    string ClientResponse::Serialize() const {
        stringstream result{};
        result << DELIMITER_START << " "
               << static_cast<int>(Type) << " "
               << TargetFD << " "
               << DATA_START << " "
               << Data << " "
               << DATA_END << " "
               << DELIMITER_END;
        return result.str();
    }


    ClientResponse::ClientResponse()=default;
} // general