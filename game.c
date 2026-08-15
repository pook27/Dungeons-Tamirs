#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "raylib.h"

#include "game_config.h"

static int s_counter = 0;

enum SpriteType {
    PLAYER,
    ENEMY,
    PICKUP,
    POT
};

enum TileType {
    TILE_FLOOR,
    TILE_WALL,
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

typedef struct {
    Vector2 pos;
    int elite;
} EnemySpawn;

typedef struct {
    int tiles[ROOM_TILE_ROWS][ROOM_TILE_COLS];
    EnemySpawn enemySpawns[MAX_ROOM_ENEMIES];
    int enemySpawnCount;
    int cleared; // once true, LoadRoom stops respawning enemies and the door stays open
} Room;

static Room roomGrid[ROOM_GRID_ROWS][ROOM_GRID_COLS];
static int currentRoomRow = ROOM_GRID_ROWS / 2;
static int currentRoomCol = ROOM_GRID_COLS / 2;

// The center room is left enemy-free (and pre-cleared) so the player has a safe start.
void InitRooms() {
    for (int r = 0; r < ROOM_GRID_ROWS; r++) {
        for (int c = 0; c < ROOM_GRID_COLS; c++) {
            Room *room = &roomGrid[r][c];

            for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
                for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
                    int isBorder = (tx == 0 || tx == ROOM_TILE_COLS - 1 || ty == 0 || ty == ROOM_TILE_ROWS - 1);
                    room->tiles[ty][tx] = isBorder ? TILE_WALL : TILE_FLOOR;
                }
            }

            int isSafeRoom = (r == ROOM_GRID_ROWS / 2 && c == ROOM_GRID_COLS / 2);
            int doorTile = isSafeRoom ? TILE_DOOR_OPEN : TILE_DOOR_CLOSED;

            // Two-tile-wide door per side that actually has a neighboring room
            // (a single tile felt razor-thin to walk through). Punched through
            // the middle of that wall - matches which directions TryChangeRoom()
            // already allows crossing into.
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

            if (isSafeRoom) {
                room->enemySpawnCount = 0;
                continue;
            }

            int isBossRoom = (r == BOSS_ROOM_ROW && c == BOSS_ROOM_COL);
            room->enemySpawnCount = isBossRoom ? MAX_ROOM_ENEMIES : 1 + rand() % MAX_ROOM_ENEMIES;
            for (int i = 0; i < room->enemySpawnCount; i++) {
                room->enemySpawns[i].pos.x = (float)(60 + rand() % (WIDTH - 120));
                room->enemySpawns[i].pos.y = (float)(60 + rand() % (HEIGHT - 120));
                room->enemySpawns[i].elite = isBossRoom ? 1 : (rand() % 100 < ELITE_SPAWN_CHANCE);
            }
        }
    }
}

// Directional tile textures (currently just doors) are authored facing
// north/up. Rotate them to match whichever wall they're actually on.
// If your art faces a different default direction, adjust the 0.0f here.
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

void DrawRoom(Room *room, Texture2D wallTex, Texture2D doorOpenTex, Texture2D doorClosedTex) {
    float tileW = (float)WIDTH / ROOM_TILE_COLS;
    float tileH = (float)HEIGHT / ROOM_TILE_ROWS;

    for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
        for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
            switch (room->tiles[ty][tx]) {
                case TILE_WALL:        DrawTile(wallTex, tx, ty, tileW, tileH); break;
                case TILE_DOOR_OPEN:   DrawTile(doorOpenTex, tx, ty, tileW, tileH); break;
                case TILE_DOOR_CLOSED: DrawTile(doorClosedTex, tx, ty, tileW, tileH); break;
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
    int upgradeType;   // PICKUP only: which UpgradeType this grants on collection

    Stats stats; // PLAYER only: current upgraded values
    int exp;     // PLAYER only
    int level;   // PLAYER only
} Sprite;

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
        DrawText(popupTexts[i].text, (int)popupTexts[i].x, (int)popupTexts[i].y, 18, Fade(BLACK, alpha));
    }
}

