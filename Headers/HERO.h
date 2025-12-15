// HERO.h - Core Networking Protocol
// C++ port of the HERO networking library
#ifndef HERO_H
#define HERO_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>
#include <sstream>
#include <algorithm>
#include <tuple>
#include <memory>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace HERO {

// Protocol version and constants
namespace Protocol {
    const uint8_t VERSION = 2;
    const int MAX_PACKET_SIZE = 65507;
    const int MAX_PAYLOAD_SIZE = 60000;
    const int DEFAULT_TIMEOUT_MS = 5000;
    const int MAX_RETRIES = 3;
    const int FRAGMENT_HEADER_SIZE = 12;
}

// Protocol flags
enum class Flag : uint8_t {
    CONN = 0,  // Start connection
    GIVE = 1,  // Send data
    TAKE = 2,  // Request data
    SEEN = 3,  // Acknowledge
    STOP = 4,  // Close connection
    FRAG = 5,  // Fragmented packet
    PING = 6,  // Keepalive ping
    PONG = 7   // Keepalive response
};

// Magic words helper for game commands
class MagicWords {
public:
    static const std::string MOVE;
    static const std::string ATTACK;
    static const std::string JUMP;
    static const std::string SHOOT;
    static const std::string INTERACT;
    static const std::string CHAT;
    static const std::string SPAWN;
    static const std::string DEATH;
    static const std::string DAMAGE;
    static const std::string HEAL;
    static const std::string PICKUP;
    static const std::string DROP;
    static const std::string USE;
    static const std::string EQUIP;
    static const std::string CAST;
    
    // State sync
    static const std::string STATE_FULL;
    static const std::string STATE_DELTA;
    static const std::string ENTITY_UPDATE;
    static const std::string ENTITY_CREATE;
    static const std::string ENTITY_DESTROY;
    
    // Matchmaking
    static const std::string JOIN_ROOM;
    static const std::string LEAVE_ROOM;
    static const std::string ROOM_READY;
    static const std::string GAME_START;
    static const std::string GAME_END;

private:
    static std::map<std::string, std::string> customWords;

public:
    static void Register(const std::string& word, const std::string& code) {
        if (code.length() != 2) {
            throw std::invalid_argument("Magic word codes must be exactly 2 characters");
        }
        customWords[word] = code;
    }

    static std::string Get(const std::string& word) {
        auto it = customWords.find(word);
        return (it != customWords.end()) ? it->second : word;
    }

    template<typename... Args>
    static std::vector<uint8_t> Encode(const std::string& word, Args... args) {
        std::string code = Get(word);
        std::vector<uint8_t> data(code.begin(), code.end());
        data.push_back('|');
        
        EncodeArgs(data, args...);
        
        return data;
    }

    static std::pair<std::string, std::vector<std::string>> Decode(const std::vector<uint8_t>& data) {
        std::string str(data.begin(), data.end());
        size_t pipe = str.find('|');
        
        if (pipe == std::string::npos) {
            return {str, {}};
        }
        
        std::string code = str.substr(0, pipe);
        std::string args_str = str.substr(pipe + 1);
        std::vector<std::string> args;
        
        size_t pos = 0;
        while (pos < args_str.length()) {
            size_t semi = args_str.find(';', pos);
            if (semi == std::string::npos) break;
            args.push_back(args_str.substr(pos, semi - pos));
            pos = semi + 1;
        }
        
        return {code, args};
    }

private:
    static void EncodeArgs(std::vector<uint8_t>& data) {}
    
    template<typename T, typename... Args>
    static void EncodeArgs(std::vector<uint8_t>& data, T arg, Args... args) {
        std::string str = std::to_string(arg);
        data.insert(data.end(), str.begin(), str.end());
        data.push_back(';');
        EncodeArgs(data, args...);
    }
};

