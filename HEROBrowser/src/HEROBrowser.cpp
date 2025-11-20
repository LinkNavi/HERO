// HEROBrowser.cpp - Main browser implementation
#include "HEROBrowser.h"
#include "../include/HERO.h"
#include "Colors.h"
#include <algorithm>
#include <iostream>
#include <regex>

HEROBrowser::HEROBrowser()
    : window(nullptr), renderer(nullptr), running(false),
      url_input_active(false), show_bookmarks(false), show_history(false),
      show_search(false), mouse_x(0), mouse_y(0), window_width(1024),
      window_height(768) {}

HEROBrowser::~HEROBrowser() { cleanup(); }

bool HEROBrowser::init() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL Init failed: " << SDL_GetError() << std::endl;
    return false;
  }
  if (TTF_Init() < 0) {
    std::cerr << "TTF Init failed: " << TTF_GetError() << std::endl;
    return false;
  }

  window = SDL_CreateWindow(
      "HERO Browser - Enhanced Edition", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, window_width, window_height,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
    return false;
  }

  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
    return false;
  }

  page_renderer = std::make_unique<RichRenderer>(renderer);
  history_manager = std::make_unique<HistoryManager>();
  bookmark_manager = std::make_unique<BookmarkManager>();

  running = true;
  SDL_StartTextInput();

  loadPage("localhost.hero:8080");
  return true;
}

bool HEROBrowser::isHeroDomain(const std::string &url) {
  return url.find(".hero") != std::string::npos || url.rfind("hero://", 0) == 0;
}

std::pair<std::string, uint16_t>
HEROBrowser::parseHeroURL(const std::string &url) {
  std::regex hero_regex(R"(^(?:hero://)?([^/:]+)(?::(\d+))?)",
                        std::regex::icase);
  std::smatch match;
  if (std::regex_search(url, match, hero_regex)) {
    std::string host = match[1].str();
    uint16_t port = 8080;
    if (match.size() > 2 && match[2].matched) {
      try {
        port = static_cast<uint16_t>(std::stoi(match[2].str()));
      } catch (...) {
      }
    }
    return {host, port};
  }
  return {"localhost", 8080};
}

std::string HEROBrowser::fetchContent(const std::string &host, uint16_t port) {
  HERO::HeroBrowser browser;
  std::string resp = browser.get(host, port, "/");
  browser.disconnect();

  if (resp.rfind("ERROR:", 0) == 0) {
    return "<h1>Connection Error</h1><p>" + resp + "</p>";
  }
  return resp;
}

void HEROBrowser::loadPage(const std::string &url, bool pushHistory) {
  if (url.empty())
    return;

  status_message = "Loading " + url + "...";
  renderFrame();

  std::string content;
  if (isHeroDomain(url)) {
    auto [host, port] = parseHeroURL(url);
    content = fetchContent(host, port);
  } else {
    content = "<h1>Protocol Error</h1><p>Only .hero domains are supported. "
              "Try <b>localhost.hero:8080</b></p>";
  }

  if (pushHistory) {
    history_manager->addEntry(url);
  }

  url_bar_text = url;
  page_renderer->layoutPage(content, window_width);
  status_message = "✓ Loaded";
}

void HEROBrowser::goBack() {
  std::string url = history_manager->goBack();
  if (!url.empty()) {
    loadPage(url, false);
  }
}

void HEROBrowser::goForward() {
  std::string url = history_manager->goForward();
  if (!url.empty()) {
    loadPage(url, false);
  }
}

void HEROBrowser::refresh() {
  if (!url_bar_text.empty()) {
    loadPage(url_bar_text, false);
  }
}

void HEROBrowser::goHome() { loadPage("localhost.hero:8080"); }

void HEROBrowser::toggleBookmark() {
  if (bookmark_manager->isBookmarked(url_bar_text)) {
    // Remove bookmark
    auto bookmarks = bookmark_manager->getBookmarks();
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      if (bookmarks[i].url == url_bar_text) {
        bookmark_manager->removeBookmark(i);
        status_message = "✗ Bookmark removed";
        break;
      }
    }
  } else {
    // Add bookmark
    bookmark_manager->addBookmark(url_bar_text, url_bar_text);
    status_message = "★ Bookmarked";
  }
}

void HEROBrowser::performPageSearch() {
  if (!search_text.empty()) {
    page_renderer->highlightSearchResults(search_text);
    status_message = "Found: " + search_text;
  }
}

