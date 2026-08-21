#ifndef ASSETS_H
#define ASSETS_H

#include "game_types.h"

GameAssets LoadGameAssets(void);
void UnloadGameAssets(GameAssets *assets);

// World-scale factor for a texture: it's drawn at 1 grid tile wide, sizeMult on top for anything oversized (the boss).
float GetSpriteScale(Texture2D tex);

#endif // ASSETS_H
