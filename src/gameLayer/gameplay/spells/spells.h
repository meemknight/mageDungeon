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
				throwVelocity, hasStandbyEmission ? &standbyEmission : nullptr);
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
					primaryThrowVelocity, hasPrimaryEmission ? &primaryEmission : nullptr);
			}
		}
		if (secondaryProjectile)
		{
			for (int i = 0; i < secondaryCount; i++)
			{
				auto pptr = secondaryProjectile->clone();
				pptr->element = element;
				standbySystem.addProjectileAsPtr(std::move(pptr), secondaryStandbyLifetime,
					secondaryThrowVelocity, hasSecondaryEmission ? &secondaryEmission : nullptr);
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

	// **state variables**
	// (none)

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, EntityHolder &entityHolder, glm::vec2 currentAimDir) override
	{
		(void)deltaTime;
		(void)map;
		(void)mainParticleSystem;
		(void)projectileHolder;
		(void)rng;
		(void)entityHolder;
		(void)currentAimDir;

		if (!summon)
		{
			return true;
		}

		auto &summonHolder = getSummonHolder();
		for (int i = 0; i < summonCount; i++)
		{
			auto sptr = summon->clone();
			summonHolder.addSummonAsPtr(std::move(sptr), player.physics.getPos());
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

struct WaterSiphonSpell: public Spell
{
	// **configuration variables**
	HitStats hitStats;
	float range = 13.0f;
	float beamWidth = 0.6f;
	float particleInterval = 0.03f;
	float tickInterval = 0.12f;
	float particleSpeed = 10.0f;
	float minDamage = 0.1f;
	float maxDamage = 1.f;
	float rampDuration = 0.5f;

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
		particleSystem.maxCount = 900;
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
			int spawnCount = 10;
			for (int i = 0; i < spawnCount; i++)
			{
				float along = getRandomFloat(rng, 0.0f, currentRange);
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
				target->physics.velocity += pushBack;
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
