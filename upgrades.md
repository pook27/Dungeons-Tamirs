# Upgrade System Reference

This documents exactly what each upgrade does, in terms of the actual code (`upgrades.c`,
`entities.c`, `constants.h`). Numbers are current as of this writing — if you retune a
`#define`, the matching number here goes stale, so treat the *mechanism* description as the
source of truth and the numbers as a snapshot.

## How a pickup gets chosen

There are two separate paths into your stats, and as of this pass **they don't behave the
same way**:

- **Level-up (`StartUpgradeChoice` → `DrawUpgradeChoiceScreen`)**: rolls 3 *distinct* upgrades,
  weighted by rarity (see below), you pick one with `1`/`2`/`3`.
- **World pickups** (dropped by elites, `SpawnPickup` in `entities.c`): still
  `rand() % UPGRADE_TYPE_COUNT` — a flat, unweighted roll across all 10 types. Elite drops can
  hand you a rare (Explosive/Chain/Bleed) just as easily as a common one.

That asymmetry isn't a bug, just something worth knowing — it means elite kills are currently
your *most* reliable way to fish for a specific rare, since the level-up screen actively makes
rares less likely. Worth revisiting if that's not the intended balance.

## Rarity (level-up screen only)

| Rarity | Weight | Upgrades |
|---|---|---|
| Common | 10 | Move Speed, Damage, Iframes, Heal |
| Uncommon | 4 | Dash Time, Dodge Chance, Dash Radius |
| Rare | 1 | Explosive, Chain, Bleed |

These are relative weights, not percentages: a common upgrade is **10x as likely** to fill a
given card slot as any one specific rare, and about **2.5x** as likely as any one specific
uncommon. `RollUpgradeChoices` does a weighted pick-without-replacement over these weights, so
the odds shift slightly as each of the 3 cards gets filled in, but the common-heavy skew holds
throughout. The card border/label color (grey/blue/purple) tells you which tier you're looking
at.

## Reading the choice-screen cards

Each card now shows **your current value → what this pick takes it to**, computed live from
your actual `Stats` (`UpgradeDescription` in `upgrades.c`). It's not a static blurb — if you've
already got 2 stacks of something, the card shows stack 2 → 3, not a generic "+X" label. This
is the main thing this pass added: previously you had to already know the underlying numbers to
know what you were picking.

---

## The stat upgrades (straightforward — number goes up)

| Upgrade | Field | Per pick | Cap |
|---|---|---|---|
| Move Speed | `stats.moveSpeed` | +0.5 | none |
| Damage | `stats.damage` (added to `DASH_DAMAGE`) | +1 | none |
| Dash Time | `stats.dashTime` | +2 frames | 40 frames (`DASH_TIME_MAX`) |
| Iframes | `stats.iframesMax` | +5 frames | none |
| Dodge Chance | `stats.dodgeChance` | +5% | 75% |
| Dash Radius | `stats.dashRadius` (added to the dash hitbox) | +8px | none |
| Heal | `player.hp` | +20 HP, immediately | doesn't stack — it's a one-time heal, not a stat |

Nothing subtle here: pick it, the number moves, `ApplyUpgrade`'s switch statement does exactly
what it says. Dash Time and Dodge Chance are capped because — per the original review — dash
time is simultaneously mobility + offense + defense (you're invulnerable and hitting things
*and* covering ground while dashing), and dodge chance heading toward 100% would make contact
damage irrelevant. Both caps are enforced in `ApplyUpgrade` itself, not just on the display.

---

## The mechanic upgrades — Explosive, Chain, Bleed

These three are different in kind from the stat upgrades above: instead of changing a number
that already existed, they turn a dash hit into something that keeps happening after the initial
hit. They're also the three that got flagged Rare, and — this is the part that answers "why
don't I understand these" — **they all run through the same two shared functions**, so once you
understand those two functions, you understand all three upgrades and how they combo together
for free.

### The two functions everything routes through

**`DashHitEnemy(assets, player, enemy, fromX, fromY, damage)`** — applies one hit's worth of
damage + knockback + stagger to `enemy`, as if struck from `(fromX, fromY)`. This is called by:
- your actual dash hit (from is your position)
- each Chain jump (from is the *previous* enemy in the chain, not you)
- each Explosive follow-up (from is the enemy that just died)

Every one of those callers ends up going through the exact same code path, including this line:

```c
if (player->stats.bleedLevel > 0) enemy->bleedTimer = player->stats.bleedLevel * BLEED_DURATION_PER_LEVEL;
```

**Bleed isn't its own separate system that only fires on your direct hit — it's baked into
`DashHitEnemy` itself.** That means Bleed applies to Chain-jumped targets and Explosive-blasted
targets too, automatically, with zero extra code. If you have Bleed + Chain, every enemy your
chain jumps to also gets bled. You didn't have to pick some combo upgrade to unlock that — it's
just what "Bleed modifies the shared hit function" means.

**`KillEnemy(assets, player, enemy)`** — the shared death handler. Grants XP, rolls an elite
pickup drop, and then:

```c
if (player->stats.explosiveLevel <= 0) return;
// ...loop over nearby enemies, calling DashHitEnemy on each one caught in the blast
```

`DashHitEnemy` itself calls `KillEnemy` if the hit brought HP to 0. So the loop is:
`DashHitEnemy → (maybe) KillEnemy → (if Explosive) more DashHitEnemy calls → (maybe) more
KillEnemy calls → ...` This is safe from infinite-looping because an enemy's `active` flag gets
cleared the instant it dies, and every check for "is this enemy hittable" filters on `active` —
so the chain reaction can only ever spread outward through enemies that are still alive, never
loop back through one that already died this frame.

