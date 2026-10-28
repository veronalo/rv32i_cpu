#pragma once
#include <cstdint>
#include "../sw/mario/timing.h"
// Frame-based, reproducible input for BOTH native C++ reference and real RTL CPU.
inline uint16_t demo_keys(uint32_t frame) {
    // Callers count displayed frames; keep the script on the original
    // 60 Hz logic timeline when each display frame contains two updates.
    frame *= MARIO_UPDATES_PER_FRAME;
    if (frame >= 40 && frame < 45) return 8; // Start, after title initialization.
    if (frame < 110) return 0;
    uint16_t keys = 128; // Right.
    if ((frame - 110) % 90 >= 20 && (frame - 110) % 90 < 40) keys |= 1;
    return keys;
}
