#ifndef HERO_GAMEDEV_H
#define HERO_GAMEDEV_H

#include "HERO.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <unordered_map>

namespace HERO {
namespace Game {

// ============================================================================
// GAME STATE SYNCHRONIZATION
// ============================================================================

// Serializable game state with automatic sync
class GameState {
private:
  std::unordered_map<std::string, std::string> state;
  uint32_t version;

public:
  GameState() : version(0) {}

  void set(const std::string &key, const std::string &value) {
    state[key] = value;
    version++;
  }

  void set(const std::string &key, int value) {
    set(key, std::to_string(value));
  }

  void set(const std::string &key, float value) {
    set(key, std::to_string(value));
  }

  void set(const std::string &key, bool value) {
    set(key, value ? "true" : "false");
  }

  std::string get(const std::string &key, const std::string &default_val = "") const {
    auto it = state.find(key);
    return it != state.end() ? it->second : default_val;
  }

  int getInt(const std::string &key, int default_val = 0) const {
    auto val = get(key);
    return val.empty() ? default_val : std::stoi(val);
  }

  float getFloat(const std::string &key, float default_val = 0.0f) const {
    auto val = get(key);
    return val.empty() ? default_val : std::stof(val);
  }

  bool getBool(const std::string &key, bool default_val = false) const {
    auto val = get(key);
    return val.empty() ? default_val : (val == "true");
  }

  std::string serialize() const {
    std::stringstream ss;
    ss << version << "|";
    for (const auto &[key, value] : state) {
      ss << key << "=" << value << ";";
    }
    return ss.str();
  }

  void deserialize(const std::string &data) {
    state.clear();
    size_t pipe_pos = data.find('|');
    if (pipe_pos == std::string::npos) return;

    version = std::stoul(data.substr(0, pipe_pos));
    std::string pairs = data.substr(pipe_pos + 1);

    size_t pos = 0;
    while (pos < pairs.size()) {
      size_t eq = pairs.find('=', pos);
      size_t semi = pairs.find(';', eq);
      if (eq == std::string::npos || semi == std::string::npos) break;

      std::string key = pairs.substr(pos, eq - pos);
      std::string value = pairs.substr(eq + 1, semi - eq - 1);
      state[key] = value;

      pos = semi + 1;
    }
  }

  uint32_t getVersion() const { return version; }
};

// ============================================================================
// VECTOR2 - Simple 2D vector for game math
// ============================================================================

struct Vector2 {
  float x, y;

  Vector2() : x(0), y(0) {}
  Vector2(float x, float y) : x(x), y(y) {}

  Vector2 operator+(const Vector2 &other) const {
    return Vector2(x + other.x, y + other.y);
  }

  Vector2 operator-(const Vector2 &other) const {
    return Vector2(x - other.x, y - other.y);
  }

  Vector2 operator*(float scalar) const {
    return Vector2(x * scalar, y * scalar);
  }

  Vector2 &operator+=(const Vector2 &other) {
    x += other.x;
    y += other.y;
    return *this;
  }

  float length() const { return std::sqrt(x * x + y * y); }

  Vector2 normalized() const {
    float len = length();
    return len > 0 ? Vector2(x / len, y / len) : Vector2(0, 0);
  }

  float distance(const Vector2 &other) const {
    return (*this - other).length();
  }

  std::string toString() const {
    return std::to_string(x) + "," + std::to_string(y);
  }

  static Vector2 fromString(const std::string &str) {
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

  Entity(const std::string &id = "") : id(id) {}

  void setProperty(const std::string &key, const std::string &value) {
    properties[key] = value;
  }

  std::string getProperty(const std::string &key,
                          const std::string &default_val = "") const {
    auto it = properties.find(key);
    return it != properties.end() ? it->second : default_val;
  }

  void update(float deltaTime) { position += velocity * deltaTime; }

  std::string serialize() const {
    std::stringstream ss;
    ss << id << "|" << position.toString() << "|" << velocity.toString()
       << "|";
    for (const auto &[key, value] : properties) {
      ss << key << "=" << value << ";";
    }
    return ss.str();
  }

  static Entity deserialize(const std::string &data) {
    Entity e;
    std::istringstream ss(data);
    std::string token;

    std::getline(ss, e.id, '|');
    std::getline(ss, token, '|');
    e.position = Vector2::fromString(token);
    std::getline(ss, token, '|');
    e.velocity = Vector2::fromString(token);

    std::string props;
    std::getline(ss, props);
    size_t pos = 0;
    while (pos < props.size()) {
      size_t eq = props.find('=', pos);
      size_t semi = props.find(';', eq);
      if (eq == std::string::npos || semi == std::string::npos)
        break;

      std::string key = props.substr(pos, eq - pos);
      std::string value = props.substr(eq + 1, semi - eq - 1);
      e.properties[key] = value;

      pos = semi + 1;
    }

    return e;
  }
};

// ============================================================================
// GAME SERVER - Authoritative server with entity management
// ============================================================================

class GameServer {
private:
  HeroServer server;
  std::unordered_map<std::string, Entity> entities;
  GameState state;
  uint32_t tick_count;

