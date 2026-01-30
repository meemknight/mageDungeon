#pragma once

#include "gameplay/Physics.h"
#include "gameplay/characterAnimator.h"
#include "gameplay/particleSystem.h"
#include "gameplay/assetsManager.h"
#include "gameplay/elements.h"
#include <memory>
#include <vector>
#include <random>

struct Map;
struct Player;
struct EntityHolder;
struct ProjectileHolder;

// Friendly summons that follow the player and assist in combat.
struct SummonEntity
{
	// **configuration variables**
	float timeLeft = 0.0f;
	float life = 0.0f;
	float maxLife = 0.0f;

	// **state variables**
	PhysicalEntity physics{glm::vec2{12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE}, true};
	CharacterAnimator animator{glm::vec2(48.f * PIXEL_SIZE, 48.f * PIXEL_SIZE)};
	TileSet tileSet;

	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder) = 0;
	virtual void render(gl2d::Renderer2D &renderer,
		ParticlePostProcessRenderer &particlePostProcessRenderer) = 0;
	virtual bool isDying() const { return false; }
	virtual bool canBeTargeted() const { return !isDying(); }
	virtual void forceKill() { timeLeft = 0.0f; }
	virtual std::unique_ptr<SummonEntity> clone() const = 0;
	virtual ~SummonEntity() = default;
};

// CRTP mixin that implements clone() for any Derived summon.
template <class Derived, class Base = SummonEntity>
struct CloneableSummon: Base
{
	std::unique_ptr<SummonEntity> clone() const override
	{
		return std::make_unique<Derived>(static_cast<const Derived &>(*this));
	}
};

// Holds and updates all active summons.
struct SummonHolder
{
	// **configuration variables**
	int maxSummons = 3;

	// **state variables**
	std::vector<std::unique_ptr<SummonEntity>> summons;

	void addSummonAsPtr(std::unique_ptr<SummonEntity> summon, glm::vec2 pos);
	void update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder);
	void render(gl2d::Renderer2D &renderer,
		ParticlePostProcessRenderer &particlePostProcessRenderer);
	void clear();
};

// Handles summon push against other entities (not the player).
void resolveSummonEntityPush(EntityHolder &entityHolder, SummonHolder &summons);

// Access the active summon holder.
SummonHolder &getSummonHolder();

// Elemental slime summon that follows and attacks nearby enemies.
struct SlimeSummon: public CloneableSummon<SlimeSummon>
{
	// **configuration variables**
	int element = Elements::Water;
	float attackDamage = 5.0f;
	float statusAmount = 5.0f;
	float projectileSizeBias = 1.0f;
	float attackPushBack = 4.6f;
	float baseSpeed = 6.0f;
	float catchupSpeed = 12.0f;
	float followDistance = 2.4f;
	float returnDistance = 7.0f;
	float returnStopDistance = 3.4f;
	float attackRange = 7.0f;
	float attackInterval = 1.15f;
	float attackProjectileSpeed = 7.5f;
	float contactDamageInterval = 0.6f;
	float idleAnimSpeed = 1.1f;
	float moveAnimSpeed = 1.7f * 0.9f;
	float particleInterval = 0.12f;
	float teleportDistance = 20.0f;
	// aim nudge control (player-directed summon steering)
	float controlDelay = 0.2f;
	float controlMaxDistance = 6.2f;
	float controlMinStrength = 0.08f;
	float deathFrameDuration = 0.12f;
	float deathHoldDuration = 0.12f;
	float deathFadeDuration = 0.4f;
	int deathFrames = 6;
	float repathInterval = 0.25f;

	// **state variables**
	bool returningToPlayer = false;
	bool dying = false;
	float attackCooldown = 0.0f;
	float contactCooldown = 0.0f;
	float deathFrameTimer = 0.0f;
	float deathHoldTimer = 0.0f;
	float fadeAlpha = 1.0f;
	int deathFrameIndex = 0;
	bool particlesInitialized = false;
	float particleTimer = 0.0f;
	ParticleSettings ambientParticle;
	ParticleSystem particleSystem;
	float repathTimer = 0.0f;
	float controlTimer = 0.0f;
	int pathIndex = 0;
	glm::ivec2 lastPathGoalTile = {999999, 999999};
	std::vector<glm::ivec2> pathTiles;

	SlimeSummon();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	bool isDying() const override { return dying; }
	bool canBeTargeted() const override { return !dying; }
	void forceKill() override;
};
