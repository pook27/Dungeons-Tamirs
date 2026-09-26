#ifndef UPGRADES_H
#define UPGRADES_H

#include "game_types.h"

// GrantExp() only queues level-ups (pendingLevelUps); main() turns each into a paused "pick 1 of 3" screen
// one at a time, so multi-level-up frames don't stack choice screens on top of each other.
extern int pendingLevelUps;
extern int awaitingUpgradeChoice;

const char *UpgradeName(int upgradeType);
const char *UpgradeDescription(int upgradeType, Stats *stats); // "current -> next" text for the choice screen
int UpgradeRarity(int upgradeType); // RARITY_* for this upgrade - see upgradeRarity[] in upgrades.c
void ApplyUpgrade(Sprite *player, int upgradeType);
int ExpNeededForLevel(int level);
void GrantExp(Sprite *player, int amount);

// Fills out[0..count-1] with distinct UpgradeTypes, weighted by rarity - shared by level-up choices and the
// shop room's table (both draw from the same rarity-weighted pool, see upgrades.md).
void RollUpgradeChoices(int *out, int count);

void StartUpgradeChoice(void);                 // pops one queued level-up into an active choice screen
void HandleUpgradeChoiceInput(Sprite *player);  // 1/2/3 picks an option while a choice screen is up
void DrawUpgradeChoiceScreen(Sprite *player, GameAssets *assets);

#endif // UPGRADES_H