#ifndef UI_H
#define UI_H

#include "game_types.h"

void SpawnPopupText(const char *text, float x, float y);
void UpdatePopupTexts(void);
void DrawPopupTexts(void);

void DrawSprite(Sprite s);
void DrawHealthBar(Sprite s);
void DrawHpBar(Sprite *player);
void DrawExpBar(Sprite *player);
void DrawDebugPanel(Sprite *player); // TAB-toggled panel of the player's current stats, for testing upgrades

#endif // UI_H