void ApplyUpgrade(Sprite *player, int upgradeType) {
    Stats *stats = &player->stats;
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:   stats->moveSpeed += UPGRADE_MOVE_SPEED_AMOUNT; break;
        case UPGRADE_DAMAGE:       stats->damage += UPGRADE_DAMAGE_AMOUNT; break;
        case UPGRADE_DASH_TIME:    stats->dashTime += UPGRADE_DASH_TIME_AMOUNT; break;
        case UPGRADE_IFRAMES:      stats->iframesMax += UPGRADE_IFRAMES_AMOUNT; break;
        case UPGRADE_DODGE_CHANCE: stats->dodgeChance += UPGRADE_DODGE_CHANCE_AMOUNT; break;
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

// Same upgrade pool and application as a pickup - leveling up is just an
// automatic, no-choice pickup (Risk of Rain 2's model, not a pick-one menu).
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

Color UpgradeColor(int upgradeType) {
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:   return SKYBLUE;
        case UPGRADE_DAMAGE:       return RED;
        case UPGRADE_DASH_TIME:    return ORANGE;
        case UPGRADE_IFRAMES:      return LIME;
        case UPGRADE_DODGE_CHANCE: return VIOLET;
        case UPGRADE_DASH_RADIUS:  return GOLD;
        case UPGRADE_HEAL:         return GREEN;
        default: return WHITE;
    }
}

void DrawSprite(Sprite s) {
    Texture2D tex = s.texture;

    float rotation = atan2f(s.vy , s.vx) * RAD2DEG;
    Vector2 position = { s.x, s.y };

    Vector2 origin = { (float)tex.width / 2.0f, (float)tex.height / 2.0f }; 

    Rectangle sourceRec = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle destRec   = { position.x, position.y, (float)tex.width, (float)tex.height };

    Color tint = WHITE;
    if (s.type == ENEMY && s.elite) tint = Fade(GOLD, 0.6f + 0.4f * sinf(GetTime() * 6.0f)); // pulse so elites actually pop out of a crowd
    if (s.type == PICKUP) tint = UpgradeColor(s.upgradeType);
    if (s.type == POT) tint = BROWN; // reusing enemy art tinted, until real pot art shows up
    if (s.iframes > 0) tint = Fade(tint, 0.4f); // flicker while invincible, on top of any base tint

    DrawTexturePro(tex, sourceRec, destRec, origin, rotation, tint);
}

// Small bar above a sprite's head, only shown once it has taken damage
void DrawHealthBar(Sprite s) {

    float barWidth = 40.0f;
    float barHeight = 5.0f;
    float x = s.x - barWidth / 2.0f;
    float y = s.y - (float)s.texture.height / 2.0f - 10.0f;

    float pct = (float)s.hp / (float)s.maxhp;
    if (pct < 0.0f) pct = 0.0f;

    DrawRectangle(x, y, barWidth, barHeight, GRAY);
    DrawRectangle(x, y, barWidth * pct, barHeight, RED);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);
}
// Top-left HP bar for player + hp number
void DrawHpBar(Sprite *player) {
    float barWidth = 200.0f;
    float barHeight = 16.0f;
    float x = 10.0f;
    float y = 10.0f;

    float pct = (float)player->hp / (float)player->maxhp;
    if (pct < 0.0f) pct = 0.0f;

    DrawRectangle(x, y, barWidth, barHeight, GRAY);
    DrawRectangle(x, y, barWidth * pct, barHeight, RED);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);
    DrawText(TextFormat("Hp: %d", player->hp), (int)x, (int)(y + barHeight + 2), 16, BLACK);
}


// Top-right EXP bar + level number
void DrawExpBar(Sprite *player) {
    float barWidth = 200.0f;
    float barHeight = 16.0f;
    float x = WIDTH - barWidth - 10.0f;
    float y = 10.0f;

    int needed = ExpNeededForLevel(player->level);
    float pct = (float)player->exp / (float)needed;
    if (pct > 1.0f) pct = 1.0f;

    DrawRectangle(x, y, barWidth, barHeight, GRAY);
    DrawRectangle(x, y, barWidth * pct, barHeight, SKYBLUE);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);
    DrawText(TextFormat("Lv: %d", player->level), (int)x, (int)(y + barHeight + 2), 16, BLACK);
}

