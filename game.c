#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "raylib.h"

#include "game_config.h"

Font customFont;

enum SpriteType {
    PLAYER,
    ENEMY,
    PICKUP,
    POT
};

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
    UPGRADE_TYPE_COUNT // sentinel - always last, used as the random pick range
};

// Enemy behavioral variants, stolen from Tamir Shooter's own tamr/horny/ULTRA
// split. Orthogonal to `elite` (which is a difficulty/loot multiplier, not a
// personality) - you can absolutely get an elite tank.
enum EnemyVariant {
    ENEMY_NORMAL,
    ENEMY_FAST,
    ENEMY_TANK,
    ENEMY_VARIANT_COUNT
};

// All textures the game needs, loaded once at startup and freed once at
// shutdown. Bundled so functions that need art (DrawRoom, LoadRoom,
// ResetGame, SpawnPickup...) take one pointer instead of a fistful of individual Texture2D params.
typedef struct {
    Texture2D player;
    Texture2D enemyVariants[ENEMY_VARIANT_COUNT]; // indexed by EnemyVariant
    Texture2D boss;
    Texture2D bossAlt; // pure reskin - same stats/AI, just a coin-flip look
    Texture2D pot;
    Texture2D aura;
    Texture2D background;
    Texture2D walls[3];
    Texture2D doorOpen;
    Texture2D doorClosed;
    Texture2D upgradeIcons[UPGRADE_TYPE_COUNT]; // indexed by UpgradeType
} GameAssets;

GameAssets LoadGameAssets(void) {
    GameAssets assets = {0};

    assets.player = LoadTexture("assets/sheshbesh.png");

    assets.enemyVariants[ENEMY_NORMAL] = LoadTexture("assets/tamir.png");
    assets.enemyVariants[ENEMY_FAST]   = LoadTexture("assets/ULTRA.png");
    assets.enemyVariants[ENEMY_TANK]   = LoadTexture("assets/horny.png");

    assets.boss    = LoadTexture("assets/boss_tamir_shooter1.png");
    assets.bossAlt = LoadTexture("assets/boss_alternate.png");
    assets.pot        = LoadTexture("assets/pot.png");
    assets.aura        = LoadTexture("assets/aura.png");
    assets.background = LoadTexture("assets/background.png");

    assets.walls[0]   = LoadTexture("assets/wall.png");
    assets.walls[1]   = LoadTexture("assets/wall2.png");
    assets.walls[2]   = LoadTexture("assets/wall3.png");
    assets.doorOpen   = LoadTexture("assets/door_open.png");
    assets.doorClosed = LoadTexture("assets/door_closed.png");

    assets.upgradeIcons[UPGRADE_MOVE_SPEED]   = LoadTexture("assets/speed_pwrp.png");
    assets.upgradeIcons[UPGRADE_DAMAGE]       = LoadTexture("assets/damage_pwrp.png");
    assets.upgradeIcons[UPGRADE_DASH_TIME]    = LoadTexture("assets/kendel.png");
    assets.upgradeIcons[UPGRADE_IFRAMES]      = LoadTexture("assets/invinc.png");
    assets.upgradeIcons[UPGRADE_DODGE_CHANCE] = LoadTexture("assets/dash_dodge.png");
    assets.upgradeIcons[UPGRADE_DASH_RADIUS]  = LoadTexture("assets/charge.png");
    assets.upgradeIcons[UPGRADE_HEAL]         = LoadTexture("assets/ugia.png");

    return assets;
}

void UnloadGameAssets(GameAssets *assets) {
    UnloadTexture(assets->player);
    for (int i = 0; i < ENEMY_VARIANT_COUNT; i++) UnloadTexture(assets->enemyVariants[i]);
    UnloadTexture(assets->boss);
    UnloadTexture(assets->bossAlt);
    UnloadTexture(assets->pot);
    UnloadTexture(assets->aura);
    UnloadTexture(assets->background);
    for (int i = 0; i < 3; i++) UnloadTexture(assets->walls[i]);
    UnloadTexture(assets->doorOpen);
    UnloadTexture(assets->doorClosed);
    for (int i = 0; i < UPGRADE_TYPE_COUNT; i++) UnloadTexture(assets->upgradeIcons[i]);
}

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
} Room;

static Room roomGrid[ROOM_GRID_ROWS][ROOM_GRID_COLS];
static int currentRoomRow = ROOM_GRID_ROWS / 2;
static int currentRoomCol = ROOM_GRID_COLS / 2;
static int dungeonDepth = 0; // floors cleared so far this run - drives difficulty scaling below
static int hitStopTimer = 0;
static int screenShakeTimer = 0;

