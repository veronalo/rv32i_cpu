#pragma once
#include <stdint.h>
// Opt-in: Mario and upstream NVBoard widgets keep their existing behavior.
void rv32i_mouse_enable(bool enabled);
// Latest absolute physical VGA position: inside[19], y[18:10], x[9:0].
// Separate from the click FIFO: motion never consumes or blocks clicks.
uint32_t rv32i_mouse_position();
bool rv32i_mouse_pop(uint32_t& packet);
bool rv32i_mouse_inject(uint32_t packet); // SDL event injection for acceptance tests
