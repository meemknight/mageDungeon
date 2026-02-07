#pragma once
#include <gameplay/particleSystem.h>
#include <gameplay/map.h>
#include <gameplay/player.h>
#include <gameplay/projectiles/projectiles.h>
#include <gameplay/summons.h>
#include <gameplay/damageViewerSystem.h>
#include <gameplay/aStar.h>
#include <gameplay/elements.h>
#include <gameplay/entities/entity.h>
#include <particles/particleCreator.h>
#include <gameLayer.h>
#include <cmath>

struct Spell
{

	int element = 0;

	//how many times it triggers
	int maxFireCount = 1; //tweak
	float triggerDelay = 0.4; //tweak
	float driftAngleDegrees = 0; //tweak
	int elementsPerCast = 1; //tweak
	bool continuousUpdate = false;
	float continuousUpdateTimer = 10;
	bool firstTime = true;

	float triggerTimer = 0; //the counter
	int currentFireCounter = 0; //the counter

	glm::vec2 createPos = {};
	glm::vec2 createAimDir = {};

	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) = 0;

	bool isFirstTime()
	{
		return currentFireCounter == 0;
	}

	bool isLastTime()
	{
		return currentFireCounter == (maxFireCount - 1);
	}

	virtual void renderBeforeEntities(gl2d::Renderer2D &renderer) { };

	virtual ~Spell() = default;
};

struct SpellsHolder
{

	std::vector<std::unique_ptr<Spell>> spells;

	template <typename T>
	void addSpell(T spell, glm::vec2 createPos, glm::vec2 createAimDir)
	{
		static_assert(std::is_base_of_v<Spell, T>);

		spell.createPos = createPos;
		spell.createAimDir = createAimDir;

		auto ptr = std::make_unique<T>(std::move(spell));
		spells.push_back(std::move(ptr));
	}

	void addSpell(std::unique_ptr<Spell> spell, glm::vec2 createPos, glm::vec2 createAimDir)
	{
		spell->createPos = createPos;
		spell->createAimDir = createAimDir;

		spells.push_back(std::move(spell));
	}

	void update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder,
		std::ranlux24_base &rng, Player &player, EntityHolder &entityHolder,
		glm::vec2 currentAimDir);

	void renderBeforeEntities(gl2d::Renderer2D &renderer,
		ParticlePostProcessRenderer &particlePostProcessRenderer);

};

//the most basic spell
struct BasicMagicMissleSpell: public Spell
{
	BasicMagicMissleSpell()
	{
		Spell::maxFireCount = 1;
	}

	// **configuration variables**
	std::unique_ptr<Projectile> projectile;
	float throwVelocity = 10;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir)
	{

		auto pptr = projectile->clone(); // copy dynamic type
		pptr->physics.velocity = currentAimDir * throwVelocity;
		pptr->element = element;

		projectileHolder.addProjectileAsPtr(std::move(pptr), player.physics.getPos());

		return true;
	};
};

// Fires a short volley with one shot centered on aim.
struct HomingMeteoriteVolleySpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<Projectile> projectile;
	int shotCount = 5;
	float throwVelocity = 10.0f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)map;
		(void)mainParticleSystem;
		(void)rng;
		(void)entityHolder;

		glm::vec2 aim = currentAimDir;
		float len = glm::length(aim);
		if (len <= 0.0001f)
		{
			len = glm::length(createAimDir);
			if (len <= 0.0001f)
			{
				aim = {1.0f, 0.0f};
				len = 1.0f;
			}
			else
			{
				aim = createAimDir;
			}
		}
		aim /= len;

		auto spawnProjectile = [&](glm::vec2 dir)
		{
			auto pptr = projectile->clone();
			pptr->element = element;
			pptr->physics.velocity = dir * throwVelocity;
			projectileHolder.addProjectileAsPtr(std::move(pptr), player.physics.getPos());
		};

		float baseAngle = std::atan2(aim.y, aim.x);
		float step = 6.2831853f / (float)(shotCount > 0 ? shotCount : 1);
		for (int i = 0; i < shotCount; i++)
		{
			float angle = baseAngle + step * (float)i;
			glm::vec2 dir = {std::cos(angle), std::sin(angle)};
			spawnProjectile(dir);
		}

		return true;
	}
};

// Fires 4 homing boulders around the aim direction.
struct HomingBouldersSpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<Projectile> projectile;
	int shotCount = 4;
	float throwVelocity = 9.0f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)map;
		(void)mainParticleSystem;
		(void)entityHolder;
		(void)rng;

		glm::vec2 aim = currentAimDir;
		float len = glm::length(aim);
		if (len <= 0.0001f)
		{
			len = glm::length(createAimDir);
			if (len <= 0.0001f)
			{
				aim = {1.0f, 0.0f};
				len = 1.0f;
			}
			else
			{
				aim = createAimDir;
			}
		}
		aim /= len;

		float baseAngle = std::atan2(aim.y, aim.x);
		float step = 6.2831853f / (float)shotCount;
		for (int i = 0; i < shotCount; i++)
		{
			float angle = baseAngle + step * (float)i;
			glm::vec2 dir = {std::cos(angle), std::sin(angle)};
			auto pptr = projectile->clone();
			pptr->element = element;
			pptr->physics.velocity = dir * throwVelocity;
			projectileHolder.addProjectileAsPtr(std::move(pptr), player.physics.getPos());
		}

		return true;
	}
};

// Fires radial waves of small bolts around the player.
struct TsunamiSpell: public Spell
{
	// **configuration variables**
	int bulletsPerWave = 16;
	float throwVelocity = 8.0f;
	float totalDamage = 70.0f;
	float pushBack = 10.4f;
	float statusAmount = 0.0f;
	float projectileSizeScale = 0.7f;
	int waveParticleCount = 6;

	// **state variables**
	bool initialized = false;
	std::unique_ptr<Projectile> projectile;
	ParticleSettings waveParticle;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)map;
		(void)entityHolder;
		(void)currentAimDir;

		if (!initialized)
		{
			initialized = true;
			int totalProjectiles = std::max(1, bulletsPerWave * std::max(1, maxFireCount));
			HitStats hitStats;
			hitStats.damage = totalDamage / (float)totalProjectiles;
			hitStats.pushBack = pushBack;

			auto bolt = std::make_unique<BasicMagicMissle>(hitStats, 0.7f);
			bolt->element = element;
			bolt->timeAlieve = 3.0f;
			bolt->physics.transform.size *= projectileSizeScale;
			if (auto missle = dynamic_cast<BasicMagicMissle *>(bolt.get()))
			{
				missle->statusAmount = statusAmount;
			}
			projectile = std::move(bolt);

			glm::vec4 startColor = elementToSecondaryColor(element); startColor.a = 0.55f;
			glm::vec4 endColor = elementToColor(element); endColor.a = 0.35f;
			waveParticle = getOrbitParticle(startColor, endColor);
			waveParticle.onCreateCount = waveParticleCount;
			waveParticle.particleLifeTime = {0.6f, 0.95f};
			waveParticle.animationScaleX = {PIXEL_SIZE * 4.5f, PIXEL_SIZE * 9.0f};
			waveParticle.animationScaleY = {PIXEL_SIZE * 4.5f, PIXEL_SIZE * 9.0f};
			waveParticle.folowParent = false;
		}

		glm::vec2 origin = player.physics.getPos();
		const float twoPi = 6.2831853f;
		float baseAngle = getRandomFloat(rng, 0.0f, twoPi);
		float angleStep = twoPi / (float)std::max(1, bulletsPerWave);
		for (int i = 0; i < bulletsPerWave; i++)
		{
			float angle = baseAngle + angleStep * (float)i + getRandomFloat(rng, -0.03f, 0.03f);
			glm::vec2 dir = {std::cos(angle), std::sin(angle)};

			auto pptr = projectile->clone();
			pptr->element = element;
			pptr->physics.velocity = dir * throwVelocity;
			projectileHolder.addProjectileAsPtr(std::move(pptr), origin);
		}

		mainParticleSystem.emitParticles(waveParticle, origin, rng, origin);
		return true;
	}
};

// Spawns chained lightning-like bolt nodes that jump forward or to nearby targets.
struct ChainBoltSpell: public Spell
{
	// **configuration variables**
	int boltCount = 4;
	float boltDelay = 0.15f;
	float minBoltDistance = 1.7f;
	float maxBoltDistance = 3.4f;
	float frontAngleDegrees = 24.0f;
	float sideAngleMinDegrees = 34.0f;
	float sideAngleMaxDegrees = 72.0f;
	float boltDamage = 4.0f;
	float secondaryDamage = 0.0f;
	float statusAmount = 0.0f;
	float hitRadius = PIXEL_SIZE * 9.0f;
	float targetRange = 3.6f;
	float firstBoltSnapRange = 1.15f;
	float particleSizeScale = 1.0f;

	// **state variables**
	bool initialized = false;
	glm::vec2 chainDir = {1.0f, 0.0f};
	glm::vec2 lastBoltPos = {};
	Entity *queuedTarget = nullptr;
	std::vector<Entity *> hitEntities;
	ParticleSettings burstParticle;
	ParticleSettings nodeParticle;
	ParticleSettings linkParticle;

	ChainBoltSpell()
	{
		maxFireCount = 4;
		triggerDelay = 0.15f;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)projectileHolder;
		(void)player;

		auto normalizeSafe = [](glm::vec2 v)
		{
			float l = glm::length(v);
			if (l <= 0.0001f)
			{
				return glm::vec2(1.0f, 0.0f);
			}
			return v / l;
		};

