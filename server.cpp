#include "include/http.cpp"


int main(int argc, char *argv[]){
    if (argc < 2) {
        printf("No port provided or path not provided\n");
        return 1;
    }

    //get port from user
    int port = atoi(argv[1]);

    HttpServer server(port);
    server.run();

    // Remove the ugly ^C
    printf("\r");
    return 0;
}