// Initialize static members
const std::string MagicWords::MOVE = "MV";
const std::string MagicWords::ATTACK = "ATK";
const std::string MagicWords::JUMP = "JMP";
const std::string MagicWords::SHOOT = "SHT";
const std::string MagicWords::INTERACT = "INT";
const std::string MagicWords::CHAT = "CHT";
const std::string MagicWords::SPAWN = "SPN";
const std::string MagicWords::DEATH = "DTH";
const std::string MagicWords::DAMAGE = "DMG";
const std::string MagicWords::HEAL = "HEL";
const std::string MagicWords::PICKUP = "PKP";
const std::string MagicWords::DROP = "DRP";
const std::string MagicWords::USE = "USE";
const std::string MagicWords::EQUIP = "EQP";
const std::string MagicWords::CAST = "CST";
const std::string MagicWords::STATE_FULL = "SF";
const std::string MagicWords::STATE_DELTA = "SD";
const std::string MagicWords::ENTITY_UPDATE = "EU";
const std::string MagicWords::ENTITY_CREATE = "EC";
const std::string MagicWords::ENTITY_DESTROY = "ED";
const std::string MagicWords::JOIN_ROOM = "JR";
const std::string MagicWords::LEAVE_ROOM = "LR";
const std::string MagicWords::ROOM_READY = "RR";
const std::string MagicWords::GAME_START = "GS";
const std::string MagicWords::GAME_END = "GE";
std::map<std::string, std::string> MagicWords::customWords;

// Packet class
class Packet {
public:
    uint8_t flag;
    uint8_t version;
    uint16_t seq;
    std::vector<uint8_t> requirements;
    std::vector<uint8_t> payload;

    Packet() : flag(0), version(Protocol::VERSION), seq(0) {}
    
    Packet(Flag f, uint16_t sequence) 
        : flag(static_cast<uint8_t>(f)), version(Protocol::VERSION), seq(sequence) {}
    
    Packet(Flag f, uint16_t sequence, const std::vector<uint8_t>& req, const std::vector<uint8_t>& data)
        : flag(static_cast<uint8_t>(f)), version(Protocol::VERSION), seq(sequence), 
          requirements(req), payload(data) {}

    std::vector<uint8_t> Serialize() const {
        uint16_t payload_len = static_cast<uint16_t>(payload.size());
        uint16_t req_len = static_cast<uint16_t>(requirements.size());

        std::vector<uint8_t> buffer;
        buffer.reserve(8 + req_len + payload_len);

        buffer.push_back(flag);
        buffer.push_back(version);
        buffer.push_back((seq >> 8) & 0xFF);
        buffer.push_back(seq & 0xFF);
        buffer.push_back((payload_len >> 8) & 0xFF);
        buffer.push_back(payload_len & 0xFF);
        buffer.push_back((req_len >> 8) & 0xFF);
        buffer.push_back(req_len & 0xFF);

        buffer.insert(buffer.end(), requirements.begin(), requirements.end());
        buffer.insert(buffer.end(), payload.begin(), payload.end());

        return buffer;
    }

    static Packet Deserialize(const std::vector<uint8_t>& data) {
        if (data.size() < 8) {
            throw std::runtime_error("Packet too small");
        }

        Packet pkt;
        pkt.flag = data[0];
        pkt.version = data[1];
        pkt.seq = (data[2] << 8) | data[3];

        uint16_t payload_len = (data[4] << 8) | data[5];
        uint16_t req_len = (data[6] << 8) | data[7];

        if (data.size() < 8 + req_len + payload_len) {
            throw std::runtime_error("Packet data incomplete");
        }

        pkt.requirements.assign(data.begin() + 8, data.begin() + 8 + req_len);
        pkt.payload.assign(data.begin() + 8 + req_len, data.begin() + 8 + req_len + payload_len);

        return pkt;
    }

    static Packet MakeConn(uint16_t seq, const std::vector<uint8_t>& client_pubkey) {
        return Packet(Flag::CONN, seq, client_pubkey, {});
    }

    static Packet MakeGive(uint16_t seq, const std::vector<uint8_t>& recipient_key, const std::vector<uint8_t>& data) {
        return Packet(Flag::GIVE, seq, recipient_key, data);
    }

