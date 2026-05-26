#include "client.hpp"
#include "server.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void printUsage(const char *program)
{
    std::cout
        << "Usage:\n"
        << "  " << program << " server <port>\n"
        << "  " << program << " client <host> <port>\n"
        << "  " << program << " client <host> <port> --perf <requests>\n";
}

int parsePort(const std::string &text)
{
    int port = std::stoi(text);
    if (port <= 0 || port > 65535)
    {
        throw std::runtime_error("port must be in range 1..65535");
    }
    return port;
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc < 3)
        {
            printUsage(argv[0]);
            return 1;
        }

        std::string mode = argv[1];

        if (mode == "server")
        {
            int port = parsePort(argv[2]);
            CalculatorServer server(port);
            server.run();
            return 0;
        }

        if (mode == "client")
        {
            if (argc < 4)
            {
                printUsage(argv[0]);
                return 1;
            }

            std::string host = argv[2];
            int port = parsePort(argv[3]);

            CalculatorClient client(host, port);

            if (argc == 6 && std::string(argv[4]) == "--perf")
            {
                int requests = std::stoi(argv[5]);
                client.runPerformanceTest(requests);
            }
            else
            {
                client.runInteractive();
            }

            return 0;
        }

        printUsage(argv[0]);
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
