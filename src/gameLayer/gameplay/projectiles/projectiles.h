#pragma once

#include "gameplay/Physics.h"
#include "gameplay/elements.h"
#include <glm/glm.hpp>
#include <map>
#include <cmath>
#include <memory>
#include "gameplay/particleSystem.h"
#include <gameplay/damageViewerSystem.h>
#include <gameplay/statusEffects.h>
#include <random>
#include <randomStuff.h>
#include <particles/particleCreator.h>
#include <gameplay/entities/entity.h>

struct Player;

struct Projectile
{
	// **configuration variables**
	int element = 0;
	int type = 0;

	// **state variables**
	PhysicalEntity physics;
	ParticleSystem particleSystem;
	float timeAlieve = 10;

	glm::vec2 &getPos()
	{
		return physics.getPos();
	}

	Projectile()
	{
		//basic size
		physics.transform.size = {PIXEL_SIZE * 8, PIXEL_SIZE * 8};
		physics.transform.isCircleCollider = true;
	}

	virtual bool runTimer(float deltaTime)
	{
		timeAlieve -= deltaTime;
		if (timeAlieve < 0) { return 0; }
		return 1;
	}

	bool basicPhysicsAndCollisionsCheck(float deltaTime, Map &map)
	{
		physics.updateForces(deltaTime, 0);
		physics.resolveConstrains(map);
		physics.updateMove();

		if (physics.leftTouch || physics.rightTouch || physics.downTouch || physics.upTouch)
		{
			return 0;
		}

		return 1;
	}


	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) = 0;

	virtual void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) = 0;

	virtual void onDestroy(std::ranlux24_base &rng) {}

	virtual ~Projectile() = default;

	virtual std::unique_ptr<Projectile> clone() const = 0;

};

//return true if hit
bool basicProjectileHitEntitiesLogic(PhysicalEntity &physics, 
	glm::vec2 projectileMoveDirection, char projectileElement,
	EntityHolder &entities, HitStats hitStats, float statusAmount = 5.0f);

struct ProjectileHolder
{

	std::vector<std::unique_ptr<Projectile>> projectiles;
	std::vector<std::unique_ptr<Projectile>> pendingProjectiles;

	template <typename T>
	void addProjectile(T projectile, glm::vec2 pos)
	{
		static_assert(std::is_base_of_v<Projectile, T>);

		auto ptr = std::make_unique<T>(std::move(projectile));
		ptr->physics.teleport(pos);
		projectiles.push_back(std::move(ptr));
	}

	void addProjectileAsPtr(std::unique_ptr<Projectile> p, glm::vec2 pos)
	{
		p->physics.teleport(pos);
		projectiles.push_back(std::move(p));
	}

	void addProjectileDeferredAsPtr(std::unique_ptr<Projectile> p, glm::vec2 pos)
	{
		p->physics.teleport(pos);
		pendingProjectiles.push_back(std::move(p));
	}

	void update(float deltaTime,
		Map &map,
		ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder)
	{
		for (auto it = projectiles.begin(); it != projectiles.end(); )
		{
			Projectile &p = **it;

			if (!p.runTimer(deltaTime))
			{
				p.onDestroy(rng);
				mainParticleSystem.copyParticles(
					p.particleSystem, rng, p.physics.getPos());
				it = projectiles.erase(it);
				continue;
			}

			if (!p.update(deltaTime, map, mainParticleSystem, rng, entityHolder))
			{
				p.onDestroy(rng);
				mainParticleSystem.copyParticles(
					p.particleSystem, rng, p.physics.getPos());
				it = projectiles.erase(it);
				continue;
			}

			++it;
		}

		if (!pendingProjectiles.empty())
		{
			for (auto &p : pendingProjectiles)
			{
				projectiles.push_back(std::move(p));
			}
			pendingProjectiles.clear();
		}
	}

	void render(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer)
	{
		for (auto &p : projectiles)
		{
			p->render(renderer, assetManager, particlePostProcessRenderer);
		}
	}

};

// Access the active projectile holder for spawning burst projectiles.
ProjectileHolder &getProjectileHolder();

