#include "TerminalUserInterface.h"
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>

namespace src::front::terminal {

    shared_ptr<stack<Context>> TerminalUserInterface::ContextStack = nullptr;
    atomic<unsigned long long> TerminalUserInterface::curRoomID = {};
    atomic<bool> TerminalUserInterface::ServerBuilt = {};
    shared_ptr<Server> TerminalUserInterface::p_Server = {};
    shared_ptr<ServerConnection> TerminalUserInterface::p_Host = {};
    shared_ptr<AccountInfo> TerminalUserInterface::p_User = {};
    mutex TerminalUserInterface::m_Server = {};
    mutex TerminalUserInterface::m_Host = {};
    mutex TerminalUserInterface::m_Context = {};
    mutex TerminalUserInterface::m_User = {};

    // Static variable initialization
    termios TerminalUserInterface::originalTermios;
    bool TerminalUserInterface::isTermiosSaved = false;

    void TerminalUserInterface::StartTerminal() {
        ClearTerminal();
        PrintLogo();
        InstructionInterpreter::Setup();
        ContextStack = make_shared<stack<Context>>();
        ContextStack->push(Context::NONE);

        string input;
        while (true) {
            cout << "  '---[Enter Instruction]---> ";
            getline(cin, input);

            if (input == "exit" || input == "q") break;
            else if (input == "cls" || input == "clear") {
                ClearTerminal();
                PrintLogo();
                continue;
            }

            try {
                auto inst = InstructionInterpreter::Parse(input);
                HandleInstruction(inst);
            } catch (const exception &e) {
                cerr << "Error: " << e.what() << "\n";
            }
        }
    }

    bool TerminalUserInterface::VerifyContext(const Instruction &inst) {
        auto &&req = inst.Type->ValidContext;
        auto &&cur = FrontContext();

        if (req == Context::ANY)
            return true;
        if (cur == req)
            return true;
        if (req == Context::CLIENT_LOGGED_IN && cur == Context::CLIENT_LOGGED_IN_ROOM)
            return true;
        if (req == Context::CLIENT_LOGGED_IN_NO_ROOM && cur == Context::CLIENT_LOGGED_IN)
            return true;

        return false;
    }