// The center room is left enemy-free (and pre-cleared) so the player has a safe start.
void InitRooms() {
    for (int r = 0; r < ROOM_GRID_ROWS; r++) {
        for (int c = 0; c < ROOM_GRID_COLS; c++) {
            Room *room = &roomGrid[r][c];

            for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
                for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
                    int isBorder = (tx == 0 || tx == ROOM_TILE_COLS - 1 || ty == 0 || ty == ROOM_TILE_ROWS - 1);
                    int randomWall = TILE_WALL_1 + (rand() % 3);
                    room->tiles[ty][tx] = isBorder ? randomWall : TILE_FLOOR;
                }
            }

            int isSafeRoom = (r == ROOM_GRID_ROWS / 2 && c == ROOM_GRID_COLS / 2);
            int doorTile = isSafeRoom ? TILE_DOOR_OPEN : TILE_DOOR_CLOSED;

            if (r > 0) {
                room->tiles[0][ROOM_TILE_COLS / 2 - 1] = doorTile;
                room->tiles[0][ROOM_TILE_COLS / 2] = doorTile;                                          // north
            }
            if (r < ROOM_GRID_ROWS - 1) {
                room->tiles[ROOM_TILE_ROWS - 1][ROOM_TILE_COLS / 2 - 1] = doorTile;
                room->tiles[ROOM_TILE_ROWS - 1][ROOM_TILE_COLS / 2] = doorTile;                          // south
            }
            if (c > 0) {
                room->tiles[ROOM_TILE_ROWS / 2 - 1][0] = doorTile;
                room->tiles[ROOM_TILE_ROWS / 2][0] = doorTile;                                           // west
            }
            if (c < ROOM_GRID_COLS - 1) {
                room->tiles[ROOM_TILE_ROWS / 2 - 1][ROOM_TILE_COLS - 1] = doorTile;
                room->tiles[ROOM_TILE_ROWS / 2][ROOM_TILE_COLS - 1] = doorTile;                          // east
            }

            room->cleared = isSafeRoom;
            room->visited = isSafeRoom; // prevents pots from spawning in the start room

            if (isSafeRoom) {
                room->enemySpawnCount = 0;
                continue;
            }

            int isBossRoom = (r == BOSS_ROOM_ROW && c == BOSS_ROOM_COL);

            if (isBossRoom) {
                // One real boss, plus 0-2 regular adds for room presence -
                // not a whole pack of reskinned elites.
                room->enemySpawnCount = 1 + rand() % 3;
                room->enemySpawns[0].pos = (Vector2){ WIDTH / 2.0f, HEIGHT / 2.0f };
                room->enemySpawns[0].elite = 1;
                room->enemySpawns[0].isBoss = 1;
                room->enemySpawns[0].variant = ENEMY_NORMAL; // irrelevant - LoadRoom always uses assets->boss/bossAlt for isBoss
                room->enemySpawns[0].bossAlt = rand() % 2;
                for (int i = 1; i < room->enemySpawnCount; i++) {
                    room->enemySpawns[i].pos.x = (float)(60 + rand() % (WIDTH - 120));
                    room->enemySpawns[i].pos.y = (float)(60 + rand() % (HEIGHT - 120));
                    room->enemySpawns[i].elite = 0;
                    room->enemySpawns[i].isBoss = 0;
                    room->enemySpawns[i].variant = rand() % ENEMY_VARIANT_COUNT;
                }
            } else {
                // Floors get denser and eliter as depth increases, capped at a full room.
                int minEnemies = 1 + dungeonDepth / DEPTH_ENEMIES_PER_FLOOR;
                if (minEnemies > MAX_ROOM_ENEMIES) minEnemies = MAX_ROOM_ENEMIES;
                int eliteChance = ELITE_SPAWN_CHANCE + dungeonDepth * DEPTH_ELITE_CHANCE_BONUS;
                if (eliteChance > 100) eliteChance = 100;

                room->enemySpawnCount = minEnemies + rand() % (MAX_ROOM_ENEMIES - minEnemies + 1);
                for (int i = 0; i < room->enemySpawnCount; i++) {
                    room->enemySpawns[i].pos.x = (float)(60 + rand() % (WIDTH - 120));
                    room->enemySpawns[i].pos.y = (float)(60 + rand() % (HEIGHT - 120));
                    room->enemySpawns[i].elite = (rand() % 100 < eliteChance);
                    room->enemySpawns[i].isBoss = 0;
                    room->enemySpawns[i].variant = rand() % ENEMY_VARIANT_COUNT;
                }
            }
        }
    }
}

// Directional tile textures (currently just doors) are authored facing
// north/up. Rotate them to match whichever wall they're actually on.
float TileRotationForSide(int tx, int ty) {
    if (ty == 0) return 0.0f;                    // north wall
    if (ty == ROOM_TILE_ROWS - 1) return 180.0f;  // south wall
    if (tx == 0) return 270.0f;                   // west wall
    if (tx == ROOM_TILE_COLS - 1) return 90.0f;   // east wall
    return 0.0f;
}

void DrawTile(Texture2D tex, int tx, int ty, float tileW, float tileH) {
    float sourceWidth = (float)tex.width;

    // North Wall
    if (ty == 0 && tx == ROOM_TILE_COLS / 2) {
        sourceWidth = -sourceWidth;
    } 
    // South Wall
    else if (ty == ROOM_TILE_ROWS - 1 && tx == ROOM_TILE_COLS / 2 - 1) {
        sourceWidth = -sourceWidth;
    }
    // East Wall
    else if (tx == ROOM_TILE_COLS - 1 && ty == ROOM_TILE_ROWS / 2) {
        sourceWidth = -sourceWidth;
    }
    // West Wall
    else if (tx == 0 && ty == ROOM_TILE_ROWS / 2 - 1) {
        sourceWidth = -sourceWidth;
    }

    Rectangle sourceRec = { 0.0f, 0.0f, sourceWidth, (float)tex.height };
    Rectangle destRec = { (tx + 0.5f) * tileW, (ty + 0.5f) * tileH, tileW, tileH };
    Vector2 origin = { tileW / 2.0f, tileH / 2.0f };
    DrawTexturePro(tex, sourceRec, destRec, origin, TileRotationForSide(tx, ty), WHITE);
}

void DrawRoom(Room *room, GameAssets *assets) {
    float tileW = (float)WIDTH / ROOM_TILE_COLS;
    float tileH = (float)HEIGHT / ROOM_TILE_ROWS;

    for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
        for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
            switch (room->tiles[ty][tx]) {
                case TILE_WALL_1:      DrawTile(assets->walls[0], tx, ty, tileW, tileH); break;
                case TILE_WALL_2:      DrawTile(assets->walls[1], tx, ty, tileW, tileH); break;
                case TILE_WALL_3:      DrawTile(assets->walls[2], tx, ty, tileW, tileH); break;
                case TILE_DOOR_OPEN:   DrawTile(assets->doorOpen, tx, ty, tileW, tileH); break;
                case TILE_DOOR_CLOSED: DrawTile(assets->doorClosed, tx, ty, tileW, tileH); break;
                default: break; // TILE_FLOOR - nothing drawn, background shows through
            }
        }
    }
}

