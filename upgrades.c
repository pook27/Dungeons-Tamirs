#include <math.h>
#include <stdlib.h>
#include "upgrades.h"
#include "ui.h" // SpawnPopupText

int pendingLevelUps = 0;
int awaitingUpgradeChoice = 0;
static int upgradeChoices[3];

// Which rarity tier each upgrade rolls at - the plain stat bumps are common, the mechanic-changing ones
// (Explosive/Chain/Bleed) are rare so finding one still feels like an event even at 10 upgrade types.
static const int upgradeRarity[UPGRADE_TYPE_COUNT] = {
    [UPGRADE_MOVE_SPEED]   = RARITY_COMMON,
    [UPGRADE_DAMAGE]       = RARITY_COMMON,
    [UPGRADE_IFRAMES]      = RARITY_COMMON,
    [UPGRADE_HEAL]         = RARITY_COMMON,
    [UPGRADE_DASH_TIME]    = RARITY_UNCOMMON,
    [UPGRADE_DODGE_CHANCE] = RARITY_UNCOMMON,
    [UPGRADE_DASH_RADIUS]  = RARITY_UNCOMMON,
    [UPGRADE_EXPLOSIVE]    = RARITY_RARE,
    [UPGRADE_CHAIN]        = RARITY_RARE,
    [UPGRADE_BLEED]        = RARITY_RARE,
};

static int RarityWeight(int rarity) {
    switch (rarity) {
        case RARITY_UNCOMMON: return RARITY_WEIGHT_UNCOMMON;
        case RARITY_RARE:     return RARITY_WEIGHT_RARE;
        default:               return RARITY_WEIGHT_COMMON;
    }
}

static Color RarityColor(int rarity) {
    switch (rarity) {
        case RARITY_UNCOMMON: return (Color){ 70, 140, 230, 255 };  // blue
        case RARITY_RARE:     return (Color){ 200, 90, 230, 255 };  // purple
        default:               return (Color){ 140, 140, 140, 255 }; // grey
    }
}

static const char *RarityLabel(int rarity) {
    switch (rarity) {
        case RARITY_UNCOMMON: return "UNCOMMON";
        case RARITY_RARE:     return "RARE";
        default:               return "COMMON";
    }
}

const char *UpgradeName(int upgradeType) {
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:   return "Run! Quickly!";
        case UPGRADE_DAMAGE:       return "Seismic Rage";
        case UPGRADE_DASH_TIME:    return "Leap of Faith";
        case UPGRADE_IFRAMES:      return "Kinda Cheating";
        case UPGRADE_DODGE_CHANCE: return "Can't Touch Me";
        case UPGRADE_DASH_RADIUS:  return "Bigger Than You";
        case UPGRADE_HEAL:         return "I'm Good, Thanks...";
        case UPGRADE_EXPLOSIVE:    return "Bombs Are Fun";
        case UPGRADE_CHAIN:        return "Who Hit Me!?";
        case UPGRADE_BLEED:        return "You Knicked Me!";
        default: return "???";
    }
}

// Concrete "current -> next" numbers for whichever upgrade this is, given the player's stats right now -
// same idea as Risk of Rain 2's stacking tooltips, just computed from our own Stats fields instead of a
// generic item-stack system. Every branch here mirrors the matching case in ApplyUpgrade exactly, so if the
// two ever drift the numbers shown stop matching what picking the card actually does - keep them in sync.
const char *UpgradeDescription(int upgradeType, Stats *stats) {
    switch (upgradeType) {
        case UPGRADE_MOVE_SPEED:
            return TextFormat("+%.1f move speed\n(%.1f -> %.1f)",
                UPGRADE_MOVE_SPEED_AMOUNT, stats->moveSpeed, stats->moveSpeed + UPGRADE_MOVE_SPEED_AMOUNT);

        case UPGRADE_DAMAGE: {
            int cur = DASH_DAMAGE + stats->damage;
            return TextFormat("+%d dash damage\n(%d -> %d per hit)", UPGRADE_DAMAGE_AMOUNT, cur, cur + UPGRADE_DAMAGE_AMOUNT);
        }

        case UPGRADE_DASH_TIME: {
            int next = stats->dashTime + UPGRADE_DASH_TIME_AMOUNT;
            if (next > DASH_TIME_MAX) next = DASH_TIME_MAX;
            return TextFormat("+%d dash duration\n(%d -> %d frames, cap %d)", UPGRADE_DASH_TIME_AMOUNT, stats->dashTime, next, DASH_TIME_MAX);
        }

        case UPGRADE_IFRAMES:
            return TextFormat("+%d iframes after a hit\n(%d -> %d frames)",
                UPGRADE_IFRAMES_AMOUNT, stats->iframesMax, stats->iframesMax + UPGRADE_IFRAMES_AMOUNT);

        case UPGRADE_DODGE_CHANCE: {
            float next = stats->dodgeChance + UPGRADE_DODGE_CHANCE_AMOUNT;
            if (next > 0.75f) next = 0.75f;
            return TextFormat("+%.0f%% dodge chance\n(%.0f%% -> %.0f%%, cap 75%%)",
                UPGRADE_DODGE_CHANCE_AMOUNT * 100.0f, stats->dodgeChance * 100.0f, next * 100.0f);
        }

        case UPGRADE_DASH_RADIUS:
            return TextFormat("+%.0f dash hit radius\n(+%.0f -> +%.0f)",
                UPGRADE_DASH_RADIUS_AMOUNT, stats->dashRadius, stats->dashRadius + UPGRADE_DASH_RADIUS_AMOUNT);

        case UPGRADE_HEAL:
            return TextFormat("Heal %d HP now\n(instant, doesn't stack)", UPGRADE_HEAL_AMOUNT);

        case UPGRADE_EXPLOSIVE: {
            int lvl = stats->explosiveLevel;
            return TextFormat("Dash kills explode\n%d -> %d dmg, r%d\n(chains into the blast)",
                lvl * EXPLOSIVE_DAMAGE_PER_LEVEL, (lvl + 1) * EXPLOSIVE_DAMAGE_PER_LEVEL, EXPLOSIVE_RADIUS);
        }

        case UPGRADE_CHAIN: {
            int lvl = stats->chainLevel;
            return TextFormat("Dash hits also jump\nto %d -> %d nearby foe(s)\n(within %dpx)", lvl, lvl + 1, CHAIN_RADIUS);
        }

        case UPGRADE_BLEED: {
            int lvl = stats->bleedLevel;
            int curTicks = (lvl * BLEED_DURATION_PER_LEVEL) / BLEED_TICK_FRAMES;
            int nextTicks = ((lvl + 1) * BLEED_DURATION_PER_LEVEL) / BLEED_TICK_FRAMES;
            return TextFormat("Dash hits apply bleed\n%d -> %d dmg over time\n(~%d -> %d ticks)",
                curTicks * BLEED_DAMAGE_PER_TICK, nextTicks * BLEED_DAMAGE_PER_TICK, curTicks, nextTicks);
        }

        default: return "???";
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

// Fills out[0..count-1] with distinct UpgradeTypes, weighted by rarity so rare upgrades show up less often -
// same swap-remove idiom as CleanUpSprites' swap-and-pop, just picking by weighted roll instead of rand()%poolSize.
static void RollUpgradeChoices(int *out, int count) {
    int pool[UPGRADE_TYPE_COUNT];
    int weight[UPGRADE_TYPE_COUNT];
    int poolSize = UPGRADE_TYPE_COUNT;
    int totalWeight = 0;
    for (int i = 0; i < UPGRADE_TYPE_COUNT; i++) {
        pool[i] = i;
        weight[i] = RarityWeight(upgradeRarity[i]);
        totalWeight += weight[i];
    }

    for (int i = 0; i < count && poolSize > 0; i++) {
        int roll = rand() % totalWeight;
        int idx = 0;
        int running = weight[0];
        while (running <= roll) running += weight[++idx];

        out[i] = pool[idx];
        totalWeight -= weight[idx];
        poolSize--;
        pool[idx] = pool[poolSize];
        weight[idx] = weight[poolSize];
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
    DrawRectangle(0, 0, WIDTH, HEIGHT, Fade(BLACK, 0.6f));

    const char *title = "LEVEL UP - Choose an upgrade";
    Vector2 titleSize = MeasureTextEx(customFont, title, 26.0f, 1.0f);
    DrawTextEx(customFont, title, (Vector2){ WIDTH / 2.0f - titleSize.x / 2.0f, HEIGHT / 2.0f - 155.0f }, 26.0f, 1.0f, WHITE);

    // Taller than before (was 160x180) - the description text needs the extra room.
    float cardW = 190.0f, cardH = 220.0f, gap = 16.0f;
    float totalW = cardW * 3 + gap * 2;
    float startX = WIDTH / 2.0f - totalW / 2.0f;
    float cardY = HEIGHT / 2.0f - cardH / 2.0f + 15.0f;

    for (int i = 0; i < 3; i++) {
        int upgrade = upgradeChoices[i];
        int rarity = upgradeRarity[upgrade];
        Color rarityColor = RarityColor(rarity);

        float cardX = startX + i * (cardW + gap);
        DrawRectangle((int)cardX, (int)cardY, (int)cardW, (int)cardH, Fade(WHITE, 0.9f));
        DrawRectangleLinesEx((Rectangle){ cardX, cardY, cardW, cardH }, 3.0f, rarityColor);

        const char *rarityLabel = RarityLabel(rarity);
        Vector2 rarityLabelSize = MeasureTextEx(customFont, rarityLabel, 12.0f, 1.0f);
        DrawTextEx(customFont, rarityLabel, (Vector2){ cardX + cardW / 2.0f - rarityLabelSize.x / 2.0f, cardY + 14.0f }, 12.0f, 1.0f, rarityColor);

        // Bounded by the LARGER of width/height, not just width - a tall/narrow icon (e.g. Move Speed's
        // diagonal lines) was previously only constrained horizontally and could balloon past iconSize
        // vertically, bleeding into the rarity label above and the title below.
        Texture2D icon = assets->upgradeIcons[upgrade];
        float iconSize = 46.0f;
        float scale = iconSize / fmaxf((float)icon.width, (float)icon.height);
        Rectangle srcRec = { 0, 0, (float)icon.width, (float)icon.height };
        Rectangle destRec = { cardX + cardW / 2.0f, cardY + 62.0f, icon.width * scale, icon.height * scale };
        Vector2 origin = { destRec.width / 2.0f, destRec.height / 2.0f };
        DrawTexturePro(icon, srcRec, destRec, origin, 0.0f, WHITE);

        const char *name = UpgradeName(upgrade);
        Vector2 nameSize = MeasureTextEx(customFont, name, 16.0f, 1.0f);
        DrawTextEx(customFont, name, (Vector2){ cardX + cardW / 2.0f - nameSize.x / 2.0f, cardY + 98.0f }, 16.0f, 1.0f, BLACK);

        // Multi-line "current -> next" text - UpgradeDescription reads the player's live stats, so it
        // always reflects what THIS pick would actually do, not a fixed generic blurb. Pushed further below
        // the title than before for breathing room, and the whole content block now spans closer to the
        // card's full height instead of clustering in the top half.
        const char *desc = UpgradeDescription(upgrade, &player->stats);
        DrawTextEx(customFont, desc, (Vector2){ cardX + 10.0f, cardY + 134.0f }, 13.0f, 1.0f, DARKGRAY);

        const char *key = TextFormat("[%d]", i + 1);
        Vector2 keySize = MeasureTextEx(customFont, key, 20.0f, 1.0f);
        DrawTextEx(customFont, key, (Vector2){ cardX + cardW / 2.0f - keySize.x / 2.0f, cardY + cardH - 30.0f }, 20.0f, 1.0f, DARKGRAY);
    }
}
