#ifndef OMPSERVER_H
#define OMPSERVER_H
 
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>          // many struct definations like sockaddr_in
#include <netdb.h>               // defines the hostent structure
#include <unistd.h>              // for ::close() , usleep()
#include <cstring>
#include <arpa/inet.h>           // defines the in_addr
#include <fcntl.h>               // to set file descriptor flags , O_NONBLOCK
#include <sys/time.h>            // timeval

#ifdef _OPENMP
#include <omp.h>                 // multiThreading 
#endif

#include <vector>
#include <cstdlib>
#include <utility>
#include <algorithm>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <cstdio>
#include <map>

 
class OmpServer
{
private:
    ///////////////////////////////////////////////
    // Static data           //avoids global access
    ///////////////////////////////////////////////
 
    const static int MaximumClients     = 50;   //< Maximum no. of clients also == No. threads in Pool
    const static int NbQueuedClients    = 4;    //< Queued clients in the listen function
    const static int BufferSize         = 256;  //< Message buffer size
 
    ///////////////////////////////////////////////
    // Attributes
    ///////////////////////////////////////////////
 
    sockaddr_in         address;    //< Internet address
    int                 sock;       //< Server socket
    std::vector< std::pair<int,std::string> >    clients;    //< Clients sockets
    int                 nbClients;  //< Current Nb Clients
    bool                hasToStop;  //< To stop the server

    enum commands{msg=1,all,add,join,cmsg};

    struct directive                   //data recieved after parsing the message
    {
        commands cmd;                        //command id
        std::string buffer;             //1:msg 2:all
        std::string dest,nick;
    };
    
    // For Login
    struct userinfo
    {
        std::string info,pswd="";

    };

    std::vector< std::vector< std::string > > channels;
    int channels_count=0;
    std::map<std::string, int> channel_names;

 
    ///////////////////////////////////////////////
    // Usual socket stuff
    ///////////////////////////////////////////////
 
    /** Bind socket to internet address */
    bool bind(const int inPort){
        address.sin_family       = AF_INET;
        address.sin_addr.s_addr  = INADDR_ANY;
        address.sin_port         = htons ( inPort );
 
        return IsValidReturn( ::bind( sock, (struct sockaddr*) &address, sizeof(address)) );
    }
 
    /** The server starts to listen to the socket */
    bool listen(){
        return IsValidReturn( ::listen( sock, NbQueuedClients ) );
    }
 
