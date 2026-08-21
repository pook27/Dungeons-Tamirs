#include <math.h>
#include <stdio.h>
#include "ui.h"
#include "assets.h"   // GetSpriteScale
#include "entities.h" // sprites[0] - the player's position, used to aim a charger's windup rotation
#include "world.h"    // currentRoomRow/currentRoomCol/dungeonDepth - debug panel only
#include "upgrades.h" // ExpNeededForLevel

// Fixed pool of floating labels - pickups, level-ups, and room-cleared all reuse this one mechanism.
typedef struct { char text[24]; float x, y; int timer; } PopupText;
static PopupText popupTexts[MAX_POPUP_TEXTS];

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

void DrawSprite(Sprite s) {
    Texture2D tex = s.texture;

    // Chargers face their locked-in target during the windup/charge instead of their (near-zero) velocity.
    float rotation;
    if (s.type == ENEMY && s.aiState == AI_WINDUP) {
        float dx = sprites[0].x - s.x, dy = sprites[0].y - s.y;
        rotation = atan2f(dy, dx) * RAD2DEG;
    } else if (s.type == ENEMY && s.aiState == AI_CHARGE) {
        rotation = atan2f(s.chargeDirY, s.chargeDirX) * RAD2DEG;
    } else {
        rotation = atan2f(s.vy, s.vx) * RAD2DEG;
    }
    Vector2 position = { s.x, s.y };

    // Windup gets a slow pulse in scale on top of the normal size, so a charger visibly "loads up".
    float chargeScale = 1.0f;
    if (s.type == ENEMY && s.aiState == AI_WINDUP) {
        chargeScale = 1.0f + 0.12f * (0.5f + 0.5f * sinf((float)GetTime() * 18.0f));
    }

    float scale = GetSpriteScale(tex) * s.sizeMult * chargeScale;
    float destW = (float)tex.width * scale;
    float destH = (float)tex.height * scale;
    Vector2 origin = { destW / 2.0f, destH / 2.0f };

    Rectangle sourceRec = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle destRec = { position.x, position.y, destW, destH };

    Color tint = WHITE;
    if (s.type == ENEMY && s.elite) tint = GOLD;
    if (s.type == ENEMY && s.bleedTimer > 0) tint = (Color){ 110, 190, 90, 255 };
    if (s.type == ENEMY && s.aiState == AI_WINDUP && ((s.aiTimer / 4) % 2 == 0)) tint = RED; // fast red flicker telegraph
    if (s.iframes > 0) tint = Fade(tint, 0.4f);

    DrawTexturePro(tex, sourceRec, destRec, origin, rotation, tint);
}

// Small bar above a sprite's head, only shown once it has taken damage.
void DrawHealthBar(Sprite s) {
    float scale = GetSpriteScale(s.texture) * s.sizeMult;
    float destH = (float)s.texture.height * scale;

    float barWidth = 40.0f, barHeight = 5.0f;
    float x = s.x - barWidth / 2.0f;
    float y = s.y - destH / 2.0f - 10.0f;

    float pct = (float)s.hp / (float)s.maxhp;
    if (pct < 0.0f) pct = 0.0f;

    DrawRectangle(x, y, barWidth, barHeight, GRAY);
    DrawRectangle(x, y, barWidth * pct, barHeight, RED);
    DrawRectangleLines(x, y, barWidth, barHeight, BLACK);
}

// Shared bar-and-label widget behind both the HP and EXP bars below.
static void DrawStatBar(float x, float y, float pct, Color fillColor, const char *label) {
    float barWidth = 200.0f, barHeight = 16.0f;
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
    DrawTextEx(customFont, TextFormat("Explosive Lv: %d  Chain Lv: %d  Bleed Lv: %d", player->stats.explosiveLevel, player->stats.chainLevel, player->stats.bleedLevel), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    y += lineHeight / 2;
    DrawTextEx(customFont, TextFormat("Room: (%d, %d)", currentRoomRow, currentRoomCol), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawTextEx(customFont, TextFormat("Depth: %d", dungeonDepth), (Vector2){ 10, y }, 16.0f, 1.0f, WHITE); y += lineHeight;
    DrawFPS(10, HEIGHT - 20);
}
