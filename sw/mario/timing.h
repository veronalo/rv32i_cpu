#pragma once

// The original game advances at 60 logic ticks/second. Present only the
// second tick of each pair: 30 presentations/second, unchanged game rules.
// Shared with the host reference and simulator; no host computation of logic.
#define MARIO_UPDATES_PER_FRAME 2
#define MARIO_DISPLAY_FPS 30
