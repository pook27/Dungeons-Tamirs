Room Types & Bigger Rooms — Plan
1. Coins economy (foundation for everything else)
New Sprite type or a flag on the existing pickup, drawn with coin.png, that on player contact adds to a new playerCoins counter instead of calling ApplyUpgrade.
Elite kills call a new SpawnCoinPickup(x, y, value) instead of today's SpawnPickup (which rolls a free UpgradeType) — that function goes away entirely, since level-ups + the shop become the only two ways to get upgrades.
Coin value scaling with dungeonDepth (same shape as DEPTH_HP_GROWTH already scales enemy HP) so late-floor elites pay out more.
"Scaling a bit smaller" — I read this as: elites were tuned to be a tough-but-worth-it fight because the payout was an instant, guaranteed upgrade. Now that the payout is smoothed through an economy instead of a direct power spike, ELITE_HP_MULTIPLIER (and whatever elite size/damage scaling exists alongside it) can come down somewhat to compensate. Exact numbers are a tuning pass once it's in, not something to lock in on paper.
Worth considering: a small chance for regular (non-elite) kills to drop a single scrap coin, much rarer than elite drops. Otherwise early floors with zero elites could leave a player broke outside the shop with nothing to spend.
2. RoomType — the architectural piece everything else sits on

Add a real enum RoomType { ROOM_START, ROOM_COMBAT, ROOM_BOSS, ROOM_SHOP, ... } field to Room, and assign it once in/after GenerateFloorShape():

Start = center room (unchanged convention)
Boss = BFS-farthest room (unchanged convention)
Shop = one other room from the floor's blob, picked with a minimum distance from start so it's never trivially adjacent to spawn
Everything else defaults to combat, with room for the new types below to get their own weighted slice

This replaces the scattered isSafeRoom/isBossRoom local booleans in InitRooms, LoadRoom, and DrawRoom with one canonical field every system branches on — spawn logic, draw logic, minimap coloring, interaction logic. Touches several files but it's mechanical, and it's the piece that makes adding a 4th, 5th, 6th room type additive instead of another special case bolted onto InitRooms.

