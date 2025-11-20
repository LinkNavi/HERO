// PageElement.h - Represents a renderable page element
#ifndef PAGE_ELEMENT_H
#define PAGE_ELEMENT_H

#include <SDL2/SDL.h>
#include <string>

struct PageElement {
  SDL_Texture *texture;
  SDL_Rect rect;
  bool is_link;
  std::string href;
  bool is_header;
  bool is_image;
  int font_size;
  std::string alt_text;

  PageElement()
      : texture(nullptr), rect({0, 0, 0, 0}), is_link(false), is_header(false),
        is_image(false), font_size(18) {}

  void destroy() {
    if (texture) {
      SDL_DestroyTexture(texture);
      texture = nullptr;
    }
  }
};

#endif // PAGE_ELEMENT_H