    void TerminalUserInterface::PrintLogo() {
        cout << R"(
   _______________________________________________________________
  / _____________________________________________________________ \
 | |     ._______.      _____  ._.             ._.      ._.      | |
 | |     | ______|     / ___ \ | |             | |      | |      | |
 | |     | |____.     | /   \/ | |__.   .___.  | |__.   | |      | |
 | |     |  ____| === | |      | '_. \ /  ^  \ |  __|   | |      | |
 | |     | |_____.    | \___/\ | | | | | (_) | |  |__.  '-'      | |
 | |     |_______|     \_____/ |_| |_| \___^._\ \____|  [=]      | |
 | |_____________________________________________________________| |
 | |                                                             | |
 | |     Welcome to the "E-Chat!" App.                           | |
 | |_____________________________________________________________| |
 | |                  (c)                                        | |
 | |     Copyright 2024   Ariel Ayalon.                          | |
 | |_____________________________________________________________| |
 | |                                                             | |
 | |     Please select an action. For help, type 'h'.            | |
 | |_____________________________________________________________| |
  \_______________________________________________________________/
)";
    }

    void TerminalUserInterface::ClearTerminal() {
        system("clear");
    }

    void TerminalUserInterface::PushContext(Context c) {
        {
            lock_guard<mutex> g_context(m_Context);
            ContextStack->push(c);
        }
    }

    void TerminalUserInterface::PopContext() {
        {
            lock_guard<mutex> g_context(m_Context);
            if (ContextStack->size() <= 1)
                return;
            ContextStack->pop();
        }
    }

    Context TerminalUserInterface::FrontContext() {
        {
            lock_guard<mutex> g_context(m_Context);
            auto res = Context(ContextStack->top());
            return res;
        }
    }

    string TerminalUserInterface::SerializeContext() {
        switch (FrontContext()) {

            default: {
                return "[ILLEGAL_CONTEXT]";
            }
            case Context::NONE: {
                return "[No Context]";
            }
            case Context::SERVER: {
                return "[Server]";
            }
            case Context::CLIENT_LOGGED_OUT: {
                return "[Client, logged-out]";
            }
            case Context::CLIENT_LOGGED_IN: {
                return "[Client, logged-in]";
            }
            case Context::CLIENT_LOGGED_IN_ROOM: {
                return "[Client, logged-in, inside chatroom]";
            }
        }
    }

    void TerminalUserInterface::HandleInstruction(const Instruction &toHandle) {
        auto &&curName = (string &&) toHandle.Type->ShortForm;
        if (!VerifyContext(toHandle)) {
            cout << "This instruction can't be performed under the current context. Instruction aborted;" << endl;
            return;
        }
        if (curName == "scx")
            cout << "Current Context is: " << SerializeContext() << endl;
        else if (curName == "ss") {
            auto &&name = any_cast<string>(toHandle.Params[0].Value);
            auto c_name = string(name);
            p_Server = make_shared<Server>((string &&) name);
            p_Server->Start();
            ServerBuilt = true;
            cout << "Created and started server '" << c_name << "'" << endl;
            PushContext(Context::SERVER);
        } else if (curName == "cc") {
            auto &&addr = any_cast<string>(toHandle.Params[0].Value);
            auto c_addr = string(addr);
            p_Host = make_shared<ServerConnection>(addr);
            if (p_Host && p_Host->FDConnection != -1) {
                p_Host->Start();
                p_Host->isLoggedIn = function<bool()>([=]() -> bool {
                    return static_cast<int>(FrontContext()) >= 3;
                });
                p_Host->event_JoinedRoom = function<void(Hash, string)>([=](Hash rID, const string &rName) {
                    cout << "\nYou joined [" << rName << "#" << rID << "]. Welcome!" << endl;
                    {
                        lock_guard<mutex> guardRooms(m_User);
                        p_User->Rooms.emplace_back(rID, rName);
                    }
                });
                p_Host->event_LeftRoom = function<void(Hash, string)>([=](Hash rID, const string &rName) {
                    cout << "\nYou left [" << rName << "#" << rID << "]. Goodbye!" << endl;
                    {
                        lock_guard<mutex> guardRooms(m_User);
                        int i = 0;
                        for (const auto &cur: p_User->Rooms) {
                            if (cur.ID == rID)
                                break;
                            ++i;
                        }
                        p_User->Rooms.erase(p_User->Rooms.begin() + i);
                    }
                });
                p_Host->event_GotMessage = function<void(Hash, Hash, string)>(
                        [=](Hash sID, Hash rID, const string &msg) {
                            {
                                lock_guard<mutex> guardRooms(m_User);
                                int i = 0;
                                for (const auto &cur: p_User->Rooms) {
                                    if (cur.ID == rID)
                                        break;
                                    ++i;
                                }
                                if (i == p_User->Rooms.size())
                                    return;
                                cout << "\nYou received a message in ["
                                     << p_User->Rooms[i].DisplayName
                                     << "#"
                                     << p_User->Rooms[i].ID
                                     << "] from user (id=" << sID << ") that reads:" << endl;
                                p_User->Rooms[i].Messages.emplace_back(sID, msg);
                            }
                            cout << "\t" << msg << endl;
                        });
                PushContext(Context::CLIENT_LOGGED_OUT);
                cout << "Successfully connected to the server: '" << p_Host->HostAddr << "'" << endl;
            } else {
                cerr << "Connect-Client failed." << endl;
                p_Host.reset();
            }
        } else if (curName == "reg") {
            string key;
            string dispName;
            string addr = p_Host->HostAddr;
            for (auto cur: toHandle.Params) {
                if (cur.Type->ShortForm == "-n")
                    dispName = any_cast<string>(cur.Value);
                else if (cur.Type->ShortForm == "-k")
                    key = any_cast<string>(cur.Value);
            }
            if (!p_Host)
                p_Host = make_shared<ServerConnection>(addr);
            auto Acc = p_Host->Request(
                    ServerRequest(ServerActionType::RegisterAccount, p_Host->FDConnection, dispName + " | " + key));
            Hash id;
            string msg;
            stringstream ss{Acc.Data};
            ss >> id;
            getline(ss, msg);
            cout << "Account registration successful:" << endl;
            cout << "Your assigned id is '#" << id << "'" << endl;

        } else if (curName == "sl") {
            if (!p_Server)
                return;
            {
                lock_guard<mutex> gl(*p_Server->m_Log);
                cout << "Printing log:" << endl;
                for (auto &cur: p_Server->ServerLog)
                    cout << "\t" << cur << endl;
            }
        } else if (curName == "li") {
            Hash id;
            string key;
            string s_key;
            for (auto cur: toHandle.Params) {
                if (cur.Type->ShortForm == "-i")
                    id = any_cast<Hash>(cur.Value);
                else if (cur.Type->ShortForm == "-k")
                    key = any_cast<string>(cur.Value);
            }
            s_key = string(key);
            stringstream ss{};
            ss << id << " " << key;
            auto resp = p_Host->Request(ServerRequest(ServerActionType::LoginAccount,
                                                      p_Host->FDConnection,
                                                      ss.str()));

            if (resp.Type == classes::general::ClientActionType::InformFailure) {
                cout << resp.Data << endl;
                return;
            }

            if (resp.Data.empty()) {
                cerr << "Unexpected response: Received empty response data." << endl;
                return;
            }

            // Modified Parsing Logic
            string s_name;
            string s_msg;
            string s_dn;

            // Using a delimiter to ensure correct splitting of server name and message
            auto sep_pos = resp.Data.find('\'');
            auto sep_pos2 = resp.Data.find('|');
            if (sep_pos != string::npos) {
                s_name = resp.Data.substr(0, sep_pos);  // Server name before the quote
                s_dn = s_name.substr(1, sep_pos2 - 2);
                s_name = s_name.substr(sep_pos2 + 2);
                s_name = s_name.substr(0, s_name.size() - 1);
                s_msg = resp.Data.substr(sep_pos + 1);  // Message after the quote
                s_msg = s_msg.substr(0, s_msg.size() - 3);
            } else {
                cerr << "Unexpected response format: " << resp.Data << endl;
                return;
            }

            // Ensure the extracted message is not empty and display it correctly
            if (s_name.empty() || s_msg.empty()) {
                cerr << "Unexpected response format: Server name or message is empty." << endl;
                return;
            }

            cout << "Server name: '" << s_name << "'" << endl;
            cout << s_msg << endl;

            p_User = make_shared<AccountInfo>();
            p_User->ID = id;
            p_User->Name = s_dn;
            p_User->Key = s_key;
            p_Host->DisplayName = s_name;
            PushContext(Context::CLIENT_LOGGED_IN);

        } else if (curName == "ecx") {
            auto cont = FrontContext();
            switch (cont) {
                case Context::SERVER: {
                    if (!AreYouSure("Exiting this context will cause the server to be closed. Do you wish to proceed?"))
                        return;
                    p_Server->Stop();
                    break;
                }
                case Context::CLIENT_LOGGED_IN: {
                    if (!AreYouSure("Exiting this context will cause you to logout. Do you wish to proceed?"))
                        return;
                    stringstream ss{};
                    ss << p_User->ID << " " << p_User->Key;
                    p_Host->Request(ServerRequest(ServerActionType::LogoutAccount, p_Host->FDConnection,
                                                  ss.str()));
                }
                case Context::CLIENT_LOGGED_IN_ROOM: {
                    curRoomID.store(-1);
                    break;
                }
                default: {
                    break;
                }
            }
            cout << "Left " << SerializeContext() << endl;
            PopContext();
        } else if (curName == "sd") {
            PopContext();
            p_Server->Stop();
            cout << "Server was shutdown." << endl;
        } else if (curName == "accd") {
            auto opt = any_cast<string>(toHandle.Params[0].Value);
            bool f_unsafe = false;
            if (opt == "unsafe" && AreYouSure("Displaying account details in 'unsafe' mode will prevent"
                                              "your password from being censored. Continue in 'unsafe' mode?"))
                f_unsafe = true;
            cout << "Account details:" << endl;
            cout << "\tLogicalName = '(" << p_User->Name << "#" << p_User->ID << ")'" << endl;
            cout << "\tHostServer = '{" << p_Host->DisplayName << "#" << p_Host->HostAddr << "}'" << endl;
            cout << "\tDisplayName = '" << p_User->Name << "'" << endl;
            cout << "\tIdentifier = '" << p_User->ID << "'" << endl;
            string pswd = p_User->Key;
            if (!f_unsafe)
                for (auto &cur: pswd)
                    cur = '*';
            cout << "\tConnectionKey = '" << pswd << "'" << endl;
        } else if (curName == "mcr") {
            string roomName;
            vector<unsigned long long> ids;
            for (auto cur: toHandle.Params) {
                if (cur.Type->LongForm == "--name")
                    roomName = any_cast<string>(cur.Value);
                else
                    ids = any_cast<vector<unsigned long long>>(cur.Value);
            }
            stringstream ss{};
            ss << p_User->ID << " " << p_User->Key << "|" << roomName;
            auto resp = p_Host->Request(ServerRequest(ServerActionType::CreateRoom,
                                                      p_Host->FDConnection,
                                                      ss.str()));
            if (resp.Type == classes::general::ClientActionType::InformFailure) {
                string p_out = resp.Data.substr(1, resp.Data.size() - 2);
                cout << p_out << endl;
                return;
            }
            Hash rID;
            string msg;
            ss = stringstream{resp.Data};
            ss >> rID;
            getline(ss, msg);
            msg = msg.substr(1, msg.size() - 2);
            cout << msg;

            for (auto &cur: ids) {
                ss = {};
                ss << p_User->ID
                   << " "
                   << rID
                   << " "
                   << cur
                   << " "
                   << p_User->Key;
                auto r = p_Host->Request(ServerRequest(ServerActionType::AddMember,
                                                       p_Host->FDConnection,
                                                       ss.str()));

                string rsp = r.Data;
                rsp = rsp.substr(1, rsp.size() - 2);
                int stat = (r.Type == classes::general::ClientActionType::InformSuccess) ? 1 :
                           (r.Type == classes::general::ClientActionType::InformFailure ? 0 : -1);
                if (stat == -1) {
                    cout << "CRITICAL ERROR: received illegal response! Aborting." << endl;
                    return;
                }
                if (stat)
                    cout << rsp;
                else
                    cout << "Failed to add a member, id = " << cur << "'" << rsp << "'" << endl;
            }
            {
                lock_guard<mutex> guard(m_User);
                p_User->Rooms.emplace_back(rID, roomName);
            }
            cout << "Chatroom was created" << endl;
        } else if (curName == "ccr") {
            if (toHandle.Params[0].Type->LongForm == "--roomName") {
                auto name = any_cast<string>(toHandle.Params[0].Value);
                {
                    lock_guard<mutex> g_Rooms(m_User);
                    Hash b_crid = curRoomID;
                    curRoomID = -1;
                    for (auto &cur: p_User->Rooms) {
                        if (cur.DisplayName == name && curRoomID == -1)
                            curRoomID = cur.ID;
                        else if (cur.DisplayName == name) {
                            cerr << "Multiple rooms by the name '"
                                 << name
                                 << "' were found, request is too ambiguous. Please try again using the room id"
                                 << endl;
                            curRoomID = b_crid;
                            return;
                        }
                    }
                }
                if (curRoomID == -1) {
                    cerr << "No room by the name '" << name << "' was found." << endl;
                    return;
                }
            } else
                curRoomID = any_cast<unsigned long long>(toHandle.Params[0].Value);
            if (FrontContext() != Context::CLIENT_LOGGED_IN_ROOM)
                PushContext(Context::CLIENT_LOGGED_IN_ROOM);
            string name;
            {
                lock_guard<mutex> g_Rooms(m_User);
                for (auto &cur: p_User->Rooms)
                    if (cur.ID == curRoomID)
                        name = cur.DisplayName;
                cout << "Successfully entered room '"
                     << name
                     << "#"
                     << curRoomID
                     << "'"
                     << endl;
            }
        } else if (curName == "msg") {
            auto msg = any_cast<string>(toHandle.Params[0].Value);
            auto b_msg = string(msg);
            stringstream ss{};
            ss << p_User->ID
               << " "
               << curRoomID
               << " "
               << p_User->Key
               << "|"
               << msg;
            auto rsp = p_Host->Request(ServerRequest(ServerActionType::SendMessage,
                                                     p_Host->FDConnection,
                                                     ss.str()));
            if (rsp.Type == classes::general::ClientActionType::InformFailure) {
                cout << rsp.Data << endl;
                return;
            }
            int mID;
            ss = stringstream{rsp.Data};
            ss >> mID;
            string out;
            getline(ss, out);
            out = out.substr(1, out.size() - 3);
            cout << out << endl;
            {
                lock_guard<mutex> g_Rooms(m_User);
                for (auto &cur: p_User->Rooms) {
                    if (cur.ID == curRoomID) {
                        cur.Messages.emplace_back(p_User->ID, b_msg);
                        break;
                    }
                }
            }
        }
    }

    bool TerminalUserInterface::AreYouSure(const string &msg) {
        string prompt = msg + " [Y/n]: \n";
        cout << prompt << flush;

        bool confirmed = false;

        while (true) {
            string input;
            getline(cin, input); // Read entire line of input

            if (input == "Y" || input == "y") {
                confirmed = true;
                break;
            } else if (input == "N" || input == "n") {
                confirmed = false;
                break;
            } else {
                // Move cursor up to the prompt line
                cout << "\033[A";   // Move cursor up to the prompt line
                cout << "\033[2K";  // Clear the prompt line

                // Move cursor down to where the error message will be and clear the line
                cout << "\033[2K";  // Clear the current line (where the error message will be)

                // Show error message below the prompt
                cout << "The received response was invalid, trying again." << endl;

                DisableInputDuringHalt();
                sleep(3);
                RestoreTerminalSettings();

                // Move cursor back up to the prompt line
                cout << "\033[A";  // Move cursor up to the prompt line
                cout << "\033[2K"; // Clear the line for the error message
            }
        }

        cout << endl;
        return confirmed;
    }

    void TerminalUserInterface::Cleanup() {

    }

    void TerminalUserInterface::DisableInputDuringHalt() {
        if (!isTermiosSaved) {
            tcgetattr(STDIN_FILENO, &originalTermios);  // Save the current terminal settings
            isTermiosSaved = true;
        }

        termios raw = originalTermios;                 // Copy the original settings
        raw.c_lflag &= ~(ICANON | ECHO);               // Disable canonical mode and echo
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);        // Apply the new settings
    }

    void TerminalUserInterface::RestoreTerminalSettings() {
        if (isTermiosSaved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);  // Restore the original settings
        }
    }

} // terminal
