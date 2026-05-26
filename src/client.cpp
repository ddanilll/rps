#include "client.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

CalculatorClient::CalculatorClient(std::string hostValue, int portValue)
    : host(std::move(hostValue)), port(portValue)
{
    connectToServer();
}

CalculatorClient::~CalculatorClient()
{
    if (fd != -1)
    {
        close(fd);
    }
}

void CalculatorClient::connectToServer()
{
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        throw std::runtime_error("socket failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    {
        throw std::runtime_error("адрес должен быть IPv4, например 127.0.0.1");
    }

    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        throw std::runtime_error(std::string("connect failed: ") + std::strerror(errno));
    }
}

std::string CalculatorClient::readLine()
{
    std::string result;
    char c = 0;

    while (true)
    {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 1)
        {
            if (c == '\n') return result;
            if (c != '\r') result.push_back(c);
        }
        else if (n == 0)
        {
            throw std::runtime_error("сервер закрыл соединение");
        }
        else
        {
            throw std::runtime_error("recv failed");
        }
    }
}

void CalculatorClient::sendLine(const std::string &line)
{
    std::string data = line + "\n";
    std::size_t sent = 0;

    while (sent < data.size())
    {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0)
        {
            throw std::runtime_error("send failed");
        }
        sent += static_cast<std::size_t>(n);
    }
}

void CalculatorClient::runInteractive()
{
    std::cout << "SERVER: " << readLine() << std::endl;

    std::cout << "Введите имя пользователя: ";
    std::string username;
    std::getline(std::cin, username);
    sendLine(username);

    std::cout << "SERVER: " << readLine() << std::endl;
    std::cout << "\nКоманды: X = 1+2, вычислить X*3, показать, выход\n";

    std::string line;
    while (true)
    {
        std::cout << "\n> ";
        if (!std::getline(std::cin, line)) break;

        sendLine(line);
        std::string answer = readLine();
        std::cout << "SERVER: " << answer << std::endl;

        if (answer == "BYE") break;
    }
}

void CalculatorClient::runPerformanceTest(int requestCount)
{
    readLine();              // HELLO
    sendLine("perf_user");
    readLine();              // OK user

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < requestCount; ++i)
    {
        sendLine("PING");
        std::string answer = readLine();
        if (answer != "PONG")
        {
            throw std::runtime_error("unexpected answer in perf test: " + answer);
        }
    }

    auto finish = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = finish - start;

    double seconds = elapsed.count();
    double rps = requestCount / seconds;
    double avgMs = seconds * 1000.0 / requestCount;

    std::cout << "Requests: " << requestCount << std::endl;
    std::cout << "Time: " << seconds << " sec" << std::endl;
    std::cout << "RPS: " << rps << std::endl;
    std::cout << "Average latency: " << avgMs << " ms" << std::endl;
}