void HEROBrowser::handleKeyboardShortcuts(SDL_Keysym &keysym) {
  if (keysym.mod & KMOD_CTRL) {
    switch (keysym.sym) {
    case SDLK_l:
      url_input_active = true;
      break;
    case SDLK_d:
      toggleBookmark();
      break;
    case SDLK_h:
      show_history = !show_history;
      show_bookmarks = false;
      break;
    case SDLK_b:
      show_bookmarks = !show_bookmarks;
      show_history = false;
      break;
    case SDLK_f:
      show_search = !show_search;
      break;
    case SDLK_r:
      refresh();
      break;
    case SDLK_LEFT:
      goBack();
      break;
    case SDLK_RIGHT:
      goForward();
      break;
    case SDLK_v:
      if (SDL_HasClipboardText()) {
        char *text = SDL_GetClipboardText();
        if (text) {
          if (url_input_active)
            url_bar_text += text;
          else if (show_search)
            search_text += text;
          SDL_free(text);
        }
      }
      break;
    }
  } else {
    // Non-Ctrl shortcuts
    switch (keysym.sym) {
    case SDLK_F5:
      refresh();
      break;
    case SDLK_HOME:
      goHome();
      break;
    case SDLK_ESCAPE:
      url_input_active = false;
      show_bookmarks = false;
      show_history = false;
      show_search = false;
      break;
    }
  }
}

void HEROBrowser::handleInputEvents(SDL_Event &event) {
  if (event.type == SDL_WINDOWEVENT) {
    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
      window_width = event.window.data1;
      window_height = event.window.data2;
      page_renderer->layoutPage(url_bar_text, window_width);
    }
  } else if (event.type == SDL_TEXTINPUT) {
    if (url_input_active) {
      url_bar_text += event.text.text;
    } else if (show_search) {
      search_text += event.text.text;
    }
  } else if (event.type == SDL_KEYDOWN) {
    handleKeyboardShortcuts(event.key.keysym);

    if (event.key.keysym.sym == SDLK_RETURN) {
      if (url_input_active) {
        url_input_active = false;
        loadPage(url_bar_text);
      } else if (show_search) {
        performPageSearch();
      }
    } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
      if (url_input_active && !url_bar_text.empty()) {
        url_bar_text.pop_back();
      } else if (show_search && !search_text.empty()) {
        search_text.pop_back();
      }
    }
  } else if (event.type == SDL_MOUSEBUTTONDOWN) {
    if (event.button.y < URL_BAR_HEIGHT) {
      // Click on URL bar area
      if (event.button.x >= 80 && event.button.x <= window_width - 80) {
        url_input_active = true;
        show_bookmarks = false;
        show_history = false;
      } else if (event.button.x < 80) {
        // Navigation buttons area
        if (event.button.x < 40) {
          goBack();
        } else {
          goForward();
        }
      }
    } else if (show_bookmarks) {
      // Handle bookmark clicks
      int y_offset = URL_BAR_HEIGHT + 40;
      int item_height = 35;
      auto bookmarks = bookmark_manager->getBookmarks();
      int index = (event.button.y - y_offset) / item_height;
      if (index >= 0 && index < (int)bookmarks.size()) {
        loadPage(bookmarks[index].url);
        show_bookmarks = false;
      }
    } else if (show_history) {
      // Handle history clicks
      int y_offset = URL_BAR_HEIGHT + 40;
      int item_height = 30;
      auto history = history_manager->getHistory();
      int index = (event.button.y - y_offset) / item_height;
      if (index >= 0 && index < (int)history.size()) {
        loadPage(history[index]);
        show_history = false;
      }
    } else {
      // Page content click
      url_input_active = false;
      std::string target = page_renderer->checkClick(
          event.button.x, event.button.y - URL_BAR_HEIGHT);
      if (!target.empty()) {
        if (target.find("://") == std::string::npos &&
            target.find(".hero") == std::string::npos) {
          status_message = "⚠ Relative links not supported";
        } else {
          loadPage(target);
        }
      }
    }
  } else if (event.type == SDL_MOUSEWHEEL) {
    if (!show_bookmarks && !show_history) {
      page_renderer->scroll(-event.wheel.y * 40);
    }
  } else if (event.type == SDL_MOUSEMOTION) {
    mouse_x = event.motion.x;
    mouse_y = event.motion.y;
    if (!show_bookmarks && !show_history) {
      page_renderer->updateHover(mouse_x, mouse_y, URL_BAR_HEIGHT);
    }
  }
}