		if (!initialized)
		{
			initialized = true;
			maxFireCount = std::max(1, boltCount);
			triggerDelay = boltDelay;

			glm::vec2 aimDir = currentAimDir;
			if (glm::length(aimDir) <= 0.0001f)
			{
				aimDir = createAimDir;
			}
			chainDir = normalizeSafe(aimDir);
			lastBoltPos = createPos;
			hitEntities.clear();

			glm::vec4 startColor = elementToSecondaryColor(element);
			glm::vec4 endColor = elementToColor(element);
			startColor.a = 1.0f;
			endColor.a = 0.78f;
			float sizeScale = std::max(0.2f, particleSizeScale);

			burstParticle = getLightningZapParticle(startColor, endColor);
			burstParticle.onCreateCount = 14;
			burstParticle.particleLifeTime = {0.12f, 0.26f};
			burstParticle.velocityX = glm::vec2{-105.0f, 105.0f} * PIXEL_SIZE;
			burstParticle.velocityY = glm::vec2{-105.0f, 105.0f} * PIXEL_SIZE;
			burstParticle.createApearence.size = glm::vec2{0.75f, 3.0f} * PIXEL_SIZE * sizeScale;
			burstParticle.endApearence.size = glm::vec2{0.2f, 1.1f} * PIXEL_SIZE * sizeScale;
			burstParticle.animationType = ParticleSettings::ANIMATION_TYPES::animationZigZag;
			burstParticle.animationSpeed = {-18.0f, 18.0f};
			burstParticle.animationScaleX = {PIXEL_SIZE * 2.0f, PIXEL_SIZE * 7.0f};
			burstParticle.animationScaleY = {PIXEL_SIZE * 0.8f, PIXEL_SIZE * 3.0f};
			burstParticle.animationScaleX *= sizeScale;
			burstParticle.animationScaleY *= sizeScale;
			burstParticle.folowParent = false;

			nodeParticle = getOrbitParticle(startColor, endColor);
			nodeParticle.onCreateCount = 7;
			nodeParticle.particleLifeTime = {0.22f, 0.38f};
			nodeParticle.velocityX = {0.0f, 0.0f};
			nodeParticle.velocityY = {0.0f, 0.0f};
			nodeParticle.dragX = {0.0f, 0.0f};
			nodeParticle.dragY = {0.0f, 0.0f};
			nodeParticle.animationSpeed = {-12.0f, 12.0f};
			nodeParticle.animationScaleX = {PIXEL_SIZE * 2.1f, PIXEL_SIZE * 5.4f};
			nodeParticle.animationScaleY = {PIXEL_SIZE * 2.1f, PIXEL_SIZE * 5.4f};
			nodeParticle.createApearence.size *= sizeScale;
			nodeParticle.endApearence.size *= sizeScale;
			nodeParticle.animationScaleX *= sizeScale;
			nodeParticle.animationScaleY *= sizeScale;
			nodeParticle.folowParent = false;

			linkParticle = getLightningZapParticle(startColor, endColor);
			linkParticle.onCreateCount = 2;
			linkParticle.particleLifeTime = {0.09f, 0.17f};
			linkParticle.velocityX = glm::vec2{-14.0f, 14.0f} * PIXEL_SIZE;
			linkParticle.velocityY = glm::vec2{-14.0f, 14.0f} * PIXEL_SIZE;
			linkParticle.createApearence.size = glm::vec2{0.65f, 2.2f} * PIXEL_SIZE * sizeScale;
			linkParticle.endApearence.size = glm::vec2{0.24f, 0.85f} * PIXEL_SIZE * sizeScale;
			linkParticle.animationType = ParticleSettings::ANIMATION_TYPES::animationZigZag;
			linkParticle.animationSpeed = {-12.0f, 12.0f};
			linkParticle.animationScaleX = {PIXEL_SIZE * 1.0f, PIXEL_SIZE * 4.0f};
			linkParticle.animationScaleY = {PIXEL_SIZE * 0.4f, PIXEL_SIZE * 1.8f};
			linkParticle.animationScaleX *= sizeScale;
			linkParticle.animationScaleY *= sizeScale;
			linkParticle.folowParent = false;
		}

		auto isBlocked = [&](glm::vec2 worldPos)
		{
			glm::ivec2 tile = WorldToTile(worldPos);
			if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
			{
				return true;
			}
			return map.isCollidableAtPosSafe(tile.x, tile.y);
		};

		auto clampDistanceToWalls = [&](glm::vec2 start, glm::vec2 dir, float wantedDistance)
		{
			const float step = PIXEL_SIZE * 2.0f;
			float safeDistance = 0.0f;
			for (float t = step; t <= wantedDistance; t += step)
			{
				glm::vec2 checkPos = start + dir * t;
				if (isBlocked(checkPos))
				{
					break;
				}
				safeDistance = t;
			}
			return safeDistance;
		};

		auto pickArcDirection = [&]()
		{
			float angleDegrees = 0.0f;
			if (getRandomChance(rng, 0.58f))
			{
				angleDegrees = getRandomFloat(rng, -frontAngleDegrees, frontAngleDegrees);
			}
			else
			{
				float sideAngle = getRandomFloat(rng, sideAngleMinDegrees, sideAngleMaxDegrees);
				angleDegrees = getRandomChance(rng, 0.5f) ? sideAngle : -sideAngle;
			}

			float angleRad = glm::radians(angleDegrees);
			float c = std::cos(angleRad);
			float s = std::sin(angleRad);
			glm::vec2 dir = {
				chainDir.x * c - chainDir.y * s,
				chainDir.x * s + chainDir.y * c
			};
			return normalizeSafe(dir);
		};

		auto pointerStillValid = [&](Entity *entity)
		{
			if (!entity) { return false; }
			for (auto &e : entityHolder.entities)
			{
				if (e.get() == entity)
				{
					return !e->dying;
				}
			}
			return false;
		};

		auto findNearbyTarget = [&](glm::vec2 origin, float maxRange)
		{
			auto wasHitBefore = [&](Entity *entity)
			{
				for (auto *hit : hitEntities)
				{
					if (hit == entity)
					{
						return true;
					}
				}
				return false;
			};

			float range2 = maxRange * maxRange;
			Entity *bestFreshEntity = nullptr;
			float bestFreshDist2 = range2;
			Entity *bestEntity = nullptr;
			float bestDist2 = range2;
			for (auto &e : entityHolder.entities)
			{
				if (e->dying) { continue; }
				glm::vec2 diff = e->physics.getPos() - origin;
				float dist2 = glm::dot(diff, diff);
				if (dist2 > range2)
				{
					continue;
				}

				if (!HasLineOfSightTiles(map, WorldToTile(origin), WorldToTile(e->physics.getPos())))
				{
					continue;
				}

				if (dist2 <= bestDist2)
				{
					bestDist2 = dist2;
					bestEntity = e.get();
				}

				if (!wasHitBefore(e.get()) && dist2 <= bestFreshDist2)
				{
					bestFreshDist2 = dist2;
					bestFreshEntity = e.get();
				}
			}
			if (bestFreshEntity)
			{
				return bestFreshEntity;
			}
			return bestEntity;
		};

		glm::vec2 spawnPos = lastBoltPos;
		glm::vec2 stepDir = chainDir;
		bool usedTarget = false;
		if (isFirstTime())
		{
			// First bolt keeps cast direction unless an enemy is almost on top of the player.
			queuedTarget = findNearbyTarget(lastBoltPos, firstBoltSnapRange);
		}
		else
		{
			queuedTarget = findNearbyTarget(lastBoltPos, targetRange);
		}
		if (pointerStillValid(queuedTarget))
		{
			glm::vec2 targetPos = queuedTarget->physics.getPos();
			glm::vec2 toTarget = targetPos - lastBoltPos;
			float targetDist = glm::length(toTarget);
			if (targetDist <= targetRange &&
				HasLineOfSightTiles(map, WorldToTile(lastBoltPos), WorldToTile(targetPos)))
			{
				spawnPos = targetPos;
				stepDir = normalizeSafe(toTarget);
				usedTarget = true;
			}
		}
		queuedTarget = nullptr;

		if (!usedTarget)
		{
			if (isFirstTime())
			{
				stepDir = chainDir;
			}
			else
			{
				stepDir = pickArcDirection();
			}
			float wantedDistance = getRandomFloat(rng, minBoltDistance, maxBoltDistance);
			float safeDistance = clampDistanceToWalls(lastBoltPos, stepDir, wantedDistance);
			spawnPos = lastBoltPos + stepDir * safeDistance;
		}

		// Emit connecting sparks to sell the electric arc between bolt nodes.
		glm::vec2 chainSegment = spawnPos - lastBoltPos;
		float segmentLength = glm::length(chainSegment);
		if (segmentLength > 0.0001f)
		{
			glm::vec2 segDir = chainSegment / segmentLength;
			glm::vec2 segPerp = {-segDir.y, segDir.x};
			int segmentCount = std::max(3, (int)std::ceil(segmentLength / (PIXEL_SIZE * 5.0f)));
			for (int i = 0; i <= segmentCount; i++)
			{
				float t = (float)i / (float)segmentCount;
				glm::vec2 p = glm::mix(lastBoltPos, spawnPos, t);
				if (i != 0 && i != segmentCount)
				{
					p += segPerp * getRandomFloat(rng, -PIXEL_SIZE * 2.2f, PIXEL_SIZE * 2.2f);
				}
				mainParticleSystem.emitParticles(linkParticle, p, rng, p);
			}
		}

		mainParticleSystem.emitParticles(nodeParticle, spawnPos, rng, spawnPos);
		mainParticleSystem.emitParticles(burstParticle, spawnPos, rng, spawnPos);

		// Each bolt can damage a primary target and an optional weaker secondary target.
		Entity *hitEntity = nullptr;
		Entity *secondHitEntity = nullptr;
		float bestHitDist2 = 999999.0f;
		float secondBestHitDist2 = 999999.0f;
		for (auto &e : entityHolder.entities)
		{
			if (e->dying) { continue; }
			float hitRange = hitRadius + std::max(e->physics.transform.size.x,
				e->physics.transform.size.y) * 0.35f;
			float hitRange2 = hitRange * hitRange;
			glm::vec2 diff = e->physics.getPos() - spawnPos;
			float dist2 = glm::dot(diff, diff);
			if (dist2 > hitRange2)
			{
				continue;
			}
			if (dist2 < bestHitDist2)
			{
				secondBestHitDist2 = bestHitDist2;
				secondHitEntity = hitEntity;
				bestHitDist2 = dist2;
				hitEntity = e.get();
			}
			else if (dist2 < secondBestHitDist2)
			{
				secondBestHitDist2 = dist2;
				secondHitEntity = e.get();
			}
		}

		auto applyHitToEntity = [&](Entity *target, float damage)
		{
			if (!target || damage <= 0.0f)
			{
				return;
			}

			HitStats hitStats;
			hitStats.damage = damage;
			hitStats.pushBack = 0.0f;
			glm::vec2 pushBack = {};
			target->life.computeHit(hitStats, element, target->element, stepDir, pushBack);
			if (hitStats.damage > 0.0f)
			{
				target->onDamaged(hitStats.damage);
			}
			target->physics.velocity += pushBack;
			if (statusAmount > 0.0f)
			{
				addStatusEffectFromElement(target->statusEffects, target->statusImmunities,
					element, statusAmount);
			}

			glm::vec2 damagePos = target->physics.getPos();
			damagePos.y -= target->physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(hitStats.damage, damagePos);

			bool alreadyStored = false;
			for (auto *hit : hitEntities)
			{
				if (hit == target)
				{
					alreadyStored = true;
					break;
				}
			}
			if (!alreadyStored)
			{
				hitEntities.push_back(target);
			}
		};

		if (hitEntity)
		{
			applyHitToEntity(hitEntity, boltDamage);
			if (secondHitEntity && secondHitEntity != hitEntity)
			{
				applyHitToEntity(secondHitEntity, secondaryDamage);
			}
		}

		queuedTarget = findNearbyTarget(spawnPos, targetRange);

		glm::vec2 forwardStep = spawnPos - lastBoltPos;
		if (glm::length(forwardStep) > 0.0001f)
		{
			chainDir = normalizeSafe(forwardStep);
		}
		lastBoltPos = spawnPos;

		return true;
	}
};