    /** Accept a new client, this function is non-blocking */
    int accept(){
        int sizeOfAddress = sizeof(address);
        // Because of the nonblocking mode, if accept returns a
        // negative number we cannot know if there is an error OR no new client
        const int newClientSock = ::accept ( sock, (sockaddr*)&address, (socklen_t*)&sizeOfAddress );
        return (newClientSock < 0 ? -1 : newClientSock);
    }
    /* Non-block socket enable simultaneous read and write to socket 
       For this however polling is needed 
       done with simple while loop 
       usleep can be added to avoid large thread utilization
    */ 
    /** Function to Process a client (manage communication) */
    void processClient(const int inClientSocket){
        
        // adds the new client until server becomes full
        if( !addClient(inClientSocket) ){
            IsValidReturn( write( inClientSocket, "Server is full!\n", 16) );
            ::close(inClientSocket);
            return;
        }
 
        // Try to send a message:: ask for nick
        if( !IsValidReturn( write( inClientSocket, "Welcome : enter your nick :\n", 28)) ){
            removeClient(inClientSocket);
            return;
        }
 
        // Wait message and broadcast
        const size_t BufferSize = 256;
        char recvBuffer[BufferSize];
 
        const int flag = fcntl(inClientSocket, F_GETFL, 0);
        fcntl(inClientSocket, F_SETFL, flag | O_NONBLOCK);
        
        bool FirstTime=true;   // to accept nickname

            /*
            Reference 
            http://beej.us/guide/bgnet/output/html/multipage/advanced.html

            FD_SET(int fd, fd_set *set);    Add fd to the set.

            FD_CLR(int fd, fd_set *set);    Remove fd from the set.

            FD_ISSET(int fd, fd_set *set);  Return true if fd is in the set.

            FD_ZERO(fd_set *set);           Clear all entries from the set.

            // Select() , Keep 3rd & 4th NULL , Don't fiddle  
            */
        while( true ){
            fd_set  readSet;        // define file desr. Set called readset 
            FD_ZERO(&readSet);
 
            FD_SET(inClientSocket, &readSet);
 
            timeval timeout = {1, 0};  // Initializes tv_sec & tv_usec on the go 
            select( inClientSocket + 1, &readSet, NULL, NULL, &timeout);
 
            // We have to perform the test after the select (may have spent 1s in the select)
            if( hasToStop ){
                break;
            }

            memset(&recvBuffer, 0, sizeof(recvBuffer));
            
            if( FD_ISSET(inClientSocket, &readSet) ){
                FD_CLR(inClientSocket, &readSet);
                const int lenghtRead = read(inClientSocket, recvBuffer, BufferSize);
                if(lenghtRead <= 0){
                    removeClient(inClientSocket);
                    break;
                }
                else if(0 < lenghtRead){
                    if(FirstTime==true)
                    {
                        int userid=SearchClient(inClientSocket);

                        recvBuffer[strlen(recvBuffer)-1]='\0';

                        if(userid>=0){
                                clients[userid].second=recvBuffer;
                            }

                        /*Add to the list of users
                        if( !adduser(userid) )
                            std::cerr<<"[ERROR] cannot add user\n";
                        */

                        //send saved messages if exists 
                        if( !sendsaved(userid) )
                            std::cerr<<"error sending saved messages";

                        FirstTime=false;

                        //logs & debugging
                        show(userid,recvBuffer);           
                        show();
                    }
                    else
                    {
                        // Make a directive ,fill it in with parse
                        //then pass it on to Broadcast
                        directive temp;
                        std::string temp_str=recvBuffer;
                        temp.nick=clients[SearchClient(inClientSocket)].second;
                        if(parse(temp_str,temp))
                        {
                            if(temp.cmd==add){
                                addChannel(temp.dest,temp.nick);
                            }
                            else if(temp.cmd==join){
                                addtochannel(temp.dest,temp.nick);
                            }
                            else
                                broadcast(temp);

                            //logs & debugging
                            show_chs();
                            show(temp);
                            //show();
                        }
                    }
                    
                }
            }
        }
    }
 
    ///////////////////////////////////////////////
    // Clients socket management
    ///////////////////////////////////////////////
 
    /** Remove a client from the vector (CriticalClient)*/
    void removeClient(const int inClientSocket){
        #pragma omp critical(CriticalClient)
        {
            int socketPosition = -1;
            for(int idxClient = 0 ; idxClient < nbClients ; ++idxClient){
                if( clients[idxClient].first == inClientSocket ){
                    socketPosition = idxClient;
                    break;
                }
            }
            if( socketPosition != -1 ){
                ::close(inClientSocket);
                --nbClients;
                // we switch only the last client and the client to removed
                // because the vector is not ordered
                // if nbClient was 1 => clients[0] = clients[0]
                // if socketPosition is nbClient - 1 => clients[socketPosition] = clients[socketPosition]
                clients[socketPosition] = clients[nbClients];
            }
        }
    }
 
    /** Add a client in the vector if possible (CriticalClient)*/
    bool addClient(const int inClientSocket){
        bool cliendAdded = false;
        #pragma omp critical(CriticalClient)
        {
            if(nbClients != MaximumClients){
                clients[nbClients++].first = inClientSocket;
                cliendAdded = true;
            }
        }
        return cliendAdded;
    }
 
