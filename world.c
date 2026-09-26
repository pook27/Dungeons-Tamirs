#include <math.h>
#include <stdlib.h>
#include "world.h"
#include "assets.h"   // GetSpriteScale, used by ResolveWallCollision
#include "entities.h" // sprites[]/spriteCount + EnemyHpMult/AdvanceAnimFrame
#include "ui.h"       // SpawnPopupText
#include "upgrades.h" // RollUpgradeChoices/UpgradeRarity/ApplyUpgrade/UpgradeName/UpgradeDescription - shop room

Room roomGrid[ROOM_GRID_ROWS][ROOM_GRID_COLS];
int currentRoomRow = ROOM_GRID_ROWS / 2;
int currentRoomCol = ROOM_GRID_COLS / 2;
int bossRoomRow = ROOM_GRID_ROWS / 2; // overwritten by GenerateFloorShape before first use
int bossRoomCol = ROOM_GRID_COLS / 2;
int dungeonDepth = 0;

#define BOSS_MARK_COLOR (Color){ 232, 55, 90, 255 } // hot pink/red - marks the boss room's door and its minimap tile

typedef struct { int r, c; } RoomCoord;

// --- Shop room (ROOM_SHOP) ---------------------------------------------------------------------------
// Layout is fixed/world-space, same idea as BOSS_MARK_COLOR above being a plain local define.
#define SHOP_TABLE_Y (HEIGHT / 2.0f + 60.0f)             // table sits a bit below room center
#define SHOP_KEEPER_POS (Vector2){ WIDTH / 2.0f, HEIGHT / 2.0f - 80.0f } // shopkeeper stands behind the table
#define SHOP_ITEM_SPACING 110.0f                          // horizontal gap between the 3 item slots
#define SHOP_ITEM_PROXIMITY 60.0f                         // how close the player must stand to buy/see a tooltip

static int shopAnimFrame = 0;                 // shopkeeper idle-loop frame - module state, there's only ever one shopkeeper on screen
static int shopAnimTimer = ANIM_FRAME_DURATION;

static int ShopPriceForRarity(int rarity) {
    switch (rarity) {
        case RARITY_UNCOMMON: return SHOP_PRICE_UNCOMMON;
        case RARITY_RARE:     return SHOP_PRICE_RARE;
        default:                return SHOP_PRICE_COMMON;
    }
}

// Rolls the table's 3 items and their prices - called once from InitRooms when a floor's shop room is set
// up, so purchases/rerolls persist for the rest of that floor (see Room.shopRerollCount).
static void RollShopRoom(Room *room) {
    RollUpgradeChoices(room->shopItemType, 3);
    for (int i = 0; i < 3; i++) {
        room->shopItemPrice[i] = ShopPriceForRarity(UpgradeRarity(room->shopItemType[i]));
        room->shopPurchased[i] = 0;
    }
    room->shopRerollCount = 0;
}

static Vector2 ShopItemSlotPos(int slot) {
    float startX = WIDTH / 2.0f - SHOP_ITEM_SPACING;
    return (Vector2){ startX + slot * SHOP_ITEM_SPACING, SHOP_TABLE_Y };
}

static int ShopRerollCost(Room *room) {
    return SHOP_REROLL_BASE_COST + room->shopRerollCount * SHOP_REROLL_COST_GROWTH;
}

