#ifndef ECHAT_ENUMS_H
#define ECHAT_ENUMS_H

namespace src::classes::general{
    enum class ClientActionType{
        NONE=0,
        InformSuccess,
        InformFailure,
        MessageIn,
        JoinRoom,
        LeaveRoom
    };
    enum class ServerActionType{
        NONE=0,
        LoginAccount,
        LogoutAccount,
        RegisterAccount,
        CreateRoom,
        GetMessages,
        GetRooms,
        AddMember,
        RemoveMember
    };
}
#endif //ECHAT_ENUMS_H

