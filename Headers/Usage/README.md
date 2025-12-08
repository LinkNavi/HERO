# HERO Protocol Documentation

## Table of Contents
1. [Overview](#overview)
2. [Protocol Specification](#protocol-specification)
3. [Core Components](#core-components)
4. [API Reference](#api-reference)
5. [Examples](#examples)
6. [Best Practices](#best-practices)

---

## Overview

**HERO** (High-Efficiency Reliable Object protocol) is a lightweight UDP-based networking protocol designed for fast, efficient communication between clients and servers. It combines the speed and low latency of UDP with reliability features like acknowledgments and connection management.

### Key Features

- **Fast**: UDP-based for minimal latency (typically <5ms on local networks)
- **Reliable**: Built-in acknowledgment system ensures packet delivery
- **Simple**: Easy-to-use C++ API with minimal dependencies
- **Cross-platform**: Supports Linux, macOS, and Windows
- **Flexible**: Supports both raw protocol access and high-level abstractions (web server, browser)

### Use Cases

- Real-time web applications requiring low latency
- IoT device communication and monitoring
- Game networking and multiplayer systems
- Microservices architecture with fast IPC
- Custom protocol implementations
- Embedded systems with resource constraints

---

## Protocol Specification

### Packet Structure

Every HERO packet consists of an 8-byte header followed by optional requirements and payload data:

```
┌─────────────────────────────────────────────────┐
│ Byte 0: Flag (packet type)                      │
│ Byte 1: Version (protocol version = 1)          │
│ Bytes 2-3: Sequence Number (uint16_t, big-endian)│
│ Bytes 4-5: Payload Length (uint16_t, big-endian)│
│ Bytes 6-7: Requirements Length (uint16_t, big-endian)│
├─────────────────────────────────────────────────┤
│ Requirements Data (variable length)             │
├─────────────────────────────────────────────────┤
│ Payload Data (variable length)                  │
└─────────────────────────────────────────────────┘
```

### Packet Types (Flags)

| Flag | Value | Name | Purpose |
|------|-------|------|---------|
| CONN | 0 | Connection | Establish connection with server |
| GIVE | 1 | Give Data | Send data payload to recipient |
| TAKE | 2 | Take Data | Request resource from server |
| SEEN | 3 | Acknowledgment | Acknowledge packet receipt |
| STOP | 4 | Disconnect | Gracefully close connection |

### Connection Flow

```
Client                          Server
  │                               │
  ├─── CONN (seq=0, pubkey) ────>│
  │                               │
  │<──── SEEN (seq=0) ────────────┤
  │                               │
  ├─── TAKE (seq=1, resource) ──>│
  │                               │
  │<──── GIVE (seq=0, data) ──────┤
  │                               │
  ├─── SEEN (seq=0) ────────────>│
  │                               │
  ├─── STOP (seq=2) ────────────>│
  │                               │
  │<──── SEEN (seq=2) ────────────┤
  │                               │
```

### Constraints

- **Maximum packet size**: 65,507 bytes (UDP limit)
- **Sequence numbers**: 16-bit (0-65535, wraps around)
- **Default timeout**: 5000ms for connection establishment
- **Max retries**: 3 attempts before failure
- **Protocol version**: Currently version 1

---

## Core Components

### 1. HeroSocket

Low-level socket wrapper providing cross-platform UDP communication.

**Key Methods:**
- `bind(port)` - Bind socket to a port
- `send(data, host, port)` - Send data to a destination
- `recv(buffer, from_host, from_port)` - Receive data (non-blocking)

### 2. Packet

Represents a HERO protocol packet with serialization/deserialization.

**Key Methods:**
- `serialize()` - Convert packet to bytes for transmission
- `deserialize(data)` - Parse bytes into packet structure
- `makeConn()`, `makeGive()`, `makeTake()`, `makeSeen()`, `makeStop()` - Factory methods

### 3. HeroClient

High-level client for connecting to HERO servers.

**Key Methods:**
- `connect(host, port, pubkey)` - Establish connection
- `send(data)` - Send data to server
- `receive(packet, timeout)` - Receive packet from server
- `disconnect()` - Close connection gracefully

### 4. HeroServer

High-level server for handling multiple clients.

**Key Methods:**
- `start()` - Start accepting connections
- `poll(handler)` - Process incoming packets with callback
- `sendTo(data, host, port)` - Send data to specific client
- `stop()` - Stop server

### 5. HeroWebServer

HTTP-compatible web server over HERO protocol.

**Features:**
- Static file serving
- MIME type detection
- Chunked transfer for large files
- Directory traversal protection

### 6. HeroDynamicWebServer

Advanced web server with dynamic routing and real-time features.

**Features:**
- Dynamic route handlers
- Query parameter parsing
- Server-Sent Events (SSE) support
- Client connection tracking
- Broadcast capabilities

### 7. HeroBrowser

Simple web client for fetching content from HERO web servers.

**Key Methods:**
- `get(host, port, path)` - Fetch resource via HTTP GET
- `disconnect()` - Close connection

---

## API Reference

### Packet Class

#### Constructor
```cpp
Packet(Flag flag, uint16_t sequence)
Packet(Flag flag, uint16_t seq, const vector<uint8_t>& req, const vector<uint8_t>& data)
```

#### Factory Methods
```cpp
static Packet makeConn(uint16_t seq, const vector<uint8_t>& client_pubkey)
static Packet makeGive(uint16_t seq, const vector<uint8_t>& recipient_key, 
                       const vector<uint8_t>& data)
static Packet makeTake(uint16_t seq, const vector<uint8_t>& resource_id = {})
static Packet makeSeen(uint16_t seq)
static Packet makeStop(uint16_t seq)
```

#### Methods
```cpp
vector<uint8_t> serialize() const
static Packet deserialize(const vector<uint8_t>& data)
bool isValid() const
```

### HeroClient Class

```cpp
// Connection management
bool connect(const string& host, uint16_t port, const vector<uint8_t>& pubkey = {1,2,3,4})
void disconnect()
bool isConnected() const

// Data transmission
bool send(const vector<uint8_t>& data, const vector<uint8_t>& recipient_key = {})
bool send(const string& text, const vector<uint8_t>& recipient_key = {})
bool send(const char* text, const vector<uint8_t>& recipient_key = {})

// Data reception
bool receive(Packet& out_packet, int timeout_ms = 100)
bool receiveString(string& out_text, int timeout_ms = 100)

// Utilities
bool ping()
```

### HeroServer Class

```cpp
// Server lifecycle
HeroServer(uint16_t listen_port)
void start()
void stop()
bool isRunning() const

// Event handling
bool poll(function<void(const Packet&, const string&, uint16_t)> handler)

// Data transmission
void sendTo(const vector<uint8_t>& data, const string& host, uint16_t port)
void sendTo(const string& text, const string& host, uint16_t port)
void sendTo(const char* text, const string& host, uint16_t port)
void reply(const Packet& original_pkt, const string& response_text,
           const string& client_host, uint16_t client_port)

// Client management
int getClientCount() const
```

### HeroWebServer Class

```cpp
// Constructor
HeroWebServer(uint16_t port, const string& root_directory = ".")

// Server operations
void serve()  // Call in loop to handle requests
bool isRunning() const
```

### HeroDynamicWebServer Class

```cpp
// Constructor
HeroDynamicWebServer(uint16_t port, const string& root_directory = ".")

// Route management
void route(const string& path, 
           function<string(const map<string, string>&)> handler)

// Real-time features
void pushToClient(const string& host, uint16_t port, const string& data)
void broadcast(const string& data)
void broadcastEvent(const string& event_name, const string& data)

// Server operations
void serve()
int getClientCount() const
void cleanupStaleClients()
bool isRunning() const
```

### HeroBrowser Class

```cpp
// HTTP operations
string get(const string& host, uint16_t port, const string& path = "/")
void disconnect()
```

---

## Examples

### Example 1: Basic Echo Server

```cpp
#include "HERO.h"
#include <iostream>

int main() {
    HERO::HeroServer server(8080);
    server.start();
    
    std::cout << "Echo server running on port 8080..." << std::endl;
    
    while (server.isRunning()) {
        server.poll([&](const HERO::Packet& pkt, 
                       const std::string& host, 
                       uint16_t port) {
            if (pkt.flag == HERO::GIVE) {
                // Echo back the received data
                std::string received(pkt.payload.begin(), pkt.payload.end());
                std::cout << "Received: " << received << std::endl;
                server.reply(pkt, "Echo: " + received, host, port);
            }
        });
    }
    
    return 0;
}
```

### Example 2: Simple Client

```cpp
#include "HERO.h"
#include <iostream>

int main() {
    HERO::HeroClient client;
    
    // Connect to server
    if (!client.connect("127.0.0.1", 8080)) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    std::cout << "Connected to server!" << std::endl;
    
    // Send message
    client.send("Hello, HERO!");
    
    // Receive response
    std::string response;
    if (client.receiveString(response, 1000)) {
        std::cout << "Server replied: " << response << std::endl;
    }
    
    client.disconnect();
    return 0;
}
```

### Example 3: Static Web Server

```cpp
#include "HERO.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // Serve files from "./www" directory
    HERO::HeroWebServer server(8080, "./www");
    
    std::cout << "Web server running on port 8080" << std::endl;
    std::cout << "Serving files from ./www/" << std::endl;
    
    while (server.isRunning()) {
        server.serve();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

### Example 4: Dynamic Web Server with Routes

```cpp
#include "HERO.h"
#include <iostream>
#include <sstream>
#include <thread>

int main() {
    HERO::HeroDynamicWebServer server(8080, ".");
    
    // Home route
    server.route("/", [](const auto& params) {
        return "<h1>Welcome!</h1><p>Visit /api/time for current time</p>";
    });
    
    // API endpoint
    server.route("/api/time", [](const auto& params) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "<h1>Current Time</h1><p>" << std::ctime(&time) << "</p>";
        return ss.str();
    });
    
    // Parameterized route
    server.route("/api/greet", [](const auto& params) {
        std::string name = "Guest";
        if (params.count("name")) {
            name = params.at("name");
        }
        return "<h1>Hello, " + name + "!</h1>";
    });
    
    std::cout << "Dynamic server running on port 8080" << std::endl;
    std::cout << "Routes: /, /api/time, /api/greet?name=YourName" << std::endl;
    
    while (server.isRunning()) {
        server.serve();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

### Example 5: Chat Server with Broadcast

```cpp
#include "HERO.h"
#include <iostream>
#include <thread>

int main() {
    HERO::HeroDynamicWebServer server(8080, ".");
    
    // Chat page
    server.route("/", [](const auto& params) {
        return R"(
            <h1>HERO Chat</h1>
            <div id="messages"></div>
            <input id="msg" type="text" placeholder="Type message...">
            <button onclick="send()">Send</button>
            <script>
                const events = new EventSource('/events');
                events.onmessage = (e) => {
                    document.getElementById('messages').innerHTML += 
                        '<p>' + e.data + '</p>';
                };
                
                function send() {
                    const msg = document.getElementById('msg').value;
                    fetch('/send?msg=' + encodeURIComponent(msg));
                    document.getElementById('msg').value = '';
                }
            </script>
        )";
    });
    
    // Send message endpoint
    server.route("/send", [&](const auto& params) {
        if (params.count("msg")) {
            // Broadcast to all connected clients
            server.broadcast("data: " + params.at("msg") + "\n\n");
        }
        return "OK";
    });
    
    std::cout << "Chat server running on port 8080" << std::endl;
    
    while (server.isRunning()) {
        server.serve();
        server.cleanupStaleClients();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

### Example 6: File Transfer Client/Server

```cpp
// Server
#include "HERO.h"
#include <fstream>
#include <iostream>

int main() {
    HERO::HeroServer server(8080);
    server.start();
    
    server.poll([&](const HERO::Packet& pkt, 
                   const std::string& host, 
                   uint16_t port) {
        if (pkt.flag == HERO::TAKE) {
            // Client requesting a file
            std::string filename(pkt.requirements.begin(), 
                               pkt.requirements.end());
            
            std::ifstream file(filename, std::ios::binary);
            if (file) {
                std::vector<uint8_t> data(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );
                
                // Send file in chunks
                const size_t CHUNK_SIZE = 60000;
                for (size_t i = 0; i < data.size(); i += CHUNK_SIZE) {
                    size_t chunk_size = std::min(CHUNK_SIZE, 
                                                data.size() - i);
                    std::vector<uint8_t> chunk(
                        data.begin() + i, 
                        data.begin() + i + chunk_size
                    );
                    server.sendTo(chunk, host, port);
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(10)
                    );
                }
                
                std::cout << "Sent file: " << filename << std::endl;
            } else {
                server.sendTo("ERROR: File not found", host, port);
            }
        }
    });
    
    return 0;
}

// Client
#include "HERO.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: client <filename>" << std::endl;
        return 1;
    }
    
    HERO::HeroClient client;
    if (!client.connect("127.0.0.1", 8080)) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }
    
    // Request file
    std::string filename = argv[1];
    std::vector<uint8_t> req(filename.begin(), filename.end());
    auto pkt = HERO::Packet::makeTake(1, req);
    client.send(pkt.serialize());
    
    // Receive file chunks
    std::vector<uint8_t> file_data;
    HERO::Packet chunk;
    while (client.receive(chunk, 2000)) {
        file_data.insert(file_data.end(), 
                        chunk.payload.begin(), 
                        chunk.payload.end());
    }
    
    // Save to disk
    std::ofstream out("received_" + filename, std::ios::binary);
    out.write((char*)file_data.data(), file_data.size());
    
    std::cout << "Received " << file_data.size() << " bytes" << std::endl;
    
    client.disconnect();
    return 0;
}
```

### Example 7: IoT Sensor Network

```cpp
#include "HERO.h"
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

// Simulated temperature sensor client
void sensorClient(int sensor_id) {
    HERO::HeroClient client;
    if (!client.connect("127.0.0.1", 8080)) {
        return;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(18.0, 28.0);
    
    while (true) {
        double temp = dis(gen);
        std::stringstream ss;
        ss << "sensor_" << sensor_id << ":" << temp;
        
        client.send(ss.str());
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// Server collecting sensor data
int main() {
    HERO::HeroServer server(8080);
    server.start();
    
    std::cout << "IoT Gateway running on port 8080" << std::endl;
    
    // Launch simulated sensors
    std::thread s1(sensorClient, 1);
    std::thread s2(sensorClient, 2);
    std::thread s3(sensorClient, 3);
    s1.detach();
    s2.detach();
    s3.detach();
    
    while (server.isRunning()) {
        server.poll([](const HERO::Packet& pkt,
                      const std::string& host,
                      uint16_t port) {
            if (pkt.flag == HERO::GIVE) {
                std::string data(pkt.payload.begin(), pkt.payload.end());
                std::cout << "Sensor reading: " << data << std::endl;
                
                // Could store in database, trigger alerts, etc.
            }
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
```

---

## Best Practices

### 1. Connection Management

- Always call `disconnect()` when done with a client connection
- Implement timeout handling for long-running connections
- Use connection pooling for high-traffic scenarios

```cpp
// Good practice
{
    HERO::HeroClient client;
    if (client.connect(host, port)) {
        // Do work...
        client.disconnect();  // Explicit cleanup
    }
}  // Client destroyed here
```

### 2. Error Handling

- Always check return values from `connect()`, `send()`, and `receive()`
- Implement retry logic for critical operations
- Use try-catch blocks when deserializing packets

```cpp
try {
    auto pkt = HERO::Packet::deserialize(data);
    if (!pkt.isValid()) {
        std::cerr << "Invalid packet received" << std::endl;
        return;
    }
    // Process packet...
} catch (const std::exception& e) {
    std::cerr << "Deserialization error: " << e.what() << std::endl;
}
```

### 3. Performance Optimization

- Use chunking for large data transfers (max 60KB per packet)
- Implement rate limiting to prevent network congestion
- Call `cleanupStaleClients()` periodically on dynamic web servers

```cpp
// Chunked transfer
const size_t CHUNK_SIZE = 60000;
for (size_t i = 0; i < data.size(); i += CHUNK_SIZE) {
    size_t chunk_size = std::min(CHUNK_SIZE, data.size() - i);
    std::vector<uint8_t> chunk(data.begin() + i, 
                              data.begin() + i + chunk_size);
    client.send(chunk);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

### 4. Security Considerations

- Implement authentication using the public key mechanism
- Validate all input data before processing
- Use the requirements field for access control
- Sanitize paths in web servers to prevent directory traversal

```cpp
// Directory traversal prevention (already implemented)
if (path.find("..") != std::string::npos) {
    send404(host, port);
    return;
}
```

### 5. Threading and Concurrency

- The server's `poll()` method is single-threaded
- For multi-threaded servers, use mutex protection
- Keep packet handlers lightweight and fast

```cpp
std::mutex data_mutex;
std::map<std::string, std::string> shared_data;

server.poll([&](const HERO::Packet& pkt, 
               const std::string& host, 
               uint16_t port) {
    std::lock_guard<std::mutex> lock(data_mutex);
    // Thread-safe access to shared_data
});
```

### 6. Resource Management

- Limit maximum clients on servers
- Implement connection timeouts
- Clean up stale connections periodically

```cpp
// In your server loop
if (server.getClientCount() > MAX_CLIENTS) {
    // Reject new connections or clean up old ones
    server.cleanupStaleClients();
}
```

---

## Troubleshooting

### Common Issues

**Connection Timeouts**
- Increase `DEFAULT_TIMEOUT_MS` if needed
- Check firewall settings
- Verify server is running and bound to correct port

**Packet Loss**
- UDP is unreliable; implement application-level retry logic
- Use the SEEN acknowledgment mechanism
- Consider TCP if reliability is critical

**Large File Transfers**
- Always chunk data larger than 60KB
- Implement sequence tracking for proper reassembly
- Add checksums for data integrity

**Platform-Specific Issues**
- Windows: Ensure Winsock2 is initialized (`WSAStartup`)
- Linux/Mac: Check socket buffer sizes with `setsockopt`
- Firewall: Allow UDP traffic on your chosen port

---

## Compilation

### Linux/Mac
```bash
g++ -std=c++17 server.cpp -o server -lpthread
g++ -std=c++17 client.cpp -o client -lpthread
```

### Windows (MSVC)
```bash
cl /EHsc /std:c++17 server.cpp /link ws2_32.lib
cl /EHsc /std:c++17 client.cpp /link ws2_32.lib
```

### CMake
```cmake
cmake_minimum_required(VERSION 3.10)
project(HEROExample)

set(CMAKE_CXX_STANDARD 17)

add_executable(server server.cpp)
add_executable(client client.cpp)

if(WIN32)
    target_link_libraries(server ws2_32)
    target_link_libraries(client ws2_32)
endif()
```

---

## License and Contributing

This protocol is designed for educational and commercial use. Feel free to extend and modify it for your specific needs. Contributions to improve reliability, add features, or fix bugs are welcome!
