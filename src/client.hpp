#pragma once
#include <string>

class CalculatorClient
{
public:
    CalculatorClient(std::string host, int port);
    ~CalculatorClient();

    void runInteractive();
    void runPerformanceTest(int requestCount);

private:
    std::string host;
    int port;
    int fd = -1;

    void connectToServer();
    std::string readLine();
    void sendLine(const std::string &line);
};