// Spawns projectiles into the standby ring around the player.
struct StandbyProjectilesSpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<Projectile> projectile;
	int standbyCount = 3;
	float standbyLifetime = 14.0f;
	float throwVelocity = 10.0f;
	bool hasStandbyEmission = false;
	ParticleEmissionSettings standbyEmission;
	bool hasSecondaryEmission = false;
	ParticleEmissionSettings secondaryEmission;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		auto &standbySystem = getStandbyProjectilesSystem();
		for (int i = 0; i < standbyCount; i++)
		{
			auto pptr = projectile->clone();
			pptr->element = element;
			standbySystem.addProjectileAsPtr(std::move(pptr), standbyLifetime,
				throwVelocity, hasStandbyEmission ? &standbyEmission : nullptr,
				hasSecondaryEmission ? &secondaryEmission : nullptr);
		}

		return true;
	}
};

// Spawns two sets of projectiles into the standby ring.
struct DualStandbyProjectilesSpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<Projectile> primaryProjectile;
	std::unique_ptr<Projectile> secondaryProjectile;
	int primaryCount = 3;
	int secondaryCount = 3;
	float primaryStandbyLifetime = 14.0f;
	float secondaryStandbyLifetime = 14.0f;
	float primaryThrowVelocity = 10.0f;
	float secondaryThrowVelocity = 10.0f;
	bool hasPrimaryEmission = false;
	ParticleEmissionSettings primaryEmission;
	bool hasSecondaryEmission = false;
	ParticleEmissionSettings secondaryEmission;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		auto &standbySystem = getStandbyProjectilesSystem();
		if (primaryProjectile)
		{
			for (int i = 0; i < primaryCount; i++)
			{
				auto pptr = primaryProjectile->clone();
				pptr->element = element;
				standbySystem.addProjectileAsPtr(std::move(pptr), primaryStandbyLifetime,
					primaryThrowVelocity, hasPrimaryEmission ? &primaryEmission : nullptr,
					nullptr);
			}
		}
		if (secondaryProjectile)
		{
			for (int i = 0; i < secondaryCount; i++)
			{
				auto pptr = secondaryProjectile->clone();
				pptr->element = element;
				standbySystem.addProjectileAsPtr(std::move(pptr), secondaryStandbyLifetime,
					secondaryThrowVelocity, hasSecondaryEmission ? &secondaryEmission : nullptr,
					nullptr);
			}
		}

		return true;
	}
};

// Spawns a summon that follows and aids the player.
struct SummonSpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<SummonEntity> summon;
	int summonCount = 1;
	float spawnOffsetMin = 0.6f;
	float spawnOffsetMax = 1.2f;
	float spawnSideAngleJitter = 0.65f;
	float spawnForwardJitter = 0.25f;
	int spawnAttempts = 10;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)mainParticleSystem;
		(void)projectileHolder;
		(void)entityHolder;

		if (!summon)
		{
			return true;
		}

		glm::vec2 aim = currentAimDir;
		float len = glm::length(aim);
		if (len <= 0.0001f)
		{
			len = glm::length(createAimDir);
			if (len <= 0.0001f)
			{
				aim = {1.0f, 0.0f};
				len = 1.0f;
			}
			else
			{
				aim = createAimDir;
			}
		}
		aim /= len;
		glm::vec2 sideDir = {-aim.y, aim.x};

		glm::vec2 playerPos = player.physics.getPos();
		glm::ivec2 playerTile = WorldToTile(playerPos);

		auto isBlocked = [&](const glm::ivec2 &tile)
		{
			if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
			{
				return true;
			}
			return map.isCollidableAtPosSafe(tile.x, tile.y);
		};

		// Spawn to the side with LOS, avoiding walls.
		auto findSpawnPos = [&](int sideSign, glm::vec2 &outPos)
		{
			glm::vec2 side = sideDir * (float)sideSign;
			float baseAngle = std::atan2(side.y, side.x);
			for (int tries = 0; tries < spawnAttempts; tries++)
			{
				float angle = baseAngle + getRandomFloat(rng, -spawnSideAngleJitter, spawnSideAngleJitter);
				float radius = getRandomFloat(rng, spawnOffsetMin, spawnOffsetMax);
				glm::vec2 offset = glm::vec2(std::cos(angle), std::sin(angle)) * radius;
				offset += aim * getRandomFloat(rng, -spawnForwardJitter, spawnForwardJitter);
				glm::vec2 spawnPos = playerPos + offset;

				glm::ivec2 spawnTile = WorldToTile(spawnPos);
				if (isBlocked(spawnTile)) { continue; }
				if (!HasLineOfSightGrid(map, playerTile, spawnTile)) { continue; }

				outPos = spawnPos;
				return true;
			}
			return false;
		};

		auto &summonHolder = getSummonHolder();
		for (int i = 0; i < summonCount; i++)
		{
			glm::vec2 spawnPos = playerPos;
			int sideSign = (summonCount > 1) ? ((i % 2 == 0) ? 1 : -1)
				: (getRandomChance(rng, 0.5f) ? 1 : -1);
			if (!findSpawnPos(sideSign, spawnPos))
			{
				if (!findSpawnPos(-sideSign, spawnPos))
				{
					continue;
				}
			}

			auto sptr = summon->clone();
			summonHolder.addSummonAsPtr(std::move(sptr), spawnPos);
		}

		return true;
	}
};

// Fires a fixed-direction volley of projectiles.
struct HomingVolleySpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<Projectile> projectile;
	std::vector<glm::vec2> directions;
	float throwVelocity = 6.0f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		if (!projectile)
		{
			return true;
		}

		for (auto dir : directions)
		{
			float len = glm::length(dir);
			if (len <= 0.0001f)
			{
				dir = {1.0f, 0.0f};
			}
			else
			{
				dir /= len;
			}

			auto pptr = projectile->clone();
			pptr->element = element;
			pptr->physics.velocity = dir * throwVelocity;
			projectileHolder.addProjectileAsPtr(std::move(pptr), player.physics.getPos());
		}

		return true;
	}
};

// Triple earth shot that fires fixed-angle ricocheting projectiles.
struct TripleEarthRicochetSpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<Projectile> projectile;
	float throwVelocity = 13.5f;
	float angleA = -30.0f;
	float angleB = 30.0f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		float len = glm::length(currentAimDir);
		if (len <= 0.0001f)
		{
			len = glm::length(createAimDir);
			currentAimDir = len <= 0.0001f ? glm::vec2{1.0f, 0.0f} : createAimDir;
		}
		currentAimDir /= glm::length(currentAimDir);

		auto rotateVec = [&](glm::vec2 v, float degrees)
		{
			float rad = degrees * (3.1415926f / 180.0f);
			float cs = std::cos(rad);
			float sn = std::sin(rad);
			return glm::vec2{v.x * cs - v.y * sn, v.x * sn + v.y * cs};
		};

		glm::vec2 dirs[] = {
			currentAimDir,
			rotateVec(currentAimDir, angleA),
			rotateVec(currentAimDir, angleB)
		};

		for (auto &dir : dirs)
		{
			auto pptr = projectile->clone();
			pptr->element = element;
			pptr->physics.velocity = dir * throwVelocity;
			projectileHolder.addProjectileAsPtr(std::move(pptr), player.physics.getPos());
		}

		return true;
	}
};

struct FlameWallSpell: public Spell
{
	// **configuration variables**
	std::unique_ptr<Projectile> projectile;
	float wallOffset = 1.2f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		auto pptr = projectile->clone();
		pptr->element = element;

		glm::vec2 aim = currentAimDir;
		float len = glm::length(aim);
		if (len <= 0.0001f)
		{
			aim = {1.0f, 0.0f};
			len = 1.0f;
		}
		aim /= len;

		if (auto wall = dynamic_cast<ElementWallProjectile *>(pptr.get()))
		{
			wall->setupWall(aim);
		}

		glm::vec2 spawnPos = player.physics.getPos() + aim * wallOffset;
		projectileHolder.addProjectileAsPtr(std::move(pptr), spawnPos);
		return true;
	}
};

// Spawns a wall of thorn projectiles in front of the player.
struct ThornWallSpell: public Spell
{
	// **configuration variables**
	int thornCount = 15;
	float wallLength = 5.0f;
	float wallOffset = 1.2f;
	float offsetJitter = 0.25f;
	float forwardJitter = 0.2f;
	float particleBurstCount = 18.0f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		glm::vec2 aim = currentAimDir;
		float len = glm::length(aim);
		if (len <= 0.0001f)
		{
			len = glm::length(createAimDir);
			if (len <= 0.0001f)
			{
				aim = {1.0f, 0.0f};
				len = 1.0f;
			}
			else
			{
				aim = createAimDir;
			}
		}
		aim /= len;

	glm::vec2 axis = {-aim.y, aim.x};
	float spacing = thornCount > 1 ? (wallLength / (thornCount - 1)) : 0.0f;
	glm::vec2 origin = player.physics.getPos() + aim * wallOffset;

	glm::vec4 startColor = elementToSecondaryColor(Elements::Earth);
	glm::vec4 endColor = elementToColor(Elements::Earth);
	startColor.g *= 0.8f;
	endColor.g *= 0.8f;
	ParticleSettings burst = getSmallSquareParticle(startColor, endColor);
	burst.onCreateCount = (short)particleBurstCount;
	burst.particleLifeTime = {0.25f, 0.4f};
	burst.velocityX = glm::vec2{-10.0f, 10.0f} * PIXEL_SIZE;
	burst.velocityY = glm::vec2{-10.0f, 10.0f} * PIXEL_SIZE;
	burst.createApearence.size = glm::vec2{2.2f, 3.0f} * PIXEL_SIZE;
	burst.endApearence.size = glm::vec2{0.8f, 1.4f} * PIXEL_SIZE;
	burst.folowParent = false;

	auto isBlocked = [&](glm::vec2 pos)
	{
		int tx = (int)std::floor(pos.x);
		int ty = (int)std::floor(pos.y);
		return map.isCollidableAtPosSafe(tx, ty);
	};

	auto trySpawn = [&](glm::vec2 basePos)
	{
		if (isBlocked(basePos))
		{
			return false;
		}

		float jitterSide = getRandomFloat(rng, -offsetJitter, offsetJitter);
		float jitterForward = getRandomFloat(rng, -forwardJitter, forwardJitter);
		glm::vec2 spawnPos = basePos + axis * jitterSide + aim * jitterForward;
		if (isBlocked(spawnPos))
		{
			return true;
		}

		auto thorn = std::make_unique<ThornProjectile>();
		thorn->element = Elements::Earth;
		projectileHolder.addProjectileAsPtr(std::move(thorn), spawnPos);
		mainParticleSystem.emitParticles(burst, spawnPos, rng, spawnPos);
		return true;
	};

	int spawned = 0;
	bool stopNeg = false;
	bool stopPos = false;

	if (trySpawn(origin))
	{
		spawned++;
	}
	else
	{
		return true;
	}

