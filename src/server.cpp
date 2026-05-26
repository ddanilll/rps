#include "server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{
const char *DB_FILE = "calc_users.db";

void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        throw std::runtime_error("fcntl(F_GETFL) failed");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        throw std::runtime_error("fcntl(F_SETFL) failed");
    }
}

std::string trim(const std::string &s)
{
    std::size_t left = 0;
    while (left < s.size() && std::isspace(static_cast<unsigned char>(s[left]))) ++left;

    std::size_t right = s.size();
    while (right > left && std::isspace(static_cast<unsigned char>(s[right - 1]))) --right;

    return s.substr(left, right - left);
}

bool startsWith(const std::string &s, const std::string &prefix)
{
    return s.rfind(prefix, 0) == 0;
}

bool isVariableName(const std::string &s)
{
    return s == "X" || s == "Y" || s == "Z";
}
}

CalculatorServer::CalculatorServer(int portValue) : port(portValue)
{
    loadUsers();
    openListenSocket();
}

CalculatorServer::~CalculatorServer()
{
    for (auto &[fd, _] : clients)
    {
        close(fd);
    }
    if (listenFd != -1)
    {
        close(listenFd);
    }
    saveUsers();
}

void CalculatorServer::openListenSocket()
{
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd == -1)
    {
        throw std::runtime_error("socket failed");
    }

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setNonBlocking(listenFd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
    }

    if (listen(listenFd, SOMAXCONN) == -1)
    {
        throw std::runtime_error("listen failed");
    }
}

void CalculatorServer::run()
{
    std::cout << "Server started on port " << port << std::endl;

    while (true)
    {
        std::vector<pollfd> fds;
        fds.push_back({listenFd, POLLIN, 0});

        for (auto &[fd, client] : clients)
        {
            short events = POLLIN;
            if (!client.output.empty())
            {
                events |= POLLOUT;
            }
            fds.push_back({fd, events, 0});
        }

        int rc = poll(fds.data(), fds.size(), -1);
        if (rc == -1)
        {
            if (errno == EINTR) continue;
            throw std::runtime_error("poll failed");
        }

        if (fds[0].revents & POLLIN)
        {
            acceptClient();
        }

        std::vector<int> toClose;

        for (std::size_t i = 1; i < fds.size(); ++i)
        {
            int fd = fds[i].fd;
            short revents = fds[i].revents;

            if (revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                toClose.push_back(fd);
                continue;
            }

            if (revents & POLLIN)
            {
                try { readFromClient(fd); }
                catch (...) { toClose.push_back(fd); }
            }

            if (clients.count(fd) && (revents & POLLOUT))
            {
                try { flushToClient(fd); }
                catch (...) { toClose.push_back(fd); }
            }
        }

        for (int fd : toClose)
        {
            closeClient(fd);
        }
    }
}

void CalculatorServer::acceptClient()
{
    while (true)
    {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);

        int fd = accept(listenFd, reinterpret_cast<sockaddr *>(&addr), &len);
        if (fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            throw std::runtime_error("accept failed");
        }

        setNonBlocking(fd);

        Client client;
        client.fd = fd;
        client.output = "HELLO enter username\n";
        clients[fd] = std::move(client);

        std::cout << "Client connected: fd=" << fd << std::endl;
    }
}

void CalculatorServer::closeClient(int fd)
{
    auto it = clients.find(fd);
    if (it != clients.end())
    {
        std::cout << "Client disconnected: fd=" << fd << std::endl;
        close(fd);
        clients.erase(it);
        saveUsers();
    }
}

void CalculatorServer::readFromClient(int fd)
{
    auto it = clients.find(fd);
    if (it == clients.end()) return;

    char buffer[4096];

    while (true)
    {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n > 0)
        {
            Client &client = it->second;
            client.input.append(buffer, buffer + n);

            std::size_t pos;
            while ((pos = client.input.find('\n')) != std::string::npos)
            {
                std::string line = client.input.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                client.input.erase(0, pos + 1);
                handleLine(client, trim(line));
            }
        }
        else if (n == 0)
        {
            throw std::runtime_error("client closed");
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            throw std::runtime_error("recv failed");
        }
    }
}

void CalculatorServer::flushToClient(int fd)
{
    auto it = clients.find(fd);
    if (it == clients.end()) return;

    Client &client = it->second;

    while (!client.output.empty())
    {
        ssize_t n = send(fd, client.output.data(), client.output.size(), 0);
        if (n > 0)
        {
            client.output.erase(0, static_cast<std::size_t>(n));
        }
        else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return;
        }
        else
        {
            throw std::runtime_error("send failed");
        }
    }
}

void CalculatorServer::handleLine(Client &client, const std::string &line)
{
    if (line.empty()) return;

    if (!client.hasUser)
    {
        client.user = line;
        client.hasUser = true;

        if (!users.count(client.user))
        {
            users[client.user] = Variables{};
            saveUsers();
        }

        client.output += "OK user " + client.user + " " + users[client.user].toString() + "\n";
        return;
    }

    if (line == "quit" || line == "exit" || line == "выход")
    {
        client.output += "BYE\n";
        return;
    }

    client.output += executeCommand(client.user, line) + "\n";
}

std::string CalculatorServer::executeCommand(const std::string &user, const std::string &line)
{
    if (line == "PING")
    {
        return "PONG";
    }

    Variables &vars = users[user];

    try
    {
        if (line == "show" || line == "показать")
        {
            return "OK " + vars.toString();
        }

        if (startsWith(line, "calc "))
        {
            long long value = evaluateExpression(line.substr(5), vars);
            return "OK " + std::to_string(value);
        }

        const std::string ruCalc = "вычислить ";
        if (startsWith(line, ruCalc))
        {
            long long value = evaluateExpression(line.substr(ruCalc.size()), vars);
            return "OK " + std::to_string(value);
        }

        std::size_t eq = line.find('=');
        if (eq != std::string::npos)
        {
            std::string left = trim(line.substr(0, eq));
            std::string expr = trim(line.substr(eq + 1));

            if (!isVariableName(left))
            {
                return "ERR слева от '=' должна быть переменная X, Y или Z";
            }

            long long value = evaluateExpression(expr, vars);
            vars.set(left[0], value);
            saveUsers();

            return "OK " + left + " = " + std::to_string(value);
        }

        return "ERR неизвестная команда";
    }
    catch (const std::exception &e)
    {
        return std::string("ERR ") + e.what();
    }
}

void CalculatorServer::loadUsers()
{
    std::ifstream in(DB_FILE);
    if (!in) return;

    std::string name;
    Variables v;
    while (in >> name >> v.x >> v.y >> v.z)
    {
        users[name] = v;
    }
}

void CalculatorServer::saveUsers() const
{
    std::ofstream out(DB_FILE, std::ios::trunc);
    for (const auto &[name, v] : users)
    {
        out << name << ' ' << v.x << ' ' << v.y << ' ' << v.z << '\n';
    }
}