    static Packet MakeTake(uint16_t seq, const std::vector<uint8_t>& resource_id = {}) {
        return Packet(Flag::TAKE, seq, resource_id, {});
    }

    static Packet MakeSeen(uint16_t seq) {
        return Packet(Flag::SEEN, seq, {}, {});
    }

    static Packet MakeStop(uint16_t seq) {
        return Packet(Flag::STOP, seq, {}, {});
    }

    static Packet MakePing(uint16_t seq) {
        return Packet(Flag::PING, seq, {}, {});
    }

    static Packet MakePong(uint16_t seq) {
        return Packet(Flag::PONG, seq, {}, {});
    }

    bool IsValid() const {
        return flag <= static_cast<uint8_t>(Flag::PONG) && version == Protocol::VERSION;
    }
};

// Fragment manager
class FragmentManager {
private:
    struct FragmentedMessage {
        uint16_t msg_id;
        uint16_t total_fragments;
        std::map<uint16_t, std::vector<uint8_t>> fragments;
        std::chrono::steady_clock::time_point last_update;

        FragmentedMessage(uint16_t id, uint16_t total) 
            : msg_id(id), total_fragments(total), last_update(std::chrono::steady_clock::now()) {}

        bool IsComplete() const {
            return fragments.size() == total_fragments;
        }

        std::vector<uint8_t> Reassemble() const {
            if (!IsComplete()) return {};

            std::vector<uint8_t> result;
            for (uint16_t i = 0; i < total_fragments; i++) {
                auto it = fragments.find(i);
                if (it == fragments.end()) return {};
                result.insert(result.end(), it->second.begin(), it->second.end());
            }
            return result;
        }
    };

    std::map<uint16_t, FragmentedMessage> messages;
    uint16_t next_msg_id;

public:
    FragmentManager() : next_msg_id(0) {}

    std::vector<Packet> Fragment(const std::vector<uint8_t>& data, Flag flag) {
        std::vector<Packet> packets;
        int chunk_size = Protocol::MAX_PAYLOAD_SIZE - Protocol::FRAGMENT_HEADER_SIZE;
        uint16_t total_fragments = (data.size() + chunk_size - 1) / chunk_size;
        uint16_t msg_id = next_msg_id++;

        for (uint16_t i = 0; i < total_fragments; i++) {
            size_t offset = i * chunk_size;
            size_t length = std::min(static_cast<size_t>(chunk_size), data.size() - offset);

            std::vector<uint8_t> fragment_data;
            fragment_data.push_back(msg_id & 0xFF);
            fragment_data.push_back((msg_id >> 8) & 0xFF);
            fragment_data.push_back(i & 0xFF);
            fragment_data.push_back((i >> 8) & 0xFF);
            fragment_data.push_back(total_fragments & 0xFF);
            fragment_data.push_back((total_fragments >> 8) & 0xFF);
            fragment_data.push_back(static_cast<uint8_t>(flag));

            fragment_data.insert(fragment_data.end(), data.begin() + offset, data.begin() + offset + length);

            Packet pkt(Flag::FRAG, i, {}, fragment_data);
            packets.push_back(pkt);
        }

        return packets;
    }

    std::tuple<bool, std::vector<uint8_t>, Flag> AddFragment(const Packet& pkt) {
        if (pkt.flag != static_cast<uint8_t>(Flag::FRAG) || pkt.payload.size() < Protocol::FRAGMENT_HEADER_SIZE) {
            return {false, {}, Flag::GIVE};
        }

        uint16_t msg_id = pkt.payload[0] | (pkt.payload[1] << 8);
        uint16_t frag_num = pkt.payload[2] | (pkt.payload[3] << 8);
        uint16_t total_frags = pkt.payload[4] | (pkt.payload[5] << 8);
        Flag original_flag = static_cast<Flag>(pkt.payload[6]);

        if (messages.find(msg_id) == messages.end()) {
            messages.emplace(msg_id, FragmentedMessage(msg_id, total_frags));
        }

        std::vector<uint8_t> fragment_data(pkt.payload.begin() + 7, pkt.payload.end());
        messages[msg_id].fragments[frag_num] = fragment_data;
        messages[msg_id].last_update = std::chrono::steady_clock::now();

        if (messages[msg_id].IsComplete()) {
            auto complete = messages[msg_id].Reassemble();
            messages.erase(msg_id);
            return {true, complete, original_flag};
        }

        return {false, {}, original_flag};
    }