	for (int step = 1; spawned < thornCount && (!stopNeg || !stopPos); step++)
	{
		float along = spacing * step;
		if (!stopNeg && spawned < thornCount)
		{
			glm::vec2 basePos = origin - axis * along;
			if (!trySpawn(basePos))
			{
				stopNeg = true;
			}
			else
			{
				spawned++;
			}
		}

		if (!stopPos && spawned < thornCount)
		{
			glm::vec2 basePos = origin + axis * along;
			if (!trySpawn(basePos))
			{
				stopPos = true;
			}
			else
			{
				spawned++;
			}
		}
	}

		return true;
	}
};

// Rapidly grows thorns outward from the player.
struct WildGrowthSpell: public Spell
{
	// **configuration variables**
	int maxThorns = 60;
	int wormCount = 14;
	float maxDistance = 10.0f;
	float maxDuration = 2.0f;
	float spawnInterval = 0.004f;
	float offsetJitter = 0.25f;

	// **state variables**
	bool initialized = false;
	float spawnTimer = 0.0f;
	int placedCount = 0;
	int wormIndex = 0;
	glm::ivec2 originTile = {0, 0};
	std::vector<glm::ivec2> worms;
	std::vector<glm::ivec2> placedTiles;

	WildGrowthSpell()
	{
		continuousUpdate = true;
		continuousUpdateTimer = 2.0f;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)currentAimDir;

		if (!initialized)
		{
			initialized = true;
			continuousUpdateTimer = maxDuration;
			originTile = WorldToTile(player.physics.getPos());
			worms.assign(wormCount, originTile);
			placedTiles.clear();
			spawnTimer = 0.0f;
		}

	if (placedCount >= maxThorns)
	{
		return false;
	}

	auto tileWithinRange = [&](const glm::ivec2 &tile)
	{
		glm::vec2 delta = glm::vec2(tile - originTile);
		return glm::dot(delta, delta) <= maxDistance * maxDistance;
	};

	auto isBlocked = [&](const glm::ivec2 &tile)
	{
		if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
		{
			return true;
		}
		return map.isCollidableAtPosSafe(tile.x, tile.y);
	};

	auto hasLocalThorn = [&](const glm::ivec2 &tile)
	{
		for (auto &t : placedTiles)
		{
			if (t == tile) { return true; }
		}
		return false;
	};

	auto hasWorldThorn = [&](const glm::ivec2 &tile)
	{
		for (auto &p : projectileHolder.projectiles)
		{
			if (dynamic_cast<ThornProjectile *>(p.get()))
			{
				glm::ivec2 pt = WorldToTile(p->physics.getPos());
				if (pt == tile) { return true; }
			}
		}
		for (auto &p : projectileHolder.pendingProjectiles)
		{
			if (dynamic_cast<ThornProjectile *>(p.get()))
			{
				glm::ivec2 pt = WorldToTile(p->physics.getPos());
				if (pt == tile) { return true; }
			}
		}
		return false;
	};

	const glm::ivec2 directions[] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1},
		{1, 1}, {-1, 1}, {1, -1}, {-1, -1}
	};

	// If a worm is boxed in, search deeper through existing thorns to reach open tiles.
	auto findStepTowardOpenTile = [&](const glm::ivec2 &start, glm::ivec2 &outStep)
	{
		int range = (int)std::ceil(maxDistance);
		int minX = std::max(0, originTile.x - range);
		int minY = std::max(0, originTile.y - range);
		int maxX = std::min(map.size.x - 1, originTile.x + range);
		int maxY = std::min(map.size.y - 1, originTile.y + range);
		int width = maxX - minX + 1;
		int height = maxY - minY + 1;
		if (width <= 0 || height <= 0)
		{
			return false;
		}

		auto toIndex = [&](const glm::ivec2 &tile)
		{
			return (tile.x - minX) + (tile.y - minY) * width;
		};
		auto toTile = [&](int index)
		{
			return glm::ivec2{index % width + minX, index / width + minY};
		};

		std::vector<int> parent(width * height, -1);
		std::vector<glm::ivec2> queue;
		queue.reserve(width * height);
		int startIndex = toIndex(start);
		parent[startIndex] = startIndex;
		queue.push_back(start);

		glm::ivec2 target = start;
		bool found = false;
		int head = 0;
		while (head < (int)queue.size())
		{
			glm::ivec2 current = queue[head++];
			if (current != start && tileWithinRange(current) && !isBlocked(current))
			{
				if (!hasLocalThorn(current) && !hasWorldThorn(current))
				{
					target = current;
					found = true;
					break;
				}
			}

			for (int i = 0; i < 8; i++)
			{
				glm::ivec2 next = current + directions[i];
				if (next.x < minX || next.y < minY || next.x > maxX || next.y > maxY)
				{
					continue;
				}
				int nextIndex = toIndex(next);
				if (parent[nextIndex] != -1) { continue; }
				if (!tileWithinRange(next)) { continue; }
				if (isBlocked(next)) { continue; }
				parent[nextIndex] = toIndex(current);
				queue.push_back(next);
			}
		}

		if (!found)
		{
			return false;
		}

		int targetIndex = toIndex(target);
		int currentIndex = targetIndex;
		while (parent[currentIndex] != startIndex && currentIndex != startIndex)
		{
			currentIndex = parent[currentIndex];
		}
		if (currentIndex == startIndex)
		{
			return false;
		}
		outStep = toTile(currentIndex);
		return true;
	};

	auto spawnThorn = [&](const glm::ivec2 &tile)
	{
		if (placedCount >= maxThorns) { return; }
		if (hasLocalThorn(tile) || hasWorldThorn(tile)) { return; }

		auto thorn = std::make_unique<ThornProjectile>();
		thorn->element = Elements::Earth;
		thorn->hitStats.damage = 3.0f;
		glm::vec2 spawnPos = glm::vec2(tile) + glm::vec2(0.5f);
		spawnPos.x += getRandomFloat(rng, -offsetJitter, offsetJitter);
		spawnPos.y += getRandomFloat(rng, -offsetJitter, offsetJitter);
		projectileHolder.addProjectileDeferredAsPtr(std::move(thorn), spawnPos);
		placedTiles.push_back(tile);
		placedCount++;

		glm::vec4 startColor = elementToSecondaryColor(Elements::Earth);
		glm::vec4 endColor = elementToColor(Elements::Earth);
		startColor.g *= 0.8f;
		endColor.g *= 0.8f;
		auto burst = getSmallSquareParticle(startColor, endColor);
		burst.onCreateCount = 3;
		burst.particleLifeTime = {0.25f, 0.4f};
		burst.velocityX = glm::vec2{-8.0f, 8.0f} * PIXEL_SIZE;
		burst.velocityY = glm::vec2{-8.0f, 8.0f} * PIXEL_SIZE;
		burst.createApearence.size = glm::vec2{2.0f, 2.8f} * PIXEL_SIZE;
		burst.endApearence.size = glm::vec2{0.6f, 1.2f} * PIXEL_SIZE;
		burst.folowParent = false;
		mainParticleSystem.emitParticles(burst, spawnPos, rng, spawnPos);
	};

	spawnTimer -= deltaTime;
	while (spawnTimer <= 0.0f)
	{
		spawnTimer += spawnInterval;
		if (placedCount >= maxThorns) { break; }
		if (worms.empty()) { break; }

		int index = wormIndex % (int)worms.size();
		wormIndex++;
		glm::ivec2 current = worms[index];

		bool moved = false;
		glm::ivec2 bestNext = current;
		int bestScore = -999;
		for (int i = 0; i < 8; i++)
		{
			glm::ivec2 next = current + directions[i];
			if (!tileWithinRange(next)) { continue; }
			if (isBlocked(next)) { continue; }
			if (hasLocalThorn(next)) { continue; }
			int score = 0;
			for (int j = 0; j < 8; j++)
			{
				glm::ivec2 neighbor = next + directions[j];
				if (!tileWithinRange(neighbor)) { continue; }
				if (isBlocked(neighbor)) { continue; }
				if (hasLocalThorn(neighbor)) { continue; }
				score++;
			}
			if (score > bestScore)
			{
				bestScore = score;
				bestNext = next;
				moved = true;
			}
		}

		if (!moved)
		{
			glm::ivec2 step = current;
			if (findStepTowardOpenTile(current, step))
			{
				current = step;
				moved = true;
			}
			else
			{
				for (int tries = 0; tries < 8; tries++)
				{
					int pick = getRandomInt(rng, 0, 7);
					glm::ivec2 next = current + directions[pick];
					if (!tileWithinRange(next)) { continue; }
					if (isBlocked(next)) { continue; }
					current = next;
					moved = true;
					break;
				}
			}
		}
		else
		{
			current = bestNext;
		}

		if (moved)
		{
			worms[index] = current;
			spawnThorn(current);
		}
	}

		return true;
	}
};

// Instantly sprouts a tight patch of thorns around the player.
struct EarthTrapSpell: public Spell
{
	// **configuration variables**
	int thornCount = 15;
	float minRadius = 0.35f;
	float maxRadius = 1.35f;
	float offsetJitter = 0.12f;
	int spawnAttempts = 10;
	float particleBurstCount = 3.0f;
	float thornDamage = 1.0f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)currentAimDir;

		glm::vec2 origin = player.physics.getPos();
		glm::ivec2 originTile = WorldToTile(origin);

		auto isBlocked = [&](const glm::ivec2 &tile)
		{
			if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
			{
				return true;
			}
			return map.isCollidableAtPosSafe(tile.x, tile.y);
		};

		auto hasLocalThorn = [&](const glm::ivec2 &tile, const std::vector<glm::ivec2> &placedTiles)
		{
			for (auto &t : placedTiles)
			{
				if (t == tile) { return true; }
			}
			return false;
		};

		glm::vec4 startColor = elementToSecondaryColor(Elements::Earth);
		glm::vec4 endColor = elementToColor(Elements::Earth);
		startColor.g *= 0.8f;
		endColor.g *= 0.8f;
		auto burst = getSmallSquareParticle(startColor, endColor);
		burst.onCreateCount = (short)particleBurstCount;
		burst.particleLifeTime = {0.25f, 0.4f};
		burst.velocityX = glm::vec2{-8.0f, 8.0f} * PIXEL_SIZE;
		burst.velocityY = glm::vec2{-8.0f, 8.0f} * PIXEL_SIZE;
		burst.createApearence.size = glm::vec2{2.0f, 2.8f} * PIXEL_SIZE;
		burst.endApearence.size = glm::vec2{0.6f, 1.2f} * PIXEL_SIZE;
		burst.folowParent = false;

		std::vector<glm::ivec2> placedTiles;
		placedTiles.reserve(thornCount);

		const float twoPi = 6.2831853f;
		for (int i = 0; i < thornCount; i++)
		{
			bool spawned = false;
			for (int tries = 0; tries < spawnAttempts && !spawned; tries++)
			{
				float angle = getRandomFloat(rng, 0.0f, twoPi);
				float radius = minRadius + (maxRadius - minRadius)
					* std::sqrt(getRandomFloat(rng, 0.0f, 1.0f));
				glm::vec2 offset = glm::vec2(std::cos(angle), std::sin(angle)) * radius;
				offset.x += getRandomFloat(rng, -offsetJitter, offsetJitter);
				offset.y += getRandomFloat(rng, -offsetJitter, offsetJitter);
				glm::vec2 spawnPos = origin + offset;

				glm::ivec2 spawnTile = WorldToTile(spawnPos);
				if (isBlocked(spawnTile)) { continue; }
				if (hasLocalThorn(spawnTile, placedTiles)) { continue; }
				if (!HasLineOfSightGrid(map, originTile, spawnTile)) { continue; }

				auto thorn = std::make_unique<ThornProjectile>();
				thorn->element = Elements::Earth;
				thorn->hitStats.damage = thornDamage;
				projectileHolder.addProjectileDeferredAsPtr(std::move(thorn), spawnPos);
				mainParticleSystem.emitParticles(burst, spawnPos, rng, spawnPos);
				placedTiles.push_back(spawnTile);
				spawned = true;
			}
		}

		return true;
	}
};

