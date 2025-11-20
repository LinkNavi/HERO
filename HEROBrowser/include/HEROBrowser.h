// HEROBrowser.h - Main browser application class
#ifndef HERO_BROWSER_H
#define HERO_BROWSER_H

#include "BookmarkManager.h"
#include "HistoryManager.h"
#include "RichRenderer.h"
#include <SDL2/SDL.h>
#include <string>
#include <memory>

class HEROBrowser {
private:
  SDL_Window *window;
  SDL_Renderer *renderer;
  std::unique_ptr<RichRenderer> page_renderer;
  std::unique_ptr<HistoryManager> history_manager;
  std::unique_ptr<BookmarkManager> bookmark_manager;

  std::string url_bar_text;
  std::string status_message;
  std::string search_text;
  
  bool running;
  bool url_input_active;
  bool show_bookmarks;
  bool show_history;
  bool show_search;
  
  int mouse_x, mouse_y;
  int window_width, window_height;

  const int URL_BAR_HEIGHT = 60;
  const int STATUS_BAR_HEIGHT = 28;

  // URL parsing and validation
  bool isHeroDomain(const std::string &url);
  std::pair<std::string, uint16_t> parseHeroURL(const std::string &url);

  // Page loading
  void loadPage(const std::string &url, bool pushHistory = true);
  std::string fetchContent(const std::string &host, uint16_t port);

  // Navigation
  void goBack();
  void goForward();
  void refresh();
  void goHome();

  // Bookmarks
  void toggleBookmark();
  void renderBookmarksPanel();

  // History
  void renderHistoryPanel();

  // Search
  void performPageSearch();
  void renderSearchBar();

  // Event handling
  void handleInputEvents(SDL_Event &event);
  void handleKeyboardShortcuts(SDL_Keysym &keysym);

  // Rendering
  void renderUI();
  void renderFrame();

public:
  HEROBrowser();
  ~HEROBrowser();

  bool init();
  void run();
  void cleanup();
};

#endif //
