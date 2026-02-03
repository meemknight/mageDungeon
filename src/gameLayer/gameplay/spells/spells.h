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
	float spreadDegrees = 28.0f;

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

		spawnProjectile(aim);
		for (int i = 1; i < shotCount; i++)
		{
			float driftRad = glm::radians(getRandomFloat(rng, -spreadDegrees, spreadDegrees));
			float c = std::cos(driftRad);
			float s = std::sin(driftRad);
			glm::vec2 dir = {aim.x * c - aim.y * s, aim.x * s + aim.y * c};
			spawnProjectile(dir);
		}

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
				projectileHolder.addProjectileDeferredAsPtr(std::move(thorn), spawnPos);
				mainParticleSystem.emitParticles(burst, spawnPos, rng, spawnPos);
				placedTiles.push_back(spawnTile);
				spawned = true;
			}
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
		if (getRandomChance(rng, 0.4f))
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
		meteor->element = Elements::Fire;
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
			int spawnCount = 10;
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