// Standby projectiles orbit the player and fire on input.
struct StandbyProjectileEntry
{
	std::unique_ptr<Projectile> projectile;
	ParticleEmissionSettings particleEmmision;
	ParticleEmissionSettings customEmission;
	float particleTimer = 0.0f;
	float timeLeft = 0.0f;
	float throwVelocity = 10.0f;
	bool hasCustomEmission = false;
	bool initialized = false;
};

// Standby projectiles stay around the player until fired or expired.
struct StandbyProjectileSystem
{
	// **configuration variables**
	float ringRadius = 1.1f;
	float fireRange = 9.0f;
	float standbyLifetime = 14.0f;
	int maxStandby = 12;
	float idleRotationSpeed = 0.6f;
	float idleRotationAccel = 0.4f;
	float idleRotationDelay = 0.8f;

	// **state variables**
	std::vector<StandbyProjectileEntry> standbyProjectiles;
	// insertion cursor for interleaving new entries
	int insertIndex = 1;
	float idleRotation = 0.0f;
	float idleRotationVelocity = 0.0f;
	float idleRotationDelayTimer = 0.0f;

	void addProjectileAsPtr(std::unique_ptr<Projectile> projectile,
		float customLifetime = -1.0f, float customThrowVelocity = 10.0f,
		const ParticleEmissionSettings *customEmission = nullptr);
	void update(float deltaTime, Map &map, ProjectileHolder &projectileHolder,
		std::ranlux24_base &rng, Player &player, EntityHolder &entityHolder,
		glm::vec2 aimDir, bool aimActive);
	bool tryFire(Map &map, ProjectileHolder &projectileHolder,
		Player &player, EntityHolder &entityHolder, glm::vec2 aimDir);
	void render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer);
};

// Access the active standby projectile system.
StandbyProjectileSystem &getStandbyProjectilesSystem();

// Access the current player aim direction.
glm::vec2 getFireDirection();
// Access the current player aim target position.
glm::vec2 getFireTargetPos();

// CRTP mixin that implements clone() for any Derived
template <class Derived, class Base = Projectile>
struct CloneableProjectile: Base
{
	std::unique_ptr<Projectile> clone() const override
	{
		return std::make_unique<Derived>(static_cast<const Derived &>(*this));
		// or: return std::make_unique<Derived>(*static_cast<const Derived*>(this));
	}
};

struct BasicMagicMissle: public CloneableProjectile<BasicMagicMissle>
{
	// **configuration variables**
	HitStats hitStats;
	float particleSizeBias = 1;
	float statusAmount = 5.0f;
	bool hasCustomEmission = false;
	ParticleEmissionSettings customEmission;

	// **state variables**
	float particleTimer = 0.0;
	bool firstTime = 1;
	ParticleEmissionSettings particleEmmision;

	BasicMagicMissle();
	BasicMagicMissle(HitStats hitStats);
	BasicMagicMissle(HitStats hitStats, float particleSizeBias);

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;


};


struct TrapProjectile: public CloneableProjectile<TrapProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float trapRadious = 1.5f;
	float statusAmount = 5.0f;
	constexpr static float activateRadiousMultiplier = 0.8f;

	// **state variables**
	float particleTimer = 0.0f;
	float orbitTimer = 0.0f;
	float ringRotation = 0.0f;
	float ringSpinSpeed = 0.0f;
	float ringEmitInterval = 0.2f;
	float orbitEmitInterval = 0.32f;
	float ringStep = 0.3f;
	bool triggered = 0;
	float triggerTimer = 0.4f;
	bool particlesInitialized = false;
	ParticleSettings ringParticle;
	ParticleSettings orbitParticle;
	ParticleSettings burstParticle;

	TrapProjectile();
	TrapProjectile(HitStats hitStats);

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;

	void onDestroy(std::ranlux24_base &rng) override;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder);



};

// Aimable bolt that steers to current aim direction.
struct AimableBoltProjectile: public CloneableProjectile<AimableBoltProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float particleSizeBias = 1.0f;
	float moveSpeed = 5.5f;
	float orbitInterval = 0.08f;
	float statusAmount = 5.0f;

	// **state variables**
	bool firstTime = true;
	float particleTimer = 0.0f;
	float orbitTimer = 0.0f;
	glm::vec2 moveDir = {1.0f, 0.0f};
	ParticleEmissionSettings particleEmmision;
	ParticleSettings orbitParticle;

	AimableBoltProjectile();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Aimable earth bolt that sheds thorns as it moves.