    void CleanupStale(int timeout_seconds = 30) {
        auto now = std::chrono::steady_clock::now();
        std::vector<uint16_t> to_remove;

        for (auto& kvp : messages) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - kvp.second.last_update);
            if (duration.count() > timeout_seconds) {
                to_remove.push_back(kvp.first);
            }
        }

        for (auto id : to_remove) {
            messages.erase(id);
        }
    }
};

// Socket wrapper
class HeroSocket {
private:
    SOCKET sock;
    bool initialized;

#ifdef _WIN32
    static bool wsa_initialized;
    static int wsa_ref_count;
#endif

public:
    HeroSocket() : sock(INVALID_SOCKET), initialized(false) {
#ifdef _WIN32
        if (!wsa_initialized) {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
                wsa_initialized = true;
            }
        }
        wsa_ref_count++;
#endif
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock != INVALID_SOCKET) {
            SetNonBlocking();
            initialized = true;
        }
    }

    ~HeroSocket() {
        Close();
#ifdef _WIN32
        wsa_ref_count--;
        if (wsa_ref_count == 0 && wsa_initialized) {
            WSACleanup();
            wsa_initialized = false;
        }
#endif
    }

    void Bind(uint16_t port) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            throw std::runtime_error("Failed to bind socket");
        }
    }

    bool Send(const std::vector<uint8_t>& data, const std::string& host, uint16_t port) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        int sent = sendto(sock, reinterpret_cast<const char*>(data.data()), data.size(), 0, 
                         (sockaddr*)&addr, sizeof(addr));
        return sent > 0;
    }

    bool Recv(std::vector<uint8_t>& buffer, std::string& from_host, uint16_t& from_port) {
        char recv_buffer[Protocol::MAX_PACKET_SIZE];
        sockaddr_in from_addr = {};
        socklen_t from_len = sizeof(from_addr);

        int received = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0, 
                               (sockaddr*)&from_addr, &from_len);

        if (received > 0) {
            buffer.assign(recv_buffer, recv_buffer + received);
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, sizeof(ip_str));
            from_host = ip_str;
            from_port = ntohs(from_addr.sin_port);
            return true;
        }

        return false;
    }

    void Close() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }

private:
    void SetNonBlocking() {
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    }
};

#ifdef _WIN32
bool HeroSocket::wsa_initialized = false;
int HeroSocket::wsa_ref_count = 0;
#endif

// Client class
class HeroClient {
private:
    HeroSocket socket;
    uint16_t seq_num;
    std::string server_host;
    uint16_t server_port;
    bool connected;
    FragmentManager fragment_mgr;
    std::chrono::steady_clock::time_point last_ping;
    int ping_ms;

public:
    HeroClient() : seq_num(0), server_port(0), connected(false), ping_ms(0) {
        last_ping = std::chrono::steady_clock::now();
    }