3. Shop room — your spec, mapped onto that architecture
Shopkeeper: the ShopMan 25-frame set finally gets used — idle animation loop, same "advance frame every N ticks" idea as UpdateBossAnimation, worth generalizing into one shared helper now that there are two independent users of frame-cycling instead of copy-pasting it.
Table + 3 items: on room generation, roll 3 UpgradeTypes using the existing rarity-weighted RollUpgradeChoices from the upgrade-clarity work — the shop's rarity tiers are the same ones already driving level-up odds, no new system needed there.
Pricing by rarity: a base coin price per tier (common cheap, rare expensive), stored per-item alongside the rolled type. I'd suggest storing this shop state directly on the Room (mirroring how enemySpawns[] already lives on Room) — an itemType[3], itemPrice[3], purchased[3], rerollCount block.
Buying: walk up to an item, press E, and if playerCoins >= price, deduct coins, call the existing ApplyUpgrade, mark that slot purchased (gone for the rest of this visit — I'd lean toward "sold out" rather than instantly refilling, so a shop visit feels finite, Isaac/Hades-style). E looks unclaimed by anything currently bound.
Reroll: rerolls all unpurchased slots, cost increases each use, tracked per-room via rerollCount so it resets on revisiting a fresh floor's shop but not mid-visit. R is only ever read on the death screen today (mutually exclusive with being alive in a shop), so it's actually free to reuse here too if you want the mnemonic — otherwise any other unclaimed key works.
Nice freebie: the "current → next" stack description work from the upgrade cards can double as a proximity tooltip here ("stand near an item, see what it does") for basically no new code.
4. Other room types worth adding
Treasure Room — no enemies, one free pedestal choice (like a tiny level-up screen sitting in the world) instead of a coin cost. Rare, and doesn't compete with the shop's economy since nothing's for sale.
Rest / Shrine Room — safe, no enemies, one binary tradeoff via the same walk-up-and-E pattern the shop establishes (e.g. "trade some max HP for permanent damage," or "spend coins for a guaranteed rare"). Cheap to build once the shop's interact pattern exists, and adds real build-defining decisions rather than pure value.
Elite Den — 1–2 elites only, no chaff, telegraphed on the minimap, bonus coin payout. Right now a room can randomly roll multiple elites via the per-enemy elite chance, but nothing tells the player that's what they're walking into — making it a deliberate, visible room type turns a random spike into an opt-in one.
Challenge / Arena Room — a genuine multi-wave gauntlet with a better-than-normal guaranteed reward. This is the room type I'd actually pair with bigger-scrollable-room support (below) — a proper gauntlet needs more than one screen's worth of space to not feel claustrophobic with several waves going at once.
5. Obstacles
Spike tiles — static, contact = bleed. Fits Vault/Challenge rooms as a risk-tax around loot.
Fire pit tiles — static or slow-pulsing (telegraphed on/off, rewarding timing over pure avoidance) = burn DoT. Same tick-based plumbing bleedTimer already uses, just a new timer field and tint — this was already scoped out in the upgrades.md write-up as reusable.
Breakable crates — same spawn/cleanup shape as the existing POT sprite, minor coin/nothing reward, mostly there to give combat/treasure rooms more texture to dash through.
Pressure plates — stand-on-tile triggers (open a door, drop a "shield" around a coin pile) — a good Vault-room gimmick that ties the room's identity to something other than pure combat.
Moving hazards (patrolling flame / rotating saw along a fixed path) — a later addition once bigger rooms exist; timing puzzles instead of static tiles, best suited to the Challenge/Arena room specifically.
6. Bigger scrollable rooms — technical shape

Rooms today are the screen (900×600, one static Camera2D). To decouple those:

Camera follows the player, clamped so it never shows past the room's actual bounds — camera.target = player position, clamped to [WIDTH/2, roomPixelWidth - WIDTH/2] per axis. The screen-shake offset logic that already exists on Camera2D composes with this fine.
TryChangeRoom currently triggers off the screen edge (player->x < 0 / > WIDTH) — this has to become the room's edge instead, which only matters once a room can exceed one screen. This was already flagged as a latent issue back when collision hardening came up.
Tile grid size — I'd bump ROOM_TILE_ROWS/COLS to some larger fixed max and let each room type just use less of it (simple, trivial memory cost at this game's scale) rather than dynamically-sized grids (more "correct," not worth the complexity here).
Everything else generalizes for free: sprite culling already filters by logical room (roomRow/roomCol), not by screen, so it doesn't care how big the room's pixel dimensions are. Enemy AI chases the player's actual position, not anything screen-relative. The minimap operates at room granularity already. None of that needs to change.
Tile-loop culling (skip drawing tiles outside the camera's view) is a nice-to-have at this scale, not a requirement — even an uncapped bigger room is a few hundred tiles, trivially cheap to draw every one of.

I'd make this opt-in per room type (a widthInScreens/heightInScreens on the room type's definition, defaulting to 1×1 for everything that exists today) rather than retrofitting every room — Shop/Start/Combat/Boss/Treasure/Shrine are all fine staying exactly the size they are now; only Challenge/Vault actually want the extra space.

Suggested build order
Coins economy (small, self-contained, unblocks everything else)
RoomType field + refactor of the existing implicit start/boss detection onto it
Shop room (your spec — now has coins + RoomType to sit on)
Treasure + Rest/Shrine (cheapest new types, reuse the shop's interact pattern directly)
Hazard-tile system (spikes/fire) shared plumbing
Elite Den (needs nothing new beyond RoomType + hazard tiles if you want them there)
Bigger scrollable rooms, built specifically for Challenge/Arena rather than speculatively