struct AimableEarthBoltProjectile: public CloneableProjectile<AimableEarthBoltProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float particleSizeBias = 1.0f;
	float moveSpeed = 4.5f;
	float trailInterval = 0.3f;
	float orbitInterval = 0.08f;
	float statusAmount = 5.0f;

	// **state variables**
	bool firstTime = true;
	float particleTimer = 0.0f;
	float trailTimer = 0.0f;
	float orbitTimer = 0.0f;
	int storedDamage = 0;
	glm::vec2 moveDir = {1.0f, 0.0f};
	ParticleEmissionSettings particleEmmision;
	ParticleSettings orbitParticle;

	AimableEarthBoltProjectile();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Unstable bolt that releases random elemental effects.
struct WildMagicBoltProjectile: public CloneableProjectile<WildMagicBoltProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float particleSizeBias = 1.2f;
	float orbitInterval = 0.06f;
	float statusAmount = 0.0f;

	// **state variables**
	bool firstTime = true;
	float particleTimer = 0.0f;
	float orbitTimer = 0.0f;
	ParticleEmissionSettings baseEmission;
	ParticleSettings orbitParticle;

	WildMagicBoltProjectile();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Stationary thorn that damages a single enemy on contact.
struct ThornProjectile: public CloneableProjectile<ThornProjectile>
{
	// **configuration variables**
	HitStats hitStats;

	// **state variables**
	// (none)

	ThornProjectile();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Earth bolt that leaves a trail of thorns.
struct EarthThornBoltProjectile: public CloneableProjectile<EarthThornBoltProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float trailInterval = 0.2f;
	int maxThorns = 10;

	// **state variables**
	bool firstTime = true;
	float trailTimer = 0.0f;
	int spawnedThorns = 0;
	ParticleEmissionSettings particleEmmision;

	EarthThornBoltProjectile();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Earth-water bolt that sheds damage into spawned thorns.
struct EarthWaterThornBoltProjectile: public CloneableProjectile<EarthWaterThornBoltProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float trailInterval = 0.12f;
	float spawnOffset = PIXEL_SIZE * 5.5f;
	float minScale = 0.8f;
	float particleSizeBias = 1.25f;
	float orbitInterval = 0.06f;
	int maxStoredDamage = 30;

	// **state variables**
	bool firstTime = true;
	float trailTimer = 0.0f;
	float orbitTimer = 0.0f;
	int storedDamage = 0;
	glm::vec2 baseColliderSize = {};
	ParticleEmissionSettings particleEmmision;
	ParticleEmissionSettings baseEmmision;
	ParticleSettings orbitParticle;

	EarthWaterThornBoltProjectile();
	void updateVisualScale();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Fast ricochet projectile that bounces and can hit repeatedly.
struct RicochetProjectile: public CloneableProjectile<RicochetProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float hitCooldownDuration = 0.3f;
	float orbitInterval = 0.12f;

	// **state variables**
	bool firstTime = true;
	float hitCooldown = 0.0f;
	float orbitTimer = 0.0f;
	ParticleSettings bodyParticle;
	ParticleSettings orbitParticle;

	RicochetProjectile();
	void setDamage(float damage)
	{
		hitStats.damage = damage;
	}
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Fast accelerating bolt that bursts into elemental effects on impact.
struct FastMagicBoltProjectile: public CloneableProjectile<FastMagicBoltProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float trailInterval = 0.03f;
	float slowHoldTime = 0.08f;
	float accelDuration = 0.16f;
	float slowSpeed = 1.8f;
	float maxSpeed = 24.0f;
	float explosionRadius = 2.4f;

	// **state variables**
	bool firstTime = true;
	bool exploded = false;
	float accelTimer = 0.0f;
	float trailTimer = 0.0f;
	glm::vec2 moveDir = {1.0f, 0.0f};
	ParticleSettings coreParticle;
	ParticleSettings trailParticle;

