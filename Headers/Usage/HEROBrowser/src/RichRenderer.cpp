// RichRenderer.cpp - Implementation of rich text rendering
#include "../include/RichRenderer.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>

RichRenderer::RichRenderer(SDL_Renderer *r)
    : renderer(r), font_body(nullptr), font_body_bold(nullptr),
      font_header(nullptr), font_header_large(nullptr), font_mono(nullptr),
      viewport_y(0), total_content_height(0), hover_element_index(-1) {

  std::vector<std::string> mono_fonts = {
      "/usr/share/fonts/TTF/Hack-Regular.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/System/Library/Fonts/Helvetica.ttc",
      "C:\\Windows\\Fonts\\arial.ttf"
  };

  std::vector<std::string> fonts = {
      "/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
      "/System/Library/Fonts/Courier.dfont",
      "C:\\Windows\\Fonts\\consola.ttf"
  };

  font_body = loadFont(fonts, 18);
  font_body_bold = loadFont(fonts, 20);
  font_header = loadFont(fonts, 28);
  font_header_large = loadFont(fonts, 36);
  font_mono = loadFont(mono_fonts, 16);

  if (!font_body)
    std::cerr << "Critical: Failed to load fonts." << std::endl;
}

RichRenderer::~RichRenderer() {
  clearPage();
  if (font_body)
    TTF_CloseFont(font_body);
  if (font_body_bold)
    TTF_CloseFont(font_body_bold);
  if (font_header)
    TTF_CloseFont(font_header);
  if (font_header_large)
    TTF_CloseFont(font_header_large);
  if (font_mono)
    TTF_CloseFont(font_mono);
}

TTF_Font *RichRenderer::loadFont(const std::vector<std::string> &candidates,
                                  int size) {
  for (const auto &p : candidates) {
    TTF_Font *f = TTF_OpenFont(p.c_str(), size);
    if (f)
      return f;
  }
  return nullptr;
}

void RichRenderer::clearPage() {
  for (auto &el : elements)
    el.destroy();
  elements.clear();
  viewport_y = 0;
  total_content_height = 0;
  hover_element_index = -1;
}

void RichRenderer::renderWord(const std::string &word, int &cur_x, int &cur_y,
                               int max_width, TTF_Font *font, SDL_Color color,
                               bool is_link, const std::string &href,
                               bool is_header) {
  SDL_Surface *surf = TTF_RenderUTF8_Blended(font, word.c_str(), color);
  if (!surf)
    return;

  if (cur_x + surf->w > max_width && cur_x > MARGIN_X) {
    cur_x = MARGIN_X;
    cur_y += LINE_HEIGHT;
  }

  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
  PageElement el;
  el.texture = tex;
  el.rect = {cur_x, cur_y, surf->w, surf->h};
  el.is_link = is_link;
  el.href = href;
  el.is_header = is_header;
  el.font_size = 18;
  elements.push_back(el);

  cur_x += surf->w + 8;
  SDL_FreeSurface(surf);
}

void RichRenderer::renderBullet(int x, int y) {
  SDL_Rect bullet = {x, y + 10, LIST_BULLET_SIZE, LIST_BULLET_SIZE};
  SDL_SetRenderDrawColor(renderer, Colors::BULLET.r, Colors::BULLET.g,
                         Colors::BULLET.b, Colors::BULLET.a);
  SDL_RenderFillRect(renderer, &bullet);
}

