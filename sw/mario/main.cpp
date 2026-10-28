#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"
#include "platform.h"
#include "chr_data.h"
#include "timing.h"

// Main.cpp/SDL is replaced; all original game logic remains in SMBEngine::update().
extern "C" int main() {
    for (unsigned i = 0; i < 8192; ++i) VIDEO8(i) = mario_chr[i];
    SMBEngine *engine = new SMBEngine(const_cast<uint8_t *>(mario_chr));
    engine->reset();
    uint32_t frame = 0, last_keys = 0;
    for (;;) {
        // Wait until prior video state has been presented at a VGA frame boundary.
        while (VIDEO32(0x2a04) & 1) {}
        uint32_t keys = IO32(0);
        if ((keys & 256) && !(last_keys & 256)) engine->reset();
        last_keys = keys;
        Controller &pad = engine->getController1();
        for (unsigned i = 0; i < 8; ++i)
            pad.setButtonState(static_cast<ControllerButton>(i), (keys & (1u << i)) != 0);
        // Preserve 60 Hz game logic while submitting only 30 Hz video.
        // Both ticks see the held button state. No intermediate PPU state
        // is submitted, so the back buffer cannot be swapped mid-pair.
        for (unsigned tick = 0; tick < MARIO_UPDATES_PER_FRAME; ++tick)
            engine->update();
        engine->render(nullptr);
        IO32(0x14) = uint32_t(engine->debugByte(0x770)) |
                     (uint32_t(engine->debugByte(0x86)) << 8) |
                     (uint32_t(engine->debugByte(0xce)) << 16) |
                     (uint32_t(engine->debugByte(0x6d)) << 24);
        IO32(0x18) = uint32_t(engine->debugByte(0x71c)) |
                     (uint32_t(engine->debugByte(0x75f)) << 8) |
                     (uint32_t(engine->debugByte(0x75c)) << 16);
        IO32(0x10) = ++frame;
    }
}