// Persistent trap that zaps nearby enemies with water-element lightning charges.
struct StormTrapSpell: public Spell
{
	// **configuration variables**
	float trapDuration = 45.0f;
	int maxCharges = 5;
	float trapRadius = 1.5f;
	float zapDamage = 5.0f;
	float zapCooldownMin = 0.35f;
	float zapCooldownMax = 0.75f;
	float zapRollInterval = 0.08f;
	float zapChance = 0.56f;
	float placementDistance = 3.0f;
	float placementStep = 0.2f;
	float ringEmitInterval = 0.12f;
	float orbitEmitInterval = 0.18f;
	float ringStep = 0.28f;
	float randomLightningInterval = 0.12f;
	int randomLightningBursts = 3;
	int hitLightningBursts = 7;
	int hitArcRepeats = 4;
	float chainDamage = 0.0f;
	float chainRange = 2.8f;
	float particleSizeScale = 1.0f;
	float particleCountScale = 1.0f;
	float arcWaveAmplitude = PIXEL_SIZE * 2.1f;
	float arcWaveFrequency = 2.2f;
	float arcWaveSpeed = 3.2f;

	// **state variables**
	bool initialized = false;
	bool trapDepleted = false;
	int chargesLeft = 0;
	float zapCooldownTimer = 0.0f;
	float zapRollTimer = 0.0f;
	float ringTimer = 0.0f;
	float orbitTimer = 0.0f;
	float randomLightningTimer = 0.0f;
	float ringRotation = 0.0f;
	float ringSpinSpeed = 0.0f;
	float arcWaveTime = 0.0f;
	glm::vec2 trapPos = {};
	glm::vec2 trapDir = {1.0f, 0.0f};
	ParticleSettings ringParticle;
	ParticleSettings orbitParticle;
	ParticleSettings zapTrailParticle;
	ParticleSettings zapBurstParticle;
	// Local particle pool so this spell does not fight the global cap.
	ParticleSystem stormParticles;

	StormTrapSpell()
	{
		continuousUpdate = true;
		continuousUpdateTimer = trapDuration;
		stormParticles.maxCount = 2800;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)projectileHolder;
		(void)mainParticleSystem;

		auto normalizeSafe = [](glm::vec2 v)
		{
			float l = glm::length(v);
			if (l <= 0.0001f)
			{
				return glm::vec2(1.0f, 0.0f);
			}
			return v / l;
		};

		if (!initialized)
		{
			initialized = true;
			trapDepleted = false;
			continuousUpdateTimer = trapDuration;
			chargesLeft = std::max(1, maxCharges);
			stormParticles.particles.clear();
			arcWaveTime = getRandomFloat(rng, 0.0f, 6.2831853f);

			glm::vec2 aim = currentAimDir;
			if (glm::length(aim) <= 0.0001f)
			{
				aim = createAimDir;
			}
			trapDir = normalizeSafe(aim);

			// Keep storm center at player position on spawn.
			trapPos = player.physics.getPos();

			glm::vec4 waterStart = elementToSecondaryColor(Elements::Water);
			glm::vec4 waterEnd = elementToColor(Elements::Water);
			waterStart.a = 0.6f;
			waterEnd.a = 0.35f;

			glm::vec4 lightningStart = {0.72f, 1.0f, 1.0f, 0.95f};
			glm::vec4 lightningEnd = {0.35f, 0.85f, 1.0f, 0.55f};

			ringParticle = getOrbitParticle(waterStart, waterEnd);
			ringParticle.onCreateCount = 3;
			ringParticle.particleLifeTime = {0.2f, 0.38f};
			ringParticle.velocityX = {0.0f, 0.0f};
			ringParticle.velocityY = {0.0f, 0.0f};
			ringParticle.dragX = {0.0f, 0.0f};
			ringParticle.dragY = {0.0f, 0.0f};
			ringParticle.createApearence.size = glm::vec2{1.25f, 3.05f} * PIXEL_SIZE;
			ringParticle.endApearence.size = glm::vec2{0.5f, 1.4f} * PIXEL_SIZE;
			ringParticle.animationScaleX = {PIXEL_SIZE * 2.45f, PIXEL_SIZE * 5.1f};
			ringParticle.animationScaleY = {PIXEL_SIZE * 2.45f, PIXEL_SIZE * 5.1f};
			ringParticle.animationSpeed = {-8.0f, 8.0f};
			ringParticle.folowParent = false;

			orbitParticle = getLightningZapParticle(lightningStart, lightningEnd);
			orbitParticle.onCreateCount = 4;
			orbitParticle.particleLifeTime = {0.08f, 0.15f};
			orbitParticle.velocityX = glm::vec2{-15.0f, 15.0f} * PIXEL_SIZE;
			orbitParticle.velocityY = glm::vec2{-15.0f, 15.0f} * PIXEL_SIZE;
			orbitParticle.createApearence.size = glm::vec2{0.7f, 2.4f} * PIXEL_SIZE;
			orbitParticle.endApearence.size = glm::vec2{0.25f, 0.9f} * PIXEL_SIZE;
			orbitParticle.animationType = ParticleSettings::ANIMATION_TYPES::animationZigZag;
			orbitParticle.animationSpeed = {-12.0f, 12.0f};
			orbitParticle.animationScaleX = {PIXEL_SIZE * 1.2f, PIXEL_SIZE * 4.5f};
			orbitParticle.animationScaleY = {PIXEL_SIZE * 1.2f, PIXEL_SIZE * 4.5f};
			orbitParticle.folowParent = false;

			zapTrailParticle = getLightningZapParticle(lightningStart, lightningEnd);
			zapTrailParticle.onCreateCount = 2;
			zapTrailParticle.particleLifeTime = {0.06f, 0.12f};
			zapTrailParticle.velocityX = glm::vec2{-18.0f, 18.0f} * PIXEL_SIZE;
			zapTrailParticle.velocityY = glm::vec2{-18.0f, 18.0f} * PIXEL_SIZE;
			zapTrailParticle.createApearence.size = glm::vec2{0.75f, 2.8f} * PIXEL_SIZE;
			zapTrailParticle.endApearence.size = glm::vec2{0.2f, 0.9f} * PIXEL_SIZE;
			zapTrailParticle.animationType = ParticleSettings::ANIMATION_TYPES::animationZigZag;
			zapTrailParticle.animationSpeed = {-14.0f, 14.0f};
			zapTrailParticle.animationScaleX = {PIXEL_SIZE * 1.2f, PIXEL_SIZE * 4.6f};
			zapTrailParticle.animationScaleY = {PIXEL_SIZE * 1.2f, PIXEL_SIZE * 4.6f};
			zapTrailParticle.folowParent = false;

			zapBurstParticle = getSparkBurstParticle(lightningStart, lightningEnd);
			zapBurstParticle.onCreateCount = 14;
			zapBurstParticle.particleLifeTime = {0.14f, 0.24f};
			zapBurstParticle.velocityX = glm::vec2{-22.0f, 22.0f} * PIXEL_SIZE;
			zapBurstParticle.velocityY = glm::vec2{-22.0f, 22.0f} * PIXEL_SIZE;
			zapBurstParticle.createApearence.size = glm::vec2{0.8f, 2.2f} * PIXEL_SIZE;
			zapBurstParticle.endApearence.size = glm::vec2{0.3f, 1.0f} * PIXEL_SIZE;
			zapBurstParticle.texture = getAssetManager().particleCircle;
			zapBurstParticle.folowParent = false;

			auto scaleStormParticle = [&](ParticleSettings &p)
			{
				if (particleCountScale != 1.0f)
				{
					int count = (int)std::round((float)p.onCreateCount * particleCountScale);
					p.onCreateCount = (short)std::max(1, count);
				}
				if (particleSizeScale != 1.0f)
				{
					p.createApearence.size *= particleSizeScale;
					p.endApearence.size *= particleSizeScale;
					p.animationScaleX *= particleSizeScale;
					p.animationScaleY *= particleSizeScale;
				}
			};

			scaleStormParticle(ringParticle);
			scaleStormParticle(orbitParticle);
			scaleStormParticle(zapTrailParticle);
			scaleStormParticle(zapBurstParticle);

			ringSpinSpeed = getRandomFloat(rng, 0.65f, 1.2f);
			if (getRandomChance(rng, 0.5f)) { ringSpinSpeed *= -1.0f; }
			ringTimer = getRandomFloat(rng, 0.0f, ringEmitInterval);
			orbitTimer = getRandomFloat(rng, 0.0f, orbitEmitInterval);
			randomLightningTimer = getRandomFloat(rng, 0.0f, randomLightningInterval);
			zapRollTimer = getRandomFloat(rng, 0.0f, zapRollInterval);
			zapCooldownTimer = 0.0f;

			stormParticles.emitParticles(zapBurstParticle, trapPos, rng, trapPos);
		}

		if (trapDepleted)
		{
			stormParticles.update(deltaTime);
			return !stormParticles.particles.empty();
		}

		arcWaveTime += deltaTime * arcWaveSpeed;

		// Shared helper for arc-shaped lightning with a slow animated wave.
		auto emitArcBetween = [&](glm::vec2 start, glm::vec2 end, float randomJitter)
		{
			glm::vec2 segment = end - start;
			float segmentLength = glm::length(segment);
			if (segmentLength <= 0.0001f)
			{
				return;
			}

			glm::vec2 segDir = segment / segmentLength;
			glm::vec2 segPerp = {-segDir.y, segDir.x};
			int segmentCount = std::max(3,
				(int)std::ceil(segmentLength / (PIXEL_SIZE * 5.0f)));
			float phaseOffset = getRandomFloat(rng, 0.0f, 6.2831853f);
			for (int i = 0; i <= segmentCount; i++)
			{
				float t = (float)i / (float)segmentCount;
				glm::vec2 p = glm::mix(start, end, t);
				if (i != 0 && i != segmentCount)
				{
					float wave = std::sin(t * 6.2831853f * arcWaveFrequency
						+ arcWaveTime + phaseOffset) * arcWaveAmplitude;
					p += segPerp * wave;
					p += segPerp * getRandomFloat(rng,
						-PIXEL_SIZE * randomJitter, PIXEL_SIZE * randomJitter);
				}
				stormParticles.emitParticles(zapTrailParticle, p, rng, p);
			}
		};