// Walk up to an unpurchased item and press E; R rerolls every still-unpurchased slot at a climbing cost.
// A no-op whenever the current room isn't a shop, so it's safe to call unconditionally each active frame.
void UpdateShopRoom(GameAssets *assets, Sprite *player) {
    (void)assets; // the shopkeeper's texture is picked at draw time from shopAnimFrame - nothing to do here yet
    Room *room = &roomGrid[currentRoomRow][currentRoomCol];
    if (room->roomType != ROOM_SHOP) return;

    AdvanceAnimFrame(&shopAnimTimer, &shopAnimFrame, SHOPKEEP_FRAME_COUNT, ANIM_FRAME_DURATION);

    if (IsKeyPressed(KEY_E)) {
        for (int i = 0; i < 3; i++) {
            if (room->shopPurchased[i]) continue;
            Vector2 slotPos = ShopItemSlotPos(i);
            float dx = player->x - slotPos.x, dy = player->y - slotPos.y;
            if (dx * dx + dy * dy > SHOP_ITEM_PROXIMITY * SHOP_ITEM_PROXIMITY) continue;

            if (player->coins < room->shopItemPrice[i]) {
                SpawnPopupText("Not enough coins", player->x, player->y - 30.0f);
                break;
            }
            player->coins -= room->shopItemPrice[i];
            room->shopPurchased[i] = 1; // sold out for the rest of this floor - not refilled
            ApplyUpgrade(player, room->shopItemType[i]);
            SpawnPopupText(UpgradeName(room->shopItemType[i]), player->x, player->y - 30.0f);
            break;
        }
    }

    if (IsKeyPressed(KEY_R)) {
        int unpurchasedCount = 0;
        for (int i = 0; i < 3; i++) if (!room->shopPurchased[i]) unpurchasedCount++;

        if (unpurchasedCount == 0) {
            // nothing left to reroll
        } else if (player->coins < ShopRerollCost(room)) {
            SpawnPopupText("Not enough coins", player->x, player->y - 30.0f);
        } else {
            player->coins -= ShopRerollCost(room);
            room->shopRerollCount++;

            int freshTypes[3];
            RollUpgradeChoices(freshTypes, unpurchasedCount);
            int freshIdx = 0;
            for (int i = 0; i < 3; i++) {
                if (room->shopPurchased[i]) continue;
                room->shopItemType[i] = freshTypes[freshIdx++];
                room->shopItemPrice[i] = ShopPriceForRarity(UpgradeRarity(room->shopItemType[i]));
            }
            SpawnPopupText("Rerolled!", player->x, player->y - 30.0f);
        }
    }
}

static void DrawShopKeeper(GameAssets *assets) {
    Texture2D tex = assets->shopkeepFrames[shopAnimFrame];
    float scale = 3*GetSpriteScale(tex);
    float destW = (float)tex.width * scale;
    float destH = (float)tex.height * scale;
    Vector2 pos = SHOP_KEEPER_POS;
    Rectangle srcRec = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle destRec = { pos.x, pos.y, destW, destH };
    Vector2 origin = { destW / 2.0f, destH / 2.0f };
    DrawTexturePro(tex, srcRec, destRec, origin, 0.0f, WHITE);
}

static void DrawShopTable(GameAssets *assets) {
    Texture2D tex = assets->table;
    float scale = GetSpriteScale(tex) * 5.0f; // wider than one tile - spans the item row
    float destW = (float)tex.width * scale;
    float destH = (float)tex.height * scale;
    Vector2 pos = { WIDTH / 2.0f, SHOP_TABLE_Y };
    Rectangle srcRec = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle destRec = { pos.x, pos.y, destW, destH };
    Vector2 origin = { destW / 2.0f, destH / 2.0f };
    DrawTexturePro(tex, srcRec, destRec, origin, 0.0f, WHITE);
}