// The player's upgradeable numbers, bundled into one struct so pickups,
// level-ups, and the debug panel all have a single thing to read/modify.
// Enemies/pickups carry a zero-valued, unused Stats - same as dashTimer
// already being irrelevant for them.
typedef struct {
    float moveSpeed;    // replaces MAX_MOVE_SPEED for whoever holds this
    int damage;         // bonus added on top of DASH_DAMAGE
    int dashTime;        // replaces DASH_TIME
    int iframesMax;      // replaces IFRAMES_DURATION
    float dodgeChance;   // 0-1 chance to take zero damage from a contact hit
    float dashRadius;    // bonus added on top of ContactRadius() for the dash attack's reach
} Stats;

typedef struct {
    Texture2D texture;
    float x;
    float y;

    float vx;
    float vy;

    float ax;
    float ay;

    int active;
    int type;
    int dashTimer; // frames left in the current dash; while >0, movement input/friction is ignored

    int hp;
    int maxhp;
    int iframes; // frames of invincibility left; while >0, this sprite can't take another hit

    int elite;        // ENEMY only: tougher, tinted gold, guaranteed pickup drop
    int variant;       // ENEMY only: EnemyVariant - speed/hp/damage profile
    int isBoss;        // ENEMY only: the boss room's single dedicated spawn, not just another elite
    float sizeMult;    // draw scale + collision radius multiplier, 1.0 for everything except the boss
    int upgradeType;   // PICKUP only: which UpgradeType this grants on collection

    int roomRow;
    int roomCol;

    Stats stats; // PLAYER only: current upgraded values
    int exp;     // PLAYER only
    int level;   // PLAYER only
} Sprite;

// Flat, pre-allocated pool of every sprite in play (player, enemies,
// pickups, pots). No malloc/free per-sprite: spawning just claims the next slot and despawning flags it inactive;
// CleanUpSprites() compacts the array. sprites[0] is always the player and is never reordered out of that slot.
static Sprite sprites[MAX_SPRITES];
static int spriteCount = 0;

const char *UpgradeName(int upgradeType) {
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:   return "+Move Speed";
        case UPGRADE_DAMAGE:       return "+Damage";
        case UPGRADE_DASH_TIME:    return "+Dash Time";
        case UPGRADE_IFRAMES:      return "+Iframes";
        case UPGRADE_DODGE_CHANCE: return "+Dodge Chance";
        case UPGRADE_DASH_RADIUS:  return "+Dash Radius";
        case UPGRADE_HEAL:         return "+Heal";
        default: return "+???";
    }
}

// Per-variant multipliers layered on top of the base ENEMY_MAX_SPEED/
// ENEMY_MAX_HP/CONTACT_DAMAGE constants - same switch-on-enum idiom as
// UpgradeName above, just for enemies instead of upgrades.
float EnemySpeedMult(int variant) {
    switch (variant) {
        case ENEMY_FAST: return ENEMY_FAST_SPEED_MULT;
        case ENEMY_TANK: return ENEMY_TANK_SPEED_MULT;
        default: return 1.0f;
    }
}

float EnemyHpMult(int variant) {
    switch (variant) {
        case ENEMY_FAST: return ENEMY_FAST_HP_MULT;
        case ENEMY_TANK: return ENEMY_TANK_HP_MULT;
        default: return 1.0f;
    }
}

float EnemyDamageMult(int variant) {
    switch (variant) {
        case ENEMY_FAST: return ENEMY_FAST_DAMAGE_MULT;
        case ENEMY_TANK: return ENEMY_TANK_DAMAGE_MULT;
        default: return 1.0f;
    }
}

// Fixed pool of floating labels - pickups, level-ups, and room-cleared all
// reuse this one mechanism instead of three separate ad-hoc UI bits.
typedef struct {
    char text[24];
    float x, y;
    int timer;
} PopupText;

static PopupText popupTexts[MAX_POPUP_TEXTS];
static int hitFlashTimer = 0; // frames left on the dash-kill screen flash

void SpawnPopupText(const char *text, float x, float y) {
    for (int i = 0; i < MAX_POPUP_TEXTS; i++) {
        if (popupTexts[i].timer <= 0) {
            snprintf(popupTexts[i].text, sizeof(popupTexts[i].text), "%s", text);
            popupTexts[i].x = x;
            popupTexts[i].y = y;
            popupTexts[i].timer = POPUP_TEXT_LIFETIME;
            return;
        }
    }
}

void UpdatePopupTexts(void) {
    for (int i = 0; i < MAX_POPUP_TEXTS; i++) {
        if (popupTexts[i].timer <= 0) continue;
        popupTexts[i].timer--;
        popupTexts[i].y -= 0.4f; // float upward
    }
}

void DrawPopupTexts(void) {
    for (int i = 0; i < MAX_POPUP_TEXTS; i++) {
        if (popupTexts[i].timer <= 0) continue;
        float alpha = (float)popupTexts[i].timer / POPUP_TEXT_LIFETIME;
        DrawTextEx(customFont, popupTexts[i].text, (Vector2){ popupTexts[i].x, popupTexts[i].y }, 18.0f, 1.0f, Fade(BLACK, alpha));
    }
}