		ringRotation += ringSpinSpeed * deltaTime;

		ringTimer -= deltaTime;
		if (ringTimer <= 0.0f)
		{
			ringTimer += ringEmitInterval;
			const float twoPi = 6.2831853f;
			for (float i = 0.0f; i < twoPi; i += ringStep)
			{
				float angle = i + ringRotation;
				glm::vec2 p = trapPos + glm::vec2(std::cos(angle), std::sin(angle)) * trapRadius;
				if (HasLineOfSightGrid(map, trapPos, p))
				{
					stormParticles.emitParticles(ringParticle, p, rng, p);
				}
			}
		}

		orbitTimer -= deltaTime;
		if (orbitTimer <= 0.0f)
		{
			orbitTimer += orbitEmitInterval;
			stormParticles.emitParticles(orbitParticle, trapPos, rng, trapPos);
		}

		// Random interior lightning keeps the storm looking active even between zaps.
		randomLightningTimer -= deltaTime;
		while (randomLightningTimer <= 0.0f)
		{
			randomLightningTimer += randomLightningInterval;

			for (int i = 0; i < randomLightningBursts; i++)
			{
				float angle = getRandomFloat(rng, 0.0f, 6.2831853f);
				float radius = std::sqrt(getRandomFloat(rng, 0.0f, 1.0f)) * trapRadius * 0.95f;
				glm::vec2 center = trapPos + glm::vec2(std::cos(angle), std::sin(angle)) * radius;

				stormParticles.emitParticles(orbitParticle, center, rng, center);
				if (getRandomChance(rng, 0.6f))
				{
					stormParticles.emitParticles(zapBurstParticle, center, rng, center);
				}

				float angle2 = getRandomFloat(rng, 0.0f, 6.2831853f);
				float radius2 = std::sqrt(getRandomFloat(rng, 0.0f, 1.0f)) * trapRadius * 0.95f;
				glm::vec2 end = trapPos + glm::vec2(std::cos(angle2), std::sin(angle2)) * radius2;
				emitArcBetween(center, end, 1.2f);
			}
		}

		if (zapCooldownTimer > 0.0f)
		{
			zapCooldownTimer -= deltaTime;
		}

		Entity *bestTarget = nullptr;
		float bestDist2 = 999999.0f;
		for (auto &e : entityHolder.entities)
		{
			if (e->dying) { continue; }
			float maxDist = trapRadius + std::max(e->physics.transform.size.x,
				e->physics.transform.size.y) * 0.45f;
			float maxDist2 = maxDist * maxDist;
			glm::vec2 diff = e->physics.getPos() - trapPos;
			float dist2 = glm::dot(diff, diff);
			if (dist2 > maxDist2) { continue; }
			if (!HasLineOfSightGrid(map, trapPos, e->physics.getPos())) { continue; }
			if (dist2 < bestDist2)
			{
				bestDist2 = dist2;
				bestTarget = e.get();
			}
		}

		if (bestTarget && chargesLeft > 0 && zapCooldownTimer <= 0.0f)
		{
			zapRollTimer -= deltaTime;
			if (zapRollTimer <= 0.0f)
			{
				zapRollTimer += zapRollInterval;
				if (getRandomChance(rng, zapChance))
				{
					glm::vec2 hitDir = bestTarget->physics.getPos() - trapPos;
					if (glm::length2(hitDir) <= 0.0001f)
					{
						hitDir = trapDir;
					}

					HitStats hitStats;
					hitStats.damage = zapDamage;
					hitStats.pushBack = 0.0f;
					glm::vec2 pushBack = {};
					bestTarget->life.computeHit(hitStats, Elements::Water, bestTarget->element,
						hitDir, pushBack);
					if (hitStats.damage > 0.0f)
					{
						bestTarget->onDamaged(hitStats.damage);
					}
					bestTarget->physics.velocity += pushBack;

					glm::vec2 damagePos = bestTarget->physics.getPos();
					damagePos.y -= bestTarget->physics.transform.size.y * 0.6f;
					getDamageViewerSystem().addDamage(hitStats.damage, damagePos);

					stormParticles.emitParticles(zapBurstParticle, trapPos, rng, trapPos);
					stormParticles.emitParticles(zapBurstParticle,
						bestTarget->physics.getPos(), rng, bestTarget->physics.getPos());

					// On hit, throw a lot of extra lightning around the victim.
					for (int burst = 0; burst < hitLightningBursts; burst++)
					{
						glm::vec2 targetCenter = bestTarget->physics.getPos();
						glm::vec2 burstPos = targetCenter + glm::vec2(
							getRandomFloat(rng, -PIXEL_SIZE * 5.5f, PIXEL_SIZE * 5.5f),
							getRandomFloat(rng, -PIXEL_SIZE * 5.5f, PIXEL_SIZE * 5.5f)
						);
						stormParticles.emitParticles(zapBurstParticle, burstPos, rng, burstPos);
						if (getRandomChance(rng, 0.8f))
						{
							stormParticles.emitParticles(orbitParticle, burstPos, rng, burstPos);
						}
					}

					glm::vec2 targetPos = bestTarget->physics.getPos();
					for (int arc = 0; arc < hitArcRepeats; arc++)
					{
						glm::vec2 arcEnd = targetPos + glm::vec2(
							getRandomFloat(rng, -PIXEL_SIZE * 3.0f, PIXEL_SIZE * 3.0f),
							getRandomFloat(rng, -PIXEL_SIZE * 3.0f, PIXEL_SIZE * 3.0f)
						);
						emitArcBetween(trapPos, arcEnd, 1.4f);
					}

					// Optional secondary chain from primary victim to a nearby enemy.
					if (chainDamage > 0.0f)
					{
						Entity *chainTarget = nullptr;
						float bestChainDist2 = chainRange * chainRange;
						for (auto &e : entityHolder.entities)
						{
							if (e->dying || e.get() == bestTarget) { continue; }
							glm::vec2 diff = e->physics.getPos() - targetPos;
							float dist2 = glm::dot(diff, diff);
							if (dist2 > bestChainDist2) { continue; }
							if (!HasLineOfSightGrid(map, targetPos, e->physics.getPos())) { continue; }
							bestChainDist2 = dist2;
							chainTarget = e.get();
						}

						if (chainTarget)
						{
							glm::vec2 chainDir = chainTarget->physics.getPos() - targetPos;
							if (glm::length2(chainDir) <= 0.0001f)
							{
								chainDir = trapDir;
							}

							HitStats chainHit;
							chainHit.damage = chainDamage;
							chainHit.pushBack = 0.0f;
							glm::vec2 chainPushBack = {};
							chainTarget->life.computeHit(chainHit, Elements::Water, chainTarget->element,
								chainDir, chainPushBack);
							if (chainHit.damage > 0.0f)
							{
								chainTarget->onDamaged(chainHit.damage);
							}
							chainTarget->physics.velocity += chainPushBack;

							glm::vec2 chainDamagePos = chainTarget->physics.getPos();
							chainDamagePos.y -= chainTarget->physics.transform.size.y * 0.6f;
							getDamageViewerSystem().addDamage(chainHit.damage, chainDamagePos);

							stormParticles.emitParticles(zapBurstParticle, chainTarget->physics.getPos(),
								rng, chainTarget->physics.getPos());
							for (int arc = 0; arc < 3; arc++)
							{
								glm::vec2 chainEnd = chainTarget->physics.getPos() + glm::vec2(
									getRandomFloat(rng, -PIXEL_SIZE * 2.6f, PIXEL_SIZE * 2.6f),
									getRandomFloat(rng, -PIXEL_SIZE * 2.6f, PIXEL_SIZE * 2.6f)
								);
								emitArcBetween(targetPos, chainEnd, 1.3f);
							}
						}
					}

					chargesLeft--;
					zapCooldownTimer = getRandomFloat(rng, zapCooldownMin, zapCooldownMax);
				}
			}
		}
		else if (!bestTarget)
		{
			zapRollTimer = 0.0f;
		}

		if (chargesLeft <= 0)
		{
			trapDepleted = true;
			stormParticles.emitParticles(zapBurstParticle, trapPos, rng, trapPos);
		}

		stormParticles.update(deltaTime);
		return true;
	}

	void renderBeforeEntities(gl2d::Renderer2D &renderer) override
	{
		stormParticles.render(renderer, getParticlePostProcessRenderer(), trapPos);
	}
};

// Gradually adds spell healing with soft green particles.
struct HealingSpell: public Spell
{
	// **configuration variables**
	float totalHealing = 3.0f;
	float healDuration = 2.0f;
	float particleInterval = 0.04f;
	float particleRadius = 0.5f;
	float particleJitter = 0.15f;

	// **state variables**
	bool initialized = false;
	float healRemaining = 0.0f;
	float particleTimer = 0.0f;
	ParticleSettings healParticle;

	HealingSpell()
	{
		continuousUpdate = true;
		continuousUpdateTimer = healDuration;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)map;
		(void)projectileHolder;
		(void)entityHolder;
		(void)currentAimDir;

		if (!initialized)
		{
			initialized = true;
			healRemaining = totalHealing;
			continuousUpdateTimer = healDuration;

			glm::vec4 startColor = {0.35f, 0.95f, 0.45f, 0.8f};
			glm::vec4 endColor = {0.18f, 0.6f, 0.25f, 0.3f};
			healParticle = getSmallSquareParticle(startColor, endColor);
			healParticle.onCreateCount = 1;
			healParticle.particleLifeTime = {0.25f, 0.45f};
			healParticle.velocityX = glm::vec2{-6.0f, 6.0f} * PIXEL_SIZE;
			healParticle.velocityY = glm::vec2{-10.0f, -4.0f} * PIXEL_SIZE;
			healParticle.createApearence.size = glm::vec2{2.0f, 3.0f} * PIXEL_SIZE * 2.f;
			healParticle.endApearence.size = glm::vec2{1.0f, 2.0f} * PIXEL_SIZE * 2.f;
			healParticle.animationType = ParticleSettings::ANIMATION_TYPES::animationBob;
			healParticle.animationSpeed = {6.0f, 10.0f};
			healParticle.animationScaleY = {PIXEL_SIZE * 1.5f, PIXEL_SIZE * 2.6f};
			healParticle.animationPhase = {0.0f, 6.2831853f};
			healParticle.texture = getAssetManager().particleCross;
			healParticle.folowParent = false;
		}

		float healRate = healDuration > 0.0001f ? (totalHealing / healDuration) : totalHealing;
		float addAmount = std::min(healRemaining, healRate * deltaTime);
		if (addAmount > 0.0f)
		{
			player.addSpellHealing(addAmount);
			healRemaining -= addAmount;
		}

