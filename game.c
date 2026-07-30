#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "raylib.h"

#define HEIGHT 600
#define WIDTH 900

#define MOVE_ACCEL 0.8f     // how fast speed ramps up while a direction is held
#define MAX_MOVE_SPEED 6.0f // speed cap during normal movement
#define FRICTION 0.75f      // multiplier applied to vx/vy when that axis has no input
#define DASH_SPEED 20.0f    // fixed speed set on dash, in whatever direction you're currently moving
#define DASH_TIME 10        // frames the dash holds at full speed before normal movement resumes (~0.16s at 60fps)

static int s_counter = 0;

enum SpriteType {
    PLAYER,
    ENEMY
};

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
} Sprite;

void DrawSprite(Sprite s) {
    Texture2D tex = s.texture;

    float rotation = atan2f(s.vy , s.vx) * RAD2DEG;
    Vector2 position = { s.x, s.y };

    Vector2 origin = { (float)tex.width / 2.0f, (float)tex.height / 2.0f }; 

    Rectangle sourceRec = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle destRec   = { position.x, position.y, (float)tex.width, (float)tex.height };

    DrawTexturePro(tex, sourceRec, destRec, origin, rotation, WHITE);
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
        if (moveSpeed > MAX_MOVE_SPEED) {
            s->vx = s->vx / moveSpeed * MAX_MOVE_SPEED;
            s->vy = s->vy / moveSpeed * MAX_MOVE_SPEED;
        }
    }

    // Dash: override velocity to a fixed DASH_SPEED along the direction you're
    // already moving (normalized, not multiplied - a dash from a crawl and a
    // dash from a sprint should travel the same distance). Can't be retriggered
    // until the current dash finishes.
    if (IsKeyPressed(KEY_SPACE) && s->dashTimer <= 0) {
        float dashDir = sqrtf(s->vx * s->vx + s->vy * s->vy);
        if (dashDir > 0.01f) {
            s->vx = s->vx / dashDir * DASH_SPEED;
            s->vy = s->vy / dashDir * DASH_SPEED;
            s->dashTimer = DASH_TIME;
        }
    }

    // Keep player on screen
    if (s->x < 0) { s->x = 0; s->vx = 0; }
    if (s->x > WIDTH) { s->x = WIDTH; s->vx = 0; }
    if (s->y < 0) { s->y = 0; s->vy = 0; }
    if (s->y > HEIGHT) { s->y = HEIGHT; s->vy = 0; }
}

void Update(Sprite** sprites_arr) {
    for (int i = 0; i < s_counter; i++) {
        if (!sprites_arr[i]->active) continue;

        //do physics
        sprites_arr[i]->vx = (sprites_arr[i]->vx + sprites_arr[i]->ax) * 0.99f;
        sprites_arr[i]->vy = (sprites_arr[i]->vy + sprites_arr[i]->ay) * 0.99f;
        sprites_arr[i]->x += sprites_arr[i]->vx;
        sprites_arr[i]->y += sprites_arr[i]->vy;
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
            free(sprites_arr[i]); // Free the memory
            
            // Shift all remaining sprites down one slot
            for (int j = i; j < s_counter - 1; j++) {
                sprites_arr[j] = sprites_arr[j + 1];
            }
            s_counter--; // Off by one
        } else {
            i++; // Only move to next index if we didn't delete anything
        }
    }
}

int main() {
    srand(time(NULL));
    Sprite *sprites[256];

    InitWindow(WIDTH, HEIGHT, "Game");
    SetTargetFPS(60);

    // 1. Load all textures ONCE at the start of the game
    Texture2D birdtex = LoadTexture("assets/bird.png");
    Texture2D stickmantex = LoadTexture("assets/stickman.png");

    // 2. Assign the loaded textures to the structs
    Sprite *bird = malloc(sizeof(Sprite));
    *bird = (Sprite){birdtex, WIDTH / 2.0f, HEIGHT / 2.0f, 0, 0, 0, 0, 1, PLAYER};
    sprites[s_counter++] = bird;

    Sprite *stickman = malloc(sizeof(Sprite));
    *stickman = (Sprite){stickmantex, 700, rand()%HEIGHT, 0, 0, 0, 0, 1, ENEMY};
    sprites[s_counter++] = stickman;

    while(!WindowShouldClose()) {
        Update(sprites);
        CleanUpSprites(sprites);
        move(bird);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        //drawing the sprites
        for (int i =0; i<s_counter; i++) {
            DrawSprite(*sprites[i]);
        }
        DrawFPS(10, 20);
        EndDrawing();
    }

    FreeSprites(sprites);

    UnloadTexture(birdtex);
    UnloadTexture(stickmantex);

    CloseWindow();

    return 0;
}