    ////sends messages to client ////
    ////or broadcasts them to everyone////
    void broadcast(const directive& data){
        if(data.cmd==all)
        {
            #pragma omp critical(CriticalClient)
            {
                for(int idxClient = 0 ; idxClient < nbClients ; ++idxClient){

                        std::string tempBuffer="msg from " +data.nick+ ": "+data.buffer;
                        // write to client , if it fails 
                        // remove the client
                        if( write(clients[idxClient].first, tempBuffer.c_str(), std::strlen(tempBuffer.c_str()) ) <= 0 ){
                            ::close(clients[idxClient].first);
                            --nbClients;
                            clients[idxClient] = clients[nbClients];
                            --idxClient;
                        }
                }
            }
        }
        else if(data.cmd==msg)
        {
            #pragma omp critical(CriticalClient)
            {
                bool found=false;
                int idxClient=SearchClient(data.dest);
                    if(idxClient>=0)
                    {
                        std::string tempBuffer="msg from " +data.nick+ ": " +data.buffer;
                        // write to client , if it fails 
                        // remove the client
                        if( write(clients[idxClient].first, tempBuffer.c_str(), std::strlen(tempBuffer.c_str()) ) <= 0 ){
                            ::close(clients[idxClient].first);
                            --nbClients;
                            clients[idxClient] = clients[nbClients];
                        }
                        found =true;
                    }
                //if client not online save the msg
                if(found==false)
                    save(data.dest,"msg from " +data.nick+ ": " +data.buffer);
            }
        }
        else if(data.cmd==cmsg)
        {
            //this is not working
            //std::map<std::string,int>::const_iterator tmp_id =channel_names.find(data.dest);
            //std::cout << "WOW : " << channel_names[data.dest] << std::endl;

            if(channel_names.find(data.dest) !=channel_names.end())
            {
                int index=channel_names[data.dest];
                for(int i=0;i<channels[index].size();i++)
                {
                    bool found=false;
                    int idxClient=SearchClient(channels[index][i]);
                    std::cout<<"\n idx :"<<idxClient<<" \n";
                    #pragma omp critical(CriticalClient)
                    {
                        if(idxClient>=0)
                        {
                            std::string tempBuffer="msg from " +data.nick+ ": " +data.buffer;
                            // write to client , if it fails 
                            // remove the client
                            if( write(clients[idxClient].first, tempBuffer.c_str(), std::strlen(tempBuffer.c_str()) ) <= 0 ){
                                ::close(clients[idxClient].first);
                                --nbClients;
                                clients[idxClient] = clients[nbClients];
                            }
                            found =true;
                        }
                    }
                    //if client not online save the msg
                    if(found==false)
                        save(channels[index][i],"msg from " +data.nick+ ": " +data.buffer);
                }
            }
            else
                std::cerr<<"[ERROR] no such channel \n";
        }

    }
 