  struct Player {
    std::string host;
    uint16_t port;
    std::string player_id;
  };

  std::unordered_map<std::string, Player> players;

  std::string makeClientKey(const std::string &host, uint16_t port) {
    return host + ":" + std::to_string(port);
  }

public:
  GameServer(uint16_t port) : server(port), tick_count(0) { server.start(); }

  // Add or update an entity
  void setEntity(const Entity &entity) { entities[entity.id] = entity; }

  // Get an entity
  Entity *getEntity(const std::string &id) {
    auto it = entities.find(id);
    return it != entities.end() ? &it->second : nullptr;
  }

  // Remove an entity
  void removeEntity(const std::string &id) { entities.erase(id); }

  // Broadcast entity state to all players
  void broadcastEntity(const std::string &entity_id) {
    auto it = entities.find(entity_id);
    if (it == entities.end())
      return;

    std::string data = "ENTITY|" + it->second.serialize();
    for (const auto &[key, player] : players) {
      server.sendTo(data, player.host, player.port);
    }
  }

  // Broadcast game state
  void broadcastState() {
    std::string data = "STATE|" + state.serialize();
    for (const auto &[key, player] : players) {
      server.sendTo(data, player.host, player.port);
    }
  }

  // Send full world snapshot to a specific player
  void sendSnapshot(const std::string &host, uint16_t port) {
    // Send state
    server.sendTo("STATE|" + state.serialize(), host, port);

    // Send all entities
    for (const auto &[id, entity] : entities) {
      server.sendTo("ENTITY|" + entity.serialize(), host, port);
    }
  }

  // Update game loop - call this regularly
  void tick(float deltaTime) {
    tick_count++;

    // Update all entities
    for (auto &[id, entity] : entities) {
      entity.update(deltaTime);
    }

    // Broadcast updates every 5 ticks (adjust as needed)
    if (tick_count % 5 == 0) {
      for (const auto &[id, entity] : entities) {
        broadcastEntity(id);
      }
    }
  }

  // Handle incoming messages
  void poll(std::function<void(const std::string &, const std::string &,
                               const std::string &, uint16_t)>
                handler = nullptr) {
    server.poll(
        [&](const Packet &pkt, const std::string &host, uint16_t port) {
          std::string msg(pkt.payload.begin(), pkt.payload.end());
          std::string client_key = makeClientKey(host, port);

          // Parse message type
          size_t pipe = msg.find('|');
          if (pipe == std::string::npos)
            return;

          std::string cmd = msg.substr(0, pipe);
          std::string data = msg.substr(pipe + 1);

          if (cmd == "JOIN") {
            // Player joining
            Player p;
            p.host = host;
            p.port = port;
            p.player_id = data;
            players[client_key] = p;

            // Send snapshot to new player
            sendSnapshot(host, port);

            // Notify others
            std::string join_msg = "PLAYER_JOIN|" + data;
            for (const auto &[key, player] : players) {
              if (key != client_key) {
                server.sendTo(join_msg, player.host, player.port);
              }
            }
          } else if (cmd == "LEAVE") {
            // Player leaving
            if (players.count(client_key)) {
              std::string leave_msg = "PLAYER_LEAVE|" + players[client_key].player_id;
              players.erase(client_key);

              for (const auto &[key, player] : players) {
                server.sendTo(leave_msg, player.host, player.port);
              }
            }
          } else if (handler) {
            // Custom message handler
            std::string player_id =
                players.count(client_key) ? players[client_key].player_id : "";
            handler(cmd, data, player_id, port);
          }
        });
  }

  GameState &getState() { return state; }
  int getPlayerCount() const { return players.size(); }
};

// ============================================================================
// GAME CLIENT - Client-side with prediction and interpolation
// ============================================================================

class GameClient {
private:
  HeroClient client;
  std::unordered_map<std::string, Entity> entities;
  GameState state;
  std::string player_id;

public:
  GameClient() {}

