#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include "raylib.h"
#include "constants.h"

extern Font customFont; // loaded once in main(), used by every Draw*Text function across modules

enum SpriteType { PLAYER, ENEMY, PICKUP, POT };

enum TileType {
    TILE_FLOOR,
    TILE_WALL_1,
    TILE_WALL_2,
    TILE_WALL_3,
    TILE_DOOR_CLOSED, // blocks movement, like a wall, until the room's enemies are cleared
    TILE_DOOR_OPEN    // passable - stepping through one is what triggers a room change
};

// The pool of stat upgrades pickups and level-ups both draw from.
enum UpgradeType {
    UPGRADE_MOVE_SPEED,
    UPGRADE_DAMAGE,
    UPGRADE_DASH_TIME,
    UPGRADE_IFRAMES,
    UPGRADE_DODGE_CHANCE,
    UPGRADE_DASH_RADIUS,
    UPGRADE_HEAL,
    UPGRADE_EXPLOSIVE, // dash-kills detonate, damaging (and chain-detonating) nearby enemies
    UPGRADE_CHAIN,     // a dash hit also strikes the nearest other enemies in range
    UPGRADE_BLEED,     // dash hits apply a damage-over-time bleed
    UPGRADE_TYPE_COUNT // sentinel - always last, used as the random pick range
};

// Enemy behavioral variants (orthogonal to `elite`, a difficulty/loot multiplier, not a personality).
enum EnemyVariant { ENEMY_NORMAL, ENEMY_FAST, ENEMY_TANK, ENEMY_VARIANT_COUNT };

// How often an UpgradeType shows up in a level-up's 3 rolled choices - see upgradeRarity[] in upgrades.c.
enum UpgradeRarity { RARITY_COMMON, RARITY_UNCOMMON, RARITY_RARE, RARITY_COUNT };

// Drives the tank/boss "charger" AI. Chasers and weavers never leave AI_CHASE.
enum EnemyAIState {
    AI_CHASE,   // closing the distance (or, for a charger, drifting into charge range)
    AI_WINDUP,  // charger stops and telegraphs
    AI_CHARGE,  // committed to a straight-line dash toward wherever the player was at windup's end
    AI_RECOVER  // briefly slow and winded after a charge - the punish window
};

// All textures the game needs, loaded once at startup and freed once at shutdown.
typedef struct {
    Texture2D player;
    Texture2D enemyVariants[ENEMY_VARIANT_COUNT]; // indexed by EnemyVariant
    Texture2D boss1Frames[BOSS1_FRAME_COUNT]; // coin-flip skin A - see EnemySpawn.bossAlt / Sprite.bossAlt
    Texture2D boss2Frames[BOSS2_FRAME_COUNT]; // coin-flip skin B
    Texture2D shopkeepFrames[SHOPKEEP_FRAME_COUNT]; // loaded for the future shop room - not drawn anywhere yet
    Texture2D pot;
    Texture2D aura;
    Texture2D explosion; // burst drawn at an Explosive blast's origin - see SpawnExplosionEffect in entities.c
    Texture2D background;
    Texture2D walls[3];
    Texture2D doorOpen;
    Texture2D doorClosed;
    Texture2D upgradeIcons[UPGRADE_TYPE_COUNT]; // indexed by UpgradeType
} GameAssets;

typedef struct {
    Vector2 pos;
    int elite;
    int variant; // EnemyVariant - ignored for the boss spawn, which always uses assets->boss/bossAlt
    int isBoss;
    int bossAlt; // isBoss only: pure reskin coin flip, no stat/AI difference
} EnemySpawn;

typedef struct {
    int tiles[ROOM_TILE_ROWS][ROOM_TILE_COLS];
    EnemySpawn enemySpawns[MAX_ROOM_ENEMIES];
    int enemySpawnCount;
    int cleared;
    int visited;
    int exists; // part of this floor's generated shape? (see GenerateFloorShape) - if not, never entered/drawn/counted
} Room;

// The player's upgradeable numbers - pickups, level-ups, and the debug panel all read/write this one struct.
typedef struct {
    float moveSpeed;
    int damage;          // bonus added on top of DASH_DAMAGE
    int dashTime;
    int iframesMax;
    float dodgeChance;    // 0-1 chance to take zero damage from a contact hit
    float dashRadius;     // bonus added on top of ContactRadius() for the dash attack's reach
    int explosiveLevel;   // dash-kills detonate for level*EXPLOSIVE_DAMAGE_PER_LEVEL in EXPLOSIVE_RADIUS
    int chainLevel;       // a dash hit also strikes this many nearby enemies within CHAIN_RADIUS
    int bleedLevel;       // dash hits apply bleed for level*BLEED_DURATION_PER_LEVEL frames
} Stats;

typedef struct {
    Texture2D texture;
    float x, y;
    float vx, vy;
    float ax, ay;

    int active;
    int type;
    int dashTimer; // frames left in the current dash; while >0, movement input/friction is ignored

    int hp, maxhp;
    int iframes; // frames of invincibility left; while >0, this sprite can't take another hit

    int elite;         // ENEMY only: tougher, tinted gold, guaranteed pickup drop
    int variant;        // ENEMY only: EnemyVariant - speed/hp/damage profile
    int isBoss;         // ENEMY only
    int bossAlt;        // ENEMY isBoss only: which animated skin (0 = Boss1/8 frames, 1 = Boss2/16 frames)
    int animFrame;       // ENEMY isBoss only: current index into assets->boss1Frames/boss2Frames
    int animTimer;        // ENEMY isBoss only: counts down ANIM_FRAME_DURATION between frame advances
    int staggerTimer;   // ENEMY only: frames left where chase AI is suspended after a dash hit
    int aiState;        // ENEMY only: EnemyAIState - only chargers (tank/boss) leave AI_CHASE
    int aiTimer;        // ENEMY only: frames left in the current aiState
    float chargeDirX, chargeDirY; // ENEMY only: direction locked in at windup's end
    float weavePhase;   // ENEMY only: running phase for the fast variant's side-to-side weave
    int bleedTimer;      // ENEMY only: frames of bleed left; ticks every BLEED_TICK_FRAMES via modulo
    float sizeMult;      // draw scale + collision radius multiplier, 1.0 except the boss
    int upgradeType;      // PICKUP only: which UpgradeType this grants on collection

    int roomRow, roomCol;

    Stats stats; // PLAYER only
    int exp, level; // PLAYER only
} Sprite;

#endif // GAME_TYPES_H
