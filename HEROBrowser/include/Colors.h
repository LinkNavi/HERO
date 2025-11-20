// Colors.h - Color constants for consistent theming
#ifndef COLORS_H
#define COLORS_H

#include <SDL2/SDL.h>

namespace Colors {
// Text colors
const SDL_Color TEXT_PRIMARY = {45, 45, 45, 255};
const SDL_Color TEXT_LINK = {37, 99, 235, 255};
const SDL_Color TEXT_HEADER = {17, 24, 39, 255};
const SDL_Color TEXT_CODE = {88, 28, 135, 255};
const SDL_Color TEXT_MUTED = {107, 114, 128, 255};

// Background colors
const SDL_Color BG_WHITE = {255, 255, 255, 255};
const SDL_Color BG_GRAY_LIGHT = {249, 250, 251, 255};
const SDL_Color BG_GRAY = {243, 244, 246, 255};
const SDL_Color BG_CODE = {243, 244, 246, 255};

// UI colors
const SDL_Color BORDER_LIGHT = {229, 231, 235, 255};
const SDL_Color BORDER_DEFAULT = {209, 213, 219, 255};
const SDL_Color BORDER_FOCUS = {59, 130, 246, 255};

// Interactive colors
const SDL_Color LINK_HOVER_BG = {219, 234, 254, 255};
const SDL_Color LINK_UNDERLINE = {147, 197, 253, 180};

// Scrollbar
const SDL_Color SCROLLBAR_TRACK = {243, 244, 246, 255};
const SDL_Color SCROLLBAR_THUMB = {156, 163, 175, 255};

// Bullet points
const SDL_Color BULLET = {107, 114, 128, 255};
} // namespace Colors

#endif // COLORS_H
