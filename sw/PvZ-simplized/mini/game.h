#pragma once
#include <stdint.h>
#include "demo.h"
#include "pointer.h"
// New integer/fixed-pool adaptation, NOT the upstream full engine.
enum { PVZ_OBJECTS=64, PVZ_ZOMBIES=8, PVZ_BULLETS=18, PVZ_SUNS=3 };
enum { PVZ_PLAY=0, PVZ_WIN=1, PVZ_LOSE=2, PVZ_PAUSE=3, PVZ_TITLE=4, PVZ_SELECT=5, PVZ_HELP=6, PVZ_DAVE=7, PVZ_READY=8 };
struct Plant { int32_t hp, cooldown; };
struct Zombie { int32_t x, hp, eating, row; }; // x in Q8 screen pixels
struct Bullet { int32_t x, active, row; };
struct Sun { int32_t x,y,life; };
struct Game {
    uint32_t age, previous_keys, sun, cursor, spawned, kills, mode, rng, selected;
    uint32_t level,cutscene,dialog;
    Plant plants[27]; Zombie zombies[PVZ_ZOMBIES];
    Bullet bullets[PVZ_BULLETS]; Sun suns[PVZ_SUNS];
};
struct Sprite { uint32_t xy, texture, size, reserved; };
struct Scene { Sprite objects[PVZ_OBJECTS]; uint32_t count,background; };
void pvz_reset(Game& game,bool title=false,unsigned level=0);
void pvz_step(Game& game,uint16_t keys,uint32_t click=0);
void pvz_scene(const Game& game,Scene& scene);
uint32_t pvz_hash(const Game& game);
uint32_t pvz_metrics(const Game& game);
