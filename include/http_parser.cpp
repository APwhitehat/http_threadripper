#ifndef HTTPPARSER_H_
#define HTTPPARSER_H_

#include <cstring>
#include <string>
#include <fstream>
#include <string>
#include <cerrno>

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

const size_t MAX_LEN_OF_REQUEST = 100;
const size_t MAX_NO_OF_FILES = 100;
const std::string BASE_HEADER = "HTTP/1.1 200 OK\nContent-Type: ";
std::string NOT_FOUND = "HTTP/1.1 404 Not Found \n\n Not Found";
char HELLO_WORLD[75] = "HTTP/1.1 200 OK\nContent-Type: text/plain;\nContent-Length: 12\n\nHello World\n";
char CONNECTION_CLOSE[18] = "Connection: close";

inline bool check_connection_close(char *buffer) {
    for(size_t i = 0; buffer[i] != '\0' and (buffer[i] != '\n' or buffer[i+1] != '\n'); i++) {
        size_t j;
        for(j = 0; j < 17 and CONNECTION_CLOSE[j] == buffer[i+j]; j++);

        if(j == 17)
            return true;
    }

    return false;
}

inline std::string file_extension(std::string path) {
    for(size_t i = path.size(); i >= 0; i--)
        if(path[i] == '.')
            return path.substr(i+1);

    return std::string();
}

// This function is taken from https://github.com/yhirose/cpp-httplib/blob/master/httplib.h
inline const char *find_content_type(const std::string &path) {
  auto ext = file_extension(path);
  if (ext == "txt") {
    return "text/plain";
  } else if (ext == "html" || ext == "htm") {
    return "text/html";
  } else if (ext == "css") {
    return "text/css";
  } else if (ext == "jpeg" || ext == "jpg") {
    return "image/jpg";
  } else if (ext == "png") {
    return "image/png";
  } else if (ext == "gif") {
    return "image/gif";
  } else if (ext == "svg") {
    return "image/svg+xml";
  } else if (ext == "ico") {
    return "image/x-icon";
  } else if (ext == "json") {
    return "application/json";
  } else if (ext == "pdf") {
    return "application/pdf";
  } else if (ext == "js") {
    return "application/javascript";
  } else if (ext == "xml") {
    return "application/xml";
  } else if (ext == "xhtml") {
    return "application/xhtml+xml";
  }
  return nullptr;
}

// This is in the hot path. parse as little as possible
std::string parse_path_from_request(const char * req) {
    // assume 1st line to be GET / HTTP/1.1
    size_t i = 4;
    while(req[i] != ' ' and req[i] != '\0' and req[i] != '\n' and i < MAX_LEN_OF_REQUEST)
        i++;
    if(req[i] == ' ')
        return std::string(req+ 4, i - 4);

    return std::string();
}

class FileServer {
private:
    size_t index;
    std::string paths[MAX_NO_OF_FILES], contents[MAX_NO_OF_FILES]={BASE_HEADER};
public:
    FileServer(): index(0) {
        // parseFilesRecursively(base);
    }

    std::string get_file_contents(const char *filename){
        std::ifstream in(filename, std::ios::in | std::ios::binary);
        if (in) {
            std::string contents;
            in.seekg(0, std::ios::end);
            contents.resize(in.tellg());
            in.seekg(0, std::ios::beg);
            in.read(&contents[0], contents.size());
            in.close();
            return(contents);
        }

        throw(errno);
    }

    void parseFilesRecursively(char *basePath) {
        char path[1000]={0};
        struct dirent *dp;
        DIR *dir = opendir(basePath);

        // Unable to open directory stream
        if (!dir)
            return;

        while ((dp = readdir(dir)) != NULL) {
            if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
                printf("%s\n", basePath);
                // Construct new path from our base path
                strcpy(path, basePath);
                strcat(path, "/");
                strcat(path, dp->d_name);

                if(index < MAX_NO_OF_FILES){
                    paths[index] += path;
                    contents[index] += find_content_type(path);
                    contents[index] += "\n\n"+ get_file_contents(paths[index].c_str()) + "\n";
                    index++;
                }

                parseFilesRecursively(path);
            }
        }

        closedir(dir);
    }

    static char* get(const char * fullpath) {
        return HELLO_WORLD;

        // std::string path = parse_path_from_request(fullpath);
        // for(size_t i = 0; i < index; i++)
        //     if(paths[i] == path)
        //         return contents[i];

        // return NOT_FOUND;
    }
};


#endif