    /** Forbid copy */
    OmpServer(const OmpServer&){}
    OmpServer& operator=(const OmpServer&){ return *this; }

//search clients
int SearchClient(const int SockId)
{
    for(int i=0;i<nbClients;i++)
        if(clients[i].first==SockId)
            return i;
    return -1;
}
int SearchClient(const std::string name)
{
    for(int i=0;i<nbClients;i++)
        if(clients[i].second.compare(name)==0)
            return i;
    return -1;
}

//          Add a new channel           ///////
bool addChannel(std::string name,std::string creator_nick)
{
    //channel_names.insert ( std::pair<std::string,int>(name,channels_count) );
    std::cout << name.size() << "\n";
    channel_names[name] = channels_count;
    channels_count++;

    channels.push_back( std::vector<std::string>(1,creator_nick) );

    /*add creator to channel
    if( !addtochannel(name,creator_nick) )
        std::cerr<<"[ERROR] Add client |"<<creator_nick<<"| failed\n";
    */
    return true;
}
bool addtochannel(std::string channel_name,std::string newname)
{
    std::map<std::string,int>::iterator tmp_id =channel_names.find(channel_name);

    if(tmp_id !=channel_names.end())
        channels[ tmp_id->second ].push_back(newname);
    return true;
}

bool sendtochannel();


///////////////////////////////////////////////
//parsing function ////////////////////////////
////////This is where the magic happens ///////    
///////////////////////////////////////////////

bool parse(std::string& buffer,directive &temp)
    {
        if(buffer.compare(0,4,"/msg")==0)
        {
            temp.cmd=msg;
            buffer=buffer.substr(5);

            temp.dest=buffer.substr(0,buffer.find(" "));
            buffer=buffer.substr(buffer.find(" ")+1);

            temp.buffer=buffer;

            return true;
        }
        else if(buffer.compare(0,4,"/all")==0)
        {
            temp.cmd=all;
            temp.buffer=buffer.substr(5);
            return true;
        }
        else if(buffer.compare(0,4,"/add")==0)
        {
            temp.cmd=add;
            //if( *(buffer.end()-1) == '\n')
            //    *(buffer.end()-1) = '\0';
            buffer=buffer.substr(5);
            temp.dest=buffer.substr(0,buffer.find("\n"));
            return true;
        }
        else if(buffer.compare(0,5,"/join")==0)
        {
            temp.cmd=join;
            //if( *(buffer.end()-1) == '\n')
            //    *(buffer.end()-1) = '\0';

            buffer=buffer.substr(6);
            temp.dest=buffer.substr(0,buffer.find("\n"));
            return true;
        }
        else if(buffer.compare(0,5,"/cmsg")==0)
        {
            temp.cmd=cmsg;
            buffer=buffer.substr(6);

            temp.dest=buffer.substr(0,buffer.find(" "));
            buffer=buffer.substr(buffer.find(" ")+1);

            temp.buffer=buffer;
            return true;
        }
        else
            return false;
    }

////save messages for offline users/////
// Use of C++11 String 
void save(const std::string name,const std::string msg)
{
    std::string newname=name+".dat";
    #pragma omp critical(FileUpdate)
    {
        std::ofstream fout( newname.c_str() , std::ios::out|std::ios::app);
        fout<<msg.c_str();
        fout.close();
    }
}
// send out saved messages if present
bool sendsaved(const int userid)
{
    bool failed=false;
    std::string newname=clients[userid].second+".dat";
    const size_t BufferSize = 256;
    #pragma omp critical(FileUpdate)
    {
        if(check(newname))
        {
            std::ifstream fin(newname.c_str());
            char buffer[BufferSize];
            fin.read( buffer, BufferSize);
            int lenghtRead= strlen( buffer );

            do
            {
                std::cerr<<"file buffer :|"<<buffer<<"|\n";

                if( write(clients[userid].first, buffer, lenghtRead) <= 0 ){
                        ::close(clients[userid].first);
                        --nbClients;
                        clients[userid] = clients[nbClients];

                        failed=true;
                    }

                fin.read( buffer, BufferSize);
                int lenghtRead= strlen( buffer );
            }while(!fin.eof());

            // Remove the file after reading it :)
            if(std::remove(newname.c_str()) != 0){
                std::cerr<<"error deleting file :"<<newname<<std::endl;
                 failed=true;
            }
        }

    }

    return !failed ;
}

int adduser(int userid)
{
    return 0;
    std::string pass;
    //ask for password
    if( write(clients[userid].first,"Enter your password :" , std::strlen( "Enter your password :" ) ) <= 0 ){
        ::close(clients[userid].first);
        --nbClients;
        clients[userid] = clients[nbClients];
    }

    //users.insert_or_assign(clients[userid].second,x)
}

// Credits : http://stackoverflow.com/a/12774387/7088182 //
//                  Nice guy btw                         //
// simple function to check if a file exists or not ////
//          uses unix system call stat              ////
//                  supposedly fast                 ////
//at least faster than opening and closing the file ////

inline bool check(const std::string name) {
  struct stat buffer;   
  return ( stat(name.c_str(), &buffer) == 0 ); 
}

    /////////////////////////////
    /////debugging functions/////
    /////////////////////////////
void show(const directive t)
{
    std::cout<<"cmd :"<<t.cmd<<"\n";
    std::cout<<"dest :|"<<t.dest<<"|\n";
    std::cout<<"buffer :"<<t.buffer<<"\n";
}
void show(const int userid,const char recvBuffer[])
{
    std::cout<<"clients nick :"<<clients[userid].second<<"\n";
    std::cout<<"recvBuffer :|"<<recvBuffer<<"|\n";
}
void show()
{
    for(int i = 0 ; i < nbClients ; ++i)
    {
        std::cout<<"client sockid :"<<clients[i].first<<"\n";
        std::cout<<"client nickname:|"<<clients[i].second<<"|\n";
    }
}
void show_chs()
{
    std::map<std::string,int>::const_iterator it=channel_names.begin();
    while(it!=channel_names.end())
    {
        std::cout<<"ch name |"<<it->first<<"| id :"<<it->second<<"\n";
        it++;
    }
    std::cout<<"size :"<<channels.size()<<"\n";
    for (unsigned int i = 0; i < channels.size(); ++i){
        std::cout<<"size[i] :"<<channels[i].size()<<"\n";
        std::cout << "ch no :"<<i<<" :";
        for (unsigned int j = 0; j < channels[i].size(); ++j){
            std::cout <<"i,j="<<i<<","<<j<<" |"<<channels[i][j]<<"| ";
        std::cout<<"\n";
        }
    std::cout << std::endl;
    }
}

// Generic method to test network C return value
    // provides error handling
    // funny though xD

