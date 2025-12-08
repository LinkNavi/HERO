// test_server.cpp - Enhanced HERO web server with multiple test pages
#include "../include/HERO.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <ctime>
#include <iomanip>

// Utility function to read file content
std::string readFile(const std::string &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Get current timestamp
std::string getCurrentTime() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

// Generate a test page with various HTML elements
std::string generateTestPage() {
  std::stringstream html;
  html << "<h1>Browser Feature Test Page</h1>\n";
  html << "<p>This page tests various HTML rendering capabilities of the HERO "
          "Browser.</p>\n";

  html << "<h2>Text Formatting</h2>\n";
  html << "<p>This is a regular paragraph with <strong>bold text</strong> and "
          "some very long content that should wrap naturally across multiple "
          "lines to test the text reflow capabilities of the rendering engine. "
          "The quick brown fox jumps over the lazy dog.</p>\n";

  html << "<h2>Lists</h2>\n";
  html << "<p>Here's an unordered list:</p>\n";
  html << "<ul>\n";
  html << "<li>First item in the list</li>\n";
  html << "<li>Second item with more text to see wrapping behavior</li>\n";
  html << "<li>Third item with a <a href=\"hero://localhost:8080/links\">link "
          "inside</a></li>\n";
  html << "<li>Fourth item</li>\n";
  html << "<li>Fifth item to test scrolling</li>\n";
  html << "</ul>\n";

  html << "<h2>Links</h2>\n";
  html << "<p>Test navigation with these links:</p>\n";
  html << "<ul>\n";
  html << "<li><a href=\"hero://localhost:8080/\">Home Page</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/about\">About HERO</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/docs\">Documentation</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/code\">Code Examples</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/test\">This Test Page</a></li>\n";
  html << "</ul>\n";

  html << "<h2>Code Blocks</h2>\n";
  html << "<p>Here's a code example:</p>\n";
  html << "<pre>\n";
  html << "#include &lt;iostream&gt;\n";
  html << "\n";
  html << "int main() {\n";
  html << "    std::cout &lt;&lt; \"Hello, HERO!\" &lt;&lt; std::endl;\n";
  html << "    return 0;\n";
  html << "}\n";
  html << "</pre>\n";

  html << "<h2>Another Code Example</h2>\n";
  html << "<pre>\n";
  html << "// HERO Protocol Example\n";
  html << "HERO::HeroWebServer server(8080);\n";
  html << "while (server.isRunning()) {\n";
  html << "    server.serve();\n";
  html << "}\n";
  html << "</pre>\n";

  html << "<p>Current server time: " << getCurrentTime() << "</p>\n";

  return html.str();
}

// Generate documentation page
std::string generateDocsPage() {
  std::stringstream html;
  html << "<h1>HERO Protocol Documentation</h1>\n";

  html << "<h2>Overview</h2>\n";
  html << "<p>HERO is a lightweight, UDP-based protocol designed for fast and "
          "efficient communication between clients and servers. It provides a "
          "simple yet powerful framework for building networked "
          "applications.</p>\n";

  html << "<h2>Packet Structure</h2>\n";
  html << "<p>Each HERO packet consists of:</p>\n";
  html << "<ul>\n";
  html << "<li><strong>Header</strong>: Contains packet type and metadata</li>\n";
  html << "<li><strong>Payload</strong>: The actual data being transmitted</li>\n";
  html << "<li><strong>Checksum</strong>: For data integrity verification</li>\n";
  html << "</ul>\n";

  html << "<h2>Packet Types</h2>\n";
  html << "<pre>\n";
  html << "CONN (0) - Connection establishment\n";
  html << "GIVE (1) - Data transmission\n";
  html << "TAKE (2) - Resource request\n";
  html << "SEEN (3) - Acknowledgment\n";
  html << "STOP (4) - Connection termination\n";
  html << "</pre>\n";

  html << "<h2>Connection Flow</h2>\n";
  html << "<ul>\n";
  html << "<li>Client sends CONN packet to server</li>\n";
  html << "<li>Server responds with SEEN acknowledgment</li>\n";
  html << "<li>Client sends TAKE request for resource</li>\n";
  html << "<li>Server sends GIVE with requested data</li>\n";
  html << "<li>Client sends SEEN to acknowledge receipt</li>\n";
  html << "<li>Either party can send STOP to close connection</li>\n";
  html << "</ul>\n";

  html << "<h2>API Reference</h2>\n";
  html << "<p>See the <a href=\"hero://localhost:8080/code\">code "
          "examples</a> page for implementation details.</p>\n";

  html << "<p><a href=\"hero://localhost:8080/\">← Back to Home</a></p>\n";

  return html.str();
}

// Generate about page
std::string generateAboutPage() {
  std::stringstream html;
  html << "<h1>About HERO Protocol</h1>\n";

  html << "<h2>What is HERO?</h2>\n";
  html << "<p>HERO (High-Efficiency Reliable Object protocol) is a modern "
          "networking protocol designed for applications that require low "
          "latency and high throughput. Built on UDP, it provides reliability "
          "features while maintaining speed.</p>\n";

  html << "<h2>Key Features</h2>\n";
  html << "<ul>\n";
  html << "<li><strong>Low Latency</strong>: UDP-based for minimal overhead</li>\n";
  html << "<li><strong>Reliability</strong>: Built-in acknowledgment system</li>\n";
  html << "<li><strong>Security</strong>: Optional encryption support</li>\n";
  html << "<li><strong>Simplicity</strong>: Easy-to-use C++ API</li>\n";
  html << "<li><strong>Cross-Platform</strong>: Works on Linux, macOS, and "
          "Windows</li>\n";
  html << "</ul>\n";

  html << "<h2>Use Cases</h2>\n";
  html << "<p>HERO is ideal for:</p>\n";
  html << "<ul>\n";
  html << "<li>Real-time web applications</li>\n";
  html << "<li>IoT device communication</li>\n";
  html << "<li>Game networking</li>\n";
  html << "<li>Microservices architecture</li>\n";
  html << "<li>Custom protocol implementations</li>\n";
  html << "</ul>\n";

  html << "<h2>Browser</h2>\n";
  html << "<p>The HERO Browser is a custom C++ application built with SDL2 "
          "that can render HTML content served over the HERO protocol. It "
          "features:</p>\n";
  html << "<ul>\n";
  html << "<li>Rich text rendering with multiple font sizes</li>\n";
  html << "<li>Clickable hyperlinks with hover effects</li>\n";
  html << "<li>Scrollable content with visual scrollbar</li>\n";
  html << "<li>Code block syntax highlighting</li>\n";
  html << "<li>Bookmark management</li>\n";
  html << "<li>Navigation history</li>\n";
  html << "</ul>\n";

  html << "<p><a href=\"hero://localhost:8080/test\">Try the test page</a> to "
          "see all features in action.</p>\n";

  html << "<p><a href=\"hero://localhost:8080/\">← Back to Home</a></p>\n";

  return html.str();
}

// Generate code examples page
std::string generateCodePage() {
  std::stringstream html;
  html << "<h1>Code Examples</h1>\n";

  html << "<h2>Creating a Server</h2>\n";
  html << "<p>Basic HERO server setup:</p>\n";
  html << "<pre>\n";
  html << "#include \"HERO.h\"\n";
  html << "#include &lt;iostream&gt;\n";
  html << "\n";
  html << "int main() {\n";
  html << "    HERO::HeroWebServer server(8080);\n";
  html << "    std::cout &lt;&lt; \"Server running...\" &lt;&lt; std::endl;\n";
  html << "    \n";
  html << "    while (server.isRunning()) {\n";
  html << "        server.serve();\n";
  html << "    }\n";
  html << "    \n";
  html << "    return 0;\n";
  html << "}\n";
  html << "</pre>\n";

  html << "<h2>Creating a Client</h2>\n";
  html << "<p>Connect and fetch data:</p>\n";
  html << "<pre>\n";
  html << "#include \"HERO.h\"\n";
  html << "#include &lt;iostream&gt;\n";
  html << "\n";
  html << "int main() {\n";
  html << "    HERO::HeroBrowser browser;\n";
  html << "    \n";
  html << "    std::string response = browser.get(\n";
  html << "        \"localhost\", 8080, \"/\"\n";
  html << "    );\n";
  html << "    \n";
  html << "    std::cout &lt;&lt; response &lt;&lt; std::endl;\n";
  html << "    browser.disconnect();\n";
  html << "    \n";
  html << "    return 0;\n";
  html << "}\n";
  html << "</pre>\n";

  html << "<h2>Dynamic Routes</h2>\n";
  html << "<p>Create custom routes with parameters:</p>\n";
  html << "<pre>\n";
  html << "HERO::HeroDynamicWebServer server(8080, \".\");\n";
  html << "\n";
  html << "server.route(\"/api/user\",\n";
  html << "    [](const auto&amp; params) {\n";
  html << "        return \"&lt;h1&gt;User Profile&lt;/h1&gt;\";\n";
  html << "    }\n";
  html << ");\n";
  html << "\n";
  html << "while (server.isRunning()) {\n";
  html << "    server.serve();\n";
  html << "}\n";
  html << "</pre>\n";

  html << "<h2>File Serving</h2>\n";
  html << "<p>Serve static files:</p>\n";
  html << "<pre>\n";
  html << "std::string readFile(const std::string&amp; path) {\n";
  html << "    std::ifstream file(path);\n";
  html << "    std::stringstream buffer;\n";
  html << "    buffer &lt;&lt; file.rdbuf();\n";
  html << "    return buffer.str();\n";
  html << "}\n";
  html << "\n";
  html << "server.route(\"/page\",\n";
  html << "    [](const auto&amp; params) {\n";
  html << "        return readFile(\"page.html\");\n";
  html << "    }\n";
  html << ");\n";
  html << "</pre>\n";

  html << "<p>More examples: <a href=\"hero://localhost:8080/docs\">Documentation</a></p>\n";
  html << "<p><a href=\"hero://localhost:8080/\">← Back to Home</a></p>\n";

  return html.str();
}

// Generate links test page
std::string generateLinksPage() {
  std::stringstream html;
  html << "<h1>Link Navigation Test</h1>\n";
  html << "<p>This page tests link navigation and history management.</p>\n";

  html << "<h2>Internal Links</h2>\n";
  html << "<ul>\n";
  html << "<li><a href=\"hero://localhost:8080/\">Home</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/about\">About</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/docs\">Docs</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/code\">Code</a></li>\n";
  html << "<li><a href=\"hero://localhost:8080/test\">Test Page</a></li>\n";
  html << "</ul>\n";

  html << "<h2>Navigation Tips</h2>\n";
  html << "<ul>\n";
  html << "<li>Use <strong>Ctrl+Left Arrow</strong> to go back</li>\n";
  html << "<li>Use <strong>Ctrl+Right Arrow</strong> to go forward</li>\n";
  html << "<li>Press <strong>Ctrl+D</strong> to bookmark this page</li>\n";
  html << "<li>Press <strong>Ctrl+H</strong> to view history</li>\n";
  html << "<li>Press <strong>Ctrl+B</strong> to view bookmarks</li>\n";
  html << "</ul>\n";

  html << "<h2>Test Sequence</h2>\n";
  html << "<p>Try clicking through these pages in order:</p>\n";
  html << "<ul>\n";
  html << "<li>1. <a href=\"hero://localhost:8080/about\">About Page</a></li>\n";
  html << "<li>2. <a href=\"hero://localhost:8080/docs\">Documentation</a></li>\n";
  html << "<li>3. <a href=\"hero://localhost:8080/code\">Code Examples</a></li>\n";
  html << "<li>4. Use back button to navigate backwards</li>\n";
  html << "<li>5. Use forward button to navigate forwards</li>\n";
  html << "</ul>\n";

  html << "<p><a href=\"hero://localhost:8080/\">← Back to Home</a></p>\n";

  return html.str();
}

int main() {
  std::cout << "==============================================\n";
  std::cout << "  HERO Enhanced Test Server\n";
  std::cout << "==============================================\n";
  std::cout << "Starting server on port 8080...\n" << std::endl;

  HERO::HeroDynamicWebServer server(8080, ".");

  // Root route - serve index.html or fallback
  server.route("/", [](const std::map<std::string, std::string> &params)
                       -> std::string {
    std::string content = readFile("./index.html");
    if (content.empty()) {
      return "<h1>Welcome to HERO Server!</h1><p>Create an index.html file or "
             "visit <a href=\"hero://localhost:8080/test\">the test "
             "page</a>.</p>";
    }
    return content;
  });

  // Test page with various HTML elements
  server.route("/test", [](const std::map<std::string, std::string> &params)
                            -> std::string { return generateTestPage(); });

  // About page
  server.route("/about", [](const std::map<std::string, std::string> &params)
                             -> std::string { return generateAboutPage(); });

  // Documentation page
  server.route("/docs", [](const std::map<std::string, std::string> &params)
                            -> std::string { return generateDocsPage(); });

  // Code examples page
  server.route("/code", [](const std::map<std::string, std::string> &params)
                            -> std::string { return generateCodePage(); });

  // Links test page
  server.route("/links", [](const std::map<std::string, std::string> &params)
                             -> std::string { return generateLinksPage(); });

  // Status page
  server.route("/status", [](const std::map<std::string, std::string> &params)
                              -> std::string {
    std::stringstream html;
    html << "<h1>Server Status</h1>\n";
    html << "<p>Server is running!</p>\n";
    html << "<p>Current time: " << getCurrentTime() << "</p>\n";
    html << "<ul>\n";
    html << "<li>Protocol: HERO/1.0</li>\n";
    html << "<li>Port: 8080</li>\n";
    html << "<li>Transport: UDP</li>\n";
    html << "</ul>\n";
    html << "<p><a href=\"hero://localhost:8080/\">← Back to Home</a></p>\n";
    return html.str();
  });

  std::cout << "Server is running with the following routes:\n";
  std::cout << "  /        - Home page (index.html)\n";
  std::cout << "  /test    - Browser feature test page\n";
  std::cout << "  /about   - About HERO protocol\n";
  std::cout << "  /docs    - Documentation\n";
  std::cout << "  /code    - Code examples\n";
  std::cout << "  /links   - Link navigation test\n";
  std::cout << "  /status  - Server status\n";
  std::cout << "\nAccess via:\n";
  std::cout << "  HERO Browser: localhost.hero:8080\n";
  std::cout << "  Direct: hero://localhost:8080\n";
  std::cout << "\nPress Ctrl+C to stop...\n" << std::endl;

  while (server.isRunning()) {
    server.serve();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return 0;
}