  bool connect(const std::string &host, uint16_t port,
               const std::string &player_name) {
    if (!client.connect(host, port)) {
      return false;
    }

    player_id = player_name;
    client.send("JOIN|" + player_name);
    return true;
  }

  void disconnect() {
    client.send("LEAVE|" + player_id);
    client.disconnect();
  }

  // Send a command to the server
  void sendCommand(const std::string &cmd, const std::string &data = "") {
    client.send(cmd + "|" + data);
  }

  // Update and receive messages
  void update(std::function<void(const std::string &, const std::string &)>
                  handler = nullptr) {
    Packet pkt;
    while (client.receive(pkt, 10)) { // Non-blocking receive
      std::string msg(pkt.payload.begin(), pkt.payload.end());

      size_t pipe = msg.find('|');
      if (pipe == std::string::npos)
        continue;

      std::string cmd = msg.substr(0, pipe);
      std::string data = msg.substr(pipe + 1);

      if (cmd == "ENTITY") {
        Entity e = Entity::deserialize(data);
        entities[e.id] = e;
      } else if (cmd == "STATE") {
        state.deserialize(data);
      } else if (handler) {
        handler(cmd, data);
      }
    }
  }

  Entity *getEntity(const std::string &id) {
    auto it = entities.find(id);
    return it != entities.end() ? &it->second : nullptr;
  }

  const std::unordered_map<std::string, Entity> &getEntities() const {
    return entities;
  }

  GameState &getState() { return state; }
  const std::string &getPlayerId() const { return player_id; }
};

// ============================================================================
// SIMPLE MATCHMAKING
// ============================================================================

class Matchmaker {
private:
  struct GameRoom {
    std::string id;
    std::vector<std::string> players;
    int max_players;
    bool started;
  };

  std::unordered_map<std::string, GameRoom> rooms;
  std::mt19937 rng;

  std::string generateRoomId() {
    const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string id;
    for (int i = 0; i < 6; i++) {
      id += chars[rng() % 36];
    }
    return id;
  }

public:
  Matchmaker() : rng(std::random_device{}()) {}

  std::string createRoom(int max_players = 4) {
    GameRoom room;
    room.id = generateRoomId();
    room.max_players = max_players;
    room.started = false;
    rooms[room.id] = room;
    return room.id;
  }

  bool joinRoom(const std::string &room_id, const std::string &player_id) {
    auto it = rooms.find(room_id);
    if (it == rooms.end())
      return false;
    if (it->second.started)
      return false;
    if (it->second.players.size() >= (size_t)it->second.max_players)
      return false;

    it->second.players.push_back(player_id);
    return true;
  }

  bool leaveRoom(const std::string &room_id, const std::string &player_id) {
    auto it = rooms.find(room_id);
    if (it == rooms.end())
      return false;

    auto &players = it->second.players;
    players.erase(std::remove(players.begin(), players.end(), player_id),
                  players.end());

    if (players.empty()) {
      rooms.erase(it);
    }
    return true;
  }

  bool isRoomFull(const std::string &room_id) {
    auto it = rooms.find(room_id);
    if (it == rooms.end())
      return false;
    return it->second.players.size() >= (size_t)it->second.max_players;
  }

  std::vector<std::string> getRoomPlayers(const std::string &room_id) {
    auto it = rooms.find(room_id);
    return it != rooms.end() ? it->second.players : std::vector<std::string>{};
  }
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

    bool operator<(const Score &other) const { return score > other.score; }
  };

  std::vector<Score> scores;

public:
  void addScore(const std::string &player_id, int score) {
    Score s;
    s.player_id = player_id;
    s.score = score;
    s.timestamp = std::chrono::system_clock::now();
    scores.push_back(s);
    std::sort(scores.begin(), scores.end());

    // Keep only top 100
    if (scores.size() > 100) {
      scores.resize(100);
    }
  }

  std::vector<std::pair<std::string, int>> getTop(int n = 10) {
    std::vector<std::pair<std::string, int>> result;
    for (int i = 0; i < std::min(n, (int)scores.size()); i++) {
      result.push_back({scores[i].player_id, scores[i].score});
    }
    return result;
  }

  int getRank(const std::string &player_id) {
    for (size_t i = 0; i < scores.size(); i++) {
      if (scores[i].player_id == player_id) {
        return i + 1;
      }
    }
    return -1;
  }

  std::string serialize() const {
    std::stringstream ss;
    for (const auto &s : scores) {
      ss << s.player_id << "=" << s.score << ";";
    }
    return ss.str();
  }
};

} // namespace Game
} // namespace HERO

#endif // HERO_GAMEDEV_H