// TAB-toggled panel of the player's current stats, for testing upgrades
void DrawDebugPanel(Sprite *player) {
    int panelWidth = 220;
    DrawRectangle(0, 0, panelWidth, HEIGHT, Fade(BLACK, 0.6f));

    int y = 10;
    int lineHeight = 18;
    DrawText(TextFormat("HP: %d / %d", player->hp, player->maxhp), 10, y, 16, WHITE); y += lineHeight;
    DrawText(TextFormat("Level %d  (%d/%d exp)", player->level, player->exp, ExpNeededForLevel(player->level)), 10, y, 16, WHITE); y += lineHeight;
    y += lineHeight / 2;
    DrawText(TextFormat("Move Speed: %.2f", player->stats.moveSpeed), 10, y, 16, WHITE); y += lineHeight;
    DrawText(TextFormat("Dash Damage: %d + %d", DASH_DAMAGE, player->stats.damage), 10, y, 16, WHITE); y += lineHeight;
    DrawText(TextFormat("Dash Time: %d frames", player->stats.dashTime), 10, y, 16, WHITE); y += lineHeight;
    DrawText(TextFormat("Iframes: %d frames", player->stats.iframesMax), 10, y, 16, WHITE); y += lineHeight;
    DrawText(TextFormat("Dodge Chance: %.0f%%", player->stats.dodgeChance * 100.0f), 10, y, 16, WHITE); y += lineHeight;
    DrawText(TextFormat("Dash Radius Bonus: %.1f", player->stats.dashRadius), 10, y, 16, WHITE); y += lineHeight;
    y += lineHeight / 2;
    DrawText(TextFormat("Room: (%d, %d)", currentRoomRow, currentRoomCol), 10, y, 16, WHITE); y += lineHeight;
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
// summed - so a hit actually matches how big the art looks, instead of a
// flat number that has nothing to do with the sprites on screen.
float ContactRadius(Sprite *a, Sprite *b) {
    return ((float)a->texture.width + (float)b->texture.width) / 4.0f;
}

void SpawnPickup(Sprite** sprites_arr, Texture2D pickupTex, float x, float y) {
    if (s_counter >= 256) return;
    Sprite *pickup = malloc(sizeof(Sprite));
    *pickup = (Sprite){pickupTex, x, y, 0, 0, 0, 0, 1, PICKUP};
    pickup->upgradeType = rand() % UPGRADE_TYPE_COUNT;
    sprites_arr[s_counter++] = pickup;
}

void Update(Sprite** sprites_arr, Texture2D pickupTex) {
    for (int i = 0; i < s_counter; i++) {
        if (!sprites_arr[i]->active) continue;

        // Swarm AI: constantly steer toward the player (sprites_arr[0], always
        // the player - see CleanUpSprites). Reuses the existing ax/ay fields
        // instead of adding anything new to Sprite.
        if (sprites_arr[i]->type == ENEMY) {
            float dx = sprites_arr[0]->x - sprites_arr[i]->x;
            float dy = sprites_arr[0]->y - sprites_arr[i]->y;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 0.01f) {
                sprites_arr[i]->ax = dx / len * ENEMY_CHASE_ACCEL;
                sprites_arr[i]->ay = dy / len * ENEMY_CHASE_ACCEL;
            }
        }

        //do physics
        sprites_arr[i]->vx = (sprites_arr[i]->vx + sprites_arr[i]->ax) * 0.99f;
        sprites_arr[i]->vy = (sprites_arr[i]->vy + sprites_arr[i]->ay) * 0.99f;

        if (sprites_arr[i]->type == ENEMY) {
            float speed = sqrtf(sprites_arr[i]->vx * sprites_arr[i]->vx + sprites_arr[i]->vy * sprites_arr[i]->vy);
            if (speed > ENEMY_MAX_SPEED) {
                sprites_arr[i]->vx = sprites_arr[i]->vx / speed * ENEMY_MAX_SPEED;
                sprites_arr[i]->vy = sprites_arr[i]->vy / speed * ENEMY_MAX_SPEED;
            }
        }

        sprites_arr[i]->x += sprites_arr[i]->vx;
        sprites_arr[i]->y += sprites_arr[i]->vy;

        if (sprites_arr[i]->iframes > 0) sprites_arr[i]->iframes--;
    }

    // Player vs enemy contact: dashing into an enemy hurts them (dash attack,
    // using the player's own dashRadius/damage stats), otherwise touching an
    // enemy hurts the player (subject to their dodge chance).
    for (int i = 0; i < s_counter; i++) {
        if (sprites_arr[i]->type != PLAYER || !sprites_arr[i]->active) continue;

        for (int j = 0; j < s_counter; j++) {
            if (sprites_arr[j]->type != ENEMY || !sprites_arr[j]->active) continue;

            float dx = sprites_arr[i]->x - sprites_arr[j]->x;
            float dy = sprites_arr[i]->y - sprites_arr[j]->y;
            float distSq = dx*dx + dy*dy; // squared distance - avoids a sqrtf per pair per frame

            if (sprites_arr[i]->dashTimer > 0) {
                float r = ContactRadius(sprites_arr[i], sprites_arr[j]) + sprites_arr[i]->stats.dashRadius;
                if (distSq < r*r && sprites_arr[j]->iframes <= 0) {
                    sprites_arr[j]->hp -= (DASH_DAMAGE + sprites_arr[i]->stats.damage);
                    sprites_arr[j]->iframes = IFRAMES_DURATION;
                    if (sprites_arr[j]->hp <= 0) {
                        sprites_arr[j]->active = 0;
                        hitFlashTimer = HIT_FLASH_DURATION;
                        GrantExp(sprites_arr[i], EXP_PER_KILL);
                        if (sprites_arr[j]->elite && (rand() % 100 < ELITE_DROP_CHANCE)) {
                            SpawnPickup(sprites_arr, pickupTex, sprites_arr[j]->x, sprites_arr[j]->y);
                        }
                    }
                }
            } else {
                float r = ContactRadius(sprites_arr[i], sprites_arr[j]) + CONTACT_RADIUS_BONUS;
                if (distSq < r*r && sprites_arr[i]->iframes <= 0) {
                    int dodged = ((float)rand() / (float)RAND_MAX) < sprites_arr[i]->stats.dodgeChance;
                    if (!dodged) {
                        sprites_arr[i]->hp -= CONTACT_DAMAGE;
                        sprites_arr[i]->iframes = sprites_arr[i]->stats.iframesMax;
                        if (sprites_arr[i]->hp < 0) sprites_arr[i]->hp = 0;
                    }
                }
            }
        }
    }

    // Player vs pickup: touching one applies its upgrade immediately, no menu.
    for (int i = 0; i < s_counter; i++) {
        if (sprites_arr[i]->type != PLAYER || !sprites_arr[i]->active) continue;

        for (int j = 0; j < s_counter; j++) {
            if (sprites_arr[j]->type != PICKUP || !sprites_arr[j]->active) continue;

            float dx = sprites_arr[i]->x - sprites_arr[j]->x;
            float dy = sprites_arr[i]->y - sprites_arr[j]->y;
            float distSq = dx*dx + dy*dy;

            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                ApplyUpgrade(sprites_arr[i], sprites_arr[j]->upgradeType);
                SpawnPopupText(UpgradeName(sprites_arr[j]->upgradeType), sprites_arr[i]->x, sprites_arr[i]->y - 30.0f);
                sprites_arr[j]->active = 0;
            }
        }
    }

    // Player vs pot: same shape as the pickup loop above, breaks on contact and heals.
    for (int i = 0; i < s_counter; i++) {
        if (sprites_arr[i]->type != PLAYER || !sprites_arr[i]->active) continue;

        for (int j = 0; j < s_counter; j++) {
            if (sprites_arr[j]->type != POT || !sprites_arr[j]->active) continue;

            float dx = sprites_arr[i]->x - sprites_arr[j]->x;
            float dy = sprites_arr[i]->y - sprites_arr[j]->y;
            float distSq = dx*dx + dy*dy;

            if (distSq < PICKUP_RADIUS * PICKUP_RADIUS) {
                sprites_arr[i]->hp += POT_HEAL_AMOUNT;
                if (sprites_arr[i]->hp > sprites_arr[i]->maxhp) sprites_arr[i]->hp = sprites_arr[i]->maxhp;
                SpawnPopupText("+Heal", sprites_arr[i]->x, sprites_arr[i]->y - 30.0f);
                sprites_arr[j]->active = 0;
            }
        }
    }
}

