#ifndef WEBSERV_CONNECTION_HPP
#define WEBSERV_CONNECTION_HPP

#include <string>

class Connection {
    int sock;
    std::string data;
    bool _isOpen;

    Connection();
    Connection(const Connection &);
    Connection &operator=(const Connection &);
public:
    Connection(int sock);
    ~Connection();

    int getSocket() const;
    void readData();
    void writeData();
    bool isOpen() const;
};


#endif
