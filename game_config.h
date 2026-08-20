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
#define DASH_TIME_MAX 40    // hard cap on upgraded dash time - it's simultaneously mobility+offense+defense, so left uncapped it snowballs faster than any other stat

// Health / damage (rescaled from a 0-5 baseline to 0-100 for finer-grained tuning)
#define PLAYER_MAX_HP 100
#define ENEMY_MAX_HP 3
#define CONTACT_DAMAGE 20   // damage dealt to the player when an enemy touches them (not upgradeable)
#define DASH_DAMAGE 1       // base damage dealt to an enemy when the player hits them while dashing
#define IFRAMES_DURATION 30 // starting frames of invincibility after taking a hit (~0.5s at 60fps)
#define DASH_KNOCKBACK_SPEED 7.0f  // speed imparted to an enemy on a successful dash hit
#define ENEMY_STAGGER_DURATION 6   // frames an enemy's AI is suspended after a dash hit - short on purpose, this is a flinch, not a stun
#define ENEMY_STAGGER_FRICTION 0.8f // per-frame velocity decay while staggered - knockback skids to a stop quickly rather than carrying far

// Chaser (normal): unchanged constant homing, no extra tuning needed here.

// Weaver (fast/ULTRA): homes in like a chaser but snakes side to side while
// doing it, so it reads as erratic/hard-to-pin-down rather than a straight line.
#define WEAVE_FREQUENCY 0.18f   // radians added to the weave phase per frame - higher = faster side-to-side
#define WEAVE_AMPLITUDE 0.9f    // how far the weave pulls off a straight line toward the player, relative to the direct-approach accel

// Charger (tank/horny): drifts in, stops at range to visibly wind up, then
// commits to a straight dash at the player's position at that moment, then
// recovers slow and open. Boss reuses the same state machine on its own tuning.
#define TANK_CHARGE_RANGE 220.0f
#define TANK_WINDUP_DURATION 35    // ~0.6s telegraph before the charge fires
#define TANK_CHARGE_SPEED 8.0f
#define TANK_CHARGE_DURATION 18
#define TANK_RECOVER_DURATION 30   // slow and open - the punish window

// Boss: same charger pattern as the tank, just longer-ranged, longer-telegraphed,
// and hits harder/faster once it commits - the pattern is more readable but the
// payoff (and punish window) is bigger.
#define BOSS_CHARGE_RANGE 320.0f
#define BOSS_WINDUP_DURATION 45
#define BOSS_CHARGE_SPEED 10.0f
#define BOSS_CHARGE_DURATION 24
#define BOSS_RECOVER_DURATION 40
#define CONTACT_RADIUS_BONUS 6.0f // leniency added on top of actual sprite size for contact/dash hits
#define PICKUP_RADIUS 40.0f  // flat, generous radius for collecting pickups/pots (not tied to sprite size)

// Enemy AI: slow constant chase, capped well under the player's own top speed
#define ENEMY_CHASE_ACCEL 0.05f
#define ENEMY_MAX_SPEED 2.0f

// Healing pots: chance to spawn one per (uncleared) room, breaks on contact
#define POT_SPAWN_CHANCE 40 // percent
#define POT_HEAL_AMOUNT 30  // scaled to the 100hp baseline

// Rooms
#define ROOM_GRID_ROWS 5      // max grid of rooms a floor can occupy, ROOM_GRID_ROWS x ROOM_GRID_COLS in size -
#define ROOM_GRID_COLS 5      // the actual per-floor layout is a randomly grown, connected subset of this grid (see GenerateFloorShape)
#define ROOM_TILE_ROWS 10     // each room is a ROOM_TILE_ROWS x ROOM_TILE_COLS grid of tiles
#define ROOM_TILE_COLS 15
#define MAX_ROOM_ENEMIES 4    // cap on enemies spawned per room

// Sprite pool: hard cap on everything alive at once, across every room -
// player + all enemies + pickups + pots, not just the current room's.
#define MAX_SPRITES 256

// Floor shape: each floor grows a random, connected blob of rooms out from
// the center starting square (see GenerateFloorShape in game.c) until it
// hits somewhere between these two room counts. The boss room is then
// placed at whichever generated room ends up farthest from the start -
// guaranteed reachable, and never the starting square itself.
#define MIN_FLOOR_ROOMS 10
#define MAX_FLOOR_ROOMS 16

// Minimap: small semi-transparent floor overview, bottom-right corner.
#define MINIMAP_CELL_SIZE 14.0f
#define MINIMAP_CELL_GAP 2.0f
#define MINIMAP_MARGIN 16.0f
#define MINIMAP_PADDING 10.0f
#define MINIMAP_BG_ALPHA 0.45f

// Dungeon depth: once every room on the current floor is cleared, the whole
// grid regenerates one floor deeper instead of the game just ending. Small
// per-floor growth that compounds - same shape as a roguelike boss-kill
// scaling curve, just keyed off floor clears instead of boss kills.
#define DEPTH_HP_GROWTH 1.15f      // enemy maxhp multiplier per floor depth
#define DEPTH_ELITE_CHANCE_BONUS 5 // added to ELITE_SPAWN_CHANCE per floor (percentage points)
#define DEPTH_ENEMIES_PER_FLOOR 2  // every N floors, the minimum enemy count per room goes up by 1

// Elites / pickups
#define ELITE_SPAWN_CHANCE 30 // percent chance a given enemy spawn point becomes elite
#define ELITE_HP_MULTIPLIER 2 // elites have this many times normal enemy hp
#define ELITE_DROP_CHANCE 100 // percent chance an elite drops a pickup on death (100 while testing)

// Enemy variants: same shape as the elite system (a multiplier layered on
// top of the ENEMY_MAX_HP/ENEMY_MAX_SPEED/CONTACT_DAMAGE baseline), just
// keyed by variant instead of a boolean. Sprites are Tamir Shooter's own
// ULTRA (fast/glass) and horny (slow/tank) enemies.
#define ENEMY_FAST_HP_MULT 0.6f
#define ENEMY_FAST_SPEED_MULT 2.0f
#define ENEMY_FAST_DAMAGE_MULT 0.75f
#define ENEMY_TANK_HP_MULT 2.5f
#define ENEMY_TANK_SPEED_MULT 0.5f
#define ENEMY_TANK_DAMAGE_MULT 1.5f

// Boss: one dedicated spawn point per boss room instead of a pack of
// reskinned elites - visibly and mechanically its own thing.
#define BOSS_HP_MULTIPLIER 8      // on top of ENEMY_MAX_HP and depth scaling
#define BOSS_SIZE_MULT 2.0f       // draw + hitbox scale
#define BOSS_CONTACT_DAMAGE_MULT 2.0f

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