// Table + shopkeeper + each unsold item's icon/price, plus a proximity tooltip (the level-up cards'
// "current -> next" UpgradeDescription text doubles as this for free) and a reroll hint. A no-op for any
// room that isn't the shop.
void DrawShopRoom(Room *room, GameAssets *assets) {
    if (room->roomType != ROOM_SHOP) return;

    DrawShopTable(assets);
    DrawShopKeeper(assets);

    Sprite *player = &sprites[0]; // sprites[0] is always the player

    for (int i = 0; i < 3; i++) {
        Vector2 slotPos = ShopItemSlotPos(i);

        if (room->shopPurchased[i]) {
            const char *sold = "SOLD";
            Vector2 soldSize = MeasureTextEx(customFont, sold, 14.0f, 1.0f);
            DrawTextEx(customFont, sold, (Vector2){ slotPos.x - soldSize.x / 2.0f, slotPos.y - 8.0f }, 14.0f, 1.0f, Fade(GRAY, 0.8f));
            continue;
        }

        Texture2D icon = assets->upgradeIcons[room->shopItemType[i]];
        float iconSize = 40.0f;
        float scale = iconSize / fmaxf((float)icon.width, (float)icon.height);
        Rectangle srcRec = { 0.0f, 0.0f, (float)icon.width, (float)icon.height };
        Rectangle destRec = { slotPos.x, slotPos.y - 24.0f, icon.width * scale, icon.height * scale };
        Vector2 origin = { destRec.width / 2.0f, destRec.height / 2.0f };
        DrawTexturePro(icon, srcRec, destRec, origin, 0.0f, WHITE);

        const char *price = TextFormat("%d", room->shopItemPrice[i]);
        Vector2 priceSize = MeasureTextEx(customFont, price, 14.0f, 1.0f);
        DrawTextEx(customFont, price, (Vector2){ slotPos.x - priceSize.x / 2.0f, slotPos.y + 2.0f }, 14.0f, 1.0f, GOLD);

        float dx = player->x - slotPos.x, dy = player->y - slotPos.y;
        if (dx * dx + dy * dy < SHOP_ITEM_PROXIMITY * SHOP_ITEM_PROXIMITY) {
            const char *name = UpgradeName(room->shopItemType[i]);
            const char *desc = UpgradeDescription(room->shopItemType[i], &player->stats);
            Vector2 nameSize = MeasureTextEx(customFont, name, 14.0f, 1.0f);

            DrawRectangle((int)(slotPos.x - 72.0f), (int)(slotPos.y + 22.0f), 144, 58, Fade(BLACK, 0.7f));
            DrawTextEx(customFont, name, (Vector2){ slotPos.x - nameSize.x / 2.0f, slotPos.y + 26.0f }, 14.0f, 1.0f, WHITE);
            DrawTextEx(customFont, desc, (Vector2){ slotPos.x - 66.0f, slotPos.y + 44.0f }, 11.0f, 1.0f, LIGHTGRAY);
            DrawTextEx(customFont, "[E] Buy", (Vector2){ slotPos.x - 22.0f, slotPos.y + 70.0f }, 11.0f, 1.0f, GOLD);
        }
    }

    const char *rerollHint = TextFormat("[R] Reroll (%d coins)", ShopRerollCost(room));
    Vector2 hintSize = MeasureTextEx(customFont, rerollHint, 13.0f, 1.0f);
    DrawTextEx(customFont, rerollHint, (Vector2){ WIDTH / 2.0f - hintSize.x / 2.0f, SHOP_KEEPER_POS.y - 30.0f }, 13.0f, 1.0f, WHITE);
}
// -------------------------------------------------------------------------------------------------------