void ApplyUpgrade(Sprite *player, int upgradeType) {
    Stats *stats = &player->stats;
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:   stats->moveSpeed += UPGRADE_MOVE_SPEED_AMOUNT; break;
        case UPGRADE_DAMAGE:       stats->damage += UPGRADE_DAMAGE_AMOUNT; break;
        case UPGRADE_DASH_TIME:    stats->dashTime += UPGRADE_DASH_TIME_AMOUNT; break;
        case UPGRADE_IFRAMES:      stats->iframesMax += UPGRADE_IFRAMES_AMOUNT; break;
        case UPGRADE_DODGE_CHANCE:
            stats->dodgeChance += UPGRADE_DODGE_CHANCE_AMOUNT;
            // Cap dodge chance at 75% so the player never becomes immortal
            if (stats->dodgeChance > 0.75f) stats->dodgeChance = 0.75f;
            break;
        case UPGRADE_DASH_RADIUS:  stats->dashRadius += UPGRADE_DASH_RADIUS_AMOUNT; break;
        case UPGRADE_HEAL:
            player->hp += UPGRADE_HEAL_AMOUNT;
            if (player->hp > player->maxhp) player->hp = player->maxhp;
            break;
        default: break;
    }
}

int ExpNeededForLevel(int level) {
    return (int)(EXP_TO_LEVEL_BASE * powf(EXP_TO_LEVEL_GROWTH, (float)(level - 1)));
}

void GrantExp(Sprite *player, int amount) {
    player->exp += amount;
    while (player->exp >= ExpNeededForLevel(player->level)) {
        player->exp -= ExpNeededForLevel(player->level);
        player->level++;
        int upgrade = rand() % UPGRADE_TYPE_COUNT;
        ApplyUpgrade(player, upgrade);
        SpawnPopupText(TextFormat("Level Up! %s", UpgradeName(upgrade)), player->x, player->y - 30.0f);
    }
}

float GetSpriteScale(Texture2D tex) {
    float targetW = (float)GetScreenWidth() / ROOM_TILE_COLS;
    return targetW / (float)tex.width;
}

void DrawSprite(Sprite s) {
    Texture2D tex = s.texture;

    float rotation = atan2f(s.vy , s.vx) * RAD2DEG;
    Vector2 position = { s.x, s.y };

    float scale = GetSpriteScale(tex) * s.sizeMult;
    float destW = (float)tex.width * scale;
    float destH = (float)tex.height * scale;

    Vector2 origin = { destW / 2.0f, destH / 2.0f }; 

    Rectangle sourceRec = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle destRec   = { position.x, position.y, destW, destH };

    Color tint = WHITE;
    if (s.type == ENEMY && s.elite) tint = GOLD;
    if (s.iframes > 0) tint = Fade(tint, 0.4f); // flicker while invincible, on top of any base tint

    DrawTexturePro(tex, sourceRec, destRec, origin, rotation, tint);
}

// Small bar above a sprite's head, only shown once it has taken damage
void DrawHealthBar(Sprite s) {
    float scale = GetSpriteScale(s.texture) * s.sizeMult;
    float destH = (float)s.texture.height * scale;

    float barWidth = 40.0f;
    float barHeight = 5.0f;
    float x = s.x - barWidth / 2.0f;
    float y = s.y - destH / 2.0f - 10.0f; // dynamically hovers right above the scaled sprite

    float pct = (float)s.hp / (float)s.maxhp;
    if (pct < 0.0f) pct = 0.0f;

    DrawRectangle(x, y, barWidth, barHeight, GRAY);
    DrawRectangle(x, y, barWidth * pct, barHeight, RED);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);
}

// Shared bar-and-label widget behind both the HP and EXP bars below - same
// geometry and layout, just a different position/fill color/label.
void DrawStatBar(float x, float y, float pct, Color fillColor, const char *label) {
    float barWidth = 200.0f;
    float barHeight = 16.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    DrawRectangle(x, y, barWidth, barHeight, GRAY);
    DrawRectangle(x, y, barWidth * pct, barHeight, fillColor);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);

    int textW = MeasureText(label, 16);
    DrawRectangle((int)x - 2, (int)(y + barHeight + 2), textW + 4, 16, Fade(WHITE, 0.7f));
    DrawTextEx(customFont, label, (Vector2){ x, y + barHeight + 2.0f }, 16.0f, 1.0f, BLACK);
}

void DrawHpBar(Sprite *player) {
    float pct = (float)player->hp / (float)player->maxhp;
    DrawStatBar(10.0f, 10.0f, pct, RED, TextFormat("Hp: %d", player->hp));
}

void DrawExpBar(Sprite *player) {
    int needed = ExpNeededForLevel(player->level);
    float pct = (float)player->exp / (float)needed;
    DrawStatBar((float)WIDTH - 200.0f - 10.0f, 10.0f, pct, SKYBLUE, TextFormat("Lv: %d", player->level));
}

