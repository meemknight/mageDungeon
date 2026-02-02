This is a rogue like made in C++, where the core mechanic is casting spells. To cast a spell you combine elements. And there are many spell recepies.


General Instructions:

- comment some basic things in the code, when you add a system describe briefly what it does. When you refactor a system update the comments if needed

- Keep the coding style like mine.

- Prefer using existing helpers and patterns (elementToColor, getRandomFloat/getRandomInt, PIXEL_SIZE).


Coding Guidelines:

- World units use PIXEL_SIZE (1/16). Physics/tiles/particles are sized with it.
- Keep systems small and mostly struct-based with inline helpers.
- Use std::ranlux24_base + randomStuff.h helpers for RNG.
- ParticleSettings::folowParent controls whether particles move with a parent.
- When adding a new projectile, use Projectile::onDestroy for death effects.


Game Architecture (high level):

- gameLayer/gameLogic is the main runtime state (map, player, entityHolder, projectiles, spells, particles, damageViewer, floorInfo).
- Init: FloorGenerator creates a floor, player spawns from FloorInfo, then enemies are spawned per-room.
- Update order: input/movement -> physics resolve -> projectiles -> particles -> damageViewer -> entities -> resolveEntityPush.
- Render order: map -> spells (before entities) -> entities -> player -> projectiles -> particles -> post-process -> map-after -> damage text.


World Generation:

- FloorGenerator in worldGen/floorGen.h (plains + dungeon + cave stub).
- Dungeon uses room placement + corridors, optional spawn room, outputs FloorInfo (rooms + spawn positions).
- FloorConnection uses edge + offset so future floors can connect.


Spells and Projectiles:

- SpellRecepie holds element combos; SpellTypes::getSpellFromRecepie maps recipes.
- Spells create projectiles (BasicMagicMissleSpell, FlameWallSpell, etc.).
- Projectiles own their ParticleSystem; ProjectileHolder calls onDestroy then copies particles to the main system.
- Elements use elementToColor/elementToSecondaryColor for visuals.


Entities and Combat:

- EntityHolder owns enemies; player is separate (Player struct, not in EntityHolder).
- EntityLifeThings::computeHit handles damage + pushback; beatsElement defines bonus damage.
- resolveEntityPush applies light separation between enemies and between enemies/player.


Physics and Map:

- Transform2D and PhysicalEntity are in gameplay/Physics.*; circle vs rect handled there.
- Map has two layers; collidable checks both layers.


Particles and Damage Numbers:

- ParticleSystem has per-instance maxCount; emitParticles copies settings into instances.
- DamageViewerSystem shows floating text (yellow->red fade) and uses renderText rotation.


gl2d and glui:

- gl2d::Renderer2D renders rectangles, outlines, text (rotationDegrees supported), and uses a camera.
- glui provides Box/Frame layout helpers, UI rendering, and hit testing (isInButton).


