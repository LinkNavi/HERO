#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <sstream>
#include "../include/HERO.h"

// Character structure for FreeType
struct Character {
    GLuint textureID;
    int sizeX, sizeY;
    int bearingX, bearingY;
    GLuint advance;
};

// Simple text renderer using OpenGL and FreeType
class TextRenderer {
private:
    std::map<char, Character> characters;
    GLuint VAO, VBO;
    GLuint shaderProgram;
    FT_Library ft;
    FT_Face face;
    int windowWidth, windowHeight;
    float scrollY;

    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec4 vertex;
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;
        uniform sampler2D text;
        uniform vec3 textColor;
        void main() {
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
            color = vec4(textColor, 1.0) * sampled;
        }
    )";

    GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        }
        return shader;
    }

public:
    TextRenderer(int width, int height) : windowWidth(width), windowHeight(height), scrollY(0.0f) {
        // Initialize FreeType
        if (FT_Init_FreeType(&ft)) {
            std::cerr << "Could not init FreeType Library" << std::endl;
            return;
        }

        // Try to load a font (try multiple paths)
        const char* fontPaths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
        };

        bool fontLoaded = false;
        for (const char* path : fontPaths) {
            if (FT_New_Face(ft, path, 0, &face) == 0) {
                fontLoaded = true;
                break;
            }
        }

        if (!fontLoaded) {
            std::cerr << "Failed to load font" << std::endl;
            return;
        }

        FT_Set_Pixel_Sizes(face, 0, 16);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Load ASCII characters
        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cerr << "Failed to load Glyph " << c << std::endl;
                continue;
            }

            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RED,
                face->glyph->bitmap.width, face->glyph->bitmap.rows,
                0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer
            );

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                (int)face->glyph->bitmap.width,
                (int)face->glyph->bitmap.rows,
                face->glyph->bitmap_left,
                face->glyph->bitmap_top,
                (GLuint)face->glyph->advance.x
            };
            characters[c] = character;
        }

        // Setup shader
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Setup VAO/VBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    ~TextRenderer() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(shaderProgram);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }

    void renderText(const std::string& text, float x, float y, float r, float g, float b) {
        y += scrollY;

        // Skip if off-screen
        if (y < -50 || y > windowHeight + 50) return;

        glUseProgram(shaderProgram);
        
        // Set projection matrix
        float projection[16] = {
            2.0f/windowWidth, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f/windowHeight, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };
        GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

        GLint colorLoc = glGetUniformLocation(shaderProgram, "textColor");
        glUniform3f(colorLoc, r, g, b);

        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        for (char c : text) {
            if (characters.find(c) == characters.end()) continue;
            
            Character ch = characters[c];

            float xpos = x + ch.bearingX;
            float ypos = y - (ch.sizeY - ch.bearingY);
            float w = ch.sizeX;
            float h = ch.sizeY;

            float vertices[6][4] = {
                { xpos, ypos + h, 0.0f, 0.0f },
                { xpos, ypos, 0.0f, 1.0f },
                { xpos + w, ypos, 1.0f, 1.0f },
                { xpos, ypos + h, 0.0f, 0.0f },
                { xpos + w, ypos, 1.0f, 1.0f },
                { xpos + w, ypos + h, 1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.textureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            x += (ch.advance >> 6);
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void renderHTML(const std::string& html) {
        std::string cleaned = html;
        
        // Remove script and style tags
        std::regex script_regex("<script[^>]*>[\\s\\S]*?</script>", std::regex::icase);
        cleaned = std::regex_replace(cleaned, script_regex, "");
        std::regex style_regex("<style[^>]*>[\\s\\S]*?</style>", std::regex::icase);
        cleaned = std::regex_replace(cleaned, style_regex, "");
        
        // Extract and render title
        std::regex title_regex("<title[^>]*>([^<]*)</title>", std::regex::icase);
        std::smatch title_match;
        if (std::regex_search(cleaned, title_match, title_regex)) {
            renderText(title_match[1], 10, 60, 0.0f, 0.0f, 0.5f);
        }
        
        // Remove all HTML tags
        std::regex tag_regex("<[^>]*>");
        cleaned = std::regex_replace(cleaned, tag_regex, "");
        
        // Decode HTML entities
        cleaned = std::regex_replace(cleaned, std::regex("&nbsp;"), " ");
        cleaned = std::regex_replace(cleaned, std::regex("&lt;"), "<");
        cleaned = std::regex_replace(cleaned, std::regex("&gt;"), ">");
        cleaned = std::regex_replace(cleaned, std::regex("&amp;"), "&");
        cleaned = std::regex_replace(cleaned, std::regex("&quot;"), "\"");
        
        // Render text with word wrap
        std::istringstream stream(cleaned);
        std::string line;
        float y = 90;
        float x = 10;
        const int maxLineWidth = windowWidth - 20;
        
        while (std::getline(stream, line)) {
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);
            
            if (!line.empty()) {
                std::istringstream words(line);
                std::string word;
                std::string currentLine;
                
                while (words >> word) {
                    std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
                    
                    if (testLine.length() * 8 > maxLineWidth) {
                        renderText(currentLine, x, y, 0.0f, 0.0f, 0.0f);
                        y += 20;
                        currentLine = word;
                    } else {
                        currentLine = testLine;
                    }
                }
                
                if (!currentLine.empty()) {
                    renderText(currentLine, x, y, 0.0f, 0.0f, 0.0f);
                    y += 20;
                }
            }
        }
    }

    void scroll(float delta) {
        scrollY += delta;
        if (scrollY > 0) scrollY = 0;
    }

    void resetScroll() {
        scrollY = 0;
    }

    void updateWindowSize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
    }
};

// Browser class
class HEROBrowser {
private:
    GLFWwindow* window;
    TextRenderer* textRenderer;
    
    std::string urlBarText;
    std::string currentContent;
    std::string statusMessage;
    bool running;
    
    int windowWidth = 1024;
    int windowHeight = 768;
    const int URL_BAR_HEIGHT = 40;

    bool isHeroDomain(const std::string& url) {
        return url.find(".hero") != std::string::npos;
    }

    std::pair<std::string, uint16_t> parseHeroURL(const std::string& url) {
        std::regex hero_regex("(?:hero://)?([^:/]+(?:\\.hero)?):?(\\d*)");
        std::smatch match;
        
        if (std::regex_search(url, match, hero_regex)) {
            std::string host = match[1];
            uint16_t port = match[2].length() > 0 ? std::stoi(match[2]) : 8080;
            return {host, port};
        }
        
        return {"localhost", 8080};
    }

    void fetchHeroPage(const std::string& url) {
        auto [host, port] = parseHeroURL(url);
        statusMessage = "Connecting to " + host + ":" + std::to_string(port) + "...";
        
        HERO::HeroBrowser browser;
        std::string content = browser.get(host, port, "/");
        browser.disconnect();
        
        if (content.find("ERROR:") == 0) {
            currentContent = "<html><body><h1>Connection Error</h1><p>" + content + "</p></body></html>";
            statusMessage = "Failed to connect";
        } else {
            currentContent = content;
            statusMessage = "Loaded " + url;
        }
        
        textRenderer->resetScroll();
    }

    void fetchHTTPPage(const std::string& url) {
        currentContent = "<html><body><h1>HTTP Not Yet Implemented</h1><p>This browser currently only supports .hero domains.</p></body></html>";
        statusMessage = "HTTP protocol not implemented";
        textRenderer->resetScroll();
    }

    void navigateTo(const std::string& url) {
        if (url.empty()) return;
        
        if (isHeroDomain(url) || url.find("hero://") == 0) {
            fetchHeroPage(url);
        } else {
            fetchHTTPPage(url);
        }
    }

    void drawRect(float x, float y, float width, float height, float r, float g, float b) {
        glBegin(GL_QUADS);
        glColor3f(r, g, b);
        glVertex2f(x, y);
        glVertex2f(x + width, y);
        glVertex2f(x + width, y + height);
        glVertex2f(x, y + height);
        glEnd();
    }

    void render() {
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Set up orthographic projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Draw content area (white)
        drawRect(0, URL_BAR_HEIGHT, windowWidth, windowHeight - URL_BAR_HEIGHT - 25, 1.0f, 1.0f, 1.0f);

        // Render HTML content
        if (!currentContent.empty()) {
            textRenderer->renderHTML(currentContent);
        }

        // Draw URL bar
        drawRect(0, 0, windowWidth, URL_BAR_HEIGHT, 0.94f, 0.94f, 0.94f);
        textRenderer->renderText("URL: " + urlBarText + "_", 10, 22, 0.0f, 0.0f, 0.0f);

        // Draw status bar
        drawRect(0, windowHeight - 25, windowWidth, 25, 0.9f, 0.9f, 0.9f);
        textRenderer->renderText(statusMessage, 10, windowHeight - 8, 0.2f, 0.2f, 0.2f);

        glfwSwapBuffers(window);
    }

public:
    HEROBrowser() : window(nullptr), textRenderer(nullptr), running(false) {
        urlBarText = "";
        currentContent = "<html><body><h1>Welcome to HERO Browser</h1><p>Enter a .hero domain in the URL bar above.</p><p>Example: localhost.hero:8080</p></body></html>";
        statusMessage = "Ready";
    }

    ~HEROBrowser() {
        cleanup();
    }

    bool init() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

        window = glfwCreateWindow(windowWidth, windowHeight, "HERO Browser", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window);
        glfwSetWindowUserPointer(window, this);

        // Initialize GLEW
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW" << std::endl;
            return false;
        }

        // Enable blending for text
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textRenderer = new TextRenderer(windowWidth, windowHeight);

        // Set up callbacks
        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int codepoint) {
            HEROBrowser* browser = static_cast<HEROBrowser*>(glfwGetWindowUserPointer(w));
            if (codepoint < 128) {
                browser->urlBarText += (char)codepoint;
            }
        });

        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
            if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
            
            HEROBrowser* browser = static_cast<HEROBrowser*>(glfwGetWindowUserPointer(w));
            
            if (key == GLFW_KEY_ENTER) {
                browser->navigateTo(browser->urlBarText);
            } else if (key == GLFW_KEY_BACKSPACE && !browser->urlBarText.empty()) {
                browser->urlBarText.pop_back();
            }
        });

        glfwSetScrollCallback(window, [](GLFWwindow* w, double xoffset, double yoffset) {
            HEROBrowser* browser = static_cast<HEROBrowser*>(glfwGetWindowUserPointer(w));
            browser->textRenderer->scroll(yoffset * 20);
        });

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
            HEROBrowser* browser = static_cast<HEROBrowser*>(glfwGetWindowUserPointer(w));
            browser->windowWidth = width;
            browser->windowHeight = height;
            browser->textRenderer->updateWindowSize(width, height);
            glViewport(0, 0, width, height);
        });

        running = true;
        return true;
    }

    void run() {
        while (running && !glfwWindowShouldClose(window)) {
            glfwPollEvents();
            render();
        }
    }

    void cleanup() {
        delete textRenderer;
        
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        
        glfwTerminate();
    }
};

int main(int argc, char* argv[]) {
    std::cout << "HERO Browser v0.1.0 (OpenGL)" << std::endl;
    std::cout << "A cross-platform browser supporting the HERO protocol" << std::endl;
    
    HEROBrowser browser;
    
    if (!browser.init()) {
        std::cerr << "Failed to initialize browser" << std::endl;
        return 1;
    }
    
    browser.run();
    
    return 0;
}