### Explosive

**What it does:** when a dash kill (direct, chain, *or* explosion-triggered) finishes an enemy
off, it detonates for `explosiveLevel * EXPLOSIVE_DAMAGE_PER_LEVEL` damage in a radius of
`EXPLOSIVE_RADIUS` around where that enemy died, via `DashHitEnemy` on everything in range. If
that blast also kills something, `KillEnemy` runs again for *that* enemy, which checks
`explosiveLevel` again — so a tightly packed group can fold in one dash if you have enough
levels. This is a real, if bounded, chain reaction, not a single fixed-radius pulse.

**Current numbers:** level 1 = 3 damage in an 8px radius. `EXPLOSIVE_RADIUS` is intentionally
tight per the code comment ("needs enemies genuinely packed together, not just roughly nearby")
— this is a deliberate balance choice, not an oversight.

**New this pass:** explosions were dealing damage with **no visual** before now — `explosion.png`
existed in `assets/` but nothing loaded it. There's now a `Texture2D explosion` in `GameAssets`,
loaded from `explosion.png`, and a small burst-effect pool (`entities.c`) that spawns a fading
explosion sprite at the blast origin whenever one actually triggers. Purely cosmetic — the damage
already happened by the time you see it — but now you can actually *see* why a pack of enemies
just died at once.

### Chain

**What it does:** after your dash directly hits an enemy, `ApplyChainStrikes` finds the nearest
*other* eligible enemy within `CHAIN_RADIUS` and calls `DashHitEnemy` on it — for `chainLevel`
jumps in total, one per level. Each jump only considers enemies that aren't already mid-iframe
(`other->iframes > 0` is excluded), so you can't ping-pong the same two enemies back and forth.

**Current numbers:** level 1 = jumps to 1 additional enemy within `CHAIN_RADIUS` = **10px**.

**Worth flagging directly, since this is likely part of why Chain feels confusing:** enemies are
drawn at roughly 60px wide (`GetSpriteScale` normalizes everything to one tile width), and nearby
systems use much larger radii for comparison — `EXPLOSIVE_RADIUS` is 8px but is explicitly tuned
tight on purpose, `PICKUP_RADIUS` is 40px, `CONTACT_RADIUS_BONUS` is 6px added on top of a
sprite's real size. A 10px chain radius means, in practice, a jump can only reach an enemy that's
*already almost touching* the one you just hit. Unless enemies happen to be clustered nearly on
top of each other, Chain will often do nothing visible, which would read exactly like "I don't
understand what this does" even though the code is working correctly — it's just rarely finding
a valid target. If you want Chain to actually fire regularly, `CHAIN_RADIUS` is the single number
to raise — something in the 60–100 range (closer to actual enemy spacing) would make it trigger
far more often. Didn't change it since it's a balance call, not a bug, but flagging it since it
directly bears on "why doesn't this seem to work."

### Bleed

**What it does:** as covered above, it's not a separate system — `DashHitEnemy` sets
`enemy->bleedTimer = bleedLevel * BLEED_DURATION_PER_LEVEL` on every hit it processes, direct or
otherwise. Then in `Update()`, any sprite with `bleedTimer > 0` ticks it down every frame, and
every `BLEED_TICK_FRAMES`-th frame deals `BLEED_DAMAGE_PER_TICK` damage:

```c
if (s->bleedTimer % BLEED_TICK_FRAMES == 0) {
    s->hp -= BLEED_DAMAGE_PER_TICK;
    if (s->hp <= 0 && s->active) KillEnemy(assets, &sprites[0], s);
}
```

If a bleed tick is what finishes an enemy off, `KillEnemy` still runs — so a bleed-only kill
still rolls an elite drop and still triggers Explosive if you have it. Bleeding enemies are also
tinted green (`ui.c`, `DrawSprite`) so you can see it's active on them.

**Current numbers:** level 1 = 4 frames of bleed, which at `BLEED_TICK_FRAMES = 9` doesn't even
reach a full tick (4 < 9, so `bleedTimer` hits 0 before `% 9 == 0` ever lands on a nonzero
timer at the right instant) — practically speaking, level 1 Bleed by itself barely does anything
yet; it becomes meaningfully visible around level 2–3 once the duration crosses a couple of
9-frame ticks. Worth knowing so a single early Bleed pickup doesn't feel like a dud.

### Why these three feel like "real build synergies" without any extra code

Because Explosive, Chain, and Bleed are all just modifiers layered onto the *same* shared
`DashHitEnemy`/`KillEnemy` pair rather than three independent systems, having more than one of
them compounds automatically:

- **Chain + Bleed:** every enemy your chain jumps to also gets bled — for free, since Chain calls
  `DashHitEnemy` and bleed application lives inside `DashHitEnemy`.
- **Chain + Explosive:** if a chain jump happens to finish an enemy off, `KillEnemy` runs and
  checks Explosive same as any other kill — so a chain jump can trigger its own separate blast.
- **Explosive + Bleed:** enemies caught in a blast go through `DashHitEnemy` too, so they get
  bled as part of being blown up.
- **All three together:** a single dash hit can jump to a second enemy (Chain), which might die
  and detonate (Explosive), catching a third enemy who's now both damaged *and* bleeding — from
  one button press.

This is the "emergent combination" the original review was asking for under build synergies —
it already exists mechanically, it just wasn't visible or well-tuned (Chain's radius) or
documented (this file) until now.
