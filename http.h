#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_

#include <stdio.h>
#include <netinet/in.h>
#include <ev.h>

#include <sys/types.h>
#include <netdb.h>               // defines the hostent structure
#include <unistd.h>              // for ::close() , usleep()
#include <cstring>
#include <arpa/inet.h>           // defines the in_addr
#include <fcntl.h>               // to set file descriptor flags , O_NONBLOCK

#include <stdatomic.h>
#include <stdlib.h>

#ifdef _OPENMP
#include <omp.h>                 // multiThreading
#endif


class HttpServer {
private:
    const static int numberOfThreads = 4;
    const static int maxQueueSize = 40;
    const static uint BUFFER_SIZE = 4 * 1024u; // 4 Bytes
    sockaddr_in address;
    int sockFD;
    bool trigger_stop_flag;
    static atomic_ushort thread_index; // the index to queue the watchers



    /** Bind socket to internet address */
    bool bind_and_listen(const int port);
    int accept();


    bool add_client(const int clientFD); // add client and start watching
    bool remove_client(const int clientFD); // remove client and stop watching

    static void accept_callback(struct ev_loop * loop, struct ev_io * watcher, int revents);
    static void read_callback(struct ev_loop * loop, struct ev_io * watcher, int revents);






    /** Forbid copy */
    HttpServer(const HttpServer&){}
    HttpServer& operator=(const HttpServer&){
        return *this;
    }

    static bool _is_valid_return(const int returnCode){
        return returnCode >= 0;
    }

    static bool _set_non_block(const int fd) {
        const int flag = fcntl(fd, F_GETFL, 0);
        return _is_valid_return(fcntl(fd, F_SETFL, flag | O_NONBLOCK));
    }

public:
    bool trigger_stop();
    bool close();
    bool run();


    HttpServer(const int port);
    virtual ~HttpServer(){
        close();
    }
};

#endif