void HEROBrowser::renderBookmarksPanel() {
  SDL_Rect panel = {window_width - 320, URL_BAR_HEIGHT, 320,
                    window_height - URL_BAR_HEIGHT - STATUS_BAR_HEIGHT};
  SDL_SetRenderDrawColor(renderer, Colors::BG_WHITE.r, Colors::BG_WHITE.g,
                         Colors::BG_WHITE.b, Colors::BG_WHITE.a);
  SDL_RenderFillRect(renderer, &panel);

  SDL_SetRenderDrawColor(renderer, Colors::BORDER_LIGHT.r,
                         Colors::BORDER_LIGHT.g, Colors::BORDER_LIGHT.b,
                         Colors::BORDER_LIGHT.a);
  SDL_RenderDrawLine(renderer, panel.x, panel.y, panel.x,
                     panel.y + panel.h);

  page_renderer->renderUIText("★ Bookmarks", panel.x + 15,
                              URL_BAR_HEIGHT + 10, Colors::TEXT_HEADER);

  auto bookmarks = bookmark_manager->getBookmarks();
  int y_offset = URL_BAR_HEIGHT + 40;
  for (const auto &bm : bookmarks) {
    page_renderer->renderUIText(bm.title, panel.x + 15, y_offset,
                                Colors::TEXT_PRIMARY);
    y_offset += 35;
  }

  if (bookmarks.empty()) {
    page_renderer->renderUIText("No bookmarks yet", panel.x + 15, y_offset,
                                Colors::TEXT_MUTED);
    page_renderer->renderUIText("Press Ctrl+D to add", panel.x + 15,
                                y_offset + 25, Colors::TEXT_MUTED);
  }
}

void HEROBrowser::renderHistoryPanel() {
  SDL_Rect panel = {window_width - 320, URL_BAR_HEIGHT, 320,
                    window_height - URL_BAR_HEIGHT - STATUS_BAR_HEIGHT};
  SDL_SetRenderDrawColor(renderer, Colors::BG_WHITE.r, Colors::BG_WHITE.g,
                         Colors::BG_WHITE.b, Colors::BG_WHITE.a);
  SDL_RenderFillRect(renderer, &panel);

  SDL_SetRenderDrawColor(renderer, Colors::BORDER_LIGHT.r,
                         Colors::BORDER_LIGHT.g, Colors::BORDER_LIGHT.b,
                         Colors::BORDER_LIGHT.a);
  SDL_RenderDrawLine(renderer, panel.x, panel.y, panel.x,
                     panel.y + panel.h);

  page_renderer->renderUIText("⏱ History", panel.x + 15, URL_BAR_HEIGHT + 10,
                              Colors::TEXT_HEADER);

  auto history = history_manager->getHistory();
  int y_offset = URL_BAR_HEIGHT + 40;
  int max_display = std::min((int)history.size(), 15);
  
  for (int i = history.size() - 1; i >= history.size() - max_display; --i) {
    std::string display_url = history[i];
    if (display_url.length() > 35) {
      display_url = display_url.substr(0, 32) + "...";
    }
    
    SDL_Color color = (i == history_manager->getCurrentIndex())
                          ? Colors::TEXT_LINK
                          : Colors::TEXT_PRIMARY;
    page_renderer->renderUIText(display_url, panel.x + 15, y_offset, color);
    y_offset += 30;
  }

  if (history.empty()) {
    page_renderer->renderUIText("No history yet", panel.x + 15, y_offset,
                                Colors::TEXT_MUTED);
  }
}

void HEROBrowser::renderSearchBar() {
  SDL_Rect search_bar = {window_width - 320, window_height - STATUS_BAR_HEIGHT - 40,
                         300, 35};
  SDL_SetRenderDrawColor(renderer, Colors::BG_WHITE.r, Colors::BG_WHITE.g,
                         Colors::BG_WHITE.b, Colors::BG_WHITE.a);
  SDL_RenderFillRect(renderer, &search_bar);

  SDL_SetRenderDrawColor(renderer, Colors::BORDER_FOCUS.r,
                         Colors::BORDER_FOCUS.g, Colors::BORDER_FOCUS.b,
                         Colors::BORDER_FOCUS.a);
  SDL_RenderDrawRect(renderer, &search_bar);

  page_renderer->renderUIText("Search: " + search_text, search_bar.x + 10,
                              search_bar.y + 8, Colors::TEXT_PRIMARY);
}

