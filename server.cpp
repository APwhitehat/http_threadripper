#include "server.h"
 
#include <cstdlib>
#include <signal.h>
 
#include <iostream>
 
// The current Server
static OmpServer* ServerToClose = 0;
 
// Ctr + c => stop the server
void stopServerSignal(int){
    if( ServerToClose ){                //if there is a server running
        ServerToClose->stopRun();
    }
}
 
int main(int argc, char *argv[]){

    if (argc < 2) {
        std::cerr<<" No port provided\n";
        exit(0);
    }

    // Install  Signal handler
    if( signal(SIGINT , stopServerSignal) == SIG_ERR ){
        std::cout << "Error cannot install signal handler..." << std::endl;
        return 56;
    }
    
    std::cout << "Press Ctr + C to stop..." << std::endl;

    //get port from user 
    int portno = atoi(argv[1]);

    // Create and run the server & pass port to server
    OmpServer server(portno);
 
    ServerToClose = &server;
 
    server.run();
 
    // Remove the ugly ^C
    std::cout << "\r";
    return 0;
}