	FastMagicBoltProjectile();
	void setupParticles(std::ranlux24_base &rng);
	void explode(EntityHolder &entityHolder, std::ranlux24_base &rng);
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

struct BoulderProjectile: public CloneableProjectile<BoulderProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float trailInterval = 0.05f;

	// **state variables**
	bool firstTime = true;
	float trailTimer = 0.0f;
	ParticleSettings bigParticle;
	ParticleSettings trailParticle;

	BoulderProjectile();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

// Heavy ice block projectile that bursts into icy shards on impact.
struct BigIceBlockProjectile: public CloneableProjectile<BigIceBlockProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float trailInterval = 0.07f;
	glm::vec4 bigStartColor = {0.7f, 0.9f, 1.0f, 0.9f};
	glm::vec4 bigEndColor = {0.45f, 0.75f, 1.0f, 0.85f};
	float orbitInterval = 0.08f;

	// **state variables**
	bool firstTime = true;
	float trailTimer = 0.0f;
	float orbitTimer = 0.0f;
	ParticleSettings bigParticle;
	ParticleSettings trailParticle;
	ParticleSettings orbitParticle;
	bool shouldBurst = false;


	BigIceBlockProjectile();
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

struct ElementWallProjectile: public CloneableProjectile<ElementWallProjectile>
{
	// **configuration variables**
	HitStats hitStats;
	float tickInterval = 0.2f;
	float hitTimerPenalty = 0.15f;
	float wallLength = 6.0f;
	float wallThickness = 0.6f;
	float particleInterval = 0.02f;
	int segmentCount = 8;
	float segmentRadius = 0.45f;
	float segmentSpacing = 0.75f;

	// **state variables**
	bool firstTime = true;
	float tickTimer = 0.0f;
	float particleTimer = 0.0f;
	glm::vec2 wallNormal = {1.0f, 0.0f};
	ParticleSettings flameParticle;

	ElementWallProjectile();
	ElementWallProjectile(int elementType);
	void setElementType(int elementType);
	void setupWall(glm::vec2 aimDir);
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

struct HomingMagicMissle: public CloneableProjectile<HomingMagicMissle>
{
	// **configuration variables**
	HitStats hitStats;
	float particleSizeBias = 1.4f;
	float homingRange = 7.0f;
	float homingTurnRate = 5.0f;

	// **state variables**
	float particleTimer = 0.0f;
	bool firstTime = 1;
	float travelSpeed = 0.0f;
	ParticleEmissionSettings particleEmmision;

	HomingMagicMissle();
	HomingMagicMissle(HitStats hitStats);
	HomingMagicMissle(HitStats hitStats, float particleSizeBias);
	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};

struct SummonHolder;

// Enemy projectile that hits player and summons instead of entities.
// Glowing orange orb with particles moving outward slowly.
struct EnemyOrbProjectile: public CloneableProjectile<EnemyOrbProjectile>
{
	// **configuration variables**
	float damage = 1.0f;
	float speed = 3.0f;
	float particleInterval = 0.02f;
	bool showCollider = false;
	bool orbitEnabled = false;            // rotate around a moving center
	float orbitRadius = PIXEL_SIZE * 12.0f;
	float orbitAngularSpeed = 3.6f;       // radians per second

	// **state variables**
	bool firstTime = true;
	float particleTimer = 0.0f;
	glm::vec2 moveDir = {1.0f, 0.0f};
	float orbitAngle = 0.0f;
	glm::vec2 orbitCenterPos = {};
	bool orbitInitialized = false;
	ParticleSettings coreParticle;
	ParticleSettings glowParticle;

	// Targets - set these before adding to holder
	Player *targetPlayer = nullptr;
	SummonHolder *targetSummons = nullptr;

	EnemyOrbProjectile();
	void setDamage(float dmg);
	void updateDamageColors();
	void setDirection(glm::vec2 dir);
	void enableOrbit(float radius, float angularSpeed, float startAngle);
	void setupParticles();

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override;
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override;
	void onDestroy(std::ranlux24_base &rng) override;
};
