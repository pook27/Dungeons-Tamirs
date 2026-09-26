#include "assets.h"

GameAssets LoadGameAssets(void) {
    GameAssets assets = {0};

    assets.player = LoadTexture("assets/sheshbesh.png");

    assets.enemyVariants[ENEMY_NORMAL] = LoadTexture("assets/tamir.png");
    assets.enemyVariants[ENEMY_FAST]   = LoadTexture("assets/ULTRA.png");
    assets.enemyVariants[ENEMY_TANK]   = LoadTexture("assets/horny.png");

    for (int i = 0; i < BOSS1_FRAME_COUNT; i++) {
        assets.boss1Frames[i] = LoadTexture(TextFormat("assets/Boss1/boss%d.png", i + 1));
    }
    for (int i = 0; i < BOSS2_FRAME_COUNT; i++) {
        assets.boss2Frames[i] = LoadTexture(TextFormat("assets/Boss2/%d.png", i + 1));
    }
    for (int i = 0; i < SHOPKEEP_FRAME_COUNT; i++) {
        assets.shopkeepFrames[i] = LoadTexture(TextFormat("assets/ShopMan/%d.png", i + 1));
    }
    assets.pot        = LoadTexture("assets/pot.png");
    assets.coin        = LoadTexture("assets/coin.png");
    assets.table       = LoadTexture("assets/table.png");
    assets.aura        = LoadTexture("assets/aura.png");
    assets.explosion   = LoadTexture("assets/explosion.png");
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
    assets.upgradeIcons[UPGRADE_EXPLOSIVE]    = LoadTexture("assets/dynamite.png");
    assets.upgradeIcons[UPGRADE_CHAIN]        = LoadTexture("assets/NRG_Ball.png");
    assets.upgradeIcons[UPGRADE_BLEED]        = LoadTexture("assets/bleed.png");

    return assets;
}

void UnloadGameAssets(GameAssets *assets) {
    UnloadTexture(assets->player);
    for (int i = 0; i < ENEMY_VARIANT_COUNT; i++) UnloadTexture(assets->enemyVariants[i]);
    for (int i = 0; i < BOSS1_FRAME_COUNT; i++) UnloadTexture(assets->boss1Frames[i]);
    for (int i = 0; i < BOSS2_FRAME_COUNT; i++) UnloadTexture(assets->boss2Frames[i]);
    for (int i = 0; i < SHOPKEEP_FRAME_COUNT; i++) UnloadTexture(assets->shopkeepFrames[i]);
    UnloadTexture(assets->pot);
    UnloadTexture(assets->coin);
    UnloadTexture(assets->table);
    UnloadTexture(assets->aura);
    UnloadTexture(assets->explosion);
    UnloadTexture(assets->background);
    for (int i = 0; i < 3; i++) UnloadTexture(assets->walls[i]);
    UnloadTexture(assets->doorOpen);
    UnloadTexture(assets->doorClosed);
    for (int i = 0; i < UPGRADE_TYPE_COUNT; i++) UnloadTexture(assets->upgradeIcons[i]);
}

float GetSpriteScale(Texture2D tex) {
    float targetW = (float)GetScreenWidth() / ROOM_TILE_COLS;
    return targetW / (float)tex.width;
}