void RichRenderer::layoutPage(const std::string &raw_html, int window_width) {
  clearPage();

  int cur_x = MARGIN_X;
  int cur_y = MARGIN_Y;
  int content_width = std::min(MAX_LINE_WIDTH, window_width - (MARGIN_X * 2));
  int max_width = MARGIN_X + content_width;

  std::string html = raw_html;
  size_t pos = 0;

  while (pos < html.length()) {
    // Handle H1
    size_t h1_start = html.find("<h1>", pos);
    if (h1_start != std::string::npos && h1_start == pos) {
      size_t h1_end = html.find("</h1>", h1_start);
      if (h1_end != std::string::npos) {
        if (!elements.empty())
          cur_y += 25;
        cur_x = MARGIN_X;

        std::string text = html.substr(h1_start + 4, h1_end - h1_start - 4);
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
          renderWord(word, cur_x, cur_y, max_width, font_header_large,
                     Colors::TEXT_HEADER, false, "", true);
        }
        cur_y += HEADER_LARGE_HEIGHT + 10;
        cur_x = MARGIN_X;
        pos = h1_end + 5;
        continue;
      }
    }

    // Handle H2
    size_t h2_start = html.find("<h2>", pos);
    if (h2_start != std::string::npos && h2_start == pos) {
      size_t h2_end = html.find("</h2>", h2_start);
      if (h2_end != std::string::npos) {
        if (!elements.empty())
          cur_y += 20;
        cur_x = MARGIN_X;

        std::string text = html.substr(h2_start + 4, h2_end - h2_start - 4);
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
          renderWord(word, cur_x, cur_y, max_width, font_header,
                     Colors::TEXT_HEADER, false, "", true);
        }
        cur_y += HEADER_HEIGHT + 8;
        cur_x = MARGIN_X;
        pos = h2_end + 5;
        continue;
      }
    }

    // Handle PRE (code blocks)
    size_t pre_start = html.find("<pre>", pos);
    if (pre_start != std::string::npos && pre_start == pos) {
      size_t pre_end = html.find("</pre>", pre_start);
      if (pre_end != std::string::npos) {
        cur_y += 15;
        cur_x = MARGIN_X;

        std::string code = html.substr(pre_start + 5, pre_end - pre_start - 5);
        int code_start_y = cur_y;

        // Render code lines
        std::stringstream ss(code);
        std::string line;
        while (std::getline(ss, line)) {
          cur_x = MARGIN_X + 10;
          if (!line.empty()) {
            renderWord(line, cur_x, cur_y, max_width - 20,
                       font_mono ? font_mono : font_body, Colors::TEXT_CODE);
          }
          cur_y += LINE_HEIGHT - 4;
        }

        // Draw background after text (so text appears on top)
        SDL_Rect bg = {MARGIN_X - 10, code_start_y - 8, content_width + 20,
                       cur_y - code_start_y + 8};
        SDL_SetRenderDrawColor(renderer, Colors::BG_CODE.r, Colors::BG_CODE.g,
                               Colors::BG_CODE.b, Colors::BG_CODE.a);
        SDL_RenderFillRect(renderer, &bg);

        cur_y += 15;
        cur_x = MARGIN_X;
        pos = pre_end + 6;
        continue;
      }
    }

    // Handle UL (lists)
    size_t ul_start = html.find("<ul>", pos);
    if (ul_start != std::string::npos && ul_start == pos) {
      size_t ul_end = html.find("</ul>", ul_start);
      if (ul_end != std::string::npos) {
        cur_y += 10;
        std::string list_content =
            html.substr(ul_start + 4, ul_end - ul_start - 4);

        size_t li_pos = 0;
        while (li_pos < list_content.length()) {
          size_t li_start = list_content.find("<li>", li_pos);
          if (li_start == std::string::npos)
            break;

          size_t li_end = list_content.find("</li>", li_start);
          if (li_end == std::string::npos)
            break;

          cur_x = MARGIN_X + LIST_INDENT;
          renderBullet(MARGIN_X + 8, cur_y);

          std::string li_content =
              list_content.substr(li_start + 4, li_end - li_start - 4);

          // Handle links in list items
          size_t link_start = li_content.find("<a href=\"");
          if (link_start != std::string::npos) {
            size_t href_end = li_content.find("\">", link_start);
            size_t link_end = li_content.find("</a>", href_end);
            if (href_end != std::string::npos &&
                link_end != std::string::npos) {
              std::string href =
                  li_content.substr(link_start + 9, href_end - link_start - 9);
              std::string link_text =
                  li_content.substr(href_end + 2, link_end - href_end - 2);

              std::string before = li_content.substr(0, link_start);
              std::stringstream ss(before);
              std::string word;
              while (ss >> word) {
                renderWord(word, cur_x, cur_y, max_width, font_body,
                           Colors::TEXT_PRIMARY);
              }

              renderWord(link_text, cur_x, cur_y, max_width, font_body,
                         Colors::TEXT_LINK, true, href);

              std::string after = li_content.substr(link_end + 4);
              std::stringstream ss2(after);
              while (ss2 >> word) {
                renderWord(word, cur_x, cur_y, max_width, font_body,
                           Colors::TEXT_PRIMARY);
              }
            }
          } else {
            li_content =
                std::regex_replace(li_content, std::regex("</?strong>"), "");

            std::stringstream ss(li_content);
            std::string word;
            while (ss >> word) {
              renderWord(word, cur_x, cur_y, max_width, font_body,
                         Colors::TEXT_PRIMARY);
            }
          }

          cur_y += LINE_HEIGHT + 2;
          li_pos = li_end + 5;
        }

        cur_y += 10;
        cur_x = MARGIN_X;
        pos = ul_end + 5;
        continue;
      }
    }

    // Handle P (paragraphs)
    size_t p_start = html.find("<p>", pos);
    if (p_start != std::string::npos && p_start == pos) {
      size_t p_end = html.find("</p>", p_start);
      if (p_end != std::string::npos) {
        if (!elements.empty())
          cur_y += 15;
        cur_x = MARGIN_X;

        std::string text = html.substr(p_start + 3, p_end - p_start - 3);
        std::stringstream ss(text);
        std::string word;
        while (ss >> word) {
          renderWord(word, cur_x, cur_y, max_width, font_body,
                     Colors::TEXT_PRIMARY);
        }
        cur_y += LINE_HEIGHT + 10;
        cur_x = MARGIN_X;
        pos = p_end + 4;
        continue;
      }
    }

    // Handle standalone links
    size_t a_start = html.find("<a href=\"", pos);
    if (a_start != std::string::npos && a_start == pos) {
      size_t href_end = html.find("\">", a_start);
      size_t a_end = html.find("</a>", href_end);
      if (href_end != std::string::npos && a_end != std::string::npos) {
        std::string href = html.substr(a_start + 9, href_end - a_start - 9);
        std::string link_text = html.substr(href_end + 2, a_end - href_end - 2);

        renderWord(link_text, cur_x, cur_y, max_width, font_body,
                   Colors::TEXT_LINK, true, href);
        pos = a_end + 4;
        continue;
      }
    }

    // Skip other tags
    if (html[pos] == '<') {
      size_t tag_end = html.find('>', pos);
      if (tag_end != std::string::npos) {
        pos = tag_end + 1;
        continue;
      }
    }

    // Plain text
    if (!isspace(html[pos])) {
      std::string word;
      while (pos < html.length() && html[pos] != '<' && !isspace(html[pos])) {
        word += html[pos++];
      }
      if (!word.empty()) {
        renderWord(word, cur_x, cur_y, max_width, font_body,
                   Colors::TEXT_PRIMARY);
      }
    } else {
      pos++;
    }
  }

  total_content_height = cur_y + 100;
}