void FreeSprites(Sprite** sprites_arr) {
    for(int i=0; i<s_counter; i++) {
        free(sprites_arr[i]);
    }
}

void CleanUpSprites(Sprite** sprites_arr) {
    for (int i = 0; i < s_counter; ) {
        if (sprites_arr[i]->active == 0) {
            free(sprites_arr[i]);
            sprites_arr[i] = sprites_arr[s_counter - 1]; // swap in the last slot...
            s_counter--;                                 // ...and pop it, instead of shifting everything down
        } else {
            i++; // only advance if we didn't just swap a new sprite into i
        }
    }
}

int IsBlockingTile(int tileType) {
    return tileType == TILE_WALL || tileType == TILE_DOOR_CLOSED;
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
    float halfW = (float)s->texture.width / 2.0f;
    float halfH = (float)s->texture.height / 2.0f;
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
void UpdateDoors(Sprite** sprites_arr, Room *room) {
    if (room->cleared) return;

    for (int i = 0; i < s_counter; i++) {
        if (sprites_arr[i]->type == ENEMY && sprites_arr[i]->active) return; // still enemies left
    }

    for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
        for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
            if (room->tiles[ty][tx] == TILE_DOOR_CLOSED) room->tiles[ty][tx] = TILE_DOOR_OPEN;
        }
    }
    room->cleared = 1;
    // sprites_arr[0] is always the player (see CleanUpSprites - it's never reordered)
    SpawnPopupText("Room Cleared!", sprites_arr[0]->x - 40.0f, sprites_arr[0]->y - 40.0f);
}