    bool Connect(const std::string& host, uint16_t port, const std::vector<uint8_t>& pubkey = {1, 2, 3, 4}) {
        server_host = host;
        server_port = port;

        auto conn_pkt = Packet::MakeConn(seq_num++, pubkey);
        auto data = conn_pkt.Serialize();

        if (!socket.Send(data, server_host, server_port)) {
            return false;
        }

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() < Protocol::DEFAULT_TIMEOUT_MS) {
            
            std::vector<uint8_t> buffer;
            std::string from_host;
            uint16_t from_port;

            if (socket.Recv(buffer, from_host, from_port)) {
                try {
                    auto pkt = Packet::Deserialize(buffer);
                    if (pkt.flag == static_cast<uint8_t>(Flag::SEEN)) {
                        connected = true;
                        last_ping = std::chrono::steady_clock::now();
                        return true;
                    }
                } catch (...) {}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return false;
    }

    bool Send(const std::vector<uint8_t>& data, const std::vector<uint8_t>& recipient_key = {}) {
        if (!connected) return false;

        auto pkt = Packet::MakeGive(seq_num++, recipient_key, data);
        return socket.Send(pkt.Serialize(), server_host, server_port);
    }

    bool Send(const std::string& text, const std::vector<uint8_t>& recipient_key = {}) {
        std::vector<uint8_t> data(text.begin(), text.end());
        return Send(data, recipient_key);
    }

    template<typename... Args>
    bool SendCommand(const std::string& command, Args... args) {
        auto data = MagicWords::Encode(command, args...);
        return Send(data);
    }

    bool Ping() {
        if (!connected) return false;

        auto ping_start = std::chrono::steady_clock::now();
        auto pkt = Packet::MakePing(seq_num++);

        if (!socket.Send(pkt.Serialize(), server_host, server_port)) {
            return false;
        }

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() < 1000) {
            
            std::vector<uint8_t> buffer;
            std::string from_host;
            uint16_t from_port;

            if (socket.Recv(buffer, from_host, from_port)) {
                try {
                    auto response = Packet::Deserialize(buffer);
                    if (response.flag == static_cast<uint8_t>(Flag::PONG)) {
                        ping_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - ping_start).count();
                        last_ping = std::chrono::steady_clock::now();
                        return true;
                    }
                } catch (...) {}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return false;
    }

    void KeepAlive() {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_ping);
        if (duration.count() > 5) {
            Ping();
        }
    }

    bool Receive(Packet& out_packet, int timeout_ms = 100) {
        auto start = std::chrono::steady_clock::now();
        
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() < timeout_ms) {
            
            std::vector<uint8_t> buffer;
            std::string from_host;
            uint16_t from_port;

            if (socket.Recv(buffer, from_host, from_port)) {
                try {
                    auto pkt = Packet::Deserialize(buffer);

                    if (pkt.flag == static_cast<uint8_t>(Flag::FRAG)) {
                        auto [complete, data, original_flag] = fragment_mgr.AddFragment(pkt);
                        if (complete) {
                            out_packet = Packet(original_flag, pkt.seq, {}, data);
                            auto seen = Packet::MakeSeen(out_packet.seq);
                            socket.Send(seen.Serialize(), from_host, from_port);
                            return true;
                        }
                        continue;
                    }

                    auto seen_pkt = Packet::MakeSeen(pkt.seq);
                    socket.Send(seen_pkt.Serialize(), from_host, from_port);

                    out_packet = pkt;
                    return true;
                } catch (...) {}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        fragment_mgr.CleanupStale();
        return false;
    }

    bool ReceiveString(std::string& out_text, int timeout_ms = 100) {
        Packet pkt;
        if (Receive(pkt, timeout_ms)) {
            out_text = std::string(pkt.payload.begin(), pkt.payload.end());
            return true;
        }
        return false;
    }

    void Disconnect() {
        if (connected) {
            auto stop_pkt = Packet::MakeStop(seq_num++);
            socket.Send(stop_pkt.Serialize(), server_host, server_port);
            connected = false;
        }
    }

    bool IsConnected() const { return connected; }
    int GetPing() const { return ping_ms; }
};

// Server class
class HeroServer {
private:
    struct Client {
        std::string host;
        uint16_t port;
        std::vector<uint8_t> pubkey;
        std::chrono::steady_clock::time_point last_seen;
        std::chrono::steady_clock::time_point last_ping;
    };

    HeroSocket socket;
    uint16_t port;
    bool running;
    FragmentManager fragment_mgr;
    std::unordered_map<std::string, Client> clients;