		particleTimer -= deltaTime;
		while (particleTimer <= 0.0f)
		{
			particleTimer += particleInterval;
			float angle = getRandomFloat(rng, 0.0f, 6.2831853f);
			float radius = getRandomFloat(rng, 0.0f, particleRadius);
			glm::vec2 offset = glm::vec2(std::cos(angle), std::sin(angle)) * radius;
			offset.x += getRandomFloat(rng, -particleJitter, particleJitter);
			offset.y += getRandomFloat(rng, -particleJitter, particleJitter);
			glm::vec2 spawnPos = player.physics.getPos() + offset + glm::vec2(0.0f, -0.25f);
			mainParticleSystem.emitParticles(healParticle, spawnPos, rng, player.physics.getPos());
		}

		return true;
	}
};

// Gradually adds shield with soft blue particles.
struct ShieldSpell: public Spell
{
	// **configuration variables**
	float totalShield = 2.0f;
	float shieldDuration = 2.0f;
	float particleInterval = 0.05f;
	float particleRadius = 0.5f;
	float particleJitter = 0.12f;

	// **state variables**
	bool initialized = false;
	float elapsed = 0.0f;
	float particleTimer = 0.0f;
	ParticleSettings shieldParticle;

	ShieldSpell()
	{
		continuousUpdate = true;
		continuousUpdateTimer = shieldDuration;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)map;
		(void)projectileHolder;
		(void)entityHolder;
		(void)currentAimDir;

		if (!initialized)
		{
			initialized = true;
			elapsed = 0.0f;
			continuousUpdateTimer = shieldDuration;

			glm::vec4 startColor = {0.85f, 0.9f, 0.95f, 0.8f};
			glm::vec4 endColor = {0.55f, 0.6f, 0.65f, 0.3f};
			shieldParticle = getSmallSquareParticle(startColor, endColor);
			shieldParticle.onCreateCount = 1;
			shieldParticle.particleLifeTime = {0.3f, 0.55f};
			shieldParticle.velocityX = {0.0f, 0.0f};
			shieldParticle.velocityY = {0.0f, 0.0f};
			shieldParticle.createApearence.size = glm::vec2{2.0f, 3.0f} * PIXEL_SIZE;
			shieldParticle.endApearence.size = glm::vec2{1.0f, 2.0f} * PIXEL_SIZE;
			shieldParticle.animationType = ParticleSettings::ANIMATION_TYPES::animationCircle;
			shieldParticle.animationSpeed = {9.0f, 13.0f};
			shieldParticle.animationScaleX = {PIXEL_SIZE * 2.0f, PIXEL_SIZE * 3.2f};
			shieldParticle.animationScaleY = {PIXEL_SIZE * 2.0f, PIXEL_SIZE * 3.2f};
			shieldParticle.animationPhase = {0.0f, 6.2831853f};
			shieldParticle.texture = getAssetManager().particleCircle;
			shieldParticle.folowParent = true;
		}

		elapsed = std::min(shieldDuration, elapsed + deltaTime);
		float alpha = shieldDuration > 0.0001f ? (elapsed / shieldDuration) : 1.0f;
		player.addShield(totalShield * alpha);

		particleTimer -= deltaTime;
		while (particleTimer <= 0.0f)
		{
			particleTimer += particleInterval;
			float angle = getRandomFloat(rng, 0.0f, 6.2831853f);
			float radius = getRandomFloat(rng, 0.0f, particleRadius);
			glm::vec2 offset = glm::vec2(std::cos(angle), std::sin(angle)) * radius;
			offset.x += getRandomFloat(rng, -particleJitter, particleJitter);
			offset.y += getRandomFloat(rng, -particleJitter, particleJitter);
			glm::vec2 spawnPos = player.physics.getPos();
			mainParticleSystem.emitParticles(shieldParticle, spawnPos, rng, player.physics.getPos());
		}

		return true;
	}
};

// Calls down a sequence of delayed meteor strikes on screen.
struct MeteoriteShowerSpell: public Spell
{
	// **configuration variables**
	float impactDelay = 0.35f;
	float explosionRadius = 1.0f;
	float explosionDamage = 7.0f;
	float explosionBurn = 2.0f;
	int spawnAttempts = 16;
	float fallbackRadiusMin = 0.6f;
	float fallbackRadiusMax = 1.4f;

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)mainParticleSystem;
		(void)entityHolder;
		(void)currentAimDir;

		auto &renderer = getRenderer();
		glm::vec4 viewRect = renderer.getViewRect();
		glm::vec2 playerPos = player.physics.getPos();
		glm::ivec2 playerTile = WorldToTile(playerPos);

		auto isBlocked = [&](const glm::ivec2 &tile)
		{
			if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
			{
				return true;
			}
			return map.isCollidableAtPosSafe(tile.x, tile.y);
		};

		auto isVisibleFromPlayer = [&](const glm::ivec2 &tile)
		{
			return HasLineOfSightGrid(map, playerTile, tile);
		};

		glm::vec2 spawnPos = playerPos;
		bool found = false;
		if (getRandomChance(rng, 0.3f))
		{
			std::vector<glm::vec2> candidates;
			candidates.reserve(entityHolder.entities.size());
			for (auto &e : entityHolder.entities)
			{
				if (e->dying) { continue; }
				glm::vec2 pos = e->physics.getPos();
				if (pos.x < viewRect.x || pos.y < viewRect.y
					|| pos.x > viewRect.x + viewRect.z || pos.y > viewRect.y + viewRect.w)
				{
					continue;
				}
				glm::ivec2 tile = WorldToTile(pos);
				if (isBlocked(tile)) { continue; }
				if (!isVisibleFromPlayer(tile)) { continue; }
				candidates.push_back(pos);
			}
			if (!candidates.empty())
			{
				int pick = getRandomInt(rng, 0, (int)candidates.size() - 1);
				spawnPos = candidates[pick];
				found = true;
			}
		}
		if (!found)
		{
			for (int tries = 0; tries < spawnAttempts; tries++)
			{
				glm::vec2 candidate = {
					getRandomFloat(rng, viewRect.x, viewRect.x + viewRect.z),
					getRandomFloat(rng, viewRect.y, viewRect.y + viewRect.w)
				};
				glm::ivec2 candidateTile = WorldToTile(candidate);
				if (isBlocked(candidateTile)) { continue; }
				if (!isVisibleFromPlayer(candidateTile)) { continue; }
				spawnPos = candidate;
				found = true;
				break;
			}
		}

		if (!found)
		{
			const float twoPi = 6.2831853f;
			for (int tries = 0; tries < spawnAttempts; tries++)
			{
				float angle = getRandomFloat(rng, 0.0f, twoPi);
				float radius = getRandomFloat(rng, fallbackRadiusMin, fallbackRadiusMax);
				glm::vec2 candidate = playerPos + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
				glm::ivec2 candidateTile = WorldToTile(candidate);
				if (isBlocked(candidateTile)) { continue; }
				if (!isVisibleFromPlayer(candidateTile)) { continue; }
				spawnPos = candidate;
				found = true;
				break;
			}
		}

		if (!found)
		{
			return true;
		}

		auto meteor = std::make_unique<MeteoriteImpactProjectile>();
		meteor->element = element;
		meteor->impactDelay = impactDelay;
		meteor->explosionDamage = explosionDamage;
		meteor->explosionBurn = explosionBurn;
		meteor->explosionRadius = explosionRadius;
		projectileHolder.addProjectileDeferredAsPtr(std::move(meteor), spawnPos);
		return true;
	}
};

// Flood-fills the visible area with fire and applies burn once per enemy.
struct InfernoSpell: public Spell
{
	// **configuration variables**
	float maxDuration = 1.8f;
	float spawnInterval = 0.004f;
	float particleJitter = 0.2f;
	float fireDebuff = 10.0f;
	bool useDiagonal = true;

	// **state variables**
	bool initialized = false;
	float spawnTimer = 0.0f;
	glm::ivec2 originTile = {0, 0};
	glm::ivec2 minTile = {0, 0};
	glm::ivec2 maxTile = {0, 0};
	std::vector<glm::ivec2> queue;
	int queueIndex = 0;
	std::vector<unsigned char> visited;
	std::vector<Entity*> affectedEntities;
	ParticleSettings fireParticle;
	ParticleSettings hitParticle;
	ParticleSystem particleSystem;
	glm::vec2 renderOrigin = {};

	InfernoSpell()
	{
		continuousUpdate = true;
		continuousUpdateTimer = maxDuration;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)projectileHolder;
		(void)currentAimDir;

		if (!initialized)
		{
			initialized = true;
			continuousUpdateTimer = maxDuration;
			auto &renderer = getRenderer();
			glm::vec4 viewRect = renderer.getViewRect();
			minTile.x = std::max(0, (int)std::floor(viewRect.x));
			minTile.y = std::max(0, (int)std::floor(viewRect.y));
			maxTile.x = std::min(map.size.x - 1, (int)std::ceil(viewRect.x + viewRect.z));
			maxTile.y = std::min(map.size.y - 1, (int)std::ceil(viewRect.y + viewRect.w));

			originTile = WorldToTile(player.physics.getPos());
			renderOrigin = player.physics.getPos();
			queue.clear();
			queueIndex = 0;
			visited.assign(map.size.x * map.size.y, 0);
			affectedEntities.clear();
			spawnTimer = 0.0f;
			particleSystem.maxCount = 900;

			glm::vec4 startColor = elementToSecondaryColor(Elements::Fire);
			glm::vec4 endColor = elementToColor(Elements::Fire);
			startColor.a = 0.85f;
			endColor.a = 0.55f;
			fireParticle = getStatusFireParticle(startColor, endColor);
			fireParticle.onCreateCount = 1;
			fireParticle.onCreateCount = 3;
			fireParticle.particleLifeTime = {0.28f, 0.5f};
			fireParticle.velocityX = glm::vec2{-8.0f, 8.0f} * PIXEL_SIZE;
			fireParticle.velocityY = glm::vec2{-14.0f, -6.0f} * PIXEL_SIZE;
			fireParticle.createApearence.size = glm::vec2{3.0f, 4.2f} * PIXEL_SIZE;
			fireParticle.endApearence.size = glm::vec2{1.6f, 2.8f} * PIXEL_SIZE;
			fireParticle.animationType = ParticleSettings::ANIMATION_TYPES::animationBob;
			fireParticle.animationSpeed = {6.0f, 10.0f};
			fireParticle.animationScaleY = {PIXEL_SIZE * 2.6f, PIXEL_SIZE * 4.0f};
			fireParticle.animationPhase = {0.0f, 6.2831853f};
			fireParticle.folowParent = false;

			hitParticle = getSparkBurstParticle(startColor, endColor);
			hitParticle.onCreateCount = 6;
			hitParticle.particleLifeTime = {0.2f, 0.35f};
			hitParticle.velocityX = glm::vec2{-9.0f, 9.0f} * PIXEL_SIZE;
			hitParticle.velocityY = glm::vec2{-16.0f, -6.0f} * PIXEL_SIZE;
			hitParticle.createApearence.size = glm::vec2{2.0f, 3.2f} * PIXEL_SIZE;
			hitParticle.endApearence.size = glm::vec2{1.0f, 2.2f} * PIXEL_SIZE;
			hitParticle.texture = getAssetManager().particleCircle;
			hitParticle.folowParent = false;

			auto tileIndex = [&](const glm::ivec2 &tile)
			{
				return tile.x + tile.y * map.size.x;
			};

			if (originTile.x >= 0 && originTile.y >= 0 && originTile.x < map.size.x && originTile.y < map.size.y)
			{
				visited[tileIndex(originTile)] = 1;
				queue.push_back(originTile);
			}
		}