    static bool IsValidReturn(const int inReturnedValue){
        return inReturnedValue >= 0;
    }
 
public:
    ///////////////////////////////////////////////
    // Public methods
    ///////////////////////////////////////////////
 
    /** The constructor initialises the socket, but not only that 
      * It also installs the socket with the bind and
      * starts the listen.
      * So, as soon the server is created, the clients
      * can be queued even if no call to run has been
      * performed.
      */

    OmpServer(const int inPort)
        : sock(-1), clients(MaximumClients,std::make_pair(-1,"")), nbClients(0), hasToStop(true) {
        memset(&address, 0, sizeof(address));
 
        if( IsValidReturn( sock = socket(AF_INET, SOCK_STREAM, 0)) ){
            // We need to be able reuse address ,thus avoid "Address already in use" error
            // set SO_REUSEADDR on a socket to true (1):
            // SOL_SOCKET is needed for denoting the level (ignore this , just syntax )  
            const int optval = 1;
            if( !IsValidReturn(setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) ) ){
                close();
            }
 
            if( !bind(inPort) ){
                close();
            }
 
            if( !listen() ){
                close();
            }
 
            const int flag = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flag | O_NONBLOCK);
        }
    }
 
    /** Destructor closes the server socket
      * The clients have already been closed (at end of run)
      */
    virtual ~OmpServer(){
        close();
    }
 
    // Isvalid checks if sock is valid i.e !=-1
    bool isValid() const {
        return sock != -1;
    }
 
    /** Simply set the loop boolean to true */
    void stopRun(){
        hasToStop = true;
    }
 
    /** This function closes the socket 
    uses ::close() defined in unistd.h 
    ::close closes a file descriptor */
    bool close(){
        if(isValid()){           
            const bool noErrorCheck = IsValidReturn(::close(sock));
            sock = -1;
            return noErrorCheck;
        }
        return false;
    }
 
    /** The run starts to accept the clients and execute
      * one thread per client.
      * When hasToStop is set to true (stopRun called),
      * the master thread stops to accept clients and waits all
      * others threads.
      * Then the clients sockets are closed
    */

    bool run(){
        // checks Whether a parallel region is active
        // if yes return false
        // And whether sock is valid
        // if not return false;
        if( omp_in_parallel() || !isValid() ){
            return false;
        }
 
        hasToStop = false;
        // this section will be run parallel with all the threads in pool
        #pragma omp parallel num_threads(MaximumClients)
        {   
            //this section however will force single thread execution 
            //nowait means thread wont wait for other threads to complete & synchronize
            #pragma omp single nowait
            {
                while( !hasToStop ){
                    const int newSocket = accept();
                    if( newSocket != -1 ){
                        // we define a task such that processClient can run in parellel
                        // untied task can be resumed by any thread in the pool  
                        // so we let this section to be run by different thread
                        #pragma omp task untied
                        {
                            processClient(newSocket);
                        }
                    }
                    else{
                        usleep(2000);       // avoids over tasking of CPU , 2mSec wait
                    }
                }
                //now all threads will wait till synchronized
                // all threads will reach here only when 
                // hastostop=true
                #pragma omp taskwait
            }
        }
 
        // Close sockets with default no of threads number
        // parallel for directive parallelizes the for loop
        #pragma omp parallel for
        for(int idxClient = 0 ; idxClient < nbClients ; ++idxClient ){
            ::close(clients[idxClient].first);
        }
        nbClients = 0;
 
        return true;
    }
};
 
#endif // OMPSERVER_H