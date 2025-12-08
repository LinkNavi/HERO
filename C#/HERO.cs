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
        public const byte VERSION = 1;
        public const int MAX_PACKET_SIZE = 65507;
        public const int DEFAULT_TIMEOUT_MS = 5000;
        public const int MAX_RETRIES = 3;
    }

    // Protocol flags
    public enum Flag : byte
    {
        CONN = 0, // Start connection (requires client public key)
        GIVE = 1, // Send data (requires recipient key + payload)
        TAKE = 2, // Request data/resources
        SEEN = 3, // Acknowledge packet receipt
        STOP = 4  // Close connection
    }

    // Packet class
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

        public bool IsValid()
        {
            return flag <= (byte)Flag.STOP && version == Protocol.VERSION;
        }
    }

    // Socket wrapper
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

    // Client implementation
    public class HeroClient
    {
        private HeroSocket socket;
        private ushort seq_num;
        private string server_host;
        private ushort server_port;
        private bool connected;

        public HeroClient()
        {
            socket = new HeroSocket();
            seq_num = 0;
            server_port = 0;
            connected = false;
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
                            return true;
                        }
                    }
                    catch { }
                }
                Thread.Sleep(10);
            }

            return false;
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

        public bool Ping()
        {
            if (!connected)
                return false;

            var pkt = Packet.MakeTake(seq_num++);
            return socket.Send(pkt.Serialize(), server_host, server_port);
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
                        out_packet = Packet.Deserialize(buffer);

                        // Send SEEN acknowledgment
                        var seen = Packet.MakeSeen(out_packet.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);

                        return true;
                    }
                    catch { }
                }
                Thread.Sleep(1);
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
    }

    // Server implementation
    public class HeroServer
    {
        private HeroSocket socket;
        private ushort port;
        private bool running;

        private class Client
        {
            public string host;
            public ushort port;
            public List<byte> pubkey;
            public DateTime last_seen;
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

                    // Handle different packet types
                    if (pkt.flag == (byte)Flag.CONN)
                    {
                        // New connection
                        Client c = new Client();
                        c.host = from_host;
                        c.port = from_port;
                        c.pubkey = pkt.requirements;
                        c.last_seen = DateTime.Now;
                        clients[client_key] = c;

                        // Send SEEN acknowledgment
                        var seen = Packet.MakeSeen(pkt.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);
                    }
                    else if (pkt.flag == (byte)Flag.STOP)
                    {
                        // Disconnect
                        clients.Remove(client_key);
                        var seen = Packet.MakeSeen(pkt.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);
                    }
                    else
                    {
                        // Update last seen
                        if (clients.ContainsKey(client_key))
                        {
                            clients[client_key].last_seen = DateTime.Now;
                        }

                        // Send SEEN acknowledgment
                        var seen = Packet.MakeSeen(pkt.seq);
                        socket.Send(seen.Serialize(), from_host, from_port);

                        // Call handler
                        if (handler != null)
                        {
                            handler(pkt, from_host, from_port);
                        }
                    }

                    return true;
                }
                catch { }
            }

            return false;
        }

        public void SendTo(List<byte> data, string host, ushort port)
        {
            var pkt = Packet.MakeGive(0, new List<byte>(), data);
            socket.Send(pkt.Serialize(), host, port);
        }

        public void SendTo(string text, string host, ushort port)
        {
            SendTo(new List<byte>(Encoding.UTF8.GetBytes(text)), host, port);
        }

        public void Reply(Packet original_pkt, List<byte> response_data, string client_host, ushort client_port)
        {
            SendTo(response_data, client_host, client_port);
        }

        public void Reply(Packet original_pkt, string response_text, string client_host, ushort client_port)
        {
            SendTo(response_text, client_host, client_port);
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

    // Simple Web Server Extension
    public class HeroWebServer
    {
        private HeroServer server;
        private string root_dir;

        private string GetMimeType(string path)
        {
            if (path.EndsWith(".html") || path.EndsWith(".htm"))
                return "text/html";
            if (path.EndsWith(".css"))
                return "text/css";
            if (path.EndsWith(".js"))
                return "application/javascript";
            if (path.EndsWith(".json"))
                return "application/json";
            if (path.EndsWith(".png"))
                return "image/png";
            if (path.EndsWith(".jpg") || path.EndsWith(".jpeg"))
                return "image/jpeg";
            if (path.EndsWith(".gif"))
                return "image/gif";
            if (path.EndsWith(".svg"))
                return "image/svg+xml";
            if (path.EndsWith(".txt"))
                return "text/plain";
            return "application/octet-stream";
        }

        private string ReadFile(string filepath)
        {
            try
            {
                return File.ReadAllText(filepath);
            }
            catch
            {
                return "";
            }
        }

        private void SendResponse(string content, string mime_type, string host, ushort port)
        {
            string response = "HTTP/1.0 200 OK\r\n";
            response += "Content-Type: " + mime_type + "\r\n";
            response += "Content-Length: " + content.Length.ToString() + "\r\n\r\n";
            response += content;

            // Send in chunks if needed (max ~60KB per packet for safety)
            const int CHUNK_SIZE = 60000;
            for (int i = 0; i < response.Length; i += CHUNK_SIZE)
            {
                int len = Math.Min(CHUNK_SIZE, response.Length - i);
                string chunk = response.Substring(i, len);
                server.SendTo(chunk, host, port);
            }
        }

        private void Send404(string host, ushort port)
        {
            string response = "HTTP/1.0 404 Not Found\r\n\r\n<h1>404 Not Found</h1>";
            server.SendTo(response, host, port);
        }

        public HeroWebServer(ushort port, string root_directory = ".")
        {
            server = new HeroServer(port);
            root_dir = root_directory;
            server.Start();
        }

        public void Serve()
        {
            server.Poll((pkt, host, port) =>
            {
                string request = Encoding.UTF8.GetString(pkt.payload.ToArray());

                // Parse GET request
                if (request.StartsWith("GET "))
                {
                    int path_start = 4;
                    int path_end = request.IndexOf(' ', path_start);
                    if (path_end == -1)
                        path_end = request.IndexOf('\r', path_start);
                    if (path_end == -1)
                        path_end = request.Length;

                    string path = request.Substring(path_start, path_end - path_start);

                    // Default to index.html
                    if (path == "/" || path == "")
                    {
                        path = "/index.html";
                    }

                    // Security: prevent directory traversal
                    if (path.Contains(".."))
                    {
                        Send404(host, port);
                        return;
                    }

                    string filepath = root_dir + path;
                    string content = ReadFile(filepath);

                    if (content != "")
                    {
                        SendResponse(content, GetMimeType(path), host, port);
                    }
                    else
                    {
                        Send404(host, port);
                    }
                }
            });
        }

        public bool IsRunning()
        {
            return server.IsRunning();
        }
    }

    // Simple Web Client (Browser)
    public class HeroBrowser
    {
        private HeroClient client;

        public HeroBrowser()
        {
            client = new HeroClient();
        }

        public string Get(string host, ushort port, string path = "/")
        {
            if (!client.IsConnected())
            {
                if (!client.Connect(host, port))
                {
                    return "ERROR: Could not connect to server";
                }
            }

            // Send GET request
            string request = "GET " + path + " HTTP/1.0\r\n\r\n";
            client.Send(request);

            // Receive response (handle multiple chunks)
            string full_response = "";
            string chunk;

            // Try to receive multiple chunks
            for (int i = 0; i < 10; i++)
            {
                if (client.ReceiveString(out chunk, 1000))
                {
                    full_response += chunk;
                }
                else
                {
                    break;
                }
            }

            // Extract body from HTTP response
            int body_start = full_response.IndexOf("\r\n\r\n");
            if (body_start != -1)
            {
                return full_response.Substring(body_start + 4);
            }

            return full_response;
        }

        public void Disconnect()
        {
            client.Disconnect();
        }
    }

    // Dynamic Web Server with persistent connections and event support
    public class HeroDynamicWebServer
    {
        private HeroServer server;
        private string root_dir;

        private class WebClient
        {
            public string host;
            public ushort port;
            public DateTime last_seen;
            public string current_path;
            public bool waiting_for_updates;
        }

        private Dictionary<string, WebClient> web_clients;
        private Dictionary<string, Func<Dictionary<string, string>, string>> route_handlers;

        private string MakeClientKey(string host, ushort port)
        {
            return host + ":" + port.ToString();
        }

        private string GetMimeType(string path)
        {
            if (path.EndsWith(".html") || path.EndsWith(".htm"))
                return "text/html";
            if (path.EndsWith(".css"))
                return "text/css";
            if (path.EndsWith(".js"))
                return "application/javascript";
            if (path.EndsWith(".json"))
                return "application/json";
            if (path.EndsWith(".png"))
                return "image/png";
            if (path.EndsWith(".jpg") || path.EndsWith(".jpeg"))
                return "image/jpeg";
            if (path.EndsWith(".gif"))
                return "image/gif";
            if (path.EndsWith(".svg"))
                return "image/svg+xml";
            if (path.EndsWith(".txt"))
                return "text/plain";
            return "application/octet-stream";
        }

        private string ReadFile(string filepath)
        {
            try
            {
                return File.ReadAllText(filepath);
            }
            catch
            {
                return "";
            }
        }

        private Dictionary<string, string> ParseQueryString(string query)
        {
            var result = new Dictionary<string, string>();
            var pairs = query.Split('&');

            foreach (var pair in pairs)
            {
                var parts = pair.Split('=');
                if (parts.Length == 2)
                {
                    result[parts[0]] = parts[1];
                }
            }
            return result;
        }

        private void SendResponse(string content, string mime_type, string host, ushort port, string extra_headers = "")
        {
            string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: " + mime_type + "\r\n";
            response += "Content-Length: " + content.Length.ToString() + "\r\n";
            if (extra_headers != "")
            {
                response += extra_headers;
            }
            response += "\r\n" + content;

            const int CHUNK_SIZE = 60000;
            for (int i = 0; i < response.Length; i += CHUNK_SIZE)
            {
                int len = Math.Min(CHUNK_SIZE, response.Length - i);
                string chunk = response.Substring(i, len);
                server.SendTo(chunk, host, port);
            }
        }

        private void SendJSON(string json, string host, ushort port)
        {
            SendResponse(json, "application/json", host, port);
        }

        private void Send404(string host, ushort port)
        {
            string response = "HTTP/1.1 404 Not Found\r\n\r\n<h1>404 Not Found</h1>";
            server.SendTo(response, host, port);
        }

        public HeroDynamicWebServer(ushort port, string root_directory = ".")
        {
            server = new HeroServer(port);
            root_dir = root_directory;
            web_clients = new Dictionary<string, WebClient>();
            route_handlers = new Dictionary<string, Func<Dictionary<string, string>, string>>();
            server.Start();
        }

        public void Route(string path, Func<Dictionary<string, string>, string> handler)
        {
            route_handlers[path] = handler;
        }

        public void PushToClient(string host, ushort port, string data)
        {
            string client_key = MakeClientKey(host, port);
            if (web_clients.ContainsKey(client_key))
            {
                SendResponse(data, "text/event-stream", host, port,
                           "Cache-Control: no-cache\r\nConnection: keep-alive\r\n");
            }
        }

        public void Broadcast(string data)
        {
            foreach (var kvp in web_clients)
            {
                if (kvp.Value.waiting_for_updates)
                {
                    PushToClient(kvp.Value.host, kvp.Value.port, data);
                    kvp.Value.waiting_for_updates = false;
                }
            }
        }

        public void BroadcastEvent(string event_name, string data)
        {
            string event_data = "event: " + event_name + "\ndata: " + data + "\n\n";
            Broadcast(event_data);
        }

        public void Serve()
        {
            server.Poll((pkt, host, port) =>
            {
                string request = Encoding.UTF8.GetString(pkt.payload.ToArray());
                string client_key = MakeClientKey(host, port);

                // Track client
                if (!web_clients.ContainsKey(client_key))
                {
                    WebClient wc = new WebClient();
                    wc.host = host;
                    wc.port = port;
                    wc.last_seen = DateTime.Now;
                    wc.waiting_for_updates = false;
                    web_clients[client_key] = wc;
                }
                else
                {
                    web_clients[client_key].last_seen = DateTime.Now;
                }

                // Parse HTTP request
                if (request.StartsWith("GET ") || request.StartsWith("POST "))
                {
                    bool is_post = request.StartsWith("POST ");
                    int path_start = is_post ? 5 : 4;
                    int path_end = request.IndexOf(' ', path_start);
                    if (path_end == -1)
                        path_end = request.IndexOf('\r', path_start);
                    if (path_end == -1)
                        path_end = request.Length;

                    string full_path = request.Substring(path_start, path_end - path_start);

                    // Split path and query string
                    string path = full_path;
                    string query = "";
                    int q_pos = full_path.IndexOf('?');
                    if (q_pos != -1)
                    {
                        path = full_path.Substring(0, q_pos);
                        query = full_path.Substring(q_pos + 1);
                    }

                    var parameters = ParseQueryString(query);

                    // Handle POST body
                    if (is_post)
                    {
                        int body_start = request.IndexOf("\r\n\r\n");
                        if (body_start != -1)
                        {
                            string body = request.Substring(body_start + 4);
                            var body_params = ParseQueryString(body);
                            foreach (var kvp in body_params)
                            {
                                parameters[kvp.Key] = kvp.Value;
                            }
                        }
                    }

                    web_clients[client_key].current_path = path;

                    // Check for Server-Sent Events (SSE) connection
                    if (path == "/events" || parameters.ContainsKey("stream"))
                    {
                        web_clients[client_key].waiting_for_updates = true;
                        SendResponse("", "text/event-stream", host, port,
                                   "Cache-Control: no-cache\r\nConnection: keep-alive\r\n");
                        return;
                    }

                    // Check for dynamic route handler
                    if (route_handlers.ContainsKey(path))
                    {
                        string response = route_handlers[path](parameters);
                        SendResponse(response, "text/html", host, port);
                        return;
                    }

                    // Default to index.html for root
                    if (path == "/" || path == "")
                    {
                        path = "/index.html";
                    }

                    // Security: prevent directory traversal
                    if (path.Contains(".."))
                    {
                        Send404(host, port);
                        return;
                    }

                    // Serve static file
                    string filepath = root_dir + path;
                    string content = ReadFile(filepath);

                    if (content != "")
                    {
                        SendResponse(content, GetMimeType(path), host, port);
                    }
                    else
                    {
                        Send404(host, port);
                    }
                }
            });
        }

        public int GetClientCount()
        {
            return web_clients.Count;
        }

        public void CleanupStaleClients()
        {
            var now = DateTime.Now;
            var to_remove = new List<string>();

            foreach (var kvp in web_clients)
            {
                if ((now - kvp.Value.last_seen).TotalSeconds > 30)
                {
                    to_remove.Add(kvp.Key);
                }
            }

            foreach (var key in to_remove)
            {
                web_clients.Remove(key);
            }
        }

        public bool IsRunning()
        {
            return server.IsRunning();
        }
    }
}
