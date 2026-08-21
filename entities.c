#include <math.h>
#include <stdlib.h>
#include "entities.h"
#include "assets.h"   // GetSpriteScale, used by ContactRadius
#include "world.h"    // currentRoomRow/currentRoomCol - which room's sprites are "live" this frame
#include "upgrades.h" // ApplyUpgrade, UpgradeName, GrantExp
#include "ui.h"       // SpawnPopupText

Sprite sprites[MAX_SPRITES];
int spriteCount = 0;

int hitStopTimer = 0;
int screenShakeTimer = 0;
int hitFlashTimer = 0;

// Per-variant multipliers layered on top of the base ENEMY_MAX_SPEED/ENEMY_MAX_HP/CONTACT_DAMAGE constants.
float EnemySpeedMult(int variant) {
    switch (variant) {
        case ENEMY_FAST: return ENEMY_FAST_SPEED_MULT;
        case ENEMY_TANK: return ENEMY_TANK_SPEED_MULT;
        default: return 1.0f;
    }
}

float EnemyHpMult(int variant) {
    switch (variant) {
        case ENEMY_FAST: return ENEMY_FAST_HP_MULT;
        case ENEMY_TANK: return ENEMY_TANK_HP_MULT;
        default: return 1.0f;
    }
}

static float EnemyDamageMult(int variant) {
    switch (variant) {
        case ENEMY_FAST: return ENEMY_FAST_DAMAGE_MULT;
        case ENEMY_TANK: return ENEMY_TANK_DAMAGE_MULT;
        default: return 1.0f;
    }
}

// Chaser (normal): constant homing.
static void UpdateChaserAI(Sprite *s) {
    float dx = sprites[0].x - s->x, dy = sprites[0].y - s->y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 0.01f) {
        s->ax = dx / len * ENEMY_CHASE_ACCEL * EnemySpeedMult(s->variant);
        s->ay = dy / len * ENEMY_CHASE_ACCEL * EnemySpeedMult(s->variant);
    }
}

// Weaver (fast/ULTRA): homes in like a chaser, but the approach vector gets a sine-driven sideways pull,
// so it snakes rather than beelines.
static void UpdateWeaverAI(Sprite *s) {
    float dx = sprites[0].x - s->x, dy = sprites[0].y - s->y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len <= 0.01f) return;

    float dirX = dx / len, dirY = dy / len;
    float perpX = -dirY, perpY = dirX;

    s->weavePhase += WEAVE_FREQUENCY;
    float weave = sinf(s->weavePhase) * WEAVE_AMPLITUDE;

    float speedMult = EnemySpeedMult(s->variant);
    s->ax = (dirX + perpX * weave) * ENEMY_CHASE_ACCEL * speedMult;
    s->ay = (dirY + perpY * weave) * ENEMY_CHASE_ACCEL * speedMult;
}

