/* Synthetic lower-bound probe, NOT a PvZ port or a PvZ frame benchmark.
 * Run unchanged on RV32I RTL: opaque copy and very cheap 50% integer blending.
 * Ignores clipping, transforms, sorting, asset decoding and all game logic.
 */
#include <stdint.h>
#define COUNT (800u * 600u)
#define PHASE (*(volatile uint32_t *)0x30000010u)
static volatile uint32_t source[COUNT];
static volatile uint32_t frame[COUNT];

static void copy_pixels(unsigned count) {
    for (unsigned i = 0; i < count; ++i) frame[i] = source[i];
}
static void blend_pixels(unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        uint32_t a = source[i], b = frame[i];
        frame[i] = ((a & 0xfefefefeu) >> 1) + ((b & 0xfefefefeu) >> 1);
    }
}
int main(void) {
    PHASE = 1;
    for (unsigned i = 0; i < COUNT; ++i) source[i] = 0x22446688u;
    PHASE = 2;
    for (unsigned i = 0; i < COUNT; ++i) frame[i] = 0x6688aacCu;
    PHASE = 3; copy_pixels(COUNT);
    if (frame[COUNT-1] != 0x22446688u) return 1;
    PHASE = 4; blend_pixels(COUNT);
    if (frame[COUNT-1] != 0x22446688u) return 2;
    PHASE = 5; copy_pixels(400u*300u);
    PHASE = 6; blend_pixels(400u*300u);
    PHASE = 7; copy_pixels(320u*240u);
    PHASE = 8; blend_pixels(320u*240u);
    PHASE = 9;
    for (unsigned i = 0; i < COUNT; ++i)
        if (frame[i] != 0x22446688u || source[i] != 0x22446688u) return 3;
    PHASE = 10;
    // Exercise distinct colors too, not only the equal-color case above.
    source[0] = 0x22446688u; frame[0] = 0x6688aaccu;
    blend_pixels(1);
    return frame[0] == 0x446688aau ? 0 : 4;
}