// Grows a random, connected "blob" of rooms out from the center starting
// square: repeatedly pick a random already-existing room and try to extend
// into a random open neighbor. Every new room attaches to one already in the
// shape, so the result is guaranteed connected no matter how it comes out.
// A BFS from the start then finds whichever room ends up farthest away and
// hands that one the boss - always reachable, never the starting square.
static void GenerateFloorShape(void) {
    for (int r = 0; r < ROOM_GRID_ROWS; r++)
        for (int c = 0; c < ROOM_GRID_COLS; c++)
            roomGrid[r][c].exists = 0;

    int startR = ROOM_GRID_ROWS / 2, startC = ROOM_GRID_COLS / 2;
    roomGrid[startR][startC].exists = 1;

    RoomCoord existingList[ROOM_GRID_ROWS * ROOM_GRID_COLS];
    int existingCount = 0;
    existingList[existingCount++] = (RoomCoord){ startR, startC };

    int maxPossibleRooms = ROOM_GRID_ROWS * ROOM_GRID_COLS;
    int cap = (MAX_FLOOR_ROOMS < maxPossibleRooms) ? MAX_FLOOR_ROOMS : maxPossibleRooms;
    int targetRooms = MIN_FLOOR_ROOMS + rand() % (cap - MIN_FLOOR_ROOMS + 1);

    static const int dr[4] = { -1, 1, 0, 0 };
    static const int dc[4] = { 0, 0, -1, 1 };

    // Guarded rather than looping unconditionally - a stalled growth just settles for a slightly smaller floor.
    int guard = 0;
    while (existingCount < targetRooms && guard++ < 4000) {
        RoomCoord from = existingList[rand() % existingCount];
        int dir = rand() % 4;
        int nr = from.r + dr[dir], nc = from.c + dc[dir];
        if (nr < 0 || nr >= ROOM_GRID_ROWS || nc < 0 || nc >= ROOM_GRID_COLS) continue;
        if (roomGrid[nr][nc].exists) continue;

        roomGrid[nr][nc].exists = 1;
        existingList[existingCount++] = (RoomCoord){ nr, nc };
    }

    int dist[ROOM_GRID_ROWS][ROOM_GRID_COLS];
    for (int r = 0; r < ROOM_GRID_ROWS; r++)
        for (int c = 0; c < ROOM_GRID_COLS; c++)
            dist[r][c] = -1;

    RoomCoord queue[ROOM_GRID_ROWS * ROOM_GRID_COLS];
    int qHead = 0, qTail = 0;
    dist[startR][startC] = 0;
    queue[qTail++] = (RoomCoord){ startR, startC };

    RoomCoord farthest = { startR, startC };
    int farthestDist = 0;

    while (qHead < qTail) {
        RoomCoord cur = queue[qHead++];
        for (int dir = 0; dir < 4; dir++) {
            int nr = cur.r + dr[dir], nc = cur.c + dc[dir];
            if (nr < 0 || nr >= ROOM_GRID_ROWS || nc < 0 || nc >= ROOM_GRID_COLS) continue;
            if (!roomGrid[nr][nc].exists || dist[nr][nc] != -1) continue;

            dist[nr][nc] = dist[cur.r][cur.c] + 1;
            queue[qTail++] = (RoomCoord){ nr, nc };
            if (dist[nr][nc] > farthestDist) { farthestDist = dist[nr][nc]; farthest = (RoomCoord){ nr, nc }; }
        }
    }

    bossRoomRow = farthest.r;
    bossRoomCol = farthest.c;

    // RoomType: everything in the blob defaults to combat, then start/boss overwrite the two rooms that
    // already had a fixed convention. Shop is one other room from the blob, picked with a minimum BFS
    // distance from start so it's never trivially adjacent to spawn.
    for (int r = 0; r < ROOM_GRID_ROWS; r++)
        for (int c = 0; c < ROOM_GRID_COLS; c++)
            roomGrid[r][c].roomType = ROOM_COMBAT;

    roomGrid[startR][startC].roomType = ROOM_START;
    roomGrid[bossRoomRow][bossRoomCol].roomType = ROOM_BOSS;

    RoomCoord shopCandidates[ROOM_GRID_ROWS * ROOM_GRID_COLS];
    int shopCandidateCount = 0;
    for (int i = 0; i < existingCount; i++) {
        RoomCoord rc = existingList[i];
        if (rc.r == startR && rc.c == startC) continue;
        if (rc.r == bossRoomRow && rc.c == bossRoomCol) continue;
        if (dist[rc.r][rc.c] < SHOP_MIN_DISTANCE_FROM_START) continue;
        shopCandidates[shopCandidateCount++] = rc;
    }
    // No eligible room (a stalled/tiny generation) just means no shop this floor - everything else already
    // defaulted to combat above, so there's nothing further to do.
    if (shopCandidateCount > 0) {
        RoomCoord shopRoom = shopCandidates[rand() % shopCandidateCount];
        roomGrid[shopRoom.r][shopRoom.c].roomType = ROOM_SHOP;
    }
}

