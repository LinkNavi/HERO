#include "../../HERO.h"
#include <iostream>

int main() {
  HERO::HeroServer server(8080);
  server.start();

  std::cout << "Server listening on port 8080...\n";

  while (true) {
    server.poll(
        [&](const HERO::Packet &pkt, const std::string &host, uint16_t port) {
          // Only respond to GIVE packets (actual messages), not SEEN acknowledgments
          if (pkt.flag == HERO::GIVE && !pkt.payload.empty()) {
            std::string message(pkt.payload.begin(), pkt.payload.end());
            std::cout << "Received from " << host << ":" << port << " - " << message << "\n";
            server.reply(pkt, "Message received!", host, port);
          }
        });
  }
}
