#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#define HEIGHT 600
#define WIDTH 900

// Movement / dash (these are just the player's starting Stats now - see Stats in game.c)
#define MOVE_ACCEL 0.8f     // how fast speed ramps up while a direction is held
#define MAX_MOVE_SPEED 6.0f // starting speed cap during normal movement
#define FRICTION 0.75f      // multiplier applied to vx/vy when that axis has no input
#define DASH_SPEED 20.0f    // fixed speed set on dash, in whatever direction you're currently moving
#define DASH_TIME 10        // starting frames the dash holds at full speed (~0.16s at 60fps)

// Health / damage (rescaled from a 0-5 baseline to 0-100 for finer-grained tuning)
#define PLAYER_MAX_HP 100
#define ENEMY_MAX_HP 3
#define CONTACT_DAMAGE 20   // damage dealt to the player when an enemy touches them (not upgradeable)
#define DASH_DAMAGE 1       // base damage dealt to an enemy when the player hits them while dashing
#define IFRAMES_DURATION 30 // starting frames of invincibility after taking a hit (~0.5s at 60fps)
#define CONTACT_RADIUS_BONUS 6.0f // leniency added on top of actual sprite size for contact/dash hits
#define PICKUP_RADIUS 40.0f  // flat, generous radius for collecting pickups/pots (not tied to sprite size)

// Enemy AI: slow constant chase, capped well under the player's own top speed
#define ENEMY_CHASE_ACCEL 0.05f
#define ENEMY_MAX_SPEED 2.0f

// Healing pots: chance to spawn one per (uncleared) room, breaks on contact
#define POT_SPAWN_CHANCE 40 // percent
#define POT_HEAL_AMOUNT 30  // scaled to the 100hp baseline, not the old 5hp one

// Rooms
#define ROOM_GRID_ROWS 3      // fixed grid of rooms, ROOM_GRID_ROWS x ROOM_GRID_COLS in size
#define ROOM_GRID_COLS 3
#define ROOM_TILE_ROWS 10     // each room is a ROOM_TILE_ROWS x ROOM_TILE_COLS grid of tiles
#define ROOM_TILE_COLS 15
#define MAX_ROOM_ENEMIES 4    // cap on enemies spawned per room

// Boss room: one fixed corner gets a full, all-elite pack instead of the
// usual random count. Bottom-right corner - as far from the safe start
// room as the 3x3 grid gets.
#define BOSS_ROOM_ROW (ROOM_GRID_ROWS - 1)
#define BOSS_ROOM_COL (ROOM_GRID_COLS - 1)

// Elites / pickups
#define ELITE_SPAWN_CHANCE 30 // percent chance a given enemy spawn point becomes elite
#define ELITE_HP_MULTIPLIER 2 // elites have this many times normal enemy hp
#define ELITE_DROP_CHANCE 100 // percent chance an elite drops a pickup on death (100 while testing)

// EXP / leveling - each level-up grants one random upgrade, same as a pickup
#define EXP_PER_KILL 10
#define EXP_TO_LEVEL_BASE 50     // exp required for level 1 -> 2
#define EXP_TO_LEVEL_GROWTH 1.3f // required exp multiplies by this per level

// Upgrade amounts - how much a single pickup/level-up adds to that stat
#define UPGRADE_MOVE_SPEED_AMOUNT 0.5f
#define UPGRADE_DAMAGE_AMOUNT 1
#define UPGRADE_DASH_TIME_AMOUNT 2
#define UPGRADE_IFRAMES_AMOUNT 5
#define UPGRADE_DODGE_CHANCE_AMOUNT 0.05f
#define UPGRADE_DASH_RADIUS_AMOUNT 8.0f
#define UPGRADE_HEAL_AMOUNT 20 // matches new CONTACT_DAMAGE - one heal undoes one hit

// Floating "+Stat" popups (pickups, level-ups, room cleared)
#define MAX_POPUP_TEXTS 8
#define POPUP_TEXT_LIFETIME 60 // frames, ~1s at 60fps
#define HIT_FLASH_DURATION 6   // frames the dash-kill screen flash lasts

#endif // GAME_CONFIG_H