void HEROBrowser::renderUI() {
  // Top bar background
  SDL_SetRenderDrawColor(renderer, Colors::BG_GRAY_LIGHT.r,
                         Colors::BG_GRAY_LIGHT.g, Colors::BG_GRAY_LIGHT.b,
                         Colors::BG_GRAY_LIGHT.a);
  SDL_Rect barRect = {0, 0, window_width, URL_BAR_HEIGHT};
  SDL_RenderFillRect(renderer, &barRect);

  // URL input box
  int box_x = 80;
  int box_y = 15;
  int box_w = window_width - 160;
  int box_h = 32;

  SDL_Rect inputBox = {box_x, box_y, box_w, box_h};
  if (url_input_active) {
    SDL_SetRenderDrawColor(renderer, Colors::BG_WHITE.r, Colors::BG_WHITE.g,
                           Colors::BG_WHITE.b, Colors::BG_WHITE.a);
  } else {
    SDL_SetRenderDrawColor(renderer, Colors::BG_GRAY.r, Colors::BG_GRAY.g,
                           Colors::BG_GRAY.b, Colors::BG_GRAY.a);
  }
  SDL_RenderFillRect(renderer, &inputBox);

  if (url_input_active) {
    SDL_SetRenderDrawColor(renderer, Colors::BORDER_FOCUS.r,
                           Colors::BORDER_FOCUS.g, Colors::BORDER_FOCUS.b,
                           Colors::BORDER_FOCUS.a);
  } else {
    SDL_SetRenderDrawColor(renderer, Colors::BORDER_DEFAULT.r,
                           Colors::BORDER_DEFAULT.g, Colors::BORDER_DEFAULT.b,
                           Colors::BORDER_DEFAULT.a);
  }
  SDL_RenderDrawRect(renderer, &inputBox);

  // URL bar divider
  SDL_SetRenderDrawColor(renderer, Colors::BORDER_LIGHT.r,
                         Colors::BORDER_LIGHT.g, Colors::BORDER_LIGHT.b,
                         Colors::BORDER_LIGHT.a);
  SDL_RenderDrawLine(renderer, 0, URL_BAR_HEIGHT - 1, window_width,
                     URL_BAR_HEIGHT - 1);

  // Navigation buttons
  SDL_Color btnColor = history_manager->canGoBack() ? Colors::TEXT_PRIMARY
                                                     : Colors::TEXT_MUTED;
  page_renderer->renderUIText("←", 15, 20, btnColor);

  btnColor = history_manager->canGoForward() ? Colors::TEXT_PRIMARY
                                              : Colors::TEXT_MUTED;
  page_renderer->renderUIText("→", 45, 20, btnColor);

  // Bookmark indicator
  if (bookmark_manager->isBookmarked(url_bar_text)) {
    page_renderer->renderUIText("★", window_width - 60, 20, Colors::TEXT_LINK);
  }

  // URL text
  page_renderer->renderUIText(url_bar_text, box_x + 8, box_y + 7,
                              Colors::TEXT_PRIMARY);

  // Cursor
  if (url_input_active && (SDL_GetTicks() / 500) % 2 == 0) {
    int w_est = url_bar_text.length() * 10;
    SDL_Rect cursor = {box_x + 8 + w_est, box_y + 6, 2, 20};
    SDL_SetRenderDrawColor(renderer, Colors::BORDER_FOCUS.r,
                           Colors::BORDER_FOCUS.g, Colors::BORDER_FOCUS.b,
                           Colors::BORDER_FOCUS.a);
    SDL_RenderFillRect(renderer, &cursor);
  }

  // Status bar
  SDL_SetRenderDrawColor(renderer, Colors::BG_GRAY_LIGHT.r,
                         Colors::BG_GRAY_LIGHT.g, Colors::BG_GRAY_LIGHT.b,
                         Colors::BG_GRAY_LIGHT.a);
  SDL_Rect statRect = {0, window_height - STATUS_BAR_HEIGHT, window_width,
                       STATUS_BAR_HEIGHT};
  SDL_RenderFillRect(renderer, &statRect);

  SDL_SetRenderDrawColor(renderer, Colors::BORDER_LIGHT.r,
                         Colors::BORDER_LIGHT.g, Colors::BORDER_LIGHT.b,
                         Colors::BORDER_LIGHT.a);
  SDL_RenderDrawLine(renderer, 0, window_height - STATUS_BAR_HEIGHT,
                     window_width, window_height - STATUS_BAR_HEIGHT);

  // Status text
  std::string stat_text = status_message;
  if (history_manager->getCurrentIndex() >= 0) {
    stat_text += "  •  Page " +
                 std::to_string(history_manager->getCurrentIndex() + 1) + "/" +
                 std::to_string(history_manager->getHistorySize());
  }
  page_renderer->renderUIText(stat_text, 15, window_height - 22,
                              Colors::TEXT_MUTED);

  // Overlays
  if (show_bookmarks) {
    renderBookmarksPanel();
  }
  if (show_history) {
    renderHistoryPanel();
  }
  if (show_search) {
    renderSearchBar();
  }
}

void HEROBrowser::renderFrame() {
  SDL_SetRenderDrawColor(renderer, Colors::BG_WHITE.r, Colors::BG_WHITE.g,
                         Colors::BG_WHITE.b, Colors::BG_WHITE.a);
  SDL_RenderClear(renderer);

  page_renderer->render(window_height - STATUS_BAR_HEIGHT, URL_BAR_HEIGHT);
  renderUI();

  SDL_RenderPresent(renderer);
}

void HEROBrowser::run() {
  SDL_Event event;
  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = false;
      else
        handleInputEvents(event);
    }
    renderFrame();
    SDL_Delay(16); // ~60 FPS
  }
  cleanup();
}

void HEROBrowser::cleanup() {
  page_renderer.reset();
  if (renderer)
    SDL_DestroyRenderer(renderer);
  if (window)
    SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();
}


