#ifndef ENTITIES_H
#define ENTITIES_H

#include "game_types.h"

// Flat, pre-allocated pool of every sprite in play (player, enemies, pickups, pots). No malloc/free per-sprite:
// spawning claims the next slot, despawning flags it inactive, CleanUpSprites() compacts. sprites[0] is always
// the player and is never reordered out of that slot.
extern Sprite sprites[MAX_SPRITES];
extern int spriteCount;

// Screen-feedback timers driven by combat, read/decremented by main()'s frame loop.
extern int hitStopTimer;    // freezes gameplay physics briefly on a dash hit
extern int screenShakeTimer;
extern int hitFlashTimer;   // white flash on a kill

float EnemyHpMult(int variant); // used by LoadRoom to scale a spawn's maxhp

// Shared "advance frame every N ticks" helper for sprite-sheet animation loops - originally boss-only
// (UpdateBossAnimation), now also driving the shop room's idle shopkeeper (see UpdateShopRoom in world.c).
void AdvanceAnimFrame(int *animTimer, int *animFrame, int frameCount, int frameDuration);

void move(Sprite *s);            // input, dash, and screen-edge clamp for a controllable sprite (currently just the player)
void Update(GameAssets *assets); // per-frame AI, physics, and all player/enemy/pickup/pot interactions
void CleanUpSprites(void);       // swap-and-pop compaction over the sprite pool
void DrawExplosionEffects(GameAssets *assets); // fireball bursts spawned by Explosive kills - call inside the camera transform

#endif // ENTITIES_H