    std::string MakeClientKey(const std::string& host, uint16_t port) {
        return host + ":" + std::to_string(port);
    }

public:
    HeroServer(uint16_t listen_port) : port(listen_port), running(false) {
        socket.Bind(port);
    }

    void Start() { running = true; }
    void Stop() { running = false; }

    bool Poll(std::function<void(const Packet&, const std::string&, uint16_t)> handler = nullptr) {
        if (!running) return false;

        std::vector<uint8_t> buffer;
        std::string from_host;
        uint16_t from_port;

        if (socket.Recv(buffer, from_host, from_port)) {
            try {
                auto pkt = Packet::Deserialize(buffer);
                std::string client_key = MakeClientKey(from_host, from_port);

                if (pkt.flag == static_cast<uint8_t>(Flag::FRAG)) {
                    auto [complete, data, original_flag] = fragment_mgr.AddFragment(pkt);
                    if (complete) {
                        pkt = Packet(original_flag, pkt.seq, {}, data);
                        auto seen = Packet::MakeSeen(pkt.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);
                    } else {
                        return false;
                    }
                }

                if (pkt.flag == static_cast<uint8_t>(Flag::CONN)) {
                    Client c;
                    c.host = from_host;
                    c.port = from_port;
                    c.pubkey = pkt.requirements;
                    c.last_seen = std::chrono::steady_clock::now();
                    c.last_ping = std::chrono::steady_clock::now();
                    clients[client_key] = c;

                    auto seen = Packet::MakeSeen(pkt.seq);
                    socket.Send(seen.Serialize(), from_host, from_port);
                } else if (pkt.flag == static_cast<uint8_t>(Flag::STOP)) {
                    clients.erase(client_key);
                    auto seen = Packet::MakeSeen(pkt.seq);
                    socket.Send(seen.Serialize(), from_host, from_port);
                } else if (pkt.flag == static_cast<uint8_t>(Flag::PING)) {
                    if (clients.find(client_key) != clients.end()) {
                        clients[client_key].last_ping = std::chrono::steady_clock::now();
                    }
                    auto pong = Packet::MakePong(pkt.seq);
                    socket.Send(pong.Serialize(), from_host, from_port);
                } else {
                    if (clients.find(client_key) != clients.end()) {
                        clients[client_key].last_seen = std::chrono::steady_clock::now();
                    }

                    auto seen = Packet::MakeSeen(pkt.seq);
                    socket.Send(seen.Serialize(), from_host, from_port);

                    if (handler) {
                        handler(pkt, from_host, from_port);
                    }
                }

                return true;
            } catch (...) {}
        }

        fragment_mgr.CleanupStale();
        return false;
    }

    void SendTo(const std::vector<uint8_t>& data, const std::string& host, uint16_t port) {
        auto pkt = Packet::MakeGive(0, {}, data);
        socket.Send(pkt.Serialize(), host, port);
    }

    void SendTo(const std::string& text, const std::string& host, uint16_t port) {
        std::vector<uint8_t> data(text.begin(), text.end());
        SendTo(data, host, port);
    }

    void Broadcast(const std::vector<uint8_t>& data) {
        for (const auto& [key, client] : clients) {
            SendTo(data, client.host, client.port);
        }
    }

    void Broadcast(const std::string& text) {
        std::vector<uint8_t> data(text.begin(), text.end());
        Broadcast(data);
    }

    int GetClientCount() const { return clients.size(); }
    bool IsRunning() const { return running; }
};

} // namespace HERO

#endif // HERO_H


// ============================================================================
// HEROGAME.h - Game Framework
// ============================================================================

#ifndef HEROGAME_H
#define HEROGAME_H

#include <cmath>
#include <random>

namespace HERO {
namespace Game {

// ============================================================================
// GAME STATE SYNCHRONIZATION
// ============================================================================

class GameState {
private:
    std::unordered_map<std::string, std::string> state;
    uint32_t version;

public:
    GameState() : version(0) {}