std::string RichRenderer::checkClick(int mouse_x, int mouse_y) {
  int doc_y = mouse_y + viewport_y;
  for (const auto &el : elements) {
    if (el.is_link) {
      if (mouse_x >= el.rect.x && mouse_x <= el.rect.x + el.rect.w &&
          doc_y >= el.rect.y && doc_y <= el.rect.y + el.rect.h) {
        return el.href;
      }
    }
  }
  return "";
}

void RichRenderer::updateHover(int mouse_x, int mouse_y, int top_offset) {
  int doc_y = mouse_y - top_offset + viewport_y;
  hover_element_index = -1;

  for (size_t i = 0; i < elements.size(); ++i) {
    const auto &el = elements[i];
    if (el.is_link) {
      if (mouse_x >= el.rect.x && mouse_x <= el.rect.x + el.rect.w &&
          doc_y >= el.rect.y && doc_y <= el.rect.y + el.rect.h) {
        hover_element_index = i;
        break;
      }
    }
  }
}

void RichRenderer::render(int window_height, int top_offset) {
  for (size_t i = 0; i < elements.size(); ++i) {
    const auto &el = elements[i];
    int screen_y = el.rect.y - viewport_y + top_offset;

    if (screen_y + el.rect.h < top_offset)
      continue;
    if (screen_y > window_height)
      break;

    SDL_Rect dest = {el.rect.x, screen_y, el.rect.w, el.rect.h};

    if (el.is_link && hover_element_index == (int)i) {
      SDL_Rect bg = {dest.x - 4, dest.y - 2, dest.w + 8, dest.h + 4};
      SDL_SetRenderDrawColor(renderer, Colors::LINK_HOVER_BG.r,
                             Colors::LINK_HOVER_BG.g, Colors::LINK_HOVER_BG.b,
                             Colors::LINK_HOVER_BG.a);
      SDL_RenderFillRect(renderer, &bg);
    }

    SDL_RenderCopy(renderer, el.texture, nullptr, &dest);

    if (el.is_link) {
      if (hover_element_index == (int)i) {
        SDL_SetRenderDrawColor(renderer, Colors::TEXT_LINK.r,
                               Colors::TEXT_LINK.g, Colors::TEXT_LINK.b,
                               Colors::TEXT_LINK.a);
      } else {
        SDL_SetRenderDrawColor(renderer, Colors::LINK_UNDERLINE.r,
                               Colors::LINK_UNDERLINE.g,
                               Colors::LINK_UNDERLINE.b,
                               Colors::LINK_UNDERLINE.a);
      }
      SDL_RenderDrawLine(renderer, dest.x, dest.y + dest.h - 1,
                         dest.x + dest.w, dest.y + dest.h - 1);
    }
  }

  // Draw scrollbar
  if (total_content_height > window_height) {
    float pct = (float)viewport_y / (total_content_height - window_height);
    int bar_h =
        std::max(30, (window_height * window_height) / total_content_height);
    int bar_y = top_offset + (int)(pct * (window_height - top_offset - bar_h));

    SDL_Rect track = {1024 - 12, top_offset, 8, window_height - top_offset};
    SDL_SetRenderDrawColor(renderer, Colors::SCROLLBAR_TRACK.r,
                           Colors::SCROLLBAR_TRACK.g, Colors::SCROLLBAR_TRACK.b,
                           Colors::SCROLLBAR_TRACK.a);
    SDL_RenderFillRect(renderer, &track);

    SDL_Rect thumb = {1024 - 11, bar_y, 6, bar_h};
    SDL_SetRenderDrawColor(renderer, Colors::SCROLLBAR_THUMB.r,
                           Colors::SCROLLBAR_THUMB.g, Colors::SCROLLBAR_THUMB.b,
                           Colors::SCROLLBAR_THUMB.a);
    SDL_RenderFillRect(renderer, &thumb);
  }
}

void RichRenderer::scroll(int delta) {
  viewport_y += delta;
  if (viewport_y < 0)
    viewport_y = 0;
  int max_scroll = std::max(0, total_content_height - 600);
  if (viewport_y > max_scroll)
    viewport_y = max_scroll;
}

void RichRenderer::renderUIText(const std::string &text, int x, int y,
                                 SDL_Color color) {
  SDL_Surface *surface =
      TTF_RenderUTF8_Blended(font_body, text.c_str(), color);
  if (!surface)
    return;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_Rect dest = {x, y, surface->w, surface->h};
  SDL_RenderCopy(renderer, texture, nullptr, &dest);
  SDL_DestroyTexture(texture);
  SDL_FreeSurface(surface);
}

void RichRenderer::highlightSearchResults(const std::string &search_term) {
  // Future feature: highlight matching text in the page
  // This would require adding highlight flags to PageElements
}
