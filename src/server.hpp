#pragma once

#include "expression.hpp"

#include <map>
#include <string>
#include <vector>

class CalculatorServer
{
public:
    explicit CalculatorServer(int port);
    ~CalculatorServer();

    void run();

private:
    struct Client
    {
        int fd = -1;
        bool hasUser = false;
        std::string user;
        std::string input;
        std::string output;
    };

    int listenFd = -1;
    int port;
    std::map<int, Client> clients;
    std::map<std::string, Variables> users;

    void openListenSocket();
    void acceptClient();
    void closeClient(int fd);

    void readFromClient(int fd);
    void flushToClient(int fd);
    void handleLine(Client &client, const std::string &line);

    std::string executeCommand(const std::string &user, const std::string &line);
    void loadUsers();
    void saveUsers() const;
};