    void Set(const std::string& key, const std::string& value) {
        state[key] = value;
        version++;
    }

    void Set(const std::string& key, int value) {
        Set(key, std::to_string(value));
    }

    void Set(const std::string& key, float value) {
        Set(key, std::to_string(value));
    }

    void Set(const std::string& key, bool value) {
        Set(key, value ? "true" : "false");
    }

    std::string Get(const std::string& key, const std::string& default_val = "") const {
        auto it = state.find(key);
        return (it != state.end()) ? it->second : default_val;
    }

    int GetInt(const std::string& key, int default_val = 0) const {
        std::string val = Get(key);
        return val.empty() ? default_val : std::stoi(val);
    }

    float GetFloat(const std::string& key, float default_val = 0.0f) const {
        std::string val = Get(key);
        return val.empty() ? default_val : std::stof(val);
    }

    bool GetBool(const std::string& key, bool default_val = false) const {
        std::string val = Get(key);
        return val.empty() ? default_val : (val == "true");
    }

    std::string Serialize() const {
        std::stringstream ss;
        ss << version << "|";
        for (const auto& [key, value] : state) {
            ss << key << "=" << value << ";";
        }
        return ss.str();
    }

    void Deserialize(const std::string& data) {
        state.clear();
        size_t pipe_pos = data.find('|');
        if (pipe_pos == std::string::npos) return;

        version = std::stoul(data.substr(0, pipe_pos));
        std::string pairs = data.substr(pipe_pos + 1);

        size_t pos = 0;
        while (pos < pairs.length()) {
            size_t eq = pairs.find('=', pos);
            size_t semi = pairs.find(';', eq);
            if (eq == std::string::npos || semi == std::string::npos) break;

            std::string key = pairs.substr(pos, eq - pos);
            std::string value = pairs.substr(eq + 1, semi - eq - 1);
            state[key] = value;

            pos = semi + 1;
        }
    }

    uint32_t GetVersion() const { return version; }
};

// ============================================================================
// VECTOR2 - Simple 2D vector for game math
// ============================================================================

struct Vector2 {
    float x, y;

    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}

    Vector2 operator+(const Vector2& other) const {
        return Vector2(x + other.x, y + other.y);
    }

    Vector2 operator-(const Vector2& other) const {
        return Vector2(x - other.x, y - other.y);
    }

    Vector2 operator*(float scalar) const {
        return Vector2(x * scalar, y * scalar);
    }

    float Length() const {
        return std::sqrt(x * x + y * y);
    }

    Vector2 Normalized() const {
        float len = Length();
        return len > 0 ? Vector2(x / len, y / len) : Vector2(0, 0);
    }

    float Distance(const Vector2& other) const {
        return (*this - other).Length();
    }

    std::string ToString() const {
        return std::to_string(x) + "," + std::to_string(y);
    }

    static Vector2 FromString(const std::string& str) {
        size_t comma = str.find(',');
        if (comma == std::string::npos) return Vector2();
        return Vector2(std::stof(str.substr(0, comma)),
                      std::stof(str.substr(comma + 1)));
    }
};

// ============================================================================
// ENTITY - Game object with position, velocity, and properties
// ============================================================================

class Entity {
public:
    std::string id;
    Vector2 position;
    Vector2 velocity;
    std::unordered_map<std::string, std::string> properties;

    Entity(const std::string& id = "") : id(id) {}

    void SetProperty(const std::string& key, const std::string& value) {
        properties[key] = value;
    }

    std::string GetProperty(const std::string& key, const std::string& default_val = "") const {
        auto it = properties.find(key);
        return (it != properties.end()) ? it->second : default_val;
    }

    void Update(float deltaTime) {
        position = position + velocity * deltaTime;
    }

    std::string Serialize() const {
        std::stringstream ss;
        ss << id << "|" << position.ToString() << "|" << velocity.ToString() << "|";
        for (const auto& [key, value] : properties) {
            ss << key << "=" << value << ";";
        }
        return ss.str();
    }

