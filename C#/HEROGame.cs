using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace HERO.Game
{
    // ============================================================================
    // GAME STATE SYNCHRONIZATION
    // ============================================================================

    public class GameState
    {
        private Dictionary<string, string> state;
        private uint version;

        public GameState()
        {
            state = new Dictionary<string, string>();
            version = 0;
        }

        public void Set(string key, string value)
        {
            state[key] = value;
            version++;
        }

        public void Set(string key, int value)
        {
            Set(key, value.ToString());
        }

        public void Set(string key, float value)
        {
            Set(key, value.ToString());
        }

        public void Set(string key, bool value)
        {
            Set(key, value ? "true" : "false");
        }

        public string Get(string key, string default_val = "")
        {
            return state.ContainsKey(key) ? state[key] : default_val;
        }

        public int GetInt(string key, int default_val = 0)
        {
            var val = Get(key);
            return string.IsNullOrEmpty(val) ? default_val : int.Parse(val);
        }

        public float GetFloat(string key, float default_val = 0.0f)
        {
            var val = Get(key);
            return string.IsNullOrEmpty(val) ? default_val : float.Parse(val);
        }

        public bool GetBool(string key, bool default_val = false)
        {
            var val = Get(key);
            return string.IsNullOrEmpty(val) ? default_val : (val == "true");
        }

        public string Serialize()
        {
            StringBuilder sb = new StringBuilder();
            sb.Append(version);
            sb.Append("|");
            foreach (var kvp in state)
            {
                sb.Append(kvp.Key);
                sb.Append("=");
                sb.Append(kvp.Value);
                sb.Append(";");
            }
            return sb.ToString();
        }

        public void Deserialize(string data)
        {
            state.Clear();
            int pipe_pos = data.IndexOf('|');
            if (pipe_pos == -1) return;

            version = uint.Parse(data.Substring(0, pipe_pos));
            string pairs = data.Substring(pipe_pos + 1);

            int pos = 0;
            while (pos < pairs.Length)
            {
                int eq = pairs.IndexOf('=', pos);
                int semi = pairs.IndexOf(';', eq);
                if (eq == -1 || semi == -1) break;

                string key = pairs.Substring(pos, eq - pos);
                string value = pairs.Substring(eq + 1, semi - eq - 1);
                state[key] = value;

                pos = semi + 1;
            }
        }

        public uint GetVersion()
        {
            return version;
        }
    }

    // ============================================================================
    // VECTOR2 - Simple 2D vector for game math
    // ============================================================================

    public struct Vector2
    {
        public float x;
        public float y;

        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public static Vector2 operator +(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x + b.x, a.y + b.y);
        }

        public static Vector2 operator -(Vector2 a, Vector2 b)
        {
            return new Vector2(a.x - b.x, a.y - b.y);
        }

        public static Vector2 operator *(Vector2 v, float scalar)
        {
            return new Vector2(v.x * scalar, v.y * scalar);
        }

        public float Length()
        {
            return (float)Math.Sqrt(x * x + y * y);
        }

        public Vector2 Normalized()
        {
            float len = Length();
            return len > 0 ? new Vector2(x / len, y / len) : new Vector2(0, 0);
        }

        public float Distance(Vector2 other)
        {
            return (this - other).Length();
        }

        public string ToString()
        {
            return x.ToString() + "," + y.ToString();
        }

        public static Vector2 FromString(string str)
        {
            int comma = str.IndexOf(',');
            if (comma == -1) return new Vector2();
            return new Vector2(float.Parse(str.Substring(0, comma)),
                             float.Parse(str.Substring(comma + 1)));
        }
    }

    // ============================================================================
    // ENTITY - Game object with position, velocity, and properties
    // ============================================================================

    public class Entity
    {
        public string id;
        public Vector2 position;
        public Vector2 velocity;
        public Dictionary<string, string> properties;

        public Entity(string id = "")
        {
            this.id = id;
            position = new Vector2();
            velocity = new Vector2();
            properties = new Dictionary<string, string>();
        }

        public void SetProperty(string key, string value)
        {
            properties[key] = value;
        }

        public string GetProperty(string key, string default_val = "")
        {
            return properties.ContainsKey(key) ? properties[key] : default_val;
        }

        public void Update(float deltaTime)
        {
            position += velocity * deltaTime;
        }

        public string Serialize()
        {
            StringBuilder sb = new StringBuilder();
            sb.Append(id);
            sb.Append("|");
            sb.Append(position.ToString());
            sb.Append("|");
            sb.Append(velocity.ToString());
            sb.Append("|");
            foreach (var kvp in properties)
            {
                sb.Append(kvp.Key);
                sb.Append("=");
                sb.Append(kvp.Value);
                sb.Append(";");
            }
            return sb.ToString();
        }

        public static Entity Deserialize(string data)
        {
            Entity e = new Entity();
            var parts = data.Split('|');

            if (parts.Length >= 1)
                e.id = parts[0];
            if (parts.Length >= 2)
                e.position = Vector2.FromString(parts[1]);
            if (parts.Length >= 3)
                e.velocity = Vector2.FromString(parts[2]);

            if (parts.Length >= 4)
            {
                string props = parts[3];
                int pos = 0;
                while (pos < props.Length)
                {
                    int eq = props.IndexOf('=', pos);
                    int semi = props.IndexOf(';', eq);
                    if (eq == -1 || semi == -1) break;

                    string key = props.Substring(pos, eq - pos);
                    string value = props.Substring(eq + 1, semi - eq - 1);
                    e.properties[key] = value;

                    pos = semi + 1;
                }
            }

            return e;
        }
    }

    // ============================================================================
    // GAME SERVER - Authoritative server with entity management
    // ============================================================================

    public class GameServer
    {
        private HeroServer server;
        private HeroSocket fast_socket;
        private ushort fast_port;
        private Dictionary<string, Entity> entities;
        private GameState state;
        private uint tick_count;

        private class Player
        {
            public string host;
            public ushort port;
            public string player_id;
        }

        private Dictionary<string, Player> players;

        private string MakeClientKey(string host, ushort port)
        {
            return host + ":" + port.ToString();
        }

        public GameServer(ushort port)
        {
            server = new HeroServer(port);
            fast_socket = new HeroSocket();
            fast_port = port;
            entities = new Dictionary<string, Entity>();
            state = new GameState();
            tick_count = 0;
            players = new Dictionary<string, Player>();

            server.Start();
            fast_socket.Bind(port);
        }

        public void SetEntity(Entity entity)
        {
            entities[entity.id] = entity;
        }

        public Entity GetEntity(string id)
        {
            return entities.ContainsKey(id) ? entities[id] : null;
        }

        public void RemoveEntity(string id)
        {
            entities.Remove(id);
        }

        public void BroadcastEntity(string entity_id)
        {
            if (!entities.ContainsKey(entity_id))
                return;

            string data = "ENTITY|" + entities[entity_id].Serialize();
            foreach (var kvp in players)
            {
                server.SendTo(data, kvp.Value.host, kvp.Value.port);
            }
        }

        public void BroadcastEntityFast(string entity_id)
        {
            if (!entities.ContainsKey(entity_id))
                return;

            string data = "FAST|ENTITY|" + entities[entity_id].Serialize();
            byte[] bytes = Encoding.UTF8.GetBytes(data);

            foreach (var kvp in players)
            {
                fast_socket.Send(bytes, kvp.Value.host, kvp.Value.port);
            }
        }

        public void SendFast(string msg, string host, ushort port)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(msg);
            fast_socket.Send(bytes, host, port);
        }

        public void BroadcastState()
        {
            string data = "STATE|" + state.Serialize();
            foreach (var kvp in players)
            {
                server.SendTo(data, kvp.Value.host, kvp.Value.port);
            }
        }

        public void SendSnapshot(string host, ushort port)
        {
            // Send state
            server.SendTo("STATE|" + state.Serialize(), host, port);

            // Send all entities
            foreach (var kvp in entities)
            {
                server.SendTo("ENTITY|" + kvp.Value.Serialize(), host, port);
            }
        }

        public void Tick(float deltaTime)
        {
            tick_count++;

            // Update all entities
            foreach (var kvp in entities)
            {
                kvp.Value.Update(deltaTime);
            }

            // Broadcast updates every 5 ticks
            if (tick_count % 5 == 0)
            {
                foreach (var kvp in entities)
                {
                    BroadcastEntity(kvp.Key);
                }
            }
        }

        public void PollFast(Action<string, string, string, ushort> handler = null)
        {
            byte[] buffer;
            string from_host;
            ushort from_port;

            while (fast_socket.Recv(out buffer, out from_host, out from_port))
            {
                string msg = Encoding.UTF8.GetString(buffer);

                if (!msg.StartsWith("FAST|"))
                    continue;

                msg = msg.Substring(5);

                int pipe = msg.IndexOf('|');
                if (pipe == -1)
                    continue;

                string cmd = msg.Substring(0, pipe);
                string data = msg.Substring(pipe + 1);

                string client_key = MakeClientKey(from_host, from_port);
                string player_id = "";
                foreach (var kvp in players)
                {
                    if (kvp.Value.host == from_host)
                    {
                        player_id = kvp.Value.player_id;
                        break;
                    }
                }

                if (handler != null && !string.IsNullOrEmpty(player_id))
                {
                    handler(cmd, data, player_id, from_port);
                }
            }
        }

        public void Poll(Action<string, string, string, ushort> handler = null)
        {
            server.Poll((pkt, host, port) =>
            {
                string msg = Encoding.UTF8.GetString(pkt.payload.ToArray());
                string client_key = MakeClientKey(host, port);

                int pipe = msg.IndexOf('|');
                if (pipe == -1)
                    return;

                string cmd = msg.Substring(0, pipe);
                string data = msg.Substring(pipe + 1);

                if (cmd == "JOIN")
                {
                    Player p = new Player();
                    p.host = host;
                    p.port = port;
                    p.player_id = data;
                    players[client_key] = p;

                    SendSnapshot(host, port);

                    string join_msg = "PLAYER_JOIN|" + data;
                    foreach (var kvp in players)
                    {
                        if (kvp.Key != client_key)
                        {
                            server.SendTo(join_msg, kvp.Value.host, kvp.Value.port);
                        }
                    }
                }
                else if (cmd == "LEAVE")
                {
                    if (players.ContainsKey(client_key))
                    {
                        string leave_msg = "PLAYER_LEAVE|" + players[client_key].player_id;
                        players.Remove(client_key);

                        foreach (var kvp in players)
                        {
                            server.SendTo(leave_msg, kvp.Value.host, kvp.Value.port);
                        }
                    }
                }
                else if (handler != null)
                {
                    string player_id = players.ContainsKey(client_key) ? players[client_key].player_id : "";
                    handler(cmd, data, player_id, port);
                }
            });
        }

        public GameState GetState()
        {
            return state;
        }

        public int GetPlayerCount()
        {
            return players.Count;
        }
    }

    // ============================================================================
    // GAME CLIENT - Client-side with prediction and interpolation
    // ============================================================================

    public class GameClient
    {
        private HeroClient client;
        private HeroSocket fast_socket;
        private string server_host;
        private ushort server_port;
        private Dictionary<string, Entity> entities;
        private GameState state;
        private string player_id;

        public GameClient()
        {
            client = new HeroClient();
            fast_socket = new HeroSocket();
            server_port = 0;
            entities = new Dictionary<string, Entity>();
            state = new GameState();
            player_id = "";
        }

        public bool Connect(string host, ushort port, string player_name)
        {
            if (!client.Connect(host, port))
            {
                return false;
            }

            server_host = host;
            server_port = port;
            player_id = player_name;
            client.Send("JOIN|" + player_name);
            return true;
        }

        public void Disconnect()
        {
            client.Send("LEAVE|" + player_id);
            client.Disconnect();
        }

        public void SendCommand(string cmd, string data = "")
        {
            client.Send(cmd + "|" + data);
        }

        public void SendFast(string cmd, string data = "")
        {
            string msg = "FAST|" + cmd + "|" + data;
            byte[] bytes = Encoding.UTF8.GetBytes(msg);
            fast_socket.Send(bytes, server_host, server_port);
        }

        public void SendPosition(Vector2 pos)
        {
            SendFast("POS", pos.ToString());
        }

        public void SendVelocity(Vector2 vel)
        {
            SendFast("VEL", vel.ToString());
        }

        public void Update(Action<string, string> handler = null)
        {
            Packet pkt;
            while (client.Receive(out pkt, 10))
            {
                string msg = Encoding.UTF8.GetString(pkt.payload.ToArray());

                int pipe = msg.IndexOf('|');
                if (pipe == -1)
                    continue;

                string cmd = msg.Substring(0, pipe);
                string data = msg.Substring(pipe + 1);

                if (cmd == "ENTITY")
                {
                    Entity e = Entity.Deserialize(data);
                    entities[e.id] = e;
                }
                else if (cmd == "STATE")
                {
                    state.Deserialize(data);
                }
                else if (handler != null)
                {
                    handler(cmd, data);
                }
            }
        }

        public Entity GetEntity(string id)
        {
            return entities.ContainsKey(id) ? entities[id] : null;
        }

        public Dictionary<string, Entity> GetEntities()
        {
            return entities;
        }

        public GameState GetState()
        {
            return state;
        }

        public string GetPlayerId()
        {
            return player_id;
        }
    }

    // ============================================================================
    // SIMPLE MATCHMAKING
    // ============================================================================

    public class Matchmaker
    {
        private class GameRoom
        {
            public string id;
            public List<string> players;
            public int max_players;
            public bool started;
        }

        private Dictionary<string, GameRoom> rooms;
        private Random rng;

        private string GenerateRoomId()
        {
            const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            StringBuilder id = new StringBuilder();
            for (int i = 0; i < 6; i++)
            {
                id.Append(chars[rng.Next(chars.Length)]);
            }
            return id.ToString();
        }

        public Matchmaker()
        {
            rooms = new Dictionary<string, GameRoom>();
            rng = new Random();
        }

        public string CreateRoom(int max_players = 4)
        {
            GameRoom room = new GameRoom();
            room.id = GenerateRoomId();
            room.max_players = max_players;
            room.started = false;
            room.players = new List<string>();
            rooms[room.id] = room;
            return room.id;
        }

        public bool JoinRoom(string room_id, string player_id)
        {
            if (!rooms.ContainsKey(room_id))
                return false;
            if (rooms[room_id].started)
                return false;
            if (rooms[room_id].players.Count >= rooms[room_id].max_players)
                return false;

            rooms[room_id].players.Add(player_id);
            return true;
        }

        public bool LeaveRoom(string room_id, string player_id)
        {
            if (!rooms.ContainsKey(room_id))
                return false;

            rooms[room_id].players.Remove(player_id);

            if (rooms[room_id].players.Count == 0)
            {
                rooms.Remove(room_id);
            }
            return true;
        }

        public bool IsRoomFull(string room_id)
        {
            if (!rooms.ContainsKey(room_id))
                return false;
            return rooms[room_id].players.Count >= rooms[room_id].max_players;
        }

        public List<string> GetRoomPlayers(string room_id)
        {
            return rooms.ContainsKey(room_id) ? rooms[room_id].players : new List<string>();
        }
    }

    // ============================================================================
    // LEADERBOARD
    // ============================================================================

    public class Leaderboard
    {
        private class Score
        {
            public string player_id;
            public int score;
            public DateTime timestamp;
        }

        private List<Score> scores;

        public Leaderboard()
        {
            scores = new List<Score>();
        }

        public void AddScore(string player_id, int score)
        {
            Score s = new Score();
            s.player_id = player_id;
            s.score = score;
            s.timestamp = DateTime.Now;
            scores.Add(s);
            scores.Sort((a, b) => b.score.CompareTo(a.score));

            if (scores.Count > 100)
            {
                scores.RemoveRange(100, scores.Count - 100);
            }
        }

        public List<Tuple<string, int>> GetTop(int n = 10)
        {
            var result = new List<Tuple<string, int>>();
            for (int i = 0; i < Math.Min(n, scores.Count); i++)
            {
                result.Add(new Tuple<string, int>(scores[i].player_id, scores[i].score));
            }
            return result;
        }

        public int GetRank(string player_id)
        {
            for (int i = 0; i < scores.Count; i++)
            {
                if (scores[i].player_id == player_id)
                {
                    return i + 1;
                }
            }
            return -1;
        }

        public string Serialize()
        {
            StringBuilder sb = new StringBuilder();
            foreach (var s in scores)
            {
                sb.Append(s.player_id);
                sb.Append("=");
                sb.Append(s.score);
                sb.Append(";");
            }
            return sb.ToString();
        }
    }
}
