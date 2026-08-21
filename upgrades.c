#include <math.h>
#include <stdlib.h>
#include "upgrades.h"
#include "ui.h" // SpawnPopupText

int pendingLevelUps = 0;
int awaitingUpgradeChoice = 0;
static int upgradeChoices[3];

const char *UpgradeName(int upgradeType) {
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:   return "+Move Speed";
        case UPGRADE_DAMAGE:       return "+Damage";
        case UPGRADE_DASH_TIME:    return "+Dash Time";
        case UPGRADE_IFRAMES:      return "+Iframes";
        case UPGRADE_DODGE_CHANCE: return "+Dodge Chance";
        case UPGRADE_DASH_RADIUS:  return "+Dash Radius";
        case UPGRADE_HEAL:         return "+Heal";
        case UPGRADE_EXPLOSIVE:    return "+Explosive Kills";
        case UPGRADE_CHAIN:        return "+Chain Strike";
        case UPGRADE_BLEED:        return "+Bleed";
        default: return "+???";
    }
}

void ApplyUpgrade(Sprite *player, int upgradeType) {
    Stats *stats = &player->stats;
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:   stats->moveSpeed += UPGRADE_MOVE_SPEED_AMOUNT; break;
        case UPGRADE_DAMAGE:       stats->damage += UPGRADE_DAMAGE_AMOUNT; break;
        case UPGRADE_DASH_TIME:
            stats->dashTime += UPGRADE_DASH_TIME_AMOUNT;
            if (stats->dashTime > DASH_TIME_MAX) stats->dashTime = DASH_TIME_MAX;
            break;
        case UPGRADE_IFRAMES: stats->iframesMax += UPGRADE_IFRAMES_AMOUNT; break;
        case UPGRADE_DODGE_CHANCE:
            stats->dodgeChance += UPGRADE_DODGE_CHANCE_AMOUNT;
            if (stats->dodgeChance > 0.75f) stats->dodgeChance = 0.75f;
            break;
        case UPGRADE_DASH_RADIUS: stats->dashRadius += UPGRADE_DASH_RADIUS_AMOUNT; break;
        case UPGRADE_HEAL:
            player->hp += UPGRADE_HEAL_AMOUNT;
            if (player->hp > player->maxhp) player->hp = player->maxhp;
            break;
        case UPGRADE_EXPLOSIVE: stats->explosiveLevel++; break;
        case UPGRADE_CHAIN:     stats->chainLevel++; break;
        case UPGRADE_BLEED:     stats->bleedLevel++; break;
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
        pendingLevelUps++;
        SpawnPopupText("Level Up!", player->x, player->y - 30.0f);
    }
}

// Fills out[0..count-1] with distinct UpgradeTypes drawn from the full pool - swap-remove over a small local
// pool, same idiom as CleanUpSprites' swap-and-pop.
static void RollUpgradeChoices(int *out, int count) {
    int pool[UPGRADE_TYPE_COUNT];
    for (int i = 0; i < UPGRADE_TYPE_COUNT; i++) pool[i] = i;
    int poolSize = UPGRADE_TYPE_COUNT;

    for (int i = 0; i < count && poolSize > 0; i++) {
        int idx = rand() % poolSize;
        out[i] = pool[idx];
        pool[idx] = pool[--poolSize];
    }
}

void StartUpgradeChoice(void) {
    RollUpgradeChoices(upgradeChoices, 3);
    awaitingUpgradeChoice = 1;
    pendingLevelUps--;
}

void HandleUpgradeChoiceInput(Sprite *player) {
    int picked = -1;
    if (IsKeyPressed(KEY_ONE))   picked = 0;
    if (IsKeyPressed(KEY_TWO))   picked = 1;
    if (IsKeyPressed(KEY_THREE)) picked = 2;
    if (picked < 0) return;

    int upgrade = upgradeChoices[picked];
    ApplyUpgrade(player, upgrade);
    SpawnPopupText(UpgradeName(upgrade), player->x, player->y - 30.0f);
    awaitingUpgradeChoice = 0;
}

// Paused overlay showing the 3 rolled options as icon cards, press 1/2/3 to pick.
void DrawUpgradeChoiceScreen(Sprite *player, GameAssets *assets) {
    (void)player;
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(BLACK, 0.6f));

    const char *title = "LEVEL UP - Choose an upgrade";
    Vector2 titleSize = MeasureTextEx(customFont, title, 28.0f, 1.0f);
    DrawTextEx(customFont, title, (Vector2){ WIDTH / 2.0f - titleSize.x / 2.0f, HEIGHT / 2.0f - 130.0f }, 28.0f, 1.0f, WHITE);

    float cardW = 160.0f, cardH = 180.0f, gap = 20.0f;
    float totalW = cardW * 3 + gap * 2;
    float startX = WIDTH / 2.0f - totalW / 2.0f;
    float cardY = HEIGHT / 2.0f - cardH / 2.0f;

    for (int i = 0; i < 3; i++) {
        float cardX = startX + i * (cardW + gap);
        DrawRectangle((int)cardX, (int)cardY, (int)cardW, (int)cardH, Fade(WHITE, 0.9f));
        DrawRectangleLines((int)cardX, (int)cardY, (int)cardW, (int)cardH, BLACK);

        Texture2D icon = assets->upgradeIcons[upgradeChoices[i]];
        float iconSize = 64.0f;
        float scale = iconSize / (float)icon.width;
        Rectangle srcRec = { 0, 0, (float)icon.width, (float)icon.height };
        Rectangle destRec = { cardX + cardW / 2.0f, cardY + 50.0f, icon.width * scale, icon.height * scale };
        Vector2 origin = { destRec.width / 2.0f, destRec.height / 2.0f };
        DrawTexturePro(icon, srcRec, destRec, origin, 0.0f, WHITE);

        const char *name = UpgradeName(upgradeChoices[i]);
        Vector2 nameSize = MeasureTextEx(customFont, name, 16.0f, 1.0f);
        DrawTextEx(customFont, name, (Vector2){ cardX + cardW / 2.0f - nameSize.x / 2.0f, cardY + 100.0f }, 16.0f, 1.0f, BLACK);

        const char *key = TextFormat("[%d]", i + 1);
        Vector2 keySize = MeasureTextEx(customFont, key, 20.0f, 1.0f);
        DrawTextEx(customFont, key, (Vector2){ cardX + cardW / 2.0f - keySize.x / 2.0f, cardY + cardH - 30.0f }, 20.0f, 1.0f, DARKGRAY);
    }
}
