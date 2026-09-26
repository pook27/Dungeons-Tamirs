#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "raylib.h"

#include "constants.h"
#include "game_types.h"
#include "assets.h"
#include "world.h"
#include "entities.h"
#include "upgrades.h"
#include "ui.h"

Font customFont;

static void ResetGame(GameAssets *assets) {
    spriteCount = 0; // sprites are plain structs now - no per-sprite free() needed

    currentRoomRow = ROOM_GRID_ROWS / 2;
    currentRoomCol = ROOM_GRID_COLS / 2;
    dungeonDepth = 0;
    pendingLevelUps = 0;
    awaitingUpgradeChoice = 0;

    Sprite *player = &sprites[spriteCount++];
    *player = (Sprite){assets->player, WIDTH / 2.0f, HEIGHT / 2.0f, 0, 0, 0, 0, 1, PLAYER};
    player->hp = PLAYER_MAX_HP;
    player->maxhp = PLAYER_MAX_HP;
    player->level = 1;
    player->exp = 0;
    player->coins = 0;
    player->sizeMult = 1.0f;
    player->stats = (Stats){ MAX_MOVE_SPEED, 0, DASH_TIME, IFRAMES_DURATION, 0.0f, CONTACT_RADIUS_BONUS };

    InitRooms();
    LoadRoom(assets);
}