    static Entity Deserialize(const std::string& data) {
        Entity e;
        std::stringstream ss(data);
        std::string part;
        std::vector<std::string> parts;

        while (std::getline(ss, part, '|')) {
            parts.push_back(part);
        }

        if (parts.size() >= 1) e.id = parts[0];
        if (parts.size() >= 2) e.position = Vector2::FromString(parts[1]);
        if (parts.size() >= 3) e.velocity = Vector2::FromString(parts[2]);

        if (parts.size() >= 4) {
            std::string props = parts[3];
            size_t pos = 0;
            while (pos < props.length()) {
                size_t eq = props.find('=', pos);
                size_t semi = props.find(';', eq);
                if (eq == std::string::npos || semi == std::string::npos) break;

                std::string key = props.substr(pos, eq - pos);
                std::string value = props.substr(eq + 1, semi - eq - 1);
                e.properties[key] = value;

                pos = semi + 1;
            }
        }

        return e;
    }
};

// ============================================================================
// GAME CLIENT
// ============================================================================

class GameClient {
private:
    HeroClient client;
    std::string server_host;
    uint16_t server_port;
    std::unordered_map<std::string, Entity> entities;
    GameState state;
    std::string player_id;

public:
    GameClient() : server_port(0) {}

    bool Connect(const std::string& host, uint16_t port, const std::string& player_name) {
        if (!client.Connect(host, port)) {
            return false;
        }

        server_host = host;
        server_port = port;
        player_id = player_name;
        client.Send("JOIN|" + player_name);
        return true;
    }

    void Disconnect() {
        client.Send("LEAVE|" + player_id);
        client.Disconnect();
    }

    void SendCommand(const std::string& cmd, const std::string& data = "") {
        client.Send(cmd + "|" + data);
    }

    void Update(std::function<void(const std::string&, const std::string&)> handler = nullptr) {
        Packet pkt;
        while (client.Receive(pkt, 10)) {
            std::string msg(pkt.payload.begin(), pkt.payload.end());

            size_t pipe = msg.find('|');
            if (pipe == std::string::npos) continue;

            std::string cmd = msg.substr(0, pipe);
            std::string data = msg.substr(pipe + 1);

            if (cmd == "ENTITY") {
                Entity e = Entity::Deserialize(data);
                entities[e.id] = e;
            } else if (cmd == "STATE") {
                state.Deserialize(data);
            } else if (handler) {
                handler(cmd, data);
            }
        }
    }

    Entity* GetEntity(const std::string& id) {
        auto it = entities.find(id);
        return (it != entities.end()) ? &it->second : nullptr;
    }

    const std::unordered_map<std::string, Entity>& GetEntities() const {
        return entities;
    }

    GameState& GetState() { return state; }
    const std::string& GetPlayerId() const { return player_id; }
};

// ============================================================================
// LEADERBOARD
// ============================================================================

class Leaderboard {
private:
    struct Score {
        std::string player_id;
        int score;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<Score> scores;

public:
    void AddScore(const std::string& player_id, int score) {
        Score s;
        s.player_id = player_id;
        s.score = score;
        s.timestamp = std::chrono::system_clock::now();
        scores.push_back(s);

        std::sort(scores.begin(), scores.end(), 
                 [](const Score& a, const Score& b) { return a.score > b.score; });

        if (scores.size() > 100) {
            scores.resize(100);
        }
    }

    std::vector<std::pair<std::string, int>> GetTop(int n = 10) const {
        std::vector<std::pair<std::string, int>> result;
        for (size_t i = 0; i < std::min(static_cast<size_t>(n), scores.size()); i++) {
            result.emplace_back(scores[i].player_id, scores[i].score);
        }
        return result;
    }

    int GetRank(const std::string& player_id) const {
        for (size_t i = 0; i < scores.size(); i++) {
            if (scores[i].player_id == player_id) {
                return i + 1;
            }
        }
        return -1;
    }
};

} // namespace Game
} // namespace HERO

#endif // HEROGAME_H
