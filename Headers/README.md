# HERO - High-Efficiency Realtime Oriented Protocol

A lightweight, high-performance UDP networking library for games and realtime applications in C++. Features automatic packet fragmentation, connection management, and a complete game framework.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Core Examples](#core-examples)
- [Game Framework Examples](#game-framework-examples)
- [Advanced Examples](#advanced-examples)
- [API Reference](#api-reference)
- [Performance Tips](#performance-tips)

---

## Features

- ✅ **UDP-based** - Low latency, perfect for realtime games
- ✅ **Automatic fragmentation** - Send messages larger than MTU
- ✅ **Connection management** - CONN/STOP/PING/PONG protocol
- ✅ **Magic words** - Efficient command encoding for games
- ✅ **Cross-platform** - Windows, Linux, macOS
- ✅ **Game framework** - Entity system, state sync, leaderboards
- ✅ **Modern C++** - Uses C++17 features, STL containers
- ✅ **No dependencies** - Just standard library + sockets

---

## Installation

### Linux/macOS

```bash
# Include the header in your project
#include "HERO.h"

# Compile with C++17
g++ -std=c++17 your_game.cpp -o game -lpthread

# Or with Clang
clang++ -std=c++17 your_game.cpp -o game -lpthread
```

### Windows

```bash
# With MinGW
g++ -std=c++17 your_game.cpp -o game.exe -lws2_32

# With Visual Studio
cl /std:c++17 your_game.cpp /link ws2_32.lib
```

---

## Quick Start

### Simple Echo Server

```cpp
#include "HERO.h"
#include <iostream>

int main() {
    HERO::HeroServer server(9999);
    server.Start();
    std::cout << "Echo server running on port 9999\n";

    while (true) {
        server.Poll([&](const HERO::Packet& pkt, const std::string& host, uint16_t port) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());
            std::cout << "Received: " << msg << " from " << host << ":" << port << "\n";
            server.SendTo("Echo: " + msg, host, port);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
```

### Simple Client

```cpp
#include "HERO.h"
#include <iostream>

int main() {
    HERO::HeroClient client;
    
    if (client.Connect("127.0.0.1", 9999)) {
        std::cout << "Connected to server!\n";
        
        client.Send("Hello, Server!");
        
        std::string response;
        if (client.ReceiveString(response, 1000)) {
            std::cout << "Server says: " << response << "\n";
        }
        
        client.Disconnect();
    } else {
        std::cout << "Failed to connect\n";
    }

    return 0;
}
```

---

## Core Examples

### Example 1: Chat Server

```cpp
#include "HERO.h"
#include <iostream>
#include <set>

struct ChatClient {
    std::string host;
    uint16_t port;
    std::string username;
};

int main() {
    HERO::HeroServer server(9999);
    server.Start();
    std::map<std::string, ChatClient> clients;

    std::cout << "Chat server running on port 9999\n";

    while (true) {
        server.Poll([&](const HERO::Packet& pkt, const std::string& host, uint16_t port) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());
            std::string client_key = host + ":" + std::to_string(port);

            if (msg.substr(0, 5) == "JOIN|") {
                // New user joining
                std::string username = msg.substr(5);
                clients[client_key] = {host, port, username};
                
                std::cout << username << " joined the chat\n";
                
                // Broadcast to all clients
                std::string broadcast = "[SERVER] " + username + " joined the chat";
                for (auto& [key, c] : clients) {
                    server.SendTo(broadcast, c.host, c.port);
                }
            }
            else if (msg.substr(0, 5) == "CHAT|") {
                // Regular chat message
                std::string message = msg.substr(5);
                std::string username = clients[client_key].username;
                
                std::cout << username << ": " << message << "\n";
                
                // Broadcast to all clients
                std::string broadcast = "[" + username + "] " + message;
                for (auto& [key, c] : clients) {
                    server.SendTo(broadcast, c.host, c.port);
                }
            }
            else if (msg == "LEAVE") {
                // User leaving
                std::string username = clients[client_key].username;
                std::cout << username << " left the chat\n";
                
                clients.erase(client_key);
                
                std::string broadcast = "[SERVER] " + username + " left the chat";
                for (auto& [key, c] : clients) {
                    server.SendTo(broadcast, c.host, c.port);
                }
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
```

### Example 2: Chat Client

```cpp
#include "HERO.h"
#include <iostream>
#include <thread>
#include <atomic>

int main() {
    HERO::HeroClient client;
    std::atomic<bool> running(true);
    
    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    
    if (!client.Connect("127.0.0.1", 9999)) {
        std::cout << "Failed to connect to server\n";
        return 1;
    }
    
    // Send join message
    client.Send("JOIN|" + username);
    
    // Receive thread
    std::thread receive_thread([&]() {
        while (running) {
            std::string msg;
            if (client.ReceiveString(msg, 100)) {
                std::cout << msg << "\n";
            }
        }
    });
    
    // Send messages
    std::string input;
    while (running) {
        std::getline(std::cin, input);
        
        if (input == "/quit") {
            running = false;
            client.Send("LEAVE");
            break;
        }
        
        client.Send("CHAT|" + input);
    }
    
    receive_thread.join();
    client.Disconnect();
    
    return 0;
}
```

### Example 3: Using Magic Words (Game Commands)

```cpp
#include "HERO.h"
#include <iostream>

int main() {
    HERO::HeroClient client;
    
    if (client.Connect("127.0.0.1", 9999)) {
        // Send movement command with position
        client.SendCommand(HERO::MagicWords::MOVE, 100.5f, 250.3f);
        
        // Send attack command with target ID
        client.SendCommand(HERO::MagicWords::ATTACK, 42);
        
        // Send chat message
        client.SendCommand(HERO::MagicWords::CHAT, "Hello world!");
        
        // Receive and decode commands
        HERO::Packet pkt;
        while (client.Receive(pkt, 1000)) {
            auto [cmd, args] = HERO::MagicWords::Decode(pkt.payload);
            
            std::cout << "Command: " << cmd << "\n";
            std::cout << "Args: ";
            for (const auto& arg : args) {
                std::cout << arg << " ";
            }
            std::cout << "\n";
        }
        
        client.Disconnect();
    }
    
    return 0;
}
```

### Example 4: Ping/Pong and Latency Monitoring

```cpp
#include "HERO.h"
#include <iostream>

int main() {
    HERO::HeroClient client;
    
    if (client.Connect("127.0.0.1", 9999)) {
        std::cout << "Connected! Monitoring latency...\n";
        
        for (int i = 0; i < 10; i++) {
            if (client.Ping()) {
                std::cout << "Ping: " << client.GetPing() << " ms\n";
            } else {
                std::cout << "Ping failed!\n";
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        client.Disconnect();
    }
    
    return 0;
}
```

### Example 5: Broadcasting to Multiple Clients

```cpp
#include "HERO.h"
#include <iostream>

int main() {
    HERO::HeroServer server(9999);
    server.Start();
    
    std::cout << "Broadcast server running. Clients: " << server.GetClientCount() << "\n";
    
    int tick = 0;
    while (true) {
        // Handle incoming packets
        server.Poll([](const HERO::Packet& pkt, const std::string& host, uint16_t port) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());
            std::cout << "Received from " << host << ": " << msg << "\n";
        });
        
        // Broadcast server tick every second
        if (tick % 100 == 0) {
            std::string msg = "Server tick: " + std::to_string(tick);
            server.Broadcast(msg);
            std::cout << "Broadcasting: " << msg << " to " << server.GetClientCount() << " clients\n";
        }
        
        tick++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

---

## Game Framework Examples

### Example 6: Simple Multiplayer Game Server

```cpp
#include "HERO.h"
#include <iostream>

using namespace HERO::Game;

int main() {
    HERO::HeroServer server(9999);
    server.Start();
    
    std::unordered_map<std::string, Entity> players;
    GameState game_state;
    
    // Initialize game state
    game_state.Set("game_started", false);
    game_state.Set("player_count", 0);
    
    std::cout << "Game server running on port 9999\n";
    
    int tick = 0;
    while (true) {
        server.Poll([&](const HERO::Packet& pkt, const std::string& host, uint16_t port) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());
            size_t pipe = msg.find('|');
            if (pipe == std::string::npos) return;
            
            std::string cmd = msg.substr(0, pipe);
            std::string data = msg.substr(pipe + 1);
            std::string player_key = host + ":" + std::to_string(port);
            
            if (cmd == "JOIN") {
                // Player joining
                Entity player(data);
                player.position = Vector2(rand() % 800, rand() % 600);
                player.SetProperty("health", "100");
                player.SetProperty("score", "0");
                players[player_key] = player;
                
                // Update game state
                game_state.Set("player_count", static_cast<int>(players.size()));
                
                std::cout << data << " joined the game at " 
                         << player.position.x << "," << player.position.y << "\n";
                
                // Send full state to new player
                server.SendTo("STATE|" + game_state.Serialize(), host, port);
                
                // Send all existing players
                for (const auto& [key, p] : players) {
                    server.SendTo("PLAYER|" + p.Serialize(), host, port);
                }
                
                // Broadcast new player to others
                std::string spawn_msg = "SPAWN|" + player.Serialize();
                for (const auto& [key, p] : players) {
                    if (key != player_key) {
                        size_t colon = key.find(':');
                        std::string h = key.substr(0, colon);
                        uint16_t pt = std::stoi(key.substr(colon + 1));
                        server.SendTo(spawn_msg, h, pt);
                    }
                }
            }
            else if (cmd == "MOVE") {
                // Player movement
                if (players.find(player_key) != players.end()) {
                    Vector2 new_pos = Vector2::FromString(data);
                    players[player_key].position = new_pos;
                    
                    // Broadcast position to all players
                    std::string move_msg = "PLAYERMOVE|" + players[player_key].id + "|" + data;
                    for (const auto& [key, p] : players) {
                        size_t colon = key.find(':');
                        std::string h = key.substr(0, colon);
                        uint16_t pt = std::stoi(key.substr(colon + 1));
                        server.SendTo(move_msg, h, pt);
                    }
                }
            }
            else if (cmd == "ATTACK") {
                // Player attack
                std::cout << players[player_key].id << " attacks!\n";
                
                std::string attack_msg = "ATTACK|" + players[player_key].id;
                for (const auto& [key, p] : players) {
                    size_t colon = key.find(':');
                    std::string h = key.substr(0, colon);
                    uint16_t pt = std::stoi(key.substr(colon + 1));
                    server.SendTo(attack_msg, h, pt);
                }
            }
            else if (cmd == "LEAVE") {
                // Player leaving
                if (players.find(player_key) != players.end()) {
                    std::string player_id = players[player_key].id;
                    players.erase(player_key);
                    
                    game_state.Set("player_count", static_cast<int>(players.size()));
                    
                    std::cout << player_id << " left the game\n";
                    
                    // Broadcast player left
                    std::string leave_msg = "PLAYERLEFT|" + player_id;
                    for (const auto& [key, p] : players) {
                        size_t colon = key.find(':');
                        std::string h = key.substr(0, colon);
                        uint16_t pt = std::stoi(key.substr(colon + 1));
                        server.SendTo(leave_msg, h, pt);
                    }
                }
            }
        });
        
        // Game tick - update physics, etc.
        if (tick % 10 == 0) {
            for (auto& [key, player] : players) {
                player.Update(0.016f); // ~60 FPS
            }
        }
        
        tick++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

### Example 7: Game Client with Entity Sync

```cpp
#include "HERO.h"
#include <iostream>

using namespace HERO::Game;

int main() {
    GameClient client;
    
    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    
    if (!client.Connect("127.0.0.1", 9999, username)) {
        std::cout << "Failed to connect\n";
        return 1;
    }
    
    std::cout << "Connected! You are: " << username << "\n";
    
    // Game loop
    bool running = true;
    while (running) {
        // Update - receive server messages
        client.Update([&](const std::string& cmd, const std::string& data) {
            if (cmd == "SPAWN") {
                Entity player = Entity::Deserialize(data);
                std::cout << "Player " << player.id << " spawned at " 
                         << player.position.x << "," << player.position.y << "\n";
            }
            else if (cmd == "PLAYERMOVE") {
                size_t pipe = data.find('|');
                std::string player_id = data.substr(0, pipe);
                Vector2 pos = Vector2::FromString(data.substr(pipe + 1));
                std::cout << player_id << " moved to " << pos.x << "," << pos.y << "\n";
            }
            else if (cmd == "ATTACK") {
                std::cout << data << " attacked!\n";
            }
            else if (cmd == "PLAYERLEFT") {
                std::cout << data << " left the game\n";
            }
        });
        
        // Simple command interface
        std::cout << "\nCommands: move x,y | attack | quit\n> ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "quit") {
            running = false;
            client.SendCommand("LEAVE");
        }
        else if (input.substr(0, 5) == "move ") {
            std::string coords = input.substr(5);
            client.SendCommand("MOVE", coords);
        }
        else if (input == "attack") {
            client.SendCommand("ATTACK");
        }
        
        // Display all entities
        std::cout << "\nPlayers in game:\n";
        for (const auto& [id, entity] : client.GetEntities()) {
            std::cout << "  " << id << " at (" 
                     << entity.position.x << "," << entity.position.y << ")\n";
        }
    }
    
    client.Disconnect();
    return 0;
}
```

### Example 8: Leaderboard Server

```cpp
#include "HERO.h"
#include <iostream>

using namespace HERO::Game;

int main() {
    HERO::HeroServer server(9999);
    server.Start();
    Leaderboard leaderboard;
    
    std::cout << "Leaderboard server running on port 9999\n";
    
    while (true) {
        server.Poll([&](const HERO::Packet& pkt, const std::string& host, uint16_t port) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());
            size_t pipe = msg.find('|');
            if (pipe == std::string::npos) return;
            
            std::string cmd = msg.substr(0, pipe);
            std::string data = msg.substr(pipe + 1);
            
            if (cmd == "SUBMIT") {
                // Parse player_id|score
                size_t pipe2 = data.find('|');
                std::string player_id = data.substr(0, pipe2);
                int score = std::stoi(data.substr(pipe2 + 1));
                
                leaderboard.AddScore(player_id, score);
                std::cout << player_id << " scored " << score << "\n";
                
                // Send updated leaderboard
                std::stringstream ss;
                ss << "LEADERBOARD|";
                auto top = leaderboard.GetTop(10);
                for (const auto& [id, sc] : top) {
                    ss << id << "=" << sc << ";";
                }
                
                server.Broadcast(ss.str());
            }
            else if (cmd == "RANK") {
                // Get player rank
                std::string player_id = data;
                int rank = leaderboard.GetRank(player_id);
                
                std::string response = "RANK|" + std::to_string(rank);
                server.SendTo(response, host, port);
            }
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

---

## Advanced Examples

### Example 9: Custom Protocol with Magic Words

```cpp
#include "HERO.h"
#include <iostream>

// Define custom game commands
namespace MyGame {
    const std::string BUILD = "BLD";
    const std::string DESTROY = "DST";
    const std::string TRADE = "TRD";
    const std::string CRAFT = "CRF";
}

int main() {
    HERO::HeroClient client;
    
    // Register custom magic words
    HERO::MagicWords::Register("BUILD", MyGame::BUILD);
    HERO::MagicWords::Register("DESTROY", MyGame::DESTROY);
    
    if (client.Connect("127.0.0.1", 9999)) {
        // Use custom commands
        client.SendCommand(MyGame::BUILD, "house", 100, 200);
        client.SendCommand(MyGame::DESTROY, "tree", 150, 250);
        client.SendCommand(MyGame::TRADE, "player123", "gold", 50);
        client.SendCommand(MyGame::CRAFT, "sword", "iron", 10);
        
        // Receive and decode
        HERO::Packet pkt;
        while (client.Receive(pkt, 1000)) {
            auto [cmd, args] = HERO::MagicWords::Decode(pkt.payload);
            
            if (cmd == MyGame::BUILD) {
                std::cout << "Building " << args[0] << " at " 
                         << args[1] << "," << args[2] << "\n";
            }
            else if (cmd == MyGame::DESTROY) {
                std::cout << "Destroying " << args[0] << " at " 
                         << args[1] << "," << args[2] << "\n";
            }
        }
        
        client.Disconnect();
    }
    
    return 0;
}
```

### Example 10: Physics-Based Game Server

```cpp
#include "HERO.h"
#include <iostream>
#include <cmath>

using namespace HERO::Game;

// Simple physics constants
const float GRAVITY = 980.0f;
const float JUMP_FORCE = -500.0f;
const float MOVE_SPEED = 200.0f;

struct PhysicsEntity : public Entity {
    Vector2 acceleration;
    bool on_ground;
    
    PhysicsEntity(const std::string& id = "") 
        : Entity(id), on_ground(false) {}
    
    void ApplyPhysics(float dt) {
        // Apply gravity
        if (!on_ground) {
            acceleration.y = GRAVITY;
        } else {
            acceleration.y = 0;
            velocity.y = 0;
        }
        
        // Update velocity
        velocity = velocity + acceleration * dt;
        
        // Update position
        position = position + velocity * dt;
        
        // Ground collision (simple)
        if (position.y >= 600) {
            position.y = 600;
            velocity.y = 0;
            on_ground = true;
        } else {
            on_ground = false;
        }
    }
};

int main() {
    HERO::HeroServer server(9999);
    server.Start();
    
    std::unordered_map<std::string, PhysicsEntity> players;
    
    std::cout << "Physics game server running\n";
    
    auto last_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;
        
        server.Poll([&](const HERO::Packet& pkt, const std::string& host, uint16_t port) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());
            size_t pipe = msg.find('|');
            if (pipe == std::string::npos) return;
            
            std::string cmd = msg.substr(0, pipe);
            std::string data = msg.substr(pipe + 1);
            std::string player_key = host + ":" + std::to_string(port);
            
            if (cmd == "JOIN") {
                PhysicsEntity player(data);
                player.position = Vector2(400, 100);
                players[player_key] = player;
                std::cout << data << " joined\n";
            }
            else if (cmd == "MOVE") {
                if (players.find(player_key) != players.end()) {
                    if (data == "left") {
                        players[player_key].velocity.x = -MOVE_SPEED;
                    } else if (data == "right") {
                        players[player_key].velocity.x = MOVE_SPEED;
                    } else if (data == "stop") {
                        players[player_key].velocity.x = 0;
                    }
                }
            }
            else if (cmd == "JUMP") {
                if (players.find(player_key) != players.end()) {
                    if (players[player_key].on_ground) {
                        players[player_key].velocity.y = JUMP_FORCE;
                        players[player_key].on_ground = false;
                    }
                }
            }
        });
        
        // Update physics for all players
        for (auto& [key, player] : players) {
            player.ApplyPhysics(dt);
            
            // Broadcast position
            size_t colon = key.find(':');
            std::string host = key.substr(0, colon);
            uint16_t port = std::stoi(key.substr(colon + 1));
            
            std::string pos_msg = "POS|" + player.id + "|" + player.position.ToString();
            server.Broadcast(pos_msg);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    return 0;
}
```

### Example 11: Reliable Message Queue with Retries

```cpp
#include "HERO.h"
#include <iostream>
#include <queue>

struct ReliableMessage {
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point send_time;
    int retries;
    uint16_t seq;
};

class ReliableClient {
private:
    HERO::HeroClient client;
    std::queue<ReliableMessage> pending_messages;
    std::set<uint16_t> acknowledged;
    uint16_t next_seq;
    
public:
    ReliableClient() : next_seq(0) {}
    
    bool Connect(const std::string& host, uint16_t port) {
        return client.Connect(host, port);
    }
    
    void SendReliable(const std::string& msg) {
        ReliableMessage rm;
        rm.data = std::vector<uint8_t>(msg.begin(), msg.end());
        rm.send_time = std::chrono::steady_clock::now();
        rm.retries = 0;
        rm.seq = next_seq++;
        
        // Prepend sequence number
        std::string with_seq = "SEQ:" + std::to_string(rm.seq) + "|" + msg;
        rm.data = std::vector<uint8_t>(with_seq.begin(), with_seq.end());
        
        pending_messages.push(rm);
        client.Send(rm.data);
        
        std::cout << "Sent message (seq=" << rm.seq << "): " << msg << "\n";
    }
    
    void Update() {
        // Check for acknowledgments
        HERO::Packet pkt;
        while (client.Receive(pkt, 10)) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());
            
            if (msg.substr(0, 4) == "ACK:") {
                uint16_t ack_seq = std::stoi(msg.substr(4));
                acknowledged.insert(ack_seq);
                std::cout << "Received ACK for seq=" << ack_seq << "\n";
            }
        }
        
        // Remove acknowledged messages
        while (!pending_messages.empty()) {
            auto& msg = pending_messages.front();
            if (acknowledged.count(msg.seq)) {
                pending_messages.pop();
            } else {
                break;
            }
        }
        
        // Retry unacknowledged messages
        if (!pending_messages.empty()) {
            auto& msg = pending_messages.front();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - msg.send_time).count();
            
            if (elapsed > 1000 && msg.retries < 5) {
                client.Send(msg.data);
                msg.send_time = now;
                msg.retries++;
                std::cout << "Retrying message (seq=" << msg.seq 
                         << ", retry=" << msg.retries << ")\n";
            }
        }
    }
    
    void Disconnect() {
        client.Disconnect();
    }
};

int main() {
    ReliableClient client;
    
    if (client.Connect("127.0.0.1", 9999)) {
        std::cout << "Connected with reliable delivery\n";
        
        // Send some messages
        client.SendReliable("Important message 1");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        client.SendReliable("Important message 2");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        client.SendReliable("Important message 3");
        
        // Keep checking for acknowledgments
        for (int i = 0; i < 100; i++) {
            client.Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        client.Disconnect();
    }
    
    return 0;
}
```

### Example 12: Complete MMO-Style Game Loop

```cpp
#include "HERO.h"
#include <iostream>
#include <thread>
#include <atomic>

using namespace HERO::Game;

class MMOServer {
private:
    HERO::HeroServer server;
    std::unordered_map<std::string, Entity> players;
    std::unordered_map<std::string, Entity> npcs;
    GameState world_state;
    Leaderboard leaderboard;
    std::atomic<bool> running;
    int tick_count;
    
public:
    MMOServer(uint16_t port) : server(port), running(false), tick_count(0) {
        // Initialize world
        world_state.Set("time_of_day", 12);
        world_state.Set("weather", "sunny");
        
        // Spawn some NPCs
        for (int i = 0; i < 5; i++) {
            Entity npc("npc_" + std::to_string(i));
            npc.position = Vector2(rand() % 1000, rand() % 1000);
            npc.SetProperty("type", "merchant");
            npc.SetProperty("health", "100");
            npcs[npc.id] = npc;
        }
    }
    
    void Start() {
        server.Start();
        running = true;
        std::cout << "MMO Server started on port 9999\n";
        
        // Game loop thread
        std::thread game_loop([this]() {
            auto last_tick = std::chrono::steady_clock::now();
            
            while (running) {
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - last_tick).count();
                last_tick = now;
                
                Tick(dt);
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 TPS
            }
        });
        
        // Network thread
        while (running) {
            server.Poll([this](const HERO::Packet& pkt, const std::string& host, uint16_t port) {
                HandlePacket(pkt, host, port);
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        game_loop.join();
    }
    
    void Stop() {
        running = false;
    }
    
private:
    void HandlePacket(const HERO::Packet& pkt, const std::string& host, uint16_t port) {
        std::string msg(pkt.payload.begin(), pkt.payload.end());
        size_t pipe = msg.find('|');
        if (pipe == std::string::npos) return;
        
        std::string cmd = msg.substr(0, pipe);
        std::string data = msg.substr(pipe + 1);
        std::string player_key = host + ":" + std::to_string(port);
        
        if (cmd == "JOIN") {
            // Player login
            Entity player(data);
            player.position = Vector2(500, 500);
            player.SetProperty("health", "100");
            player.SetProperty("level", "1");
            player.SetProperty("gold", "0");
            players[player_key] = player;
            
            std::cout << data << " joined the world\n";
            
            // Send world snapshot
            server.SendTo("STATE|" + world_state.Serialize(), host, port);
            
            // Send all NPCs
            for (const auto& [id, npc] : npcs) {
                server.SendTo("NPC|" + npc.Serialize(), host, port);
            }
            
            // Send all players
            for (const auto& [key, p] : players) {
                server.SendTo("PLAYER|" + p.Serialize(), host, port);
            }
            
            // Broadcast new player
            BroadcastExcept("SPAWN|" + player.Serialize(), player_key);
        }
        else if (cmd == "MOVE") {
            if (players.find(player_key) != players.end()) {
                players[player_key].position = Vector2::FromString(data);
                BroadcastExcept("PMOVE|" + players[player_key].id + "|" + data, player_key);
            }
        }
        else if (cmd == "ATTACK") {
            // Process attack
            if (players.find(player_key) != players.end()) {
                std::string target_id = data;
                std::cout << players[player_key].id << " attacks " << target_id << "\n";
                
                // Apply damage, etc.
                Broadcast("COMBAT|" + players[player_key].id + "|" + target_id);
            }
        }
        else if (cmd == "INTERACT") {
            // Interact with NPC
            std::string npc_id = data;
            if (npcs.find(npc_id) != npcs.end()) {
                std::string response = "DIALOG|" + npc_id + "|Hello traveler!";
                server.SendTo(response, host, port);
            }
        }
        else if (cmd == "SCORE") {
            int score = std::stoi(data);
            if (players.find(player_key) != players.end()) {
                leaderboard.AddScore(players[player_key].id, score);
                
                // Broadcast leaderboard update
                std::stringstream ss;
                ss << "LEADERBOARD|";
                auto top = leaderboard.GetTop(10);
                for (const auto& [id, sc] : top) {
                    ss << id << "=" << sc << ";";
                }
                Broadcast(ss.str());
            }
        }
    }
    
    void Tick(float dt) {
        tick_count++;
        
        // Update NPCs
        for (auto& [id, npc] : npcs) {
            // Simple wandering AI
            if (tick_count % 100 == 0) {
                float angle = (rand() % 360) * 3.14159f / 180.0f;
                npc.velocity = Vector2(cos(angle) * 50, sin(angle) * 50);
            }
            npc.Update(dt);
        }
        
        // Update world state
        if (tick_count % 1200 == 0) { // Every minute
            int time = world_state.GetInt("time_of_day");
            time = (time + 1) % 24;
            world_state.Set("time_of_day", time);
            Broadcast("TIME|" + std::to_string(time));
        }
        
        // Broadcast NPC positions occasionally
        if (tick_count % 20 == 0) {
            for (const auto& [id, npc] : npcs) {
                Broadcast("NPCMOVE|" + id + "|" + npc.position.ToString());
            }
        }
    }
    
    void Broadcast(const std::string& msg) {
        for (const auto& [key, player] : players) {
            size_t colon = key.find(':');
            std::string host = key.substr(0, colon);
            uint16_t port = std::stoi(key.substr(colon + 1));
            server.SendTo(msg, host, port);
        }
    }
    
    void BroadcastExcept(const std::string& msg, const std::string& except_key) {
        for (const auto& [key, player] : players) {
            if (key != except_key) {
                size_t colon = key.find(':');
                std::string host = key.substr(0, colon);
                uint16_t port = std::stoi(key.substr(colon + 1));
                server.SendTo(msg, host, port);
            }
        }
    }
};

int main() {
    MMOServer server(9999);
    server.Start();
    return 0;
}
```

---

## API Reference

### HeroClient

```cpp
// Connection
bool Connect(const std::string& host, uint16_t port, const std::vector<uint8_t>& pubkey = {1,2,3,4});
void Disconnect();
bool IsConnected() const;

// Sending
bool Send(const std::vector<uint8_t>& data, const std::vector<uint8_t>& recipient_key = {});
bool Send(const std::string& text, const std::vector<uint8_t>& recipient_key = {});
template<typename... Args>
bool SendCommand(const std::string& command, Args... args);

// Receiving
bool Receive(Packet& out_packet, int timeout_ms = 100);
bool ReceiveString(std::string& out_text, int timeout_ms = 100);

// Utilities
bool Ping();
void KeepAlive();
int GetPing() const;
```

### HeroServer

```cpp
// Lifecycle
void Start();
void Stop();
bool IsRunning() const;

// Networking
bool Poll(std::function<void(const Packet&, const std::string&, uint16_t)> handler = nullptr);

// Sending
void SendTo(const std::vector<uint8_t>& data, const std::string& host, uint16_t port);
void SendTo(const std::string& text, const std::string& host, uint16_t port);
void Broadcast(const std::vector<uint8_t>& data);
void Broadcast(const std::string& text);

// Utilities
int GetClientCount() const;
```

### GameState

```cpp
void Set(const std::string& key, const std::string& value);
void Set(const std::string& key, int value);
void Set(const std::string& key, float value);
void Set(const std::string& key, bool value);

std::string Get(const std::string& key, const std::string& default_val = "") const;
int GetInt(const std::string& key, int default_val = 0) const;
float GetFloat(const std::string& key, float default_val = 0.0f) const;
bool GetBool(const std::string& key, bool default_val = false) const;

std::string Serialize() const;
void Deserialize(const std::string& data);
uint32_t GetVersion() const;
```

### Entity

```cpp
std::string id;
Vector2 position;
Vector2 velocity;

void SetProperty(const std::string& key, const std::string& value);
std::string GetProperty(const std::string& key, const std::string& default_val = "") const;
void Update(float deltaTime);

std::string Serialize() const;
static Entity Deserialize(const std::string& data);
```

### Vector2

```cpp
Vector2(float x, float y);

Vector2 operator+(const Vector2& other) const;
Vector2 operator-(const Vector2& other) const;
Vector2 operator*(float scalar) const;

float Length() const;
Vector2 Normalized() const;
float Distance(const Vector2& other) const;

std::string ToString() const;
static Vector2 FromString(const std::string& str);
```

---

## Performance Tips

1. **Batch Messages**: Instead of sending many small packets, batch them together
   ```cpp
   std::vector<std::string> messages = {"msg1", "msg2", "msg3"};
   std::string batched = "";
   for (const auto& msg : messages) {
       batched += msg + ";";
   }
   client.Send(batched);
   ```

2. **Use Magic Words**: They're more efficient than full strings
   ```cpp
   // Instead of: client.Send("MOVE|100|200");
   client.SendCommand(MagicWords::MOVE, 100, 200); // More efficient
   ```

3. **Reduce Broadcast Frequency**: Don't broadcast every frame
   ```cpp
   if (tick % 3 == 0) { // Broadcast every 3 ticks
       server.Broadcast(update);
   }
   ```

4. **Use Dead Reckoning**: Predict entity positions client-side
   ```cpp
   // Client-side prediction
   entity.position = entity.position + entity.velocity * dt;
   ```

5. **Prioritize Important Updates**: Send critical data more frequently
   ```cpp
   // Player position every tick
   if (important) {
       server.Broadcast(player_update);
   }
   // Environmental updates less frequently
   if (tick % 60 == 0) {
       server.Broadcast(env_update);
   }
   ```

6. **Use Fixed Timestep**: For consistent physics
   ```cpp
   const float FIXED_DT = 1.0f / 60.0f;
   float accumulator = 0.0f;
   
   while (running) {
       float frame_time = /* calculate */;
       accumulator += frame_time;
       
       while (accumulator >= FIXED_DT) {
           Update(FIXED_DT);
           accumulator -= FIXED_DT;
       }
   }
   ```

---

## License

MIT License - Feel free to use in your projects!

## Contributing

Contributions welcome! This is a learning-focused library.

## Support

For issues or questions, check the examples above or the source code comments.
