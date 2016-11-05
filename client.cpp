#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>
 
#include <cstdio>
#include <cstdlib>
 
/* Client program to comlpiment the OMP server 
*/
 void showhelp()
 {
    /*std::cout<<"\n[Status] Connected, type \"quit\" to exit\n\n"
            <<"::::::::::MANNUAL:::::::::\n"
            <<"--For simple one person msg--\n"
            <<"/msg nickname \"your msg here\" \n"
            <<"--To broadcast your msg to everyone online--\n"
            <<"/all \"your broadcast msg\" \n\n"
            <<"Type help to show this msg \n\n";
    */
    std::cout   <<"For single one to one msg \n"
                <<"/msg nick \"your message\" \n"
                <<"For channel msg \n"
                <<"/cmsg chname \"your meassage\" \n"
                <<"For public broadcast \n"
                <<"/all \"your message\" \n"
                <<"To create a new channel \n"
                <<"/add channelname  \n"
                <<"To join a channel  \n"
                <<"/join channelname   \n"
                <<"To quit your client  \n"
                <<"quit \n";
 }
int main(int argc, char *argv[])
{
    // Const Declaration
    const int Stdin = STDIN_FILENO; //0
    const size_t BufferSize = 256;


    if (argc < 3) {
        std::cerr<<"usage "<<argv[0]<<" hostname port\n";
        std::exit(0);
    }

    int portno = std::atoi(argv[2]);
    // Our socket
    int sock = -1;
 
    if( (sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ){
        std::cout << "[Error] Cannot create the socket" << std::endl;
        return 1;
    }

    struct hostent *server_addr = gethostbyname(argv[1]);
    if (server_addr == NULL) {
        std::cerr<<"[ERROR] No such host\n";
        std::exit(0);
    }
 
    // The Sockaddr
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family       = AF_INET;
    memcpy((char *)&server.sin_addr.s_addr,(char *)server_addr->h_addr,server_addr->h_length);
    server.sin_port         = htons(portno);
 
    if( connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0){
        std::cout << "[Error] Cannot connect to server" << std::endl;
        close(sock);
        return 1;
    }
 
    // Set the socket to non blocking state
    const int flag = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flag | O_NONBLOCK);
 
    // We are connected
    showhelp();
 
    char sendBuffer[BufferSize + 1];
    char recvBuffer[BufferSize + 1];
 
    bool stop = false;
 
    while( !stop ){
        // Set the FD to our socket & stdin
        fd_set  readSet;
        FD_ZERO(&readSet);
 
        FD_SET(Stdin, &readSet);
        FD_SET(sock, &readSet);
 
        // if stdin != 0 first parameter should be Max(Stdin , sock) + 1
        select( sock + 1, &readSet, NULL, NULL, NULL);
 
        // read from the socket
        if( FD_ISSET(sock, &readSet) ){
            FD_CLR(sock, &readSet);
            const int lenghtRead = read(sock, recvBuffer, BufferSize);
            if(lenghtRead <= 0){
                std::cout << "[Disconnected from server]" << std::endl;
                // we do not want to break the loop because
                // we may have to clean the stdin before
                stop = true;
            }
            else if(0 < lenghtRead){
                recvBuffer[lenghtRead] = '\0';
                std::cout << recvBuffer;
            }
        }
 
        // Can we read from stdin?
        if( FD_ISSET(Stdin, &readSet) ){
            FD_CLR(Stdin, &readSet);
            memset(sendBuffer, 0, BufferSize);
            const int lenghtRead = read(Stdin, sendBuffer, BufferSize);
            if(lenghtRead){
                if( lenghtRead == 5 && memcmp("quit", sendBuffer, 4) == 0 ){
                    stop = true;
                }
                else if( lenghtRead == 5 && memcmp("help", sendBuffer, 4) == 0 )
                    showhelp();
                else if( write(sock, sendBuffer, lenghtRead) < 0){
                    std::cout << "[Disconnected from server]" << std::endl;
                    stop = true;
                }
            }
        }
    }
 
    close(sock);
 
    return 0;
}