void LoadRoom(Sprite** sprites_arr, Texture2D enemyTex, Texture2D potTex) {
    // Leftover enemies, pots, and any uncollected pickups don't carry over between rooms
    for (int i = 0; i < s_counter; i++) {
        if (sprites_arr[i]->type == ENEMY || sprites_arr[i]->type == PICKUP || sprites_arr[i]->type == POT) sprites_arr[i]->active = 0;
    }
    CleanUpSprites(sprites_arr);

    Room *room = &roomGrid[currentRoomRow][currentRoomCol];
    if (room->cleared) return; // already cleared - stays empty, door stays open

    if (currentRoomRow == BOSS_ROOM_ROW && currentRoomCol == BOSS_ROOM_COL) {
        SpawnPopupText("Boss Room!", sprites_arr[0]->x - 30.0f, sprites_arr[0]->y - 40.0f);
    }

    for (int i = 0; i < room->enemySpawnCount && s_counter < 256; i++) {
        Sprite *enemy = malloc(sizeof(Sprite));
        EnemySpawn spawn = room->enemySpawns[i];
        *enemy = (Sprite){enemyTex, spawn.pos.x, spawn.pos.y, 0, 0, 0, 0, 1, ENEMY};
        enemy->elite = spawn.elite;
        enemy->maxhp = ENEMY_MAX_HP * (spawn.elite ? ELITE_HP_MULTIPLIER : 1);
        enemy->hp = enemy->maxhp;
        sprites_arr[s_counter++] = enemy;
    }

    if (s_counter < 256 && rand() % 100 < POT_SPAWN_CHANCE) {
        Sprite *pot = malloc(sizeof(Sprite));
        float px = (float)(60 + rand() % (WIDTH - 120));
        float py = (float)(60 + rand() % (HEIGHT - 120));
        *pot = (Sprite){potTex, px, py, 0, 0, 0, 0, 1, POT};
        sprites_arr[s_counter++] = pot;
    }
}

int TryChangeRoom(Sprite *player) {
    // Landing exactly on the door tile (x=0/WIDTH) was the softlock: a room
    // with enemies has ALL its doors closed, including the one you just
    // walked through, so ResolveWallCollision shoved you right back out next
    // frame. Step in past the wall thickness instead, onto real floor.
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

// Wipes any in-flight sprites, respawns a fresh player, and regenerates the
// dungeon - same setup the game already did once at boot, just reusable so
// death doesn't require a process restart.
void ResetGame(Sprite** sprites_arr, Texture2D birdTex, Texture2D enemyTex, Texture2D potTex) {
    for (int i = 0; i < s_counter; i++) free(sprites_arr[i]);
    s_counter = 0;

    currentRoomRow = ROOM_GRID_ROWS / 2;
    currentRoomCol = ROOM_GRID_COLS / 2;

    Sprite *bird = malloc(sizeof(Sprite));
    *bird = (Sprite){birdTex, WIDTH / 2.0f, HEIGHT / 2.0f, 0, 0, 0, 0, 1, PLAYER};
    bird->hp = PLAYER_MAX_HP;
    bird->maxhp = PLAYER_MAX_HP;
    bird->level = 1;
    bird->exp = 0;
    bird->stats = (Stats){ MAX_MOVE_SPEED, 0, DASH_TIME, IFRAMES_DURATION, 0.0f, CONTACT_RADIUS_BONUS };
    sprites_arr[s_counter++] = bird;

    InitRooms();
    LoadRoom(sprites_arr, enemyTex, potTex);
}

int main() {
    srand(time(NULL));
    Sprite *sprites[256];
    int showDebugPanel = 0;

    InitWindow(WIDTH, HEIGHT, "Game");
    SetTargetFPS(60);

    // 1. Load all textures ONCE at the start of the game
    Texture2D birdtex = LoadTexture("assets/bird.png");
    Texture2D stickmantex = LoadTexture("assets/stickman.png");
    Texture2D walltex = LoadTexture("assets/wall.png");
    Texture2D doorOpenTex = LoadTexture("assets/door_open.png");
    Texture2D doorClosedTex = LoadTexture("assets/door_closed.png");
    Texture2D pickupTex = LoadTexture("assets/pickup.png");
    Texture2D potTex = LoadTexture("assets/pot.png");
    Texture2D auraTex = LoadTexture("assets/aura.png");

    // 2. Assign the loaded textures to the structs
    ResetGame(sprites, birdtex, stickmantex, potTex);
    Sprite *bird = sprites[0]; // sprites[0] is always the player (see CleanUpSprites)

    while(!WindowShouldClose()) {
        if (bird->hp > 0) {
            Update(sprites, pickupTex);
            CleanUpSprites(sprites);

            Room *room = &roomGrid[currentRoomRow][currentRoomCol];
            UpdateDoors(sprites, room);
            ResolveWallCollision(bird, room);

            if (TryChangeRoom(bird)) {
                LoadRoom(sprites, stickmantex, potTex);
            }
            move(bird);
            UpdatePopupTexts();
            if (hitFlashTimer > 0) hitFlashTimer--;
        } else if (IsKeyPressed(KEY_R)) {
            ResetGame(sprites, birdtex, stickmantex, potTex);
            bird = sprites[0];
        }

        if (IsKeyPressed(KEY_TAB)) showDebugPanel = !showDebugPanel;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawRoom(&roomGrid[currentRoomRow][currentRoomCol], walltex, doorOpenTex, doorClosedTex);
        //drawing the sprites
        for (int i =0; i<s_counter; i++) {
            DrawSprite(*sprites[i]);
            if (sprites[i]->type == ENEMY) DrawHealthBar(*sprites[i]); //skip over player, pickups, and pots

            if (sprites[i]->type == PLAYER && sprites[i]->dashTimer > 0) {
                float powerRatio = fminf((float)sprites[i]->stats.damage / 20.0f, 1.0f); 
                float sizeRatio = fminf((sprites[i]->stats.dashRadius - MAX_MOVE_SPEED) / 10.0f, 1.0f);

                // Scale the aura (grows taller much faster than it grows wide)
                float scaleX = 1.0f + (sizeRatio * 1.0f); 
                float scaleY = 1.0f + (sizeRatio * 3.0f);
                float destW = auraTex.width * scaleX;
                float destH = auraTex.height * scaleY;

                // Color tint shifts from a warm Yellow/Orange to an intense Blue/White as power goes up
                unsigned char r = (unsigned char)(255 - (powerRatio * 50));
                unsigned char g = (unsigned char)(200 - (powerRatio * 150));
                unsigned char b = (unsigned char)(powerRatio * 255);
                unsigned char a = (unsigned char)(150 + (powerRatio * 80)); // Increases opacity with power
                Color auraTint = { r, g, b, a };

                float rot = atan2f(-sprites[i]->vy, -sprites[i]->vx) * RAD2DEG + 90.0f;

                Rectangle srcRec = { 0.0f, 0.0f, (float)auraTex.width, (float)auraTex.height };
                Rectangle destRec = { sprites[i]->x, sprites[i]->y, destW, destH };
                Vector2 origin = { destW / 2.0f, destH / 2.0f };

                DrawTexturePro(auraTex, srcRec, destRec, origin, rot, auraTint);
            }
        }
        DrawExpBar(bird);
        DrawHpBar(bird);
        DrawPopupTexts();
        if (hitFlashTimer > 0) DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(WHITE, 0.5f * hitFlashTimer / HIT_FLASH_DURATION));
        if (bird->hp <= 0) {
            DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(BLACK, 0.6f));
            const char *msg = "GAME OVER";
            const char *hint = "Press R to restart";
            DrawText(msg, WIDTH / 2 - MeasureText(msg, 48) / 2, HEIGHT / 2 - 40, 48, RED);
            DrawText(hint, WIDTH / 2 - MeasureText(hint, 20) / 2, HEIGHT / 2 + 20, 20, WHITE);
        }
        if (showDebugPanel) DrawDebugPanel(bird);
        DrawFPS(10, HEIGHT - 20);
        EndDrawing();
    }

    FreeSprites(sprites);

    UnloadTexture(birdtex);
    UnloadTexture(stickmantex);
    UnloadTexture(walltex);
    UnloadTexture(doorOpenTex);
    UnloadTexture(doorClosedTex);
    UnloadTexture(pickupTex);
    UnloadTexture(auraTex);

    CloseWindow();

    return 0;
}