// TAB-toggled panel of the player's current stats, for testing upgrades
void DrawDebugPanel(Sprite *player) {
    int panelWidth = 220;
    DrawRectangle(0, 0, panelWidth, HEIGHT, Fade(BLACK, 0.6f));

    int y = 10;
    int lineHeight = 18;
    DrawTextEx(customFont, TextFormat("HP: %d / %d", player->hp, player->maxhp), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Level %d  (%d/%d exp)", player->level, player->exp, ExpNeededForLevel(player->level)), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    y += lineHeight / 2;
    DrawTextEx(customFont, TextFormat("Move Speed: %.2f", player->stats.moveSpeed), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Dash Damage: %d + %d", DASH_DAMAGE, player->stats.damage), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Dash Time: %d frames", player->stats.dashTime), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Iframes: %d frames", player->stats.iframesMax), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Dodge Chance: %.0f%%", player->stats.dodgeChance * 100.0f), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Dash Radius Bonus: %.1f", player->stats.dashRadius), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    y += lineHeight / 2;
    DrawTextEx(customFont, TextFormat("Room: (%d, %d)", currentRoomRow, currentRoomCol), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Depth: %d", dungeonDepth), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawFPS(10, HEIGHT - 20);
}

void move(Sprite *s) {
    if (s->dashTimer > 0) {
        s->dashTimer--;
    } else {
        if (IsKeyDown(KEY_D)) s->vx += MOVE_ACCEL;
        if (IsKeyDown(KEY_A)) s->vx -= MOVE_ACCEL;
        if (IsKeyDown(KEY_W)) s->vy -= MOVE_ACCEL;
        if (IsKeyDown(KEY_S)) s->vy += MOVE_ACCEL;

        // Bleed off speed on any axis with no input, instead of coasting forever
        if (!IsKeyDown(KEY_D) && !IsKeyDown(KEY_A)) s->vx *= FRICTION;
        if (!IsKeyDown(KEY_W) && !IsKeyDown(KEY_S)) s->vy *= FRICTION;

        // Clamp overall speed (not per-axis, so diagonal movement isn't faster)
        float moveSpeed = sqrtf(s->vx * s->vx + s->vy * s->vy);
        if (moveSpeed > s->stats.moveSpeed) {
            s->vx = s->vx / moveSpeed * s->stats.moveSpeed;
            s->vy = s->vy / moveSpeed * s->stats.moveSpeed;
        }
    }

    if (IsKeyPressed(KEY_SPACE) && s->dashTimer <= 0) {
        float dashDir = sqrtf(s->vx * s->vx + s->vy * s->vy);
        if (dashDir > 0.01f) {
            s->vx = s->vx / dashDir * DASH_SPEED;
            s->vy = s->vy / dashDir * DASH_SPEED;
            s->dashTimer = s->stats.dashTime;
        }
    }

    // Keep player on screen
    if (s->x < 0) { s->x = 0; s->vx = 0; }
    if (s->x > WIDTH) { s->x = WIDTH; s->vx = 0; }
    if (s->y < 0) { s->y = 0; s->vy = 0; }
    if (s->y > HEIGHT) { s->y = HEIGHT; s->vy = 0; }
}

// Approximate circle-collision radius: half of each sprite's texture width,
float ContactRadius(Sprite *a, Sprite *b) {
    float aScaledW = (float)a->texture.width * GetSpriteScale(a->texture) * a->sizeMult;
    float bScaledW = (float)b->texture.width * GetSpriteScale(b->texture) * b->sizeMult;
    return (aScaledW + bScaledW) / 4.0f;
}

// Squared-distance radius check - avoids a sqrtf per pair per frame
int WithinRadius(Sprite *a, Sprite *b, float radius) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return dx * dx + dy * dy < radius * radius;
}

//  is this sprite the given type, alive, and in the room the player is currently standing in.
int IsActiveInRoom(Sprite *s, int type) {
    return s->type == type && s->active && s->roomRow == currentRoomRow && s->roomCol == currentRoomCol;
}

void SpawnPickup(GameAssets *assets, float x, float y) {
    if (spriteCount >= MAX_SPRITES) return;
    int upgradeType = rand() % UPGRADE_TYPE_COUNT;
    Sprite *pickup = &sprites[spriteCount++];
    *pickup = (Sprite){assets->upgradeIcons[upgradeType], x, y, 0, 0, 0, 0, 1, PICKUP};
    pickup->upgradeType = upgradeType;
    pickup->sizeMult = 1.0f;
    pickup->roomRow = currentRoomRow; // Tag the item's location
    pickup->roomCol = currentRoomCol;
}