		auto tileIndex = [&](const glm::ivec2 &tile)
		{
			return tile.x + tile.y * map.size.x;
		};

		auto isBlocked = [&](const glm::ivec2 &tile)
		{
			if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
			{
				return true;
			}
			return map.isCollidableAtPosSafe(tile.x, tile.y);
		};

		const glm::ivec2 directions4[] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
		const glm::ivec2 directions8[] = {
			{1, 0}, {-1, 0}, {0, 1}, {0, -1},
			{1, 1}, {-1, 1}, {1, -1}, {-1, -1}
		};
		const glm::ivec2 *dirs = useDiagonal ? directions8 : directions4;
		int dirCount = useDiagonal ? 8 : 4;

		spawnTimer -= deltaTime;
		while (spawnTimer <= 0.0f)
		{
			spawnTimer += spawnInterval;
			if (queueIndex >= (int)queue.size())
			{
				break;
			}

			glm::ivec2 tile = queue[queueIndex++];
			glm::vec2 spawnPos = glm::vec2(tile) + glm::vec2(0.5f);
			spawnPos.x += getRandomFloat(rng, -particleJitter, particleJitter);
			spawnPos.y += getRandomFloat(rng, -particleJitter, particleJitter);
			particleSystem.emitParticles(fireParticle, spawnPos, rng, spawnPos);

			for (int i = 0; i < dirCount; i++)
			{
				glm::ivec2 next = tile + dirs[i];
				if (next.x < minTile.x || next.y < minTile.y || next.x > maxTile.x || next.y > maxTile.y)
				{
					continue;
				}
				if (isBlocked(next)) { continue; }
				int index = tileIndex(next);
				if (visited[index]) { continue; }
				visited[index] = 1;
				queue.push_back(next);
			}
		}

		for (auto &e : entityHolder.entities)
		{
			if (e->dying) { continue; }
			bool alreadyHit = false;
			for (auto *hit : affectedEntities)
			{
				if (hit == e.get())
				{
					alreadyHit = true;
					break;
				}
			}
			if (alreadyHit) { continue; }

			glm::ivec2 tile = WorldToTile(e->physics.getPos());
			if (tile.x < minTile.x || tile.y < minTile.y || tile.x > maxTile.x || tile.y > maxTile.y)
			{
				continue;
			}
			int index = tileIndex(tile);
			if (index >= 0 && index < (int)visited.size() && visited[index])
			{
				addStatusEffectFromElement(e->statusEffects, e->statusImmunities, Elements::Fire, fireDebuff);
				mainParticleSystem.emitParticles(hitParticle, e->physics.getPos(), rng, e->physics.getPos());
				affectedEntities.push_back(e.get());
			}
		}

		particleSystem.update(deltaTime);
		return queueIndex < (int)queue.size() || !particleSystem.particles.empty();
	}

	void renderBeforeEntities(gl2d::Renderer2D &renderer) override
	{
		particleSystem.render(renderer, getParticlePostProcessRenderer(), renderOrigin);
	}
};

struct WaterSiphonSpell: public Spell
{
	// **configuration variables**
	HitStats hitStats;
	float range = 13.0f;
	float beamWidth = 0.6f;
	float particleInterval = 0.025f;
	float tickInterval = 0.12f;
	float particleSpeed = 10.0f;
	float minDamage = 0.1f;
	float maxDamage = 0.8f;
	float rampDuration = 0.5f;
	float particleStartOffset = 0.0f;
	int particleSpawnCount = 10;
	float statusAmount = 0.0f;

	// **state variables**
	ParticleSystem particleSystem;
	float particleTimer = 0.0f;
	float tickTimer = 0.0f;
	float damageRampTimer = 0.0f;
	bool initialized = false;
	glm::vec2 origin = {};
	glm::vec2 aimDir = {1.0f, 0.0f};
	float currentRange = 0.0f;
	glm::vec2 prevOrigin = {};
	glm::vec2 prevAimDir = {1.0f, 0.0f};

	WaterSiphonSpell()
	{
		continuousUpdate = true;
		continuousUpdateTimer = 6.0f;
		particleSystem.maxCount = 800;
		hitStats.damage = maxDamage;
		hitStats.pushBack = 0.6f;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		origin = player.physics.getPos();
		float len = glm::length(currentAimDir);
		if (len <= 0.0001f)
		{
			len = glm::length(createAimDir);
			if (len <= 0.0001f)
			{
				currentAimDir = {1.0f, 0.0f};
				len = 1.0f;
			}
			else
			{
				currentAimDir = createAimDir;
			}
		}
		aimDir = currentAimDir / len;

		if (!initialized)
		{
			initialized = true;
			prevOrigin = origin;
			prevAimDir = aimDir;
		}
		else
		{
			glm::vec2 originDelta = origin - prevOrigin;
			if (originDelta.x != 0.0f || originDelta.y != 0.0f)
			{
				for (auto &p : particleSystem.particles)
				{
					p.pos += originDelta;
				}
			}

			float prevAngle = std::atan2(prevAimDir.y, prevAimDir.x);
			float newAngle = std::atan2(aimDir.y, aimDir.x);
			float deltaAngle = newAngle - prevAngle;
			if (std::abs(deltaAngle) > 0.0001f)
			{
				float cs = std::cos(deltaAngle);
				float sn = std::sin(deltaAngle);
				for (auto &p : particleSystem.particles)
				{
					glm::vec2 local = p.pos - origin;
					glm::vec2 rotated = {local.x * cs - local.y * sn, local.x * sn + local.y * cs};
					p.pos = origin + rotated;
				}
			}

			prevOrigin = origin;
			prevAimDir = aimDir;
		}

		currentRange = range;
		float step = 0.25f;
		for (float t = 0.0f; t <= range; t += step)
		{
			glm::vec2 checkPos = origin + aimDir * t;
			int tx = (int)std::floor(checkPos.x);
			int ty = (int)std::floor(checkPos.y);
			if (tx < 0 || ty < 0 || tx >= map.size.x || ty >= map.size.y || map.isCollidableAtPosSafe(tx, ty))
			{
				currentRange = std::max(0.0f, t - step);
				break;
			}
		}

		particleTimer -= deltaTime;
		while (particleTimer <= 0.0f)
		{
			particleTimer += particleInterval;
			auto particle = getOrbitParticle(elementToColor(element), elementToSecondaryColor(element));
			particle.folowParent = false;
			particle.onCreateCount = 1;
			particle.positionX = {0.0f, 0.0f};
			particle.positionY = {0.0f, 0.0f};
			particle.velocityX = {0.0f, 0.0f};
			particle.velocityY = {0.0f, 0.0f};
			particle.createApearence.size *= 1.2f;
			particle.endApearence.size *= 1.2f;
			particle.animationScaleX *= 0.52f;
			particle.animationScaleY *= 0.6f;
			particle.animationSpeed *= 0.85f;
			particle.texture = getAssetManager().particleCircle;
			particle.createApearence.color1.a *= 0.7f;
			particle.createApearence.color2.a *= 0.7f;
			particle.endApearence.color1.a *= 0.3f;
			particle.endApearence.color2.a *= 0.3f;

			glm::vec2 perp = {-aimDir.y, aimDir.x};
			int spawnCount = particleSpawnCount > 0 ? particleSpawnCount : 1;
			for (int i = 0; i < spawnCount; i++)
			{
			float alongStart = std::min(currentRange, particleStartOffset);
			float along = getRandomFloat(rng, alongStart, currentRange);
				float across = getRandomFloat(rng, -beamWidth * 0.5f, beamWidth * 0.5f);
				glm::vec2 spawnPos = origin + aimDir * along + perp * across;
				particleSystem.emitParticles(particle, spawnPos, rng, spawnPos);
			}
		}

		particleSystem.update(deltaTime);
		particleSystem.killParticlesColliding(map, origin);

		for (int i = 0; i < particleSystem.particles.size(); i++)
		{
			auto &p = particleSystem.particles[i];
			glm::vec2 worldPos = p.pos;
			float along = glm::dot(worldPos - origin, aimDir);
			if (along < 0.0f || along > currentRange)
			{
				particleSystem.particles[i] = particleSystem.particles.back();
				particleSystem.particles.pop_back();
				--i;
			}
		}

		tickTimer -= deltaTime;
		damageRampTimer = std::min(rampDuration, damageRampTimer + deltaTime);
		float rampAlpha = rampDuration > 0.0001f ? (damageRampTimer / rampDuration) : 1.0f;
		float rampDamage = minDamage + (maxDamage - minDamage) * rampAlpha;
		if (tickTimer <= 0.0f)
		{
			tickTimer += tickInterval;
			Entity *target = nullptr;
			float bestAlong = currentRange + 1.0f;

			for (auto &e : entityHolder.entities)
			{
				glm::vec2 diff = e->physics.getPos() - origin;
				float along = glm::dot(diff, aimDir);
				if (along < 0.0f || along > currentRange)
				{
					continue;
				}
				glm::vec2 perp = diff - aimDir * along;
				float maxWidth = beamWidth + e->physics.transform.size.x * 0.5f;
				if (glm::length(perp) > maxWidth)
				{
					continue;
				}

				if (!HasLineOfSightGrid(map, WorldToTile(origin), WorldToTile(e->physics.getPos())))
				{
					continue;
				}

				if (along < bestAlong)
				{
					bestAlong = along;
					target = e.get();
				}
			}

			if (target)
			{
				glm::vec2 pushBack = {};
				HitStats rampStats = hitStats;
				rampStats.damage = rampDamage;
				target->life.computeHit(rampStats, element, target->element, aimDir, pushBack);
				if (rampStats.damage > 0.0f)
				{
					target->onDamaged(rampStats.damage);
				}
				target->physics.velocity += pushBack;
				if (statusAmount > 0.0f)
				{
					addStatusEffectFromElement(target->statusEffects, target->statusImmunities, element, statusAmount);
				}
				glm::vec2 damagePos = target->physics.getPos();
				damagePos.y -= target->physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(rampDamage, damagePos);
			}
		}

		return true;
	}

	void renderBeforeEntities(gl2d::Renderer2D &renderer) override
	{
		particleSystem.render(renderer, getParticlePostProcessRenderer(), origin);

		float width = beamWidth;
		float angle = -std::atan2(aimDir.y, aimDir.x) * (180.0f / 3.14159265f);
		glm::vec2 center = origin + aimDir * (currentRange * 0.5f);
		glm::vec4 rect = {center.x - currentRange * 0.5f, center.y - width * 0.5f, currentRange, width};
		renderer.renderRectangleOutline(rect, Colors_Blue, 0.02f, {}, angle);
	}
};
