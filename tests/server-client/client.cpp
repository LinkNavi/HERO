#include "../../HERO.h"
#include <iostream>
#include <thread>

int main() {
    HERO::HeroClient client;
    
    std::cout << "Connecting to server at 127.0.0.1:8080...\n";
    
    if (!client.connect("127.0.0.1", 8080)) {
        std::cerr << "Failed to connect to server!\n";
        return 1;
    }
    
    std::cout << "Connected! Type messages to send (Ctrl+C to quit)\n\n";
    
    // Start a receiver thread
    std::thread receiver([&client]() {
        while (client.isConnected()) {
            HERO::Packet pkt;
            if (client.receive(pkt, 100)) {
                // Only show GIVE packets (actual data), ignore SEEN acknowledgments
                if (pkt.flag == HERO::GIVE && !pkt.payload.empty()) {
                    std::string response(pkt.payload.begin(), pkt.payload.end());
                    std::cout << "\n[Server]: " << response << "\n> " << std::flush;
                }
            }
        }
    });
    
    // Main input loop
    std::string input;
    while (client.isConnected()) {
        std::cout << "> ";
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        if (input == "quit" || input == "exit") {
            break;
        }
        
        if (!client.send(input)) {
            std::cerr << "Failed to send message!\n";
            break;
        }
    }
    
    std::cout << "\nDisconnecting...\n";
    client.disconnect();
    
    // Wait for receiver thread to finish
    if (receiver.joinable()) {
        receiver.join();
    }
    
    return 0;
}
