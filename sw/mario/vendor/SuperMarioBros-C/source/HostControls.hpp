#pragma once
#include <cstdint>
#include <SDL2/SDL.h>

// Bit order matches ControllerButton and SoC MMIO. OR physical key states so
// releasing one alias cannot cancel another key that is still held.
inline uint16_t smbHostButtons(const Uint8* keys)
{
    return uint16_t(
        ((keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_X]) ? 1 : 0) |
        (keys[SDL_SCANCODE_Z] ? 2 : 0) |
        ((keys[SDL_SCANCODE_TAB] || keys[SDL_SCANCODE_BACKSPACE]) ? 4 : 0) |
        ((keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]) ? 8 : 0) |
        ((keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) ? 16 : 0) |
        ((keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) ? 32 : 0) |
        ((keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) ? 64 : 0) |
        ((keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) ? 128 : 0));
}
