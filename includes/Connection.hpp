#ifndef WEBSERV_CONNECTION_HPP
#define WEBSERV_CONNECTION_HPP

#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include "Request.hpp"

class Connection {
    int sock;
    struct sockaddr_in sockAddr;
    bool _isOpen;
    std::string data;
    Request request;

    Connection();
    Connection(const Connection &);
    Connection &operator=(const Connection &);
public:
    Connection(int sock, struct sockaddr_in sockAddr);
    ~Connection();

    int getSocket() const;
    Request& getRequest() const;
    void readData();
    void writeData();
    bool isOpen() const;
};


#endif
