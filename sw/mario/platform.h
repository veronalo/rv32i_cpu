#pragma once
#include <stdint.h>
#define VIDEO8(off) (*(volatile uint8_t *)(0x20000000u + (off)))
#define VIDEO32(off) (*(volatile uint32_t *)(0x20000000u + (off)))
#define IO32(off) (*(volatile uint32_t *)(0x30000000u + (off)))
// 0: keys, 0xc: VGA frames, 0x10: submitted display frames (two logic ticks each),
// 0x14/18: game telemetry after the second logic tick.