int main(void) {
    srand((unsigned int)time(NULL));
    int showDebugPanel = 0;
    int paused = 0;

    InitWindow(WIDTH, HEIGHT, "Dungeons & Tamirs");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // ESC no longer closes the window by default - it's remapped to pause below

    GameAssets assets = LoadGameAssets(); // load every texture once, up front
    customFont = LoadFont("assets/Good-Game.ttf");

    ResetGame(&assets);
    Sprite *player = &sprites[0]; // sprites[0] is always the player (see CleanUpSprites)

    while (!WindowShouldClose()) {
        if (player->hp <= 0) {
            if (IsKeyPressed(KEY_R)) {
                ResetGame(&assets);
                player = &sprites[0];
            }
        } else if (paused) {
            // Frozen on purpose - no Update(), no timers ticking, no input handling. Only the ESC toggle
            // below still runs every frame regardless of this branch, so unpausing always works.
        } else {
            if (awaitingUpgradeChoice) {
                HandleUpgradeChoiceInput(player);
            } else if (hitStopTimer > 0) {
                hitStopTimer--;
            } else {
                Update(&assets);
                CleanUpSprites();

                Room *room = &roomGrid[currentRoomRow][currentRoomCol];
                UpdateDoors(room);
                ResolveWallCollision(player, room);

                // Enemies never got this before - they could walk (or get dash-knocked) straight through
                // walls since nothing ever checked. Same function the player uses, just looped over enemies.
                for (int i = 0; i < spriteCount; i++) {
                    Sprite *s = &sprites[i];
                    if (s->type == ENEMY && s->active && s->roomRow == currentRoomRow && s->roomCol == currentRoomCol) {
                        ResolveWallCollision(s, room);
                    }
                }

                // Whole floor cleared - descend instead of just running out of game.
                if (AllRoomsCleared()) {
                    dungeonDepth++;
                    SpawnPopupText(TextFormat("Floor: %d", dungeonDepth + 1), player->x - 20.0f, player->y - 40.0f);

                    for (int i = 0; i < spriteCount; i++) if (sprites[i].type != PLAYER) sprites[i].active = 0;
                    CleanUpSprites();

                    InitRooms();
                    currentRoomRow = ROOM_GRID_ROWS / 2;
                    currentRoomCol = ROOM_GRID_COLS / 2;
                    LoadRoom(&assets);
                }

                if (TryChangeRoom(player)) LoadRoom(&assets);
                move(player);
                UpdateShopRoom(&assets, player); // no-op unless the current room is the shop

                // Pop one queued level-up into an active choice screen; the rest wait until this one resolves.
                if (pendingLevelUps > 0) StartUpgradeChoice();
            }
            UpdatePopupTexts();
            if (hitFlashTimer > 0) hitFlashTimer--;
            if (screenShakeTimer > 0) screenShakeTimer--;
        }

        // ESC toggles pause - runs unconditionally (outside the branch above) so it isn't itself gated by
        // `paused`, or unpausing would never fire. Doesn't apply once dead; R/restart owns that screen instead.
        if (player->hp > 0 && IsKeyPressed(KEY_ESCAPE)) paused = !paused;

        // TAB is disabled while paused - the stats panel is already forced on during pause (see the draw
        // section below), so there's nothing for TAB to toggle there, and it stays out of the pause menu's way.
        if (!paused && IsKeyPressed(KEY_TAB)) showDebugPanel = !showDebugPanel;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        Camera2D camera = { 0 };
        camera.target = (Vector2){ 0, 0 };
        camera.zoom = 1.0f;
        camera.rotation = 0.0f;
        camera.offset = (screenShakeTimer > 0)
            ? (Vector2){ (float)((rand() % 11) - 5), (float)((rand() % 11) - 5) } // shake -5..+5px
            : (Vector2){ 0, 0 };

        BeginMode2D(camera);
        DrawTexture(assets.background, 0, 0, WHITE); // native res on purpose - no scaling to WIDTH/HEIGHT
        DrawRoom(&roomGrid[currentRoomRow][currentRoomCol], &assets, currentRoomRow, currentRoomCol);
        DrawShopRoom(&roomGrid[currentRoomRow][currentRoomCol], &assets); // no-op unless the current room is the shop

        for (int i = 0; i < spriteCount; i++) {
            Sprite *s = &sprites[i];
            if (!s->active) continue;
            if (s->type != PLAYER && (s->roomRow != currentRoomRow || s->roomCol != currentRoomCol)) continue;

            if (s->type == PLAYER && s->dashTimer > 0) {
                float powerRatio = fminf((float)s->stats.damage / 20.0f, 1.0f);
                float sizeRatio = fminf(s->stats.dashRadius / 10.0f, 1.0f);

                float baseScale = GetSpriteScale(assets.aura);
                float scaleX = baseScale * (1.0f + (sizeRatio * 1.0f));
                float scaleY = baseScale * (1.0f + (sizeRatio * 3.0f));
                float destW = (float)assets.aura.width * scaleX;
                float destH = (float)assets.aura.height * scaleY;

                // Tint shifts from warm yellow/orange to intense blue/white as power goes up
                unsigned char r = (unsigned char)(255 - (powerRatio * 50));
                unsigned char g = (unsigned char)(200 - (powerRatio * 150));
                unsigned char b = (unsigned char)(powerRatio * 255);
                unsigned char a = (unsigned char)(150 + (powerRatio * 80));
                Color auraTint = { r, g, b, a };

                float rot = atan2f(-s->vy, -s->vx) * RAD2DEG + 90.0f;
                Rectangle srcRec = { 0.0f, 0.0f, (float)assets.aura.width, (float)assets.aura.height };
                Rectangle destRec = { s->x, s->y, destW, destH };
                Vector2 origin = { destW / 2.0f, destH / 2.0f };
                DrawTexturePro(assets.aura, srcRec, destRec, origin, rot, auraTint);
            }

            DrawSprite(*s);
            if (s->type == ENEMY) DrawHealthBar(*s);
        }
        DrawExplosionEffects(&assets); // inside BeginMode2D so bursts shake/shift with everything else
        EndMode2D();

        DrawExpBar(player);
        DrawHpBar(player);
        DrawCoinHud(player, &assets);
        DrawMinimap();
        DrawPopupTexts();
        if (hitFlashTimer > 0) DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(WHITE, 0.5f * hitFlashTimer / HIT_FLASH_DURATION));
        if (awaitingUpgradeChoice) DrawUpgradeChoiceScreen(player, &assets);

        if (player->hp <= 0) {
            DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(BLACK, 0.6f));
            const char *msg = "GAME OVER";
            const char *hint = "Press R to restart";
            Vector2 msgSize = MeasureTextEx(customFont, msg, 48.0f, 1.0f);
            Vector2 hintSize = MeasureTextEx(customFont, hint, 20.0f, 1.0f);
            DrawTextEx(customFont, msg, (Vector2){ WIDTH / 2.0f - msgSize.x / 2.0f, HEIGHT / 2.0f - 40.0f }, 48.0f, 1.0f, RED);
            DrawTextEx(customFont, hint, (Vector2){ WIDTH / 2.0f - hintSize.x / 2.0f, HEIGHT / 2.0f + 20.0f }, 20.0f, 1.0f, WHITE);
        }
        if (paused) DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(BLACK, 0.5f));
        if (showDebugPanel || paused) DrawDebugPanel(player); // forced on during pause, TAB can't hide it there
        if (paused) {
            const char *msg = "PAUSED";
            Vector2 msgSize = MeasureTextEx(customFont, msg, 48.0f, 1.0f);
            DrawTextEx(customFont, msg, (Vector2){ WIDTH / 2.0f - msgSize.x / 2.0f, HEIGHT / 2.0f - 24.0f }, 48.0f, 1.0f, WHITE);
        }
        EndDrawing();
    }

    UnloadGameAssets(&assets);
    CloseWindow();
    return 0;
}
