#include <csignal>

#include "src/Testing/ServerConnectionTest.h"

void signalHandler(int signum) {
    cout << "\n\nInterrupt signal (" << signum << ") received.\n";

    src::Testing::ServerConnectionTest::Free();

    // Exit the program
    exit(signum);
}

int main(){
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGBUS, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGPIPE, SIG_IGN);


    src::Testing::ServerConnectionTest::BuildServer();
    while(!src::Testing::ServerConnectionTest::b_finished->load());
    sleep(1);
    src::Testing::ServerConnectionTest::TestClient();
    return 0;
}