// Charger (tank, and boss reusing the pattern): drift into range, wind up, commit to a straight dash at
// wherever the player was when the windup ended, then recover slow and open.
static void UpdateChargerAI(Sprite *s, float range, int windupDuration, float chargeSpeed, int chargeDuration, int recoverDuration) {
    float dx = sprites[0].x - s->x, dy = sprites[0].y - s->y;
    float dist = sqrtf(dx * dx + dy * dy);

    switch (s->aiState) {
        default:
        case AI_CHASE:
            if (dist > 0.01f) {
                s->ax = dx / dist * ENEMY_CHASE_ACCEL * EnemySpeedMult(s->variant);
                s->ay = dy / dist * ENEMY_CHASE_ACCEL * EnemySpeedMult(s->variant);
            }
            if (dist < range) { s->aiState = AI_WINDUP; s->aiTimer = windupDuration; }
            break;

        case AI_WINDUP:
            s->ax = 0.0f; s->ay = 0.0f;
            s->vx *= 0.8f; s->vy *= 0.8f;
            if (--s->aiTimer <= 0) {
                float len = dist > 0.01f ? dist : 1.0f;
                s->chargeDirX = dx / len;
                s->chargeDirY = dy / len;
                s->aiState = AI_CHARGE;
                s->aiTimer = chargeDuration;
            }
            break;

        case AI_CHARGE:
            s->ax = 0.0f; s->ay = 0.0f;
            s->vx = s->chargeDirX * chargeSpeed;
            s->vy = s->chargeDirY * chargeSpeed;
            if (--s->aiTimer <= 0) { s->aiState = AI_RECOVER; s->aiTimer = recoverDuration; }
            break;

        case AI_RECOVER:
            s->ax = 0.0f; s->ay = 0.0f;
            s->vx *= 0.9f; s->vy *= 0.9f;
            if (--s->aiTimer <= 0) s->aiState = AI_CHASE;
            break;
    }
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

        float moveSpeed = sqrtf(s->vx * s->vx + s->vy * s->vy);
        if (moveSpeed > s->stats.moveSpeed) { // clamp overall speed, not per-axis, so diagonals aren't faster
            s->vx = s->vx / moveSpeed * s->stats.moveSpeed;
            s->vy = s->vy / moveSpeed * s->stats.moveSpeed;
        }
    }

    if (IsKeyPressed(KEY_SPACE) && s->dashTimer <= 0) {
        float dashDir = sqrtf(s->vx * s->vx + s->vy * s->vy);
        if (dashDir > 0.01f) {
            s->vx = s->vx / dashDir * DASH_SPEED;
            s->vy = s->vy / dashDir * DASH_SPEED;
            s->dashTimer = s->stats.dashTime;
        }
    }

    if (s->x < 0) { s->x = 0; s->vx = 0; }
    if (s->x > WIDTH) { s->x = WIDTH; s->vx = 0; }
    if (s->y < 0) { s->y = 0; s->vy = 0; }
    if (s->y > HEIGHT) { s->y = HEIGHT; s->vy = 0; }
}

// Approximate circle-collision radius: half of each sprite's texture width.
static float ContactRadius(Sprite *a, Sprite *b) {
    float aScaledW = (float)a->texture.width * GetSpriteScale(a->texture) * a->sizeMult;
    float bScaledW = (float)b->texture.width * GetSpriteScale(b->texture) * b->sizeMult;
    return (aScaledW + bScaledW) / 4.0f;
}

// Squared-distance radius check - avoids a sqrtf per pair per frame.
static int WithinRadius(Sprite *a, Sprite *b, float radius) {
    float dx = a->x - b->x, dy = a->y - b->y;
    return dx * dx + dy * dy < radius * radius;
}

static int IsActiveInRoom(Sprite *s, int type) {
    return s->type == type && s->active && s->roomRow == currentRoomRow && s->roomCol == currentRoomCol;
}

static void SpawnPickup(GameAssets *assets, float x, float y) {
    if (spriteCount >= MAX_SPRITES) return;
    int upgradeType = rand() % UPGRADE_TYPE_COUNT;
    Sprite *pickup = &sprites[spriteCount++];
    *pickup = (Sprite){assets->upgradeIcons[upgradeType], x, y, 0, 0, 0, 0, 1, PICKUP};
    pickup->upgradeType = upgradeType;
    pickup->sizeMult = 1.0f;
    pickup->roomRow = currentRoomRow;
    pickup->roomCol = currentRoomCol;
}

static void DashHitEnemy(GameAssets *assets, Sprite *player, Sprite *enemy, float fromX, float fromY, int damage); // fwd decl - KillEnemy's explosion follow-up needs this first

// Shared kill payout - exp, elite pickup drop, and any explosive follow-up. Used by a direct dash kill, an
// explosion kill, a chain kill, and a bleed tick finishing an enemy off, so all four behave identically.
static void KillEnemy(GameAssets *assets, Sprite *player, Sprite *enemy) {
    enemy->active = 0;
    hitFlashTimer = HIT_FLASH_DURATION;
    GrantExp(player, EXP_PER_KILL);
    if (enemy->elite && (rand() % 100 < ELITE_DROP_CHANCE)) SpawnPickup(assets, enemy->x, enemy->y);

    if (player->stats.explosiveLevel <= 0) return;
    int blastDamage = player->stats.explosiveLevel * EXPLOSIVE_DAMAGE_PER_LEVEL;
    for (int i = 0; i < spriteCount; i++) {
        Sprite *other = &sprites[i];
        if (other == enemy || !IsActiveInRoom(other, ENEMY)) continue;
        // Recurses into KillEnemy if this finishes `other` off too - safe: every enemy can only die once
        // (active gates it out of IsActiveInRoom after), so the chain is a DAG bounded by room size, never a loop.
        if (WithinRadius(enemy, other, EXPLOSIVE_RADIUS)) DashHitEnemy(assets, player, other, enemy->x, enemy->y, blastDamage);
    }
}

// Applies one hit's worth of damage/knockback/stagger/bleed to `enemy`, as if struck from (fromX, fromY).
// Shared by the player's direct dash hit, chain jumps, and explosion follow-ups.
static void DashHitEnemy(GameAssets *assets, Sprite *player, Sprite *enemy, float fromX, float fromY, int damage) {
    enemy->hp -= damage;
    enemy->iframes = IFRAMES_DURATION;

    float dx = enemy->x - fromX, dy = enemy->y - fromY;
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 0.01f) {
        enemy->vx = (dx / len) * DASH_KNOCKBACK_SPEED;
        enemy->vy = (dy / len) * DASH_KNOCKBACK_SPEED;
    }
    enemy->staggerTimer = ENEMY_STAGGER_DURATION;
    if (enemy->aiState == AI_WINDUP || enemy->aiState == AI_CHARGE) {
        enemy->aiState = AI_RECOVER;
        enemy->aiTimer = enemy->isBoss ? BOSS_RECOVER_DURATION : TANK_RECOVER_DURATION;
    }
    if (player->stats.bleedLevel > 0) enemy->bleedTimer = player->stats.bleedLevel * BLEED_DURATION_PER_LEVEL;

    if (enemy->hp <= 0) KillEnemy(assets, player, enemy);
}

// Chain upgrade: after a direct hit on `enemy`, jump to up to chainLevel of the nearest other eligible enemies
// in range - one jump per level, not recursive (unlike explosions), so it stays a fixed, predictable cost.
static void ApplyChainStrikes(GameAssets *assets, Sprite *player, Sprite *enemy) {
    for (int jump = 0; jump < player->stats.chainLevel; jump++) {
        Sprite *nearest = NULL;
        float nearestDistSq = CHAIN_RADIUS * CHAIN_RADIUS;
        for (int i = 0; i < spriteCount; i++) {
            Sprite *other = &sprites[i];
            if (other == enemy || !IsActiveInRoom(other, ENEMY) || other->iframes > 0) continue;
            float dx = other->x - enemy->x, dy = other->y - enemy->y;
            float distSq = dx * dx + dy * dy;
            if (distSq < nearestDistSq) { nearestDistSq = distSq; nearest = other; }
        }
        if (!nearest) break;
        DashHitEnemy(assets, player, nearest, enemy->x, enemy->y, DASH_DAMAGE + player->stats.damage);
    }
}

void Update(GameAssets *assets) {
    for (int i = 0; i < spriteCount; i++) {
        Sprite *s = &sprites[i];
        if (!s->active) continue;
        if (s->type != PLAYER && (s->roomRow != currentRoomRow || s->roomCol != currentRoomCol)) continue;

        if (s->type == ENEMY) {
            if (s->staggerTimer > 0) {
                s->staggerTimer--;
                s->ax = 0; s->ay = 0;
                s->vx *= ENEMY_STAGGER_FRICTION;
                s->vy *= ENEMY_STAGGER_FRICTION;
            } else if (s->isBoss) {
                UpdateChargerAI(s, BOSS_CHARGE_RANGE, BOSS_WINDUP_DURATION, BOSS_CHARGE_SPEED, BOSS_CHARGE_DURATION, BOSS_RECOVER_DURATION);
            } else {
                switch (s->variant) {
                    case ENEMY_FAST: UpdateWeaverAI(s); break;
                    case ENEMY_TANK: UpdateChargerAI(s, TANK_CHARGE_RANGE, TANK_WINDUP_DURATION, TANK_CHARGE_SPEED, TANK_CHARGE_DURATION, TANK_RECOVER_DURATION); break;
                    default:         UpdateChaserAI(s); break;
                }
            }
        }

        s->vx = (s->vx + s->ax) * 0.99f;
        s->vy = (s->vy + s->ay) * 0.99f;

        // Chargers set vx/vy directly for the charge itself, so let that ride uncapped - this clamp is for chase-state steering only.
        if (s->type == ENEMY && s->staggerTimer <= 0 && s->aiState != AI_CHARGE) {
            float speed = sqrtf(s->vx * s->vx + s->vy * s->vy);
            float maxSpeed = ENEMY_MAX_SPEED * EnemySpeedMult(s->variant);
            if (speed > maxSpeed) { s->vx = s->vx / speed * maxSpeed; s->vy = s->vy / speed * maxSpeed; }
        }

        s->x += s->vx;
        s->y += s->vy;

        if (s->iframes > 0) s->iframes--;

        if (s->bleedTimer > 0) {
            s->bleedTimer--;
            if (s->bleedTimer % BLEED_TICK_FRAMES == 0) {
                s->hp -= BLEED_DAMAGE_PER_TICK;
                if (s->hp <= 0 && s->active) KillEnemy(assets, &sprites[0], s); // sprites[0] is always the player
            }
        }
    }

    // Player vs enemy contact: dashing into an enemy hurts them, otherwise it hurts the player.
    for (int i = 0; i < spriteCount; i++) {
        Sprite *player = &sprites[i];
        if (player->type != PLAYER || !player->active) continue;

        for (int j = 0; j < spriteCount; j++) {
            Sprite *enemy = &sprites[j];
            if (!IsActiveInRoom(enemy, ENEMY)) continue;

            if (player->dashTimer > 0) {
                float r = ContactRadius(player, enemy) + player->stats.dashRadius;
                if (WithinRadius(player, enemy, r) && enemy->iframes <= 0) {
                    hitStopTimer = 4;
                    screenShakeTimer = 10;
                    DashHitEnemy(assets, player, enemy, player->x, player->y, DASH_DAMAGE + player->stats.damage);
                    if (player->stats.chainLevel > 0) ApplyChainStrikes(assets, player, enemy);
                }
            } else {
                float r = ContactRadius(player, enemy) + CONTACT_RADIUS_BONUS;
                if (WithinRadius(player, enemy, r) && player->iframes <= 0) {
                    int dodged = ((float)rand() / (float)RAND_MAX) < player->stats.dodgeChance;
                    if (!dodged) {
                        float dmgMult = enemy->isBoss ? BOSS_CONTACT_DAMAGE_MULT : EnemyDamageMult(enemy->variant);
                        player->hp -= (int)(CONTACT_DAMAGE * dmgMult);
                        player->iframes = player->stats.iframesMax;
                        if (player->hp < 0) player->hp = 0;
                    }
                }
            }
        }
    }

    // Player vs pickup: touching one applies its upgrade immediately, no menu.
    for (int i = 0; i < spriteCount; i++) {
        Sprite *player = &sprites[i];
        if (player->type != PLAYER || !player->active) continue;

        for (int j = 0; j < spriteCount; j++) {
            Sprite *pickup = &sprites[j];
            if (!IsActiveInRoom(pickup, PICKUP)) continue;

            if (WithinRadius(player, pickup, PICKUP_RADIUS)) {
                ApplyUpgrade(player, pickup->upgradeType);
                SpawnPopupText(UpgradeName(pickup->upgradeType), player->x, player->y - 30.0f);
                pickup->active = 0;
            }
        }
    }

    // Player vs pot: same shape as the pickup loop above, breaks on contact and heals.
    for (int i = 0; i < spriteCount; i++) {
        Sprite *player = &sprites[i];
        if (player->type != PLAYER || !player->active) continue;

        for (int j = 0; j < spriteCount; j++) {
            Sprite *pot = &sprites[j];
            if (!IsActiveInRoom(pot, POT)) continue;

            if (WithinRadius(player, pot, PICKUP_RADIUS)) {
                player->hp += POT_HEAL_AMOUNT;
                if (player->hp > player->maxhp) player->hp = player->maxhp;
                SpawnPopupText("+Heal", player->x, player->y - 30.0f);
                pot->active = 0;
            }
        }
    }
}

// Swap-and-pop compaction over the flat sprite array. No free() needed - despawned sprites are just plain
// structs getting overwritten.
void CleanUpSprites(void) {
    for (int i = 0; i < spriteCount; ) {
        if (sprites[i].active == 0) {
            sprites[i] = sprites[spriteCount - 1];
            spriteCount--;
        } else {
            i++;
        }
    }
}
