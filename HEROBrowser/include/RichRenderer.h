// RichRenderer.h - Rich text and HTML rendering engine
#ifndef RICH_RENDERER_H
#define RICH_RENDERER_H

#include "Colors.h"
#include "PageElement.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

class RichRenderer {
private:
  SDL_Renderer *renderer;
  TTF_Font *font_body;
  TTF_Font *font_body_bold;
  TTF_Font *font_header;
  TTF_Font *font_header_large;
  TTF_Font *font_mono;

  std::vector<PageElement> elements;
  int viewport_y;
  int total_content_height;
  int hover_element_index;

  const int LINE_HEIGHT = 28;
  const int HEADER_HEIGHT = 40;
  const int HEADER_LARGE_HEIGHT = 52;
  const int MARGIN_X = 40;
  const int MARGIN_Y = 30;
  const int MAX_LINE_WIDTH = 800;
  const int LIST_INDENT = 30;
  const int LIST_BULLET_SIZE = 6;

  TTF_Font *loadFont(const std::vector<std::string> &candidates, int size);
  void renderWord(const std::string &word, int &cur_x, int &cur_y,
                  int max_width, TTF_Font *font, SDL_Color color,
                  bool is_link = false, const std::string &href = "",
                  bool is_header = false);
  void renderBullet(int x, int y);

public:
  RichRenderer(SDL_Renderer *r);
  ~RichRenderer();

  void clearPage();
  void layoutPage(const std::string &raw_html, int window_width);
  std::string checkClick(int mouse_x, int mouse_y);
  void updateHover(int mouse_x, int mouse_y, int top_offset);
  void render(int window_height, int top_offset);
  void scroll(int delta);
  void renderUIText(const std::string &text, int x, int y, SDL_Color color);
  
  // Search functionality
  void highlightSearchResults(const std::string &search_term);
  int getViewportY() const { return viewport_y; }
  int getTotalContentHeight() const { return total_content_height; }
};

#endif // RICH_RENDERER_H