void InitRooms(void) {
    GenerateFloorShape();

    for (int r = 0; r < ROOM_GRID_ROWS; r++) {
        for (int c = 0; c < ROOM_GRID_COLS; c++) {
            Room *room = &roomGrid[r][c];
            room->cleared = 0;
            room->visited = 0;
            room->enemySpawnCount = 0;

            if (!room->exists) continue; // outside this floor's shape - left blank, never loaded or drawn

            for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
                for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
                    int isBorder = (tx == 0 || tx == ROOM_TILE_COLS - 1 || ty == 0 || ty == ROOM_TILE_ROWS - 1);
                    int randomWall = TILE_WALL_1 + (rand() % 3);
                    room->tiles[ty][tx] = isBorder ? randomWall : TILE_FLOOR;
                }
            }

            int isSafeRoom = (room->roomType == ROOM_START);
            int isShopRoom = (room->roomType == ROOM_SHOP);
            int doorTile = (isSafeRoom || isShopRoom) ? TILE_DOOR_OPEN : TILE_DOOR_CLOSED; // shop is safe too - no combat gate on its doors

            // A side only gets a door if that neighboring room actually exists this floor - otherwise it's a solid wall.
            if (r > 0 && roomGrid[r - 1][c].exists) {
                room->tiles[0][ROOM_TILE_COLS / 2 - 1] = doorTile;
                room->tiles[0][ROOM_TILE_COLS / 2] = doorTile;                                          // north
            }
            if (r < ROOM_GRID_ROWS - 1 && roomGrid[r + 1][c].exists) {
                room->tiles[ROOM_TILE_ROWS - 1][ROOM_TILE_COLS / 2 - 1] = doorTile;
                room->tiles[ROOM_TILE_ROWS - 1][ROOM_TILE_COLS / 2] = doorTile;                          // south
            }
            if (c > 0 && roomGrid[r][c - 1].exists) {
                room->tiles[ROOM_TILE_ROWS / 2 - 1][0] = doorTile;
                room->tiles[ROOM_TILE_ROWS / 2][0] = doorTile;                                           // west
            }
            if (c < ROOM_GRID_COLS - 1 && roomGrid[r][c + 1].exists) {
                room->tiles[ROOM_TILE_ROWS / 2 - 1][ROOM_TILE_COLS - 1] = doorTile;
                room->tiles[ROOM_TILE_ROWS / 2][ROOM_TILE_COLS - 1] = doorTile;                          // east
            }

            room->cleared = isSafeRoom || isShopRoom;
            room->visited = isSafeRoom; // prevents pots from spawning in the start room

            if (isSafeRoom) {
                room->enemySpawnCount = 0;
                continue;
            }

            if (isShopRoom) {
                room->enemySpawnCount = 0;
                RollShopRoom(room); // rolled once per floor - purchases/rerolls persist across re-visits
                continue;
            }

            int isBossRoom = (room->roomType == ROOM_BOSS);

            if (isBossRoom) {
                // One real boss, plus 0-2 regular adds for room presence - not a whole pack of reskinned elites.
                room->enemySpawnCount = 1 + rand() % 3;
                room->enemySpawns[0].pos = (Vector2){ WIDTH / 2.0f, HEIGHT / 2.0f };
                room->enemySpawns[0].elite = 1;
                room->enemySpawns[0].isBoss = 1;
                room->enemySpawns[0].variant = ENEMY_NORMAL; // irrelevant - LoadRoom always uses assets->boss1Frames/boss2Frames for isBoss
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

// Directional tile textures (currently just doors) are authored facing north/up - rotate to match the actual wall.
static float TileRotationForSide(int tx, int ty) {
    if (ty == 0) return 0.0f;                    // north wall
    if (ty == ROOM_TILE_ROWS - 1) return 180.0f;  // south wall
    if (tx == 0) return 270.0f;                   // west wall
    if (tx == ROOM_TILE_COLS - 1) return 90.0f;   // east wall
    return 0.0f;
}

static void DrawTile(Texture2D tex, int tx, int ty, float tileW, float tileH, Color tint) {
    float sourceWidth = (float)tex.width;

    if (ty == 0 && tx == ROOM_TILE_COLS / 2)                        sourceWidth = -sourceWidth; // north wall
    else if (ty == ROOM_TILE_ROWS - 1 && tx == ROOM_TILE_COLS / 2 - 1) sourceWidth = -sourceWidth; // south wall
    else if (tx == ROOM_TILE_COLS - 1 && ty == ROOM_TILE_ROWS / 2)     sourceWidth = -sourceWidth; // east wall
    else if (tx == 0 && ty == ROOM_TILE_ROWS / 2 - 1)                  sourceWidth = -sourceWidth; // west wall

    Rectangle sourceRec = { 0.0f, 0.0f, sourceWidth, (float)tex.height };
    Rectangle destRec = { (tx + 0.5f) * tileW, (ty + 0.5f) * tileH, tileW, tileH };
    Vector2 origin = { tileW / 2.0f, tileH / 2.0f };
    DrawTexturePro(tex, sourceRec, destRec, origin, TileRotationForSide(tx, ty), tint);
}

// Which wall (if any) of `roomRow, roomCol` borders the boss room directly - the door on that wall gets tinted
// as a standing "this way to the boss" landmark on approach.
enum BossDoorSide { BOSS_SIDE_NONE, BOSS_SIDE_NORTH, BOSS_SIDE_SOUTH, BOSS_SIDE_WEST, BOSS_SIDE_EAST };

static int GetBossDoorSide(int roomRow, int roomCol) {
    if (roomRow - 1 == bossRoomRow && roomCol == bossRoomCol) return BOSS_SIDE_NORTH;
    if (roomRow + 1 == bossRoomRow && roomCol == bossRoomCol) return BOSS_SIDE_SOUTH;
    if (roomRow == bossRoomRow && roomCol - 1 == bossRoomCol) return BOSS_SIDE_WEST;
    if (roomRow == bossRoomRow && roomCol + 1 == bossRoomCol) return BOSS_SIDE_EAST;
    return BOSS_SIDE_NONE;
}

static int IsBossDoorTile(int tx, int ty, int bossSide) {
    switch (bossSide) {
        case BOSS_SIDE_NORTH: return ty == 0 && (tx == ROOM_TILE_COLS / 2 - 1 || tx == ROOM_TILE_COLS / 2);
        case BOSS_SIDE_SOUTH: return ty == ROOM_TILE_ROWS - 1 && (tx == ROOM_TILE_COLS / 2 - 1 || tx == ROOM_TILE_COLS / 2);
        case BOSS_SIDE_WEST:  return tx == 0 && (ty == ROOM_TILE_ROWS / 2 - 1 || ty == ROOM_TILE_ROWS / 2);
        case BOSS_SIDE_EAST:  return tx == ROOM_TILE_COLS - 1 && (ty == ROOM_TILE_ROWS / 2 - 1 || ty == ROOM_TILE_ROWS / 2);
        default: return 0;
    }
}

void DrawRoom(Room *room, GameAssets *assets, int roomRow, int roomCol) {
    float tileW = (float)WIDTH / ROOM_TILE_COLS;
    float tileH = (float)HEIGHT / ROOM_TILE_ROWS;
    int bossSide = GetBossDoorSide(roomRow, roomCol);

    for (int ty = 0; ty < ROOM_TILE_ROWS; ty++) {
        for (int tx = 0; tx < ROOM_TILE_COLS; tx++) {
            int isDoor = (room->tiles[ty][tx] == TILE_DOOR_OPEN || room->tiles[ty][tx] == TILE_DOOR_CLOSED);
            Color tint = (isDoor && IsBossDoorTile(tx, ty, bossSide)) ? BOSS_MARK_COLOR : WHITE;

            switch (room->tiles[ty][tx]) {
                case TILE_WALL_1:      DrawTile(assets->walls[0], tx, ty, tileW, tileH, WHITE); break;
                case TILE_WALL_2:      DrawTile(assets->walls[1], tx, ty, tileW, tileH, WHITE); break;
                case TILE_WALL_3:      DrawTile(assets->walls[2], tx, ty, tileW, tileH, WHITE); break;
                case TILE_DOOR_OPEN:   DrawTile(assets->doorOpen, tx, ty, tileW, tileH, tint); break;
                case TILE_DOOR_CLOSED: DrawTile(assets->doorClosed, tx, ty, tileW, tileH, tint); break;
                default: break; // TILE_FLOOR - nothing drawn, background shows through
            }
        }
    }
}

// Has the player found a tinted door pointing at the boss room yet (by standing in it, or in a bordering room)?
static int BossRoomRevealed(void) {
    if (roomGrid[bossRoomRow][bossRoomCol].visited) return 1;

    static const int dr[4] = { -1, 1, 0, 0 };
    static const int dc[4] = { 0, 0, -1, 1 };
    for (int i = 0; i < 4; i++) {
        int nr = bossRoomRow + dr[i], nc = bossRoomCol + dc[i];
        if (nr < 0 || nr >= ROOM_GRID_ROWS || nc < 0 || nc >= ROOM_GRID_COLS) continue;
        if (roomGrid[nr][nc].exists && roomGrid[nr][nc].visited) return 1;
    }
    return 0;
}

// Semi-transparent floor overview, bottom-right corner. Shows only visited rooms (fog of war) plus the current
// room highlighted; the boss room gets a marker as soon as its door has been spotted (see BossRoomRevealed).
void DrawMinimap(void) {
    float cell = MINIMAP_CELL_SIZE, gap = MINIMAP_CELL_GAP;
    float gridW = ROOM_GRID_COLS * cell + (ROOM_GRID_COLS - 1) * gap;
    float gridH = ROOM_GRID_ROWS * cell + (ROOM_GRID_ROWS - 1) * gap;

    float panelW = gridW + MINIMAP_PADDING * 2;
    float panelH = gridH + MINIMAP_PADDING * 2;
    float panelX = (float)WIDTH - panelW - MINIMAP_MARGIN;
    float panelY = (float)HEIGHT - panelH - MINIMAP_MARGIN;

    DrawRectangle((int)panelX, (int)panelY, (int)panelW, (int)panelH, Fade(BLACK, MINIMAP_BG_ALPHA));
    DrawRectangleLines((int)panelX, (int)panelY, (int)panelW, (int)panelH, Fade(WHITE, 0.5f));

    int bossRevealed = BossRoomRevealed();

    for (int r = 0; r < ROOM_GRID_ROWS; r++) {
        for (int c = 0; c < ROOM_GRID_COLS; c++) {
            Room *room = &roomGrid[r][c];
            int isBoss = (room->roomType == ROOM_BOSS);
            int isCurrent = (r == currentRoomRow && c == currentRoomCol);
            float x = panelX + MINIMAP_PADDING + c * (cell + gap);
            float y = panelY + MINIMAP_PADDING + r * (cell + gap);

            if (!room->exists || !room->visited) {
                if (isBoss && bossRevealed) DrawRectangleLines((int)x, (int)y, (int)cell, (int)cell, Fade(BOSS_MARK_COLOR, 0.85f));
                continue;
            }

            Color fill = room->cleared ? Fade(LIGHTGRAY, 0.9f) : Fade(GRAY, 0.9f);
            if (isBoss) fill = Fade(BOSS_MARK_COLOR, 0.9f);
            else if (room->roomType == ROOM_SHOP) fill = Fade(GOLD, 0.9f);

            DrawRectangle((int)x, (int)y, (int)cell, (int)cell, fill);
            if (isCurrent) DrawRectangleLines((int)x - 1, (int)y - 1, (int)cell + 2, (int)cell + 2, YELLOW);
            else           DrawRectangleLines((int)x, (int)y, (int)cell, (int)cell, Fade(BLACK, 0.6f));
        }
    }
}

static int IsBlockingTile(int tileType) {
    return tileType == TILE_WALL_1 || tileType == TILE_WALL_2 || tileType == TILE_WALL_3 || tileType == TILE_DOOR_CLOSED;
}

// Which tile occupies a given world position. Positions outside the room's tile grid (already past a door,
// between rooms) read as open floor - move()'s screen-edge clamp and TryChangeRoom() take over from there.
int TileTypeAt(Room *room, float worldX, float worldY) {
    float tileW = (float)WIDTH / ROOM_TILE_COLS;
    float tileH = (float)HEIGHT / ROOM_TILE_ROWS;
    int tx = (int)floorf(worldX / tileW);
    int ty = (int)floorf(worldY / tileH);
    if (tx < 0 || tx >= ROOM_TILE_COLS || ty < 0 || ty >= ROOM_TILE_ROWS) return TILE_FLOOR;
    return room->tiles[ty][tx];
}

// Samples 3 points along a leading edge instead of just its midpoint - top/mid/bottom of a vertical edge,
// left/mid/right of a horizontal one. The old single-point test could let a sprite graze past a wall corner
// that only its middle happened to miss; this catches that without a full swept-AABB rewrite. The 1px inset
// off each end keeps the sample points on the sprite's actual edge rather than spilling into its neighbor's.
static int EdgeBlocked(Room *room, float edgeX, float edgeY, float halfW, float halfH, int vertical) {
    if (vertical) {
        return IsBlockingTile(TileTypeAt(room, edgeX, edgeY - halfH + 1.0f)) ||
               IsBlockingTile(TileTypeAt(room, edgeX, edgeY)) ||
               IsBlockingTile(TileTypeAt(room, edgeX, edgeY + halfH - 1.0f));
    }
    return IsBlockingTile(TileTypeAt(room, edgeX - halfW + 1.0f, edgeY)) ||
           IsBlockingTile(TileTypeAt(room, edgeX, edgeY)) ||
           IsBlockingTile(TileTypeAt(room, edgeX + halfW - 1.0f, edgeY));
}

// Axis-separated collision: check the sprite's leading edge against the tiles there, undo that axis if blocked.
void ResolveWallCollision(Sprite *s, Room *room) {
    float scale = GetSpriteScale(s->texture);
    float halfW = ((float)s->texture.width * scale) / 2.0f;
    float halfH = ((float)s->texture.height * scale) / 2.0f;
    float prevX = s->x - s->vx;
    float prevY = s->y - s->vy;

    float leadingX = s->x + (s->vx > 0 ? halfW : -halfW);
    if (EdgeBlocked(room, leadingX, prevY, halfW, halfH, 1)) { s->x = prevX; s->vx = 0; }

    float leadingY = s->y + (s->vy > 0 ? halfH : -halfH);
    if (EdgeBlocked(room, s->x, leadingY, halfW, halfH, 0)) { s->y = prevY; s->vy = 0; }
}

// Once every enemy in the room is dead, swap any closed doors open and mark the room cleared.
void UpdateDoors(Room *room) {
    if (room->cleared) return;

    for (int i = 0; i < spriteCount; i++) {
        if (sprites[i].type == ENEMY && sprites[i].active) return; // still enemies left
    }

    for (int ty = 0; ty < ROOM_TILE_ROWS; ty++)
        for (int tx = 0; tx < ROOM_TILE_COLS; tx++)
            if (room->tiles[ty][tx] == TILE_DOOR_CLOSED) room->tiles[ty][tx] = TILE_DOOR_OPEN;

    room->cleared = 1;
    SpawnPopupText("Room Cleared!", sprites[0].x - 40.0f, sprites[0].y - 40.0f); // sprites[0] is always the player
}

int AllRoomsCleared(void) {
    for (int r = 0; r < ROOM_GRID_ROWS; r++)
        for (int c = 0; c < ROOM_GRID_COLS; c++)
            if (roomGrid[r][c].exists && !roomGrid[r][c].cleared) return 0;
    return 1;
}

void LoadRoom(GameAssets *assets) {
    for (int i = 0; i < spriteCount; i++) if (sprites[i].type == ENEMY) sprites[i].active = 0; // pickups/pots persist
    CleanUpSprites();

    Room *room = &roomGrid[currentRoomRow][currentRoomCol];

    if (!room->visited) { // spawn a pot only the first time we ever visit this room
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

    int isBossRoom = (room->roomType == ROOM_BOSS);
    if (isBossRoom) SpawnPopupText("Boss Room!", sprites[0].x - 30.0f, sprites[0].y - 40.0f);

    float hpMult = powf(DEPTH_HP_GROWTH, (float)dungeonDepth);
    for (int i = 0; i < room->enemySpawnCount && spriteCount < MAX_SPRITES; i++) {
        Sprite *enemy = &sprites[spriteCount++];
        EnemySpawn spawn = room->enemySpawns[i];
        Texture2D tex = spawn.isBoss ? (spawn.bossAlt ? assets->boss2Frames[0] : assets->boss1Frames[0]) : assets->enemyVariants[spawn.variant];
        *enemy = (Sprite){tex, spawn.pos.x, spawn.pos.y, 0, 0, 0, 0, 1, ENEMY};
        enemy->elite = spawn.elite;
        enemy->variant = spawn.variant;
        enemy->isBoss = spawn.isBoss;
        enemy->bossAlt = spawn.bossAlt;
        enemy->animTimer = ANIM_FRAME_DURATION; // animFrame starts at 0 via the initializer above
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

    // The `.exists` checks are a safety net, not the real gate - a missing neighbor never gets a door,
    // so a wall already stops the player from reaching these edges in practice.
    if (player->x < 0 && currentRoomCol > 0 && roomGrid[currentRoomRow][currentRoomCol - 1].exists) {
        currentRoomCol--; player->x = WIDTH - insetX; return 1;
    }
    if (player->x > WIDTH && currentRoomCol < ROOM_GRID_COLS - 1 && roomGrid[currentRoomRow][currentRoomCol + 1].exists) {
        currentRoomCol++; player->x = insetX; return 1;
    }
    if (player->y < 0 && currentRoomRow > 0 && roomGrid[currentRoomRow - 1][currentRoomCol].exists) {
        currentRoomRow--; player->y = HEIGHT - insetY; return 1;
    }
    if (player->y > HEIGHT && currentRoomRow < ROOM_GRID_ROWS - 1 && roomGrid[currentRoomRow + 1][currentRoomCol].exists) {
        currentRoomRow++; player->y = insetY; return 1;
    }
    return 0;
}