void Update(GameAssets *assets) {
    for (int i = 0; i < spriteCount; i++) {
        Sprite *s = &sprites[i];
        if (!s->active) continue;
        if (s->type != PLAYER && (s->roomRow != currentRoomRow || s->roomCol != currentRoomCol)) continue;

        // Swarm AI: constantly steer toward the player
        if (s->type == ENEMY) {
            float dx = sprites[0].x - s->x;
            float dy = sprites[0].y - s->y;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 0.01f) {
                s->ax = dx / len * ENEMY_CHASE_ACCEL * EnemySpeedMult(s->variant);
                s->ay = dy / len * ENEMY_CHASE_ACCEL * EnemySpeedMult(s->variant);
            }
        }

        //do physics
        s->vx = (s->vx + s->ax) * 0.99f;
        s->vy = (s->vy + s->ay) * 0.99f;

        if (s->type == ENEMY) {
            float speed = sqrtf(s->vx * s->vx + s->vy * s->vy);
            float maxSpeed = ENEMY_MAX_SPEED * EnemySpeedMult(s->variant);
            if (speed > maxSpeed) {
                s->vx = s->vx / speed * maxSpeed;
                s->vy = s->vy / speed * maxSpeed;
            }
        }

        s->x += s->vx;
        s->y += s->vy;

        if (s->iframes > 0) s->iframes--;
    }

    // Player vs enemy contact: dashing into an enemy hurts them
    for (int i = 0; i < spriteCount; i++) {
        Sprite *player = &sprites[i];
        if (player->type != PLAYER || !player->active) continue;

        for (int j = 0; j < spriteCount; j++) {
            Sprite *enemy = &sprites[j];
            if (!IsActiveInRoom(enemy, ENEMY)) continue;

            if (player->dashTimer > 0) {
                float r = ContactRadius(player, enemy) + player->stats.dashRadius;
                if (WithinRadius(player, enemy, r) && enemy->iframes <= 0) {
                    enemy->hp -= (DASH_DAMAGE + player->stats.damage);
                    enemy->iframes = IFRAMES_DURATION;
                    hitStopTimer = 4;       // Freeze the game physics for 4 frames
                    screenShakeTimer = 10;  // Shake the screen for 10 frames
                    
                    float dx = enemy->x - player->x;
                    float dy = enemy->y - player->y;
                    float len = sqrtf(dx*dx + dy*dy);
                    if (len > 0.01f) {
                        enemy->vx = (dx / len) * 12.0f; 
                        enemy->vy = (dy / len) * 12.0f;
                    }
                    if (enemy->hp <= 0) {
                        enemy->active = 0;
                        hitFlashTimer = HIT_FLASH_DURATION;
                        GrantExp(player, EXP_PER_KILL);
                        if (enemy->elite && (rand() % 100 < ELITE_DROP_CHANCE)) {
                            SpawnPickup(assets, enemy->x, enemy->y);
                        }
                    }
                }
            } else {
                float r = ContactRadius(player, enemy) + CONTACT_RADIUS_BONUS;
                if (WithinRadius(player, enemy, r) && player->iframes <= 0) {
                    int dodged = ((float)rand() / (float)RAND_MAX) < player->stats.dodgeChance;
                    if (!dodged) {
                        float dmgMult = enemy->isBoss ? BOSS_CONTACT_DAMAGE_MULT : EnemyDamageMult(enemy->variant);
                        player->hp -= (int)(CONTACT_DAMAGE * dmgMult);
                        player->iframes = player->stats.iframesMax;
                        if (player->hp < 0) player->hp = 0;
                    }
                }
            }
        }
    }

    // Player vs pickup: touching one applies its upgrade immediately, no menu.
    for (int i = 0; i < spriteCount; i++) {
        Sprite *player = &sprites[i];
        if (player->type != PLAYER || !player->active) continue;

        for (int j = 0; j < spriteCount; j++) {
            Sprite *pickup = &sprites[j];
            if (!IsActiveInRoom(pickup, PICKUP)) continue;

            if (WithinRadius(player, pickup, PICKUP_RADIUS)) {
                ApplyUpgrade(player, pickup->upgradeType);
                SpawnPopupText(UpgradeName(pickup->upgradeType), player->x, player->y - 30.0f);
                pickup->active = 0;
            }
        }
    }

    // Player vs pot: same shape as the pickup loop above, breaks on contact and heals.
    for (int i = 0; i < spriteCount; i++) {
        Sprite *player = &sprites[i];
        if (player->type != PLAYER || !player->active) continue;

        for (int j = 0; j < spriteCount; j++) {
            Sprite *pot = &sprites[j];
            if (!IsActiveInRoom(pot, POT)) continue;

            if (WithinRadius(player, pot, PICKUP_RADIUS)) {
                player->hp += POT_HEAL_AMOUNT;
                if (player->hp > player->maxhp) player->hp = player->maxhp;
                SpawnPopupText("+Heal", player->x, player->y - 30.0f);
                pot->active = 0;
            }
        }
    }
}

// Swap-and-pop compaction over the flat sprite array. No free() needed
// anymore - despawned sprites are just plain structs getting overwritten.
void CleanUpSprites(void) {
    for (int i = 0; i < spriteCount; ) {
        if (sprites[i].active == 0) {
            sprites[i] = sprites[spriteCount - 1]; // swap in the last slot...
            spriteCount--;                          // ...and pop it, instead of shifting everything down
        } else {
            i++; // only advance if we didn't just swap a new sprite into i
        }
    }
}

int IsBlockingTile(int tileType) {
    return tileType == TILE_WALL_1 || tileType == TILE_WALL_2 || tileType == TILE_WALL_3 || tileType == TILE_DOOR_CLOSED;
}

// Which tile occupies a given world position. Positions outside the room's
// tile grid entirely (i.e. already past a door, between rooms) read as open
// floor - move()'s screen-edge clamp and TryChangeRoom() take over from there.
int TileTypeAt(Room *room, float worldX, float worldY) {
    float tileW = (float)WIDTH / ROOM_TILE_COLS;
    float tileH = (float)HEIGHT / ROOM_TILE_ROWS;
    int tx = (int)floorf(worldX / tileW);
    int ty = (int)floorf(worldY / tileH);
    if (tx < 0 || tx >= ROOM_TILE_COLS || ty < 0 || ty >= ROOM_TILE_ROWS) return TILE_FLOOR;
    return room->tiles[ty][tx];
}

// Simple axis-separated collision: check the sprite's leading edge (in
// whichever direction it just moved) against the tile there, and undo that
// axis of movement if it's blocked. Reconstructs "before this frame" from
// vx/vy since Update() already applied them straight to x/y.
void ResolveWallCollision(Sprite *s, Room *room) {

    float scale = GetSpriteScale(s->texture);
    float halfW = ((float)s->texture.width * scale) / 2.0f;
    float halfH = ((float)s->texture.height * scale) / 2.0f;
    float prevX = s->x - s->vx;
    float prevY = s->y - s->vy;

    float leadingX = s->x + (s->vx > 0 ? halfW : -halfW);
    if (IsBlockingTile(TileTypeAt(room, leadingX, prevY))) {
        s->x = prevX;
        s->vx = 0;
    }

    float leadingY = s->y + (s->vy > 0 ? halfH : -halfH);
    if (IsBlockingTile(TileTypeAt(room, s->x, leadingY))) {
        s->y = prevY;
        s->vy = 0;
    }
}

