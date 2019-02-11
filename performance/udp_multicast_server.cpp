//
// Created by Ivan Shynkarenka on 29.03.2018
//

#include "server/asio/service.h"
#include "server/asio/udp_server.h"
#include "system/cpu.h"
#include "threads/thread.h"
#include "time/timestamp.h"

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

#include <OptionParser.h>

using namespace CppCommon;
using namespace CppServer::Asio;

class MulticastServer : public UDPServer
{
public:
    using UDPServer::UDPServer;

protected:
    void onError(int error, const std::string& category, const std::string& message) override
    {
        std::cout << "Server caught an error with code " << error << " and category '" << category << "': " << message << std::endl;
    }
};

int main(int argc, char** argv)
{
    auto parser = optparse::OptionParser().version("1.0.0.0");

    parser.add_option("-h", "--help").help("Show help");
    parser.add_option("-a", "--address").set_default("239.255.0.1").help("Server address. Default: %default");
    parser.add_option("-p", "--port").action("store").type("int").set_default(3333).help("Server port. Default: %default");
    parser.add_option("-t", "--threads").action("store").type("int").set_default(CPU::PhysicalCores()).help("Count of working threads. Default: %default");
    parser.add_option("-m", "--messages").action("store").type("int").set_default(1000000).help("Rate of messages per second to send. Default: %default");
    parser.add_option("-s", "--size").action("store").type("int").set_default(32).help("Single message size. Default: %default");

    optparse::Values options = parser.parse_args(argc, argv);

    // Print help
    if (options.get("help"))
    {
        parser.print_help();
        return 0;
    }

    // Server port
    std::string address(options.get("address"));
    int port = options.get("port");
    int threads = options.get("threads");
    int messages_rate = options.get("messages");
    int message_size = options.get("size");

    std::cout << "Server address: " << address << std::endl;
    std::cout << "Server port: " << port << std::endl;
    std::cout << "Working threads: " << threads << std::endl;
    std::cout << "Messages rate: " << messages_rate << std::endl;
    std::cout << "Message size: " << message_size << std::endl;

    std::cout << std::endl;

    // Create a new Asio service
    auto service = std::make_shared<Service>(threads);

    // Start the Asio service
    std::cout << "Asio service starting...";
    service->Start();
    std::cout << "Done!" << std::endl;

    // Create a new multicast server
    auto server = std::make_shared<MulticastServer>(service, 0);
    server->SetupReuseAddress(true);
    server->SetupReusePort(true);

    // Start the server
    std::cout << "Server starting...";
    server->Start(address, port);
    std::cout << "Done!" << std::endl;

    // Start the multicasting thread
    std::atomic<bool> multicasting(true);
    auto multicaster = std::thread([&server, &multicasting, messages_rate, message_size]()
    {
        // Prepare message to multicast
        std::vector<uint8_t> message_to_send(message_size);

        // Multicasting loop
        while (multicasting)
        {
            auto start = UtcTimestamp();
            for (int i = 0; i < messages_rate; ++i)
                server->Multicast(message_to_send.data(), message_to_send.size());
            auto end = UtcTimestamp();

            // Sleep for remaining time or yield
            auto milliseconds = (end - start).milliseconds();
            if (milliseconds < 1000)
                Thread::Sleep(milliseconds);
            else
                Thread::Yield();
        }
    });

    std::cout << "Press Enter to stop the server or '!' to restart the server..." << std::endl;

    // Perform text input
    std::string line;
    while (getline(std::cin, line))
    {
        if (line.empty())
            break;

        // Restart the server
        if (line == "!")
        {
            std::cout << "Server restarting...";
            server->Restart();
            std::cout << "Done!" << std::endl;
            continue;
        }
    }

    // Stop the multicasting thread
    multicasting = false;
    multicaster.join();

    // Stop the server
    std::cout << "Server stopping...";
    server->Stop();
    std::cout << "Done!" << std::endl;

    // Stop the Asio service
    std::cout << "Asio service stopping...";
    service->Stop();
    std::cout << "Done!" << std::endl;

    return 0;
}
