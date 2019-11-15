#include "http.h"


void HttpServer::accept_callback(struct ev_loop * loop, struct ev_io * watcher, int revents) {
    struct sockaddr_in clientAddr;
    socklen_t client_len = sizeof(clientAddr);
    ev_io *w_client = (ev_io*) malloc(sizeof(struct ev_io));

    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    // Accept client request
    int clientFD = ::accept(watcher -> fd, (sockaddr*) &clientAddr, &client_len);

    if (clientFD < 0) {
        perror("accept error");
        return;
    }

    _set_non_block(clientFD);

    printf("Successfully connected with client.\n");

    // Initialize and start watcher to read client requests
    ev_io_init(w_client, read_callback, clientFD, EV_READ);
    ev_io_start(loop, w_client);
}

/* Read client message */
void HttpServer::read_callback(struct ev_loop * loop, struct ev_io * watcher, int revents) {
    char buffer[BUFFER_SIZE];
    ssize_t read;

    if (EV_ERROR & revents) {
        perror("got invalid event");
        return;
    }

    // Receive message from client socket
    read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);

    if (!_is_valid_return(read)) {
        perror("read error");
        return;
    }

    if (read == 0) {
        // Stop and free watchet if client socket is closing
        ev_io_stop(loop, watcher);
        ::close(watcher->fd);
        free(watcher);
        printf("peer might closing\n");
        return;
    } else {
        printf("message:%s\n", buffer);
    }

    // Send message bach to the client
    if(!_is_valid_return(send(watcher->fd, buffer, read, 0))) {
        perror("send error");
    }

    bzero(buffer, read);
}


bool HttpServer::bind_and_listen(const int port) {
    address.sin_family       = AF_INET;
    address.sin_addr.s_addr  = INADDR_ANY;
    address.sin_port         = htons(port);

    if(!_is_valid_return(::bind(sockFD, (struct sockaddr*) &address, sizeof(address))))
        return false;

    return _is_valid_return(::listen(sockFD, maxQueueSize));
}


bool HttpServer::close(){
    if(sockFD != -1){
        const bool noErrorCheck = _is_valid_return(::close(sockFD));
        sockFD = -1;
        return noErrorCheck;
    }

    return false;
}

bool HttpServer::trigger_stop() {
    trigger_stop_flag = true;
    return true;
}


atomic_ushort HttpServer::thread_index = 0;

HttpServer::HttpServer(const int port):sockFD(-1), trigger_stop_flag(false) {
    memset(&address, 0, sizeof(address));

    if(_is_valid_return(sockFD = socket(AF_INET, SOCK_STREAM, 0))){
        // We need to be able reuse address ,thus avoid "Address already in use" error
        // set SO_REUSEADDR on a socket to true (1):
        // SOL_SOCKET is needed for denoting the level (ignore this , just syntax )
        const int optval = 1;
        if( !_is_valid_return(setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) ) ){
            close();
        }

        if(!bind_and_listen(port)){
            close();
        }

        _set_non_block(sockFD);
    }
}


bool HttpServer::run() {
    struct ev_loop *loop = EV_DEFAULT;
    ev_io accept_watcher;

    ev_io_init (&accept_watcher, accept_callback, sockFD, EV_READ);
    ev_io_start (loop, &accept_watcher);

    // now wait for events to arrive
    ev_run (loop, 0);

    // should not be reachable
    return false;
}