// Once every enemy in the room is dead, swap any closed doors open and mark
// the room cleared so LoadRoom stops respawning enemies into it.
void UpdateDoors(Room *room) {
    if (room->cleared) return;

    for (int i = 0; i < spriteCount; i++) {
        if (sprites[i].type == ENEMY && sprites[i].active) return; // still enemies left
    }

    for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
        for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
            if (room->tiles[ty][tx] == TILE_DOOR_CLOSED) room->tiles[ty][tx] = TILE_DOOR_OPEN;
        }
    }
    room->cleared = 1;
    // sprites[0] is always the player (see CleanUpSprites - it's never reordered)
    SpawnPopupText("Room Cleared!", sprites[0].x - 40.0f, sprites[0].y - 40.0f);
}

int AllRoomsCleared(void) {
    for (int r = 0; r < ROOM_GRID_ROWS; r++) {
        for (int c = 0; c < ROOM_GRID_COLS; c++) {
            if (!roomGrid[r][c].cleared) return 0;
        }
    }
    return 1;
}

void LoadRoom(GameAssets *assets) {
    // Only clear enemies. Pickups and pots stay in memory!
    for (int i = 0; i < spriteCount; i++) {
        if (sprites[i].type == ENEMY) sprites[i].active = 0;
    }
    CleanUpSprites();

    Room *room = &roomGrid[currentRoomRow][currentRoomCol];

    // Spawn a pot only the first time we ever visit this room
    if (!room->visited) {
        if (spriteCount < MAX_SPRITES && rand() % 100 < POT_SPAWN_CHANCE) {
            Sprite *pot = &sprites[spriteCount++];
            float px = (float)(60 + rand() % (WIDTH - 120));
            float py = (float)(60 + rand() % (HEIGHT - 120));
            *pot = (Sprite){assets->pot, px, py, 0, 0, 0, 0, 1, POT};
            pot->roomRow = currentRoomRow;
            pot->roomCol = currentRoomCol;
            pot->sizeMult = 1.0f;
        }
        room->visited = 1;
    }

    if (room->cleared) return;

    int isBossRoom = (currentRoomRow == BOSS_ROOM_ROW && currentRoomCol == BOSS_ROOM_COL);
    if (isBossRoom) {
        SpawnPopupText("Boss Room!", sprites[0].x - 30.0f, sprites[0].y - 40.0f);
    }

    float hpMult = powf(DEPTH_HP_GROWTH, (float)dungeonDepth);
    for (int i = 0; i < room->enemySpawnCount && spriteCount < MAX_SPRITES; i++) {
        Sprite *enemy = &sprites[spriteCount++];
        EnemySpawn spawn = room->enemySpawns[i];
        Texture2D tex = spawn.isBoss ? (spawn.bossAlt ? assets->bossAlt : assets->boss) : assets->enemyVariants[spawn.variant];
        *enemy = (Sprite){tex, spawn.pos.x, spawn.pos.y, 0, 0, 0, 0, 1, ENEMY};
        enemy->elite = spawn.elite;
        enemy->variant = spawn.variant;
        enemy->isBoss = spawn.isBoss;
        enemy->sizeMult = spawn.isBoss ? BOSS_SIZE_MULT : 1.0f;
        float eliteMult = spawn.elite ? ELITE_HP_MULTIPLIER : 1;
        float bossMult = spawn.isBoss ? BOSS_HP_MULTIPLIER : 1;
        enemy->maxhp = (int)(ENEMY_MAX_HP * EnemyHpMult(spawn.variant) * eliteMult * bossMult * hpMult);
        enemy->hp = enemy->maxhp;
        enemy->roomRow = currentRoomRow; 
        enemy->roomCol = currentRoomCol;
    }
}

int TryChangeRoom(Sprite *player) {
    float tileW = (float)WIDTH / ROOM_TILE_COLS;
    float tileH = (float)HEIGHT / ROOM_TILE_ROWS;
    float insetX = tileW * 1.5f;
    float insetY = tileH * 1.5f;

    if (player->x < 0 && currentRoomCol > 0) {
        currentRoomCol--;
        player->x = WIDTH - insetX;
        return 1;
    }
    if (player->x > WIDTH && currentRoomCol < ROOM_GRID_COLS - 1) {
        currentRoomCol++;
        player->x = insetX;
        return 1;
    }
    if (player->y < 0 && currentRoomRow > 0) {
        currentRoomRow--;
        player->y = HEIGHT - insetY;
        return 1;
    }
    if (player->y > HEIGHT && currentRoomRow < ROOM_GRID_ROWS - 1) {
        currentRoomRow++;
        player->y = insetY;
        return 1;
    }
    return 0;
}

void ResetGame(GameAssets *assets) {
    spriteCount = 0; // sprites are plain structs now - no per-sprite free() needed

    currentRoomRow = ROOM_GRID_ROWS / 2;
    currentRoomCol = ROOM_GRID_COLS / 2;
    dungeonDepth = 0; // fresh run - back to floor 1 difficulty

    Sprite *player = &sprites[spriteCount++];
    *player = (Sprite){assets->player, WIDTH / 2.0f, HEIGHT / 2.0f, 0, 0, 0, 0, 1, PLAYER};
    player->hp = PLAYER_MAX_HP;
    player->maxhp = PLAYER_MAX_HP;
    player->level = 1;
    player->exp = 0;
    player->sizeMult = 1.0f;
    player->stats = (Stats){ MAX_MOVE_SPEED, 0, DASH_TIME, IFRAMES_DURATION, 0.0f, CONTACT_RADIUS_BONUS };

    InitRooms();
    LoadRoom(assets);
}

