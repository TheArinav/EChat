#ifndef EPOLLCHAT_TERMINALUSERINTERFACE_H
#define EPOLLCHAT_TERMINALUSERINTERFACE_H

#include <stack>
#include <mutex>
#include <atomic>
#include <memory>
#include <termios.h>

#include "InstructionInterperter.h"
#include "../../classes/server/Server.h"
#include "../../front/IO/ServerConnection.h"
#include "../../classes/client/AccountInfo.h"

using namespace src::classes::server;
using namespace src::front::IO;
namespace src::front::terminal {

    class TerminalUserInterface {
    public:
        static void StartTerminal();
        static void Cleanup();
    private:
        static shared_ptr<stack<Context>> ContextStack;
        static atomic<unsigned long long> curRoomID;
        static atomic<bool> ServerBuilt;
        static shared_ptr<Server> p_Server;
        static shared_ptr<ServerConnection> p_Host;
        static shared_ptr<AccountInfo> p_User;
        static mutex m_Server;
        static mutex m_Host;
        static mutex m_Context;
        static mutex m_User;

        static bool VerifyContext(const Instruction& inst);
        static void PrintLogo();
        static void ClearTerminal();
        static void PushContext(Context c);
        static void PopContext();
        static Context FrontContext();
        static string SerializeContext();
        static void HandleInstruction(const Instruction&);

        static bool AreYouSure(const string& msg);

        static void DisableInputDuringHalt();
        static void RestoreTerminalSettings();

        static termios originalTermios;
        static bool isTermiosSaved;

        static atomic<bool> acceptingInput;
        static mutex inputMutex;
    };

} // terminal

#endif //EPOLLCHAT_TERMINALUSERINTERFACE_H
