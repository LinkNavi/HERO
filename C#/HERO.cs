using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace HERO
{
    // Protocol version
    public static class Protocol
    {
        public const byte VERSION = 2;
        public const int MAX_PACKET_SIZE = 65507;
        public const int MAX_PAYLOAD_SIZE = 60000; // Safe size for fragmentation
        public const int DEFAULT_TIMEOUT_MS = 5000;
        public const int MAX_RETRIES = 3;
        public const int FRAGMENT_HEADER_SIZE = 12;
    }

    // Protocol flags
    public enum Flag : byte
    {
        CONN = 0,  // Start connection (requires client public key)
        GIVE = 1,  // Send data (requires recipient key + payload)
        TAKE = 2,  // Request data/resources
        SEEN = 3,  // Acknowledge packet receipt
        STOP = 4,  // Close connection
        FRAG = 5,  // Fragmented packet
        PING = 6,  // Keepalive ping
        PONG = 7   // Keepalive response
    }

    // Magic word helper for game commands
    public static class MagicWords
    {
        // Common game commands as short byte codes for efficiency
        public const string MOVE = "MV";
        public const string ATTACK = "ATK";
        public const string JUMP = "JMP";
        public const string SHOOT = "SHT";
        public const string INTERACT = "INT";
        public const string CHAT = "CHT";
        public const string SPAWN = "SPN";
        public const string DEATH = "DTH";
        public const string DAMAGE = "DMG";
        public const string HEAL = "HEL";
        public const string PICKUP = "PKP";
        public const string DROP = "DRP";
        public const string USE = "USE";
        public const string EQUIP = "EQP";
        public const string CAST = "CST";

        // State sync
        public const string STATE_FULL = "SF";
        public const string STATE_DELTA = "SD";
        public const string ENTITY_UPDATE = "EU";
        public const string ENTITY_CREATE = "EC";
        public const string ENTITY_DESTROY = "ED";

        // Matchmaking
        public const string JOIN_ROOM = "JR";
        public const string LEAVE_ROOM = "LR";
        public const string ROOM_READY = "RR";
        public const string GAME_START = "GS";
        public const string GAME_END = "GE";

        private static Dictionary<string, string> customWords = new Dictionary<string, string>();

        public static void Register(string word, string code)
        {
            if (code.Length != 2)
                throw new ArgumentException("Magic word codes must be exactly 2 characters");
            customWords[word] = code;
        }

        public static string Get(string word)
        {
            return customWords.ContainsKey(word) ? customWords[word] : word;
        }

        public static byte[] Encode(string word, params object[] args)
        {
            string code = Get(word);
            List<byte> data = new List<byte>(Encoding.UTF8.GetBytes(code));
            data.Add((byte)'|');
            
            foreach (var arg in args)
            {
                string str = arg.ToString();
                data.AddRange(Encoding.UTF8.GetBytes(str));
                data.Add((byte)';');
            }
            
            return data.ToArray();
        }

        public static Tuple<string, List<string>> Decode(byte[] data)
        {
            string str = Encoding.UTF8.GetString(data);
            int pipe = str.IndexOf('|');
            
            if (pipe == -1)
                return new Tuple<string, List<string>>(str, new List<string>());
            
            string code = str.Substring(0, pipe);
            string args_str = str.Substring(pipe + 1);
            List<string> args = new List<string>(args_str.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries));
            
            return new Tuple<string, List<string>>(code, args);
        }
    }

    // Fragment manager for large packets
    public class FragmentManager
    {
        private class FragmentedMessage
        {
            public ushort msg_id;
            public ushort total_fragments;
            public Dictionary<ushort, byte[]> fragments;
            public DateTime last_update;

            public FragmentedMessage(ushort id, ushort total)
            {
                msg_id = id;
                total_fragments = total;
                fragments = new Dictionary<ushort, byte[]>();
                last_update = DateTime.Now;
            }

            public bool IsComplete()
            {
                return fragments.Count == total_fragments;
            }

            public byte[] Reassemble()
            {
                if (!IsComplete())
                    return null;

                List<byte> result = new List<byte>();
                for (ushort i = 0; i < total_fragments; i++)
                {
                    if (!fragments.ContainsKey(i))
                        return null;
                    result.AddRange(fragments[i]);
                }
                return result.ToArray();
            }
        }

        private Dictionary<ushort, FragmentedMessage> messages;
        private ushort next_msg_id;

        public FragmentManager()
        {
            messages = new Dictionary<ushort, FragmentedMessage>();
            next_msg_id = 0;
        }

        public List<Packet> Fragment(byte[] data, Flag flag)
        {
            List<Packet> packets = new List<Packet>();
            int chunk_size = Protocol.MAX_PAYLOAD_SIZE - Protocol.FRAGMENT_HEADER_SIZE;
            ushort total_fragments = (ushort)((data.Length + chunk_size - 1) / chunk_size);
            ushort msg_id = next_msg_id++;

            for (ushort i = 0; i < total_fragments; i++)
            {
                int offset = i * chunk_size;
                int length = Math.Min(chunk_size, data.Length - offset);
                
                List<byte> fragment_data = new List<byte>();
                fragment_data.AddRange(BitConverter.GetBytes(msg_id));
                fragment_data.AddRange(BitConverter.GetBytes(i));
                fragment_data.AddRange(BitConverter.GetBytes(total_fragments));
                fragment_data.Add((byte)flag);
                
                byte[] chunk = new byte[length];
                Array.Copy(data, offset, chunk, 0, length);
                fragment_data.AddRange(chunk);

                Packet pkt = new Packet(Flag.FRAG, i, new List<byte>(), fragment_data);
                packets.Add(pkt);
            }

            return packets;
        }

        public Tuple<bool, byte[], Flag> AddFragment(Packet pkt)
        {
            if (pkt.flag != (byte)Flag.FRAG || pkt.payload.Count < Protocol.FRAGMENT_HEADER_SIZE)
                return new Tuple<bool, byte[], Flag>(false, null, Flag.GIVE);

            byte[] payload = pkt.payload.ToArray();
            ushort msg_id = BitConverter.ToUInt16(payload, 0);
            ushort frag_num = BitConverter.ToUInt16(payload, 2);
            ushort total_frags = BitConverter.ToUInt16(payload, 4);
            Flag original_flag = (Flag)payload[6];

            if (!messages.ContainsKey(msg_id))
            {
                messages[msg_id] = new FragmentedMessage(msg_id, total_frags);
            }

            byte[] fragment_data = new byte[payload.Length - 7];
            Array.Copy(payload, 7, fragment_data, 0, fragment_data.Length);
            messages[msg_id].fragments[frag_num] = fragment_data;
            messages[msg_id].last_update = DateTime.Now;

            if (messages[msg_id].IsComplete())
            {
                byte[] complete = messages[msg_id].Reassemble();
                messages.Remove(msg_id);
                return new Tuple<bool, byte[], Flag>(true, complete, original_flag);
            }

            return new Tuple<bool, byte[], Flag>(false, null, original_flag);
        }

        public void CleanupStale(int timeout_seconds = 30)
        {
            var now = DateTime.Now;
            var to_remove = new List<ushort>();

            foreach (var kvp in messages)
            {
                if ((now - kvp.Value.last_update).TotalSeconds > timeout_seconds)
                {
                    to_remove.Add(kvp.Key);
                }
            }

            foreach (var id in to_remove)
            {
                messages.Remove(id);
            }
        }
    }

    // Packet class with compression support
    public class Packet
    {
        public byte flag;
        public byte version;
        public ushort seq;
        public List<byte> requirements;
        public List<byte> payload;

        public Packet()
        {
            flag = 0;
            version = Protocol.VERSION;
            seq = 0;
            requirements = new List<byte>();
            payload = new List<byte>();
        }

        public Packet(Flag f, ushort sequence)
        {
            flag = (byte)f;
            version = Protocol.VERSION;
            seq = sequence;
            requirements = new List<byte>();
            payload = new List<byte>();
        }

        public Packet(Flag f, ushort sequence, List<byte> req, List<byte> data)
        {
            flag = (byte)f;
            version = Protocol.VERSION;
            seq = sequence;
            requirements = req ?? new List<byte>();
            payload = data ?? new List<byte>();
        }

        public byte[] Serialize()
        {
            ushort payload_len = (ushort)payload.Count;
            ushort req_len = (ushort)requirements.Count;

            List<byte> buffer = new List<byte>(8 + req_len + payload_len);

            buffer.Add(flag);
            buffer.Add(version);
            buffer.Add((byte)((seq >> 8) & 0xFF));
            buffer.Add((byte)(seq & 0xFF));
            buffer.Add((byte)((payload_len >> 8) & 0xFF));
            buffer.Add((byte)(payload_len & 0xFF));
            buffer.Add((byte)((req_len >> 8) & 0xFF));
            buffer.Add((byte)(req_len & 0xFF));

            buffer.AddRange(requirements);
            buffer.AddRange(payload);

            return buffer.ToArray();
        }

        public static Packet Deserialize(byte[] data)
        {
            if (data.Length < 8)
            {
                throw new Exception("Packet too small");
            }

            Packet pkt = new Packet();
            pkt.flag = data[0];
            pkt.version = data[1];
            pkt.seq = (ushort)((data[2] << 8) | data[3]);

            ushort payload_len = (ushort)((data[4] << 8) | data[5]);
            ushort req_len = (ushort)((data[6] << 8) | data[7]);

            if (data.Length < 8 + req_len + payload_len)
            {
                throw new Exception("Packet data incomplete");
            }

            pkt.requirements = new List<byte>(data.Skip(8).Take(req_len));
            pkt.payload = new List<byte>(data.Skip(8 + req_len).Take(payload_len));

            return pkt;
        }

        public static Packet MakeConn(ushort seq, List<byte> client_pubkey)
        {
            return new Packet(Flag.CONN, seq, client_pubkey, new List<byte>());
        }

        public static Packet MakeGive(ushort seq, List<byte> recipient_key, List<byte> data)
        {
            return new Packet(Flag.GIVE, seq, recipient_key, data);
        }

        public static Packet MakeTake(ushort seq, List<byte> resource_id = null)
        {
            return new Packet(Flag.TAKE, seq, resource_id ?? new List<byte>(), new List<byte>());
        }

        public static Packet MakeSeen(ushort seq)
        {
            return new Packet(Flag.SEEN, seq, new List<byte>(), new List<byte>());
        }

        public static Packet MakeStop(ushort seq)
        {
            return new Packet(Flag.STOP, seq, new List<byte>(), new List<byte>());
        }

        public static Packet MakePing(ushort seq)
        {
            return new Packet(Flag.PING, seq, new List<byte>(), new List<byte>());
        }

        public static Packet MakePong(ushort seq)
        {
            return new Packet(Flag.PONG, seq, new List<byte>(), new List<byte>());
        }

        public bool IsValid()
        {
            return flag <= (byte)Flag.PONG && version == Protocol.VERSION;
        }
    }

    // Socket wrapper with improved error handling
    public class HeroSocket
    {
        private UdpClient socket;
        private bool initialized;

        public HeroSocket()
        {
            socket = new UdpClient();
            socket.Client.Blocking = false;
            initialized = true;
        }

        public void Bind(ushort port)
        {
            socket = new UdpClient(port);
            socket.Client.Blocking = false;
        }

        public bool Send(byte[] data, string host, ushort port)
        {
            try
            {
                int sent = socket.Send(data, data.Length, host, port);
                return sent > 0;
            }
            catch
            {
                return false;
            }
        }

        public bool Recv(out byte[] buffer, out string from_host, out ushort from_port)
        {
            buffer = null;
            from_host = "";
            from_port = 0;

            try
            {
                IPEndPoint remoteEP = new IPEndPoint(IPAddress.Any, 0);
                buffer = socket.Receive(ref remoteEP);
                from_host = remoteEP.Address.ToString();
                from_port = (ushort)remoteEP.Port;
                return true;
            }
            catch (SocketException)
            {
                return false;
            }
        }

        public void Close()
        {
            socket?.Close();
        }
    }

    // Enhanced client with automatic fragmentation and reconnection
    public class HeroClient
    {
        private HeroSocket socket;
        private ushort seq_num;
        private string server_host;
        private ushort server_port;
        private bool connected;
        private FragmentManager fragment_mgr;
        private DateTime last_ping;
        private int ping_ms;

        public HeroClient()
        {
            socket = new HeroSocket();
            seq_num = 0;
            server_port = 0;
            connected = false;
            fragment_mgr = new FragmentManager();
            last_ping = DateTime.Now;
            ping_ms = 0;
        }

        public bool Connect(string host, ushort port, List<byte> pubkey = null)
        {
            server_host = host;
            server_port = port;

            if (pubkey == null)
                pubkey = new List<byte> { 1, 2, 3, 4 };

            // Send CONN packet
            var conn_pkt = Packet.MakeConn(seq_num++, pubkey);
            var data = conn_pkt.Serialize();

            if (!socket.Send(data, server_host, server_port))
            {
                return false;
            }

            // Wait for SEEN acknowledgment
            var start = DateTime.Now;
            while ((DateTime.Now - start).TotalMilliseconds < Protocol.DEFAULT_TIMEOUT_MS)
            {
                byte[] buffer;
                string from_host;
                ushort from_port;

                if (socket.Recv(out buffer, out from_host, out from_port))
                {
                    try
                    {
                        var pkt = Packet.Deserialize(buffer);
                        if (pkt.flag == (byte)Flag.SEEN)
                        {
                            connected = true;
                            last_ping = DateTime.Now;
                            return true;
                        }
                    }
                    catch { }
                }
                Thread.Sleep(10);
            }

            return false;
        }

        public bool SendLarge(List<byte> data, List<byte> recipient_key = null)
        {
            if (!connected)
                return false;

            if (data.Count <= Protocol.MAX_PAYLOAD_SIZE)
            {
                return Send(data, recipient_key);
            }

            // Fragment large messages
            var fragments = fragment_mgr.Fragment(data.ToArray(), Flag.GIVE);
            bool all_sent = true;

            foreach (var frag in fragments)
            {
                if (!socket.Send(frag.Serialize(), server_host, server_port))
                {
                    all_sent = false;
                }
                Thread.Sleep(1); // Small delay between fragments
            }

            return all_sent;
        }

        public bool Send(List<byte> data, List<byte> recipient_key = null)
        {
            if (!connected)
                return false;

            if (recipient_key == null)
                recipient_key = new List<byte>();

            var pkt = Packet.MakeGive(seq_num++, recipient_key, data);
            return socket.Send(pkt.Serialize(), server_host, server_port);
        }

        public bool Send(string text, List<byte> recipient_key = null)
        {
            return Send(new List<byte>(Encoding.UTF8.GetBytes(text)), recipient_key);
        }

        public bool SendCommand(string command, params object[] args)
        {
            byte[] data = MagicWords.Encode(command, args);
            return Send(new List<byte>(data));
        }

        public bool Ping()
        {
            if (!connected)
                return false;

            var ping_start = DateTime.Now;
            var pkt = Packet.MakePing(seq_num++);
            
            if (!socket.Send(pkt.Serialize(), server_host, server_port))
                return false;

            // Wait for PONG
            var start = DateTime.Now;
            while ((DateTime.Now - start).TotalMilliseconds < 1000)
            {
                byte[] buffer;
                string from_host;
                ushort from_port;

                if (socket.Recv(out buffer, out from_host, out from_port))
                {
                    try
                    {
                        var response = Packet.Deserialize(buffer);
                        if (response.flag == (byte)Flag.PONG)
                        {
                            ping_ms = (int)(DateTime.Now - ping_start).TotalMilliseconds;
                            last_ping = DateTime.Now;
                            return true;
                        }
                    }
                    catch { }
                }
                Thread.Sleep(1);
            }

            return false;
        }

        public void KeepAlive()
        {
            if ((DateTime.Now - last_ping).TotalSeconds > 5)
            {
                Ping();
            }
        }

        public bool Receive(out Packet out_packet, int timeout_ms = 100)
        {
            out_packet = null;

            var start = DateTime.Now;
            while ((DateTime.Now - start).TotalMilliseconds < timeout_ms)
            {
                byte[] buffer;
                string from_host;
                ushort from_port;

                if (socket.Recv(out buffer, out from_host, out from_port))
                {
                    try
                    {
                        var pkt = Packet.Deserialize(buffer);

                        // Handle fragments
                        if (pkt.flag == (byte)Flag.FRAG)
                        {
                            var result = fragment_mgr.AddFragment(pkt);
                            if (result.Item1)
                            {
                                // Fragment complete, reconstruct packet
                                out_packet = new Packet(result.Item3, pkt.seq, new List<byte>(), new List<byte>(result.Item2));
                                var seen = Packet.MakeSeen(out_packet.seq);
                                socket.Send(seen.Serialize(), from_host, from_port);
                                return true;
                            }
                            continue; // Wait for more fragments
                        }

                        // Send SEEN acknowledgment
                        var seen_pkt = Packet.MakeSeen(pkt.seq);
                        socket.Send(seen_pkt.Serialize(), from_host, from_port);

                        out_packet = pkt;
                        return true;
                    }
                    catch { }
                }
                Thread.Sleep(1);
            }
            
            fragment_mgr.CleanupStale();
            return false;
        }

        public bool ReceiveCommand(out string command, out List<string> args, int timeout_ms = 100)
        {
            command = "";
            args = new List<string>();
            
            Packet pkt;
            if (Receive(out pkt, timeout_ms))
            {
                var decoded = MagicWords.Decode(pkt.payload.ToArray());
                command = decoded.Item1;
                args = decoded.Item2;
                return true;
            }
            return false;
        }

        public bool ReceiveString(out string out_text, int timeout_ms = 100)
        {
            out_text = "";
            Packet pkt;
            if (Receive(out pkt, timeout_ms))
            {
                out_text = Encoding.UTF8.GetString(pkt.payload.ToArray());
                return true;
            }
            return false;
        }

        public void Disconnect()
        {
            if (connected)
            {
                var stop_pkt = Packet.MakeStop(seq_num++);
                socket.Send(stop_pkt.Serialize(), server_host, server_port);
                connected = false;
            }
        }

        public bool IsConnected()
        {
            return connected;
        }

        public int GetPing()
        {
            return ping_ms;
        }
    }

    // Enhanced server with automatic fragmentation
    public class HeroServer
    {
        private HeroSocket socket;
        private ushort port;
        private bool running;
        private FragmentManager fragment_mgr;

        private class Client
        {
            public string host;
            public ushort port;
            public List<byte> pubkey;
            public DateTime last_seen;
            public DateTime last_ping;
        }

        private Dictionary<string, Client> clients;

        private string MakeClientKey(string host, ushort port)
        {
            return host + ":" + port.ToString();
        }

        public HeroServer(ushort listen_port)
        {
            port = listen_port;
            running = false;
            clients = new Dictionary<string, Client>();
            socket = new HeroSocket();
            fragment_mgr = new FragmentManager();
            socket.Bind(port);
        }

        public void Start()
        {
            running = true;
        }

        public void Stop()
        {
            running = false;
        }

        public bool Poll(Action<Packet, string, ushort> handler)
        {
            if (!running)
                return false;

            byte[] buffer;
            string from_host;
            ushort from_port;

            if (socket.Recv(out buffer, out from_host, out from_port))
            {
                try
                {
                    var pkt = Packet.Deserialize(buffer);
                    string client_key = MakeClientKey(from_host, from_port);

                    // Handle fragments
                    if (pkt.flag == (byte)Flag.FRAG)
                    {
                        var result = fragment_mgr.AddFragment(pkt);
                        if (result.Item1)
                        {
                            // Reconstruct original packet
                            pkt = new Packet(result.Item3, pkt.seq, new List<byte>(), new List<byte>(result.Item2));
                            var seen = Packet.MakeSeen(pkt.seq);
                            socket.Send(seen.Serialize(), from_host, from_port);
                        }
                        else
                        {
                            return false; // Still waiting for more fragments
                        }
                    }

                    // Handle different packet types
                    if (pkt.flag == (byte)Flag.CONN)
                    {
                        Client c = new Client();
                        c.host = from_host;
                        c.port = from_port;
                        c.pubkey = pkt.requirements;
                        c.last_seen = DateTime.Now;
                        c.last_ping = DateTime.Now;
                        clients[client_key] = c;

                        var seen = Packet.MakeSeen(pkt.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);
                    }
                    else if (pkt.flag == (byte)Flag.STOP)
                    {
                        clients.Remove(client_key);
                        var seen = Packet.MakeSeen(pkt.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);
                    }
                    else if (pkt.flag == (byte)Flag.PING)
                    {
                        if (clients.ContainsKey(client_key))
                        {
                            clients[client_key].last_ping = DateTime.Now;
                        }
                        var pong = Packet.MakePong(pkt.seq);
                        socket.Send(pong.Serialize(), from_host, from_port);
                    }
                    else
                    {
                        if (clients.ContainsKey(client_key))
                        {
                            clients[client_key].last_seen = DateTime.Now;
                        }

                        var seen = Packet.MakeSeen(pkt.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);

                        if (handler != null)
                        {
                            handler(pkt, from_host, from_port);
                        }
                    }

                    return true;
                }
                catch { }
            }

            fragment_mgr.CleanupStale();
            return false;
        }

        public void SendTo(List<byte> data, string host, ushort port)
        {
            if (data.Count <= Protocol.MAX_PAYLOAD_SIZE)
            {
                var pkt = Packet.MakeGive(0, new List<byte>(), data);
                socket.Send(pkt.Serialize(), host, port);
            }
            else
            {
                // Fragment large messages
                var fragments = fragment_mgr.Fragment(data.ToArray(), Flag.GIVE);
                foreach (var frag in fragments)
                {
                    socket.Send(frag.Serialize(), host, port);
                    Thread.Sleep(1);
                }
            }
        }

        public void SendTo(string text, string host, ushort port)
        {
            SendTo(new List<byte>(Encoding.UTF8.GetBytes(text)), host, port);
        }

        public void SendCommand(string command, string host, ushort port, params object[] args)
        {
            byte[] data = MagicWords.Encode(command, args);
            SendTo(new List<byte>(data), host, port);
        }

        public void Broadcast(List<byte> data)
        {
            foreach (var kvp in clients)
            {
                SendTo(data, kvp.Value.host, kvp.Value.port);
            }
        }

        public void Broadcast(string text)
        {
            Broadcast(new List<byte>(Encoding.UTF8.GetBytes(text)));
        }

        public void BroadcastCommand(string command, params object[] args)
        {
            byte[] data = MagicWords.Encode(command, args);
            Broadcast(new List<byte>(data));
        }

        public void Reply(Packet original_pkt, List<byte> response_data, string client_host, ushort client_port)
        {
            SendTo(response_data, client_host, client_port);
        }

        public void Reply(Packet original_pkt, string response_text, string client_host, ushort client_port)
        {
            SendTo(response_text, client_host, client_port);
        }

        public void CleanupStaleClients(int timeout_seconds = 30)
        {
            var now = DateTime.Now;
            var to_remove = new List<string>();

            foreach (var kvp in clients)
            {
                if ((now - kvp.Value.last_seen).TotalSeconds > timeout_seconds)
                {
                    to_remove.Add(kvp.Key);
                }
            }

            foreach (var key in to_remove)
            {
                clients.Remove(key);
            }
        }

        public int GetClientCount()
        {
            return clients.Count;
        }

        public bool IsRunning()
        {
            return running;
        }
    }
}