int main() {
    srand(time(NULL));
    int showDebugPanel = 0;

    InitWindow(WIDTH, HEIGHT, "Dungeons & Tamirs");
    SetTargetFPS(60);

    // 1. Load all textures ONCE at the start of the game
    GameAssets assets = LoadGameAssets();
    customFont = LoadFont("assets/Good-Game.ttf");

    // 2. Spawn the player and the starting dungeon
    ResetGame(&assets);
    Sprite *player = &sprites[0]; // sprites[0] is always the player (see CleanUpSprites)

    while(!WindowShouldClose()) {
        if (player->hp > 0) {
            if (hitStopTimer > 0) {
                hitStopTimer--;
            } else {
            Update(&assets);
            CleanUpSprites();

            Room *room = &roomGrid[currentRoomRow][currentRoomCol];
            UpdateDoors(room);
            ResolveWallCollision(player, room);

            // Whole floor cleared - descend instead of just running out of game.
            if (AllRoomsCleared()) {
                dungeonDepth++;
                SpawnPopupText(TextFormat("Floor: %d", dungeonDepth + 1), player->x - 20.0f, player->y - 40.0f);
                
                // Clear old floor's lingering items
                for (int i = 0; i < spriteCount; i++) {
                    if (sprites[i].type != PLAYER) sprites[i].active = 0;
                }
                CleanUpSprites();

                InitRooms();
                currentRoomRow = ROOM_GRID_ROWS / 2;
                currentRoomCol = ROOM_GRID_COLS / 2;
                LoadRoom(&assets);
            }

            if (TryChangeRoom(player)) {
                LoadRoom(&assets);
            }
            move(player);
            }
            UpdatePopupTexts();
            if (hitFlashTimer > 0) hitFlashTimer--;
            if (screenShakeTimer > 0) screenShakeTimer--;
        } else if (IsKeyPressed(KEY_R)) {
            ResetGame(&assets);
            player = &sprites[0];
        }

        if (IsKeyPressed(KEY_TAB)) showDebugPanel = !showDebugPanel;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        Camera2D camera = { 0 };
        camera.target = (Vector2){ 0, 0 };
        camera.zoom = 1.0f;
        camera.rotation = 0.0f;
        
        if (screenShakeTimer > 0) {
            camera.offset.x = (float)((rand() % 11) - 5); // Random shake between -5 to +5 pixels
            camera.offset.y = (float)((rand() % 11) - 5);
        } else {
            camera.offset = (Vector2){ 0, 0 };
        }
        
        // Begin rendering the game world through the shaking camera
        BeginMode2D(camera);
        DrawTexture(assets.background, 0, 0, WHITE); // native res on purpose - no scaling to WIDTH/HEIGHT
        DrawRoom(&roomGrid[currentRoomRow][currentRoomCol], &assets);
        //drawing the sprites
        for (int i = 0; i < spriteCount; i++) {
            Sprite *s = &sprites[i];
            if (!s->active) continue;
            if (s->type != PLAYER && (s->roomRow != currentRoomRow || s->roomCol != currentRoomCol)) continue;
            if (s->type == PLAYER && s->dashTimer > 0) {
                float powerRatio = fminf((float)s->stats.damage / 20.0f, 1.0f); 
                float sizeRatio = fminf(s->stats.dashRadius / 10.0f, 1.0f);

                float baseScale = GetSpriteScale(assets.aura);

                // Scale the aura dynamically 
                float scaleX = baseScale * (1.0f + (sizeRatio * 1.0f)); 
                float scaleY = baseScale * (1.0f + (sizeRatio * 3.0f));
                float destW = (float)assets.aura.width * scaleX;
                float destH = (float)assets.aura.height * scaleY;

                // Color tint shifts from a warm Yellow/Orange to an intense Blue/White as power goes up
                unsigned char r = (unsigned char)(255 - (powerRatio * 50));
                unsigned char g = (unsigned char)(200 - (powerRatio * 150));
                unsigned char b = (unsigned char)(powerRatio * 255);
                unsigned char a = (unsigned char)(150 + (powerRatio * 80)); // Increases opacity with power
                Color auraTint = { r, g, b, a };

                float rot = atan2f(-s->vy, -s->vx) * RAD2DEG + 90.0f;

                Rectangle srcRec = { 0.0f, 0.0f, (float)assets.aura.width, (float)assets.aura.height };
                Rectangle destRec = { s->x, s->y, destW, destH };
                Vector2 origin = { destW / 2.0f, destH / 2.0f };

                DrawTexturePro(assets.aura, srcRec, destRec, origin, rot, auraTint);
            }

            DrawSprite(*s);
            if (s->type == ENEMY) DrawHealthBar(*s); //skip over player, pickups, and pots
        }
        EndMode2D();
        DrawExpBar(player);
        DrawHpBar(player);
        DrawPopupTexts();
        if (hitFlashTimer > 0) DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(WHITE, 0.5f * hitFlashTimer / HIT_FLASH_DURATION));
        if (player->hp <= 0) {
            DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(BLACK, 0.6f));
            const char *msg = "GAME OVER";
            const char *hint = "Press R to restart";
            
            Vector2 msgSize = MeasureTextEx(customFont, msg, 48.0f, 1.0f);
            Vector2 hintSize = MeasureTextEx(customFont, hint, 20.0f, 1.0f);
            
            DrawTextEx(customFont, msg, (Vector2){ WIDTH / 2.0f - msgSize.x / 2.0f, HEIGHT / 2.0f - 40.0f }, 48.0f, 1.0f, RED);
            DrawTextEx(customFont, hint, (Vector2){ WIDTH / 2.0f - hintSize.x / 2.0f, HEIGHT / 2.0f + 20.0f }, 20.0f, 1.0f, WHITE);
        }
        if (showDebugPanel) DrawDebugPanel(player);
        EndDrawing();
    }

    UnloadGameAssets(&assets);
    CloseWindow();

    return 0;
}
