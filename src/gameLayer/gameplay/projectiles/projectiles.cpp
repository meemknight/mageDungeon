#include "projectiles.h"
#include <gameplay/projectiles/projectiles.h>
#include <gameplay/damageViewerSystem.h>
#include <gameplay/statusEffects.h>
#include <gameplay/assetsManager.h>
#include <gameplay/aStar.h>
#include <gameplay/player.h>
#include <gameplay/summons.h>
#include <algorithm>
#include <cmath>



bool basicProjectileHitEntitiesLogic(PhysicalEntity &physics,
	glm::vec2 projectileMoveDirection, char projectileElement,
	EntityHolder &entities, HitStats hitStats, float statusAmount)
{

	auto projectile = physics.transform;

	for (auto &e : entities.entities)
	{
		if (e->dying) continue; // skip dying entities

		if (projectile.intersectTransform(e->physics.transform))
		{
			//hit enemy
			glm::vec2 pushBack = {};

			e->life.computeHit(hitStats, projectileElement, e->element, projectileMoveDirection, pushBack);
			e->physics.velocity += pushBack;
			addStatusEffectFromElement(e->statusEffects, e->statusImmunities, projectileElement, statusAmount);

			glm::vec2 damagePos = e->physics.getPos();
			damagePos.y -= e->physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(hitStats.damage, damagePos);

			return true;
		}


	}

	return false;
}

void StandbyProjectileSystem::addProjectileAsPtr(std::unique_ptr<Projectile> projectile,
	float customLifetime, float customThrowVelocity, const ParticleEmissionSettings *customEmission)
{
	if (!projectile)
	{
		return;
	}

	while ((int)standbyProjectiles.size() >= maxStandby && !standbyProjectiles.empty())
	{
		int removeIndex = 0;
		float lowestTime = standbyProjectiles[0].timeLeft;
		for (int i = 1; i < (int)standbyProjectiles.size(); i++)
		{
			if (standbyProjectiles[i].timeLeft < lowestTime)
			{
				lowestTime = standbyProjectiles[i].timeLeft;
				removeIndex = i;
			}
		}

		if (removeIndex != (int)standbyProjectiles.size() - 1)
		{
			standbyProjectiles[removeIndex] = std::move(standbyProjectiles.back());
		}
		standbyProjectiles.pop_back();
	}
	if (insertIndex > (int)standbyProjectiles.size())
	{
		insertIndex = 1;
	}

	StandbyProjectileEntry entry;
	entry.projectile = std::move(projectile);
	entry.projectile->physics.velocity = {};
	entry.timeLeft = customLifetime > 0.0f ? customLifetime : standbyLifetime;
	entry.throwVelocity = customThrowVelocity;
	entry.hasCustomEmission = customEmission != nullptr;
	if (customEmission)
	{
		entry.customEmission = *customEmission;
	}
	entry.initialized = false;

	if (standbyProjectiles.empty())
	{
		standbyProjectiles.push_back(std::move(entry));
		insertIndex = 1;
		return;
	}

	int maxIndex = (int)standbyProjectiles.size();
	int index = insertIndex;
	if (index < 0) { index = 0; }
	if (index > maxIndex) { index = maxIndex; }

	standbyProjectiles.insert(standbyProjectiles.begin() + index, std::move(entry));
	insertIndex = index + 2;
	if (insertIndex > (int)standbyProjectiles.size())
	{
		insertIndex = 1;
	}
}

void StandbyProjectileSystem::update(float deltaTime, Map &map, ProjectileHolder &projectileHolder,
	std::ranlux24_base &rng, Player &player, EntityHolder &entityHolder,
	glm::vec2 aimDir, bool aimActive)
{
	for (int i = 0; i < (int)standbyProjectiles.size(); )
	{
		auto &entry = standbyProjectiles[i];
		entry.timeLeft -= deltaTime;
		if (entry.timeLeft <= 0.0f)
		{
			standbyProjectiles[i] = std::move(standbyProjectiles.back());
			standbyProjectiles.pop_back();
			continue;
		}
		++i;
	}

	if (!standbyProjectiles.empty())
	{
		int maxIndex = (int)standbyProjectiles.size();
		if (insertIndex > maxIndex) { insertIndex = 1; }
	}
	else
	{
		insertIndex = 1;
	}

	if (standbyProjectiles.empty())
	{
		idleRotation = 0.0f;
		idleRotationVelocity = 0.0f;
		idleRotationDelayTimer = idleRotationDelay;
		return;
	}

	glm::vec2 playerPos = player.physics.getPos();
	float aimLen = glm::length(aimDir);
	if (aimLen <= 0.0001f)
	{
		aimDir = {1.0f, 0.0f};
	}
	else
	{
		aimDir /= aimLen;
	}

	const float twoPi = 6.2831853f;
	if (aimActive)
	{
		idleRotationVelocity = 0.0f;
		idleRotation = 0.0f;
		idleRotationDelayTimer = idleRotationDelay;
	}
	else
	{
		idleRotationDelayTimer = std::max(0.0f, idleRotationDelayTimer - deltaTime);
		if (idleRotationDelayTimer <= 0.0f)
		{
			idleRotationVelocity = std::min(idleRotationSpeed, idleRotationVelocity + idleRotationAccel * deltaTime);
			idleRotation += idleRotationVelocity * deltaTime;
		}
	}

	float baseAngle = std::atan2(aimDir.y, aimDir.x);
	if (!aimActive)
	{
		baseAngle += idleRotation;
	}
	float angleStep = twoPi / (float)standbyProjectiles.size();

	for (int i = 0; i < (int)standbyProjectiles.size(); i++)
	{
		auto &entry = standbyProjectiles[i];
		float angle = baseAngle + angleStep * (float)i;
		glm::vec2 offset = {std::cos(angle) * ringRadius, std::sin(angle) * ringRadius};
		glm::vec2 ringPos = playerPos + offset;
		entry.projectile->physics.teleport(ringPos);

		if (!entry.initialized)
		{
			float sizeBias = entry.projectile->physics.transform.size.x / (PIXEL_SIZE * 8.0f);
			if (entry.hasCustomEmission)
			{
				entry.particleEmmision = entry.customEmission;
				entry.particleEmmision.sustain.createApearence.size *= sizeBias;
				entry.particleEmmision.sustain.endApearence.size *= sizeBias;
				entry.particleEmmision.release.createApearence.size *= sizeBias;
				entry.particleEmmision.release.endApearence.size *= sizeBias;
				entry.particleEmmision.create.createApearence.size *= sizeBias;
				entry.particleEmmision.create.endApearence.size *= sizeBias;
			}
			else
			{
				entry.particleEmmision = getBasicMagicMissleParticleEmision(entry.projectile->element, sizeBias);
			}
			entry.particleTimer = getRandomFloat(rng, 0.0f, entry.particleEmmision.emitTimer);
			entry.initialized = true;
		}

		entry.particleTimer -= deltaTime;
		while (entry.particleTimer <= 0.0f)
		{
			entry.particleTimer += entry.particleEmmision.emitTimer;
			entry.projectile->particleSystem.emitParticles(entry.particleEmmision.sustain, ringPos, rng, ringPos);
		}

		entry.projectile->particleSystem.update(deltaTime);
	}

}

bool StandbyProjectileSystem::tryFire(Map &map, ProjectileHolder &projectileHolder,
	Player &player, EntityHolder &entityHolder, glm::vec2 aimDir)
{
	if (standbyProjectiles.empty())
	{
		return false;
	}

	(void)map;

	glm::vec2 playerPos = player.physics.getPos();
	glm::vec2 targetPos = {};
	bool hasTarget = false;
	float bestDist2 = fireRange * fireRange;

	for (auto &e : entityHolder.entities)
	{
		if (e->dying) continue; // skip dying entities
		glm::vec2 diff = e->physics.getPos() - playerPos;
		float dist2 = glm::dot(diff, diff);
		if (dist2 > bestDist2)
		{
			continue;
		}

		bestDist2 = dist2;
		targetPos = e->physics.getPos();
		hasTarget = true;
	}

	glm::vec2 targetDir = hasTarget ? (targetPos - playerPos) : aimDir;
	float targetLen = glm::length(targetDir);
	if (targetLen <= 0.0001f)
	{
		targetDir = {1.0f, 0.0f};
	}
	else
	{
		targetDir /= targetLen;
	}

	int bestIndex = 0;
	float bestTime = standbyProjectiles[0].timeLeft;
	for (int i = 1; i < (int)standbyProjectiles.size(); i++)
	{
		if (standbyProjectiles[i].timeLeft < bestTime)
		{
			bestTime = standbyProjectiles[i].timeLeft;
			bestIndex = i;
		}
	}

	StandbyProjectileEntry firedEntry = std::move(standbyProjectiles[bestIndex]);
	if (bestIndex != (int)standbyProjectiles.size() - 1)
	{
		standbyProjectiles[bestIndex] = std::move(standbyProjectiles.back());
	}
	standbyProjectiles.pop_back();

	firedEntry.projectile->physics.teleport(playerPos);
	firedEntry.projectile->physics.velocity = targetDir * firedEntry.throwVelocity;
	projectileHolder.addProjectileDeferredAsPtr(std::move(firedEntry.projectile), playerPos);
	return true;
}

void StandbyProjectileSystem::render(gl2d::Renderer2D &renderer,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	for (auto &entry : standbyProjectiles)
	{
		entry.projectile->particleSystem.render(renderer, particlePostProcessRenderer,
			entry.projectile->physics.getPos());
	}
}

BasicMagicMissle::BasicMagicMissle()
{
	hitStats.damage = 2;
	hitStats.pushBack = 5.2;
}

BasicMagicMissle::BasicMagicMissle(HitStats hitStats)
{
	this->hitStats = hitStats;
}

BasicMagicMissle::BasicMagicMissle(HitStats hitStats, float particleSizeBias)
{
	this->hitStats = hitStats;
	this->particleSizeBias = particleSizeBias;
}

bool BasicMagicMissle::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = 0;
		if (hasCustomEmission)
		{
			particleEmmision = customEmission;
			particleEmmision.sustain.createApearence.size *= particleSizeBias;
			particleEmmision.sustain.endApearence.size *= particleSizeBias;
			particleEmmision.release.createApearence.size *= particleSizeBias;
			particleEmmision.release.endApearence.size *= particleSizeBias;
			particleEmmision.create.createApearence.size *= particleSizeBias;
			particleEmmision.create.endApearence.size *= particleSizeBias;
		}
		else
		{
			particleEmmision = getBasicMagicMissleParticleEmision(element, particleSizeBias);
		}
		particleSystem.emitParticles(particleEmmision.create, physics.getPos(), rng, physics.getPos());
	}

	particleTimer -= deltaTime;
	if (particleTimer < 0)
	{
		particleTimer += particleEmmision.emitTimer;
		particleSystem.emitParticles(particleEmmision.sustain, physics.getPos(), rng, physics.getPos());
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats, statusAmount))
	{
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void BasicMagicMissle::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	physics.renderCollider(renderer);
}

void BasicMagicMissle::onDestroy(std::ranlux24_base &rng)
{
}

static ParticleEmissionSettings buildAimableEmission(int element, float sizeBias)
{
	ParticleEmissionSettings emission;
	glm::vec4 startColor = {0.7f, 0.3f, 0.95f, 0.6f};
	glm::vec4 endColor = elementToColor(element); endColor.a = 0.6f;

	emission.sustain = getBasicMagicMissleParticle(startColor, endColor);
	emission.release = getBasicMagicMissleParticle(startColor, endColor);
	emission.release.particleLifeTime *= 1.6f;
	emission.emitTimer = 0.02f;
	emission.create = emission.sustain;

	emission.sustain.createApearence.size *= sizeBias;
	emission.sustain.endApearence.size *= sizeBias;
	emission.release.createApearence.size *= sizeBias;
	emission.release.endApearence.size *= sizeBias;
	emission.create.createApearence.size *= sizeBias;
	emission.create.endApearence.size *= sizeBias;

	return emission;
}

static ParticleSettings buildAimableOrbitParticle()
{
	glm::vec4 startColor = {0.7f, 0.3f, 0.95f, 0.85f};
	glm::vec4 endColor = {0.55f, 0.2f, 0.85f, 0.3f};

	ParticleSettings orbit = getSpiralParticle(startColor, endColor);
	orbit.onCreateCount = 2;
	orbit.particleLifeTime = {0.35f, 0.5f};
	orbit.velocityX = {0.0f, 0.0f};
	orbit.velocityY = {0.0f, 0.0f};
	orbit.dragX = {0.0f, 0.0f};
	orbit.dragY = {0.0f, 0.0f};
	orbit.createApearence.size = glm::vec2{2.4f, 3.1f} * PIXEL_SIZE;
	orbit.endApearence.size = glm::vec2{1.4f, 2.2f} * PIXEL_SIZE;
	orbit.animationSpeed = {-12.0f, 12.0f};
	orbit.animationScaleX = {PIXEL_SIZE * 5.0f, PIXEL_SIZE * 11.0f};
	orbit.animationScaleY = {PIXEL_SIZE * 5.0f, PIXEL_SIZE * 11.0f};
	orbit.positionX = {0.0f, 0.0f};
	orbit.positionY = {0.0f, 0.0f};
	orbit.folowParent = true;

	return orbit;
}

static int getRandomElementForWild(std::ranlux24_base &rng)
{
	return getRandomInt(rng, Elements::Fire, Elements::Ice);
}

static void applyWildColors(ParticleSettings &p, glm::vec4 startColor, glm::vec4 endColor)
{
	p.createApearence.color1 = startColor;
	p.createApearence.color2 = startColor;
	p.endApearence.color1 = endColor;
	p.endApearence.color2 = endColor;
}

AimableBoltProjectile::AimableBoltProjectile()
{
	hitStats.damage = 18.0f;
	hitStats.pushBack = 4.0f;
	element = Elements::Fire;
	timeAlieve = 7.0f;
	physics.transform.size = {PIXEL_SIZE * 7.0f, PIXEL_SIZE * 7.0f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 140;
}

bool AimableBoltProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		particleEmmision = buildAimableEmission(element, particleSizeBias);
		orbitParticle = buildAimableOrbitParticle();
		orbitParticle.createApearence.size *= particleSizeBias;
		orbitParticle.endApearence.size *= particleSizeBias;
		particleSystem.emitParticles(particleEmmision.create, physics.getPos(), rng, physics.getPos());
		orbitTimer = getRandomFloat(rng, 0.0f, orbitInterval);
	}

	glm::vec2 targetPos = getFireTargetPos();
	glm::vec2 toTarget = targetPos - physics.getPos();
	float targetLen = glm::length(toTarget);
	if (targetLen > 0.0001f)
	{
		moveDir = toTarget / targetLen;
	}
	physics.velocity = moveDir * moveSpeed;

	particleTimer -= deltaTime;
	if (particleTimer < 0)
	{
		particleTimer += particleEmmision.emitTimer;
		ParticleSettings mainParticle = particleEmmision.sustain;
		mainParticle.onCreateCount = std::max<short>(2, mainParticle.onCreateCount + 1);
		if (getRandomChance(rng, 0.5f))
		{
			mainParticle.folowParent = false;
		}
		particleSystem.emitParticles(mainParticle, physics.getPos(), rng, physics.getPos());
	}

	orbitTimer -= deltaTime;
	if (orbitTimer <= 0.0f)
	{
		orbitTimer += orbitInterval;
		ParticleSettings spark = orbitParticle;
		if (getRandomChance(rng, 0.5f))
		{
			spark.folowParent = false;
		}
		particleSystem.emitParticles(spark, physics.getPos(), rng, physics.getPos());
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats, statusAmount))
	{
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void AimableBoltProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	physics.renderCollider(renderer);
}

void AimableBoltProjectile::onDestroy(std::ranlux24_base &rng)
{
}

AimableEarthBoltProjectile::AimableEarthBoltProjectile()
{
	hitStats.damage = 18.0f;
	hitStats.pushBack = 4.0f;
	element = Elements::Earth;
	timeAlieve = 7.0f;
	physics.transform.size = {PIXEL_SIZE * 7.5f, PIXEL_SIZE * 7.5f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 120;
}

bool AimableEarthBoltProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		storedDamage = std::max(1, (int)std::round(hitStats.damage));
		hitStats.damage = (float)storedDamage;
		particleEmmision = buildAimableEmission(element, particleSizeBias);
		orbitParticle = buildAimableOrbitParticle();
		orbitParticle.createApearence.size *= particleSizeBias;
		orbitParticle.endApearence.size *= particleSizeBias;
		particleSystem.emitParticles(particleEmmision.create, physics.getPos(), rng, physics.getPos());
		orbitTimer = getRandomFloat(rng, 0.0f, orbitInterval);
	}

	glm::vec2 targetPos = getFireTargetPos();
	glm::vec2 toTarget = targetPos - physics.getPos();
	float targetLen = glm::length(toTarget);
	if (targetLen > 0.0001f)
	{
		moveDir = toTarget / targetLen;
	}
	physics.velocity = moveDir * moveSpeed;

	trailTimer -= deltaTime;
	if (trailTimer <= 0.0f)
	{
		trailTimer += trailInterval;
		auto thorn = std::make_unique<ThornProjectile>();
		thorn->element = element;
		getProjectileHolder().addProjectileDeferredAsPtr(std::move(thorn), physics.getPos());
		storedDamage = std::max(0, storedDamage - 1);
		hitStats.damage = (float)storedDamage;
		if (storedDamage <= 0)
		{
			particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
			return false;
		}
	}

	particleTimer -= deltaTime;
	if (particleTimer < 0)
	{
		particleTimer += particleEmmision.emitTimer;
		particleSystem.emitParticles(particleEmmision.sustain, physics.getPos(), rng, physics.getPos());
	}

	orbitTimer -= deltaTime;
	if (orbitTimer <= 0.0f)
	{
		orbitTimer += orbitInterval;
		ParticleSettings spark = orbitParticle;
		if (getRandomChance(rng, 0.5f))
		{
			spark.folowParent = false;
		}
		particleSystem.emitParticles(spark, physics.getPos(), rng, physics.getPos());
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats, statusAmount))
	{
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void AimableEarthBoltProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	physics.renderCollider(renderer);
}

void AimableEarthBoltProjectile::onDestroy(std::ranlux24_base &rng)
{
}

WildMagicBoltProjectile::WildMagicBoltProjectile()
{
	hitStats.damage = 12.0f;
	hitStats.pushBack = 4.0f;
	element = Elements::NoneElement;
	timeAlieve = 7.0f;
	physics.transform.size = {PIXEL_SIZE * 7.0f, PIXEL_SIZE * 7.0f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 180;
}

bool WildMagicBoltProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		glm::vec4 startColor = {0.9f, 0.9f, 0.9f, 0.7f};
		glm::vec4 endColor = {0.7f, 0.7f, 0.7f, 0.7f};
		baseEmission.sustain = getBasicMagicMissleParticle(startColor, endColor);
		baseEmission.release = getBasicMagicMissleParticle(startColor, endColor);
		baseEmission.release.particleLifeTime *= 1.4f;
		baseEmission.emitTimer = 0.02f;
		baseEmission.create = baseEmission.sustain;
		baseEmission.sustain.createApearence.size *= particleSizeBias;
		baseEmission.sustain.endApearence.size *= particleSizeBias;
		baseEmission.release.createApearence.size *= particleSizeBias;
		baseEmission.release.endApearence.size *= particleSizeBias;
		baseEmission.create.createApearence.size *= particleSizeBias;
		baseEmission.create.endApearence.size *= particleSizeBias;
		baseEmission.sustain.folowParent = true;
		baseEmission.release.folowParent = false;
		baseEmission.create.folowParent = true;

		orbitParticle = getOrbitParticle({0.8f, 0.4f, 0.95f, 0.7f}, {0.6f, 0.2f, 0.9f, 0.0f});
		orbitParticle.onCreateCount = 2;
		orbitParticle.particleLifeTime = {0.4f, 0.6f};
		orbitParticle.createApearence.size = glm::vec2{2.2f, 3.0f} * PIXEL_SIZE;
		orbitParticle.endApearence.size = glm::vec2{1.2f, 2.0f} * PIXEL_SIZE;
		orbitParticle.animationScaleX = {PIXEL_SIZE * 6.0f, PIXEL_SIZE * 12.0f};
		orbitParticle.animationScaleY = {PIXEL_SIZE * 6.0f, PIXEL_SIZE * 12.0f};
		orbitParticle.folowParent = true;

		particleSystem.emitParticles(baseEmission.create, physics.getPos(), rng, physics.getPos());
		orbitTimer = getRandomFloat(rng, 0.0f, orbitInterval);
	}

	particleTimer -= deltaTime;
	if (particleTimer < 0.0f)
	{
		particleTimer += baseEmission.emitTimer;
		ParticleSettings mainParticle = baseEmission.sustain;
		int randElement = getRandomElementForWild(rng);
		auto startColor = elementToSecondaryColor(randElement); startColor.a = 0.75f;
		auto endColor = elementToColor(randElement); endColor.a = 0.7f;
		applyWildColors(mainParticle, startColor, endColor);
		particleSystem.emitParticles(mainParticle, physics.getPos(), rng, physics.getPos());
	}

	orbitTimer -= deltaTime;
	if (orbitTimer <= 0.0f)
	{
		orbitTimer += orbitInterval;
		ParticleSettings spark = orbitParticle;
		int randElement = getRandomElementForWild(rng);
		auto startColor = elementToSecondaryColor(randElement); startColor.a = 0.85f;
		auto endColor = elementToColor(randElement); endColor.a = 0.2f;
		applyWildColors(spark, startColor, endColor);
		particleSystem.emitParticles(spark, physics.getPos(), rng, physics.getPos());
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats, statusAmount))
	{
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		particleSystem.emitParticles(baseEmission.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void WildMagicBoltProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	physics.renderCollider(renderer);
}

void WildMagicBoltProjectile::onDestroy(std::ranlux24_base &rng)
{
	glm::vec2 pos = physics.getPos();
	int outcome = getRandomInt(rng, 0, 3);

	if (outcome == 0)
	{
		HitStats burstStats;
		burstStats.damage = 18.0f;
		burstStats.pushBack = 7.0f;
		int burstElement = getRandomElementForWild(rng);
		auto burst = std::make_unique<BasicMagicMissle>(burstStats, 1.8f);
		burst->element = burstElement;
		burst->physics.transform.size = glm::vec2{PIXEL_SIZE * 18.0f, PIXEL_SIZE * 18.0f};
		burst->timeAlieve = 0.2f;
		getProjectileHolder().addProjectileDeferredAsPtr(std::move(burst), pos);
		return;
	}
	else if (outcome == 1)
	{
		HitStats fusionStats;
		fusionStats.damage = 4.0f;
		fusionStats.pushBack = 4.0f;
		int burstCount = 10;
		float angleStep = 6.2831853f / (float)burstCount;
		float angleOffset = getRandomFloat(rng, 0.0f, angleStep);
		for (int i = 0; i < burstCount; i++)
		{
			float angle = angleOffset + angleStep * i;
			glm::vec2 dir = {std::cos(angle), std::sin(angle)};
			int elem = (i % 2 == 0) ? Elements::Fire : Elements::Ice;
			auto bolt = std::make_unique<HomingMagicMissle>(fusionStats);
			bolt->element = elem;
			bolt->physics.velocity = dir * 6.5f;
			getProjectileHolder().addProjectileDeferredAsPtr(std::move(bolt), pos);
		}
		return;
	}
	else if (outcome == 2)
	{
		int thornCount = 16;
		for (int i = 0; i < thornCount; i++)
		{
			float angle = (6.2831853f / thornCount) * i;
			float radius = getRandomFloat(rng, 0.6f, 1.2f);
			glm::vec2 spawnPos = pos + glm::vec2{std::cos(angle), std::sin(angle)} * radius;
			auto thorn = std::make_unique<ThornProjectile>();
			thorn->element = Elements::Earth;
			getProjectileHolder().addProjectileDeferredAsPtr(std::move(thorn), spawnPos);
		}
		return;
	}
	else
	{
		HitStats shardStats;
		shardStats.damage = 2.5f;
		shardStats.pushBack = 3.0f;
		int shardCount = 12;
		for (int i = 0; i < shardCount; i++)
		{
			float angle = (6.2831853f / shardCount) * i + getRandomFloat(rng, -0.2f, 0.2f);
			glm::vec2 dir = {std::cos(angle), std::sin(angle)};
			int shardElement = getRandomElementForWild(rng);
			auto shard = std::make_unique<BasicMagicMissle>(shardStats, 0.9f);
			shard->element = shardElement;
			shard->physics.velocity = dir * 7.0f;
			shard->timeAlieve = 1.2f;
			getProjectileHolder().addProjectileDeferredAsPtr(std::move(shard), pos);
		}
	}
}

TrapProjectile::TrapProjectile()
{
	timeAlieve = 20;
}

TrapProjectile::TrapProjectile(HitStats hitStats): hitStats(hitStats)
{
	timeAlieve = 20;
}

void TrapProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	float size = 1;
	glm::vec4 aabb(getPos() - glm::vec2(size / 2.f), size, size);

	auto &assetManaget = getAssetManager();
	auto &elements = assetManaget.elements;

	renderer.renderRectangle(aabb, elements.texture, Colors_White, {}, 0,
		elements.atlas.get(element, 0));

	physics.renderCollider(renderer);
	auto smallerTransform = physics.transform;
	smallerTransform.size *= activateRadiousMultiplier;
	smallerTransform.renderCollider(renderer);

	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
}

void TrapProjectile::onDestroy(std::ranlux24_base &rng)
{
}

bool TrapProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	physics.transform.size = glm::vec2(trapRadious * 2);

	particleTimer -= deltaTime;
	if (particleTimer < 0)
	{
		particleTimer += 0.20f;
		auto particle = getSmallSquareParticle(elementToColor(element),
			elementToSecondaryColor(element));
		particle.folowParent = false;

		for (float i = 0; i < 3.14159f * 2; i += 0.3f)
		{
			float dist = trapRadious;
			glm::vec2 p = getPos() + glm::vec2(std::cos(i), std::sin(i)) * dist;

			if (HasLineOfSightGrid(map, getPos(), p))
			{
				particleSystem.emitParticles(particle, p, rng, physics.getPos());
			}
		}
	}

	auto projectile = physics.transform;
	auto smallerTransform = physics.transform;
	smallerTransform.size *= activateRadiousMultiplier;

	if (!triggered)
	for (auto &e : entityHolder.entities)
	{
		if (e->dying) continue; // skip dying entities
		if (smallerTransform.intersectTransform(e->physics.transform))
		{
			triggered = true;
			break;
		}
	}

	if (triggered)
	{
		triggerTimer -= deltaTime;
		if (triggerTimer <= 0)
		{
			for (auto &e : entityHolder.entities)
			{
				if (e->dying) continue; // skip dying entities
				if (projectile.intersectTransform(e->physics.transform))
				{
					glm::vec2 pushBack = {};
					e->life.computeHit(hitStats, element, e->element, {e->physics.getPos() - getPos()}, pushBack);
					e->physics.velocity += pushBack;
				}
			}
			return false;
		}
	}

	particleSystem.update(deltaTime);
	return true;
}

ThornProjectile::ThornProjectile()
{
	hitStats.damage = 1.0f;
	hitStats.pushBack = 0.0f;
	element = Elements::Earth;
	timeAlieve = 14.0f;
	physics.transform.size = {PIXEL_SIZE * 8.0f, PIXEL_SIZE * 8.0f};
	physics.transform.isCircleCollider = true;
}

bool ThornProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	auto projectile = physics.transform;
	for (auto &e : entityHolder.entities)
	{
		if (e->dying) continue; // skip dying entities
		if (projectile.intersectTransform(e->physics.transform))
		{
			glm::vec2 pushBack = {};
			e->life.computeHit(hitStats, element, e->element, {}, pushBack);
			e->physics.velocity += pushBack;

			glm::vec2 damagePos = e->physics.getPos();
			damagePos.y -= e->physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(hitStats.damage, damagePos);

			return false;
		}
	}

	return true;
}

void ThornProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	float renderSize = PIXEL_SIZE * 16.0f;
	glm::vec2 pos = physics.getPos();
	glm::vec4 rect = {pos.x - renderSize * 0.5f, pos.y - renderSize * 0.5f,
		renderSize, renderSize};
	renderer.renderRectangle(rect, assetManager.thorn, {1, 1, 1, 1});
	physics.renderCollider(renderer);
}

void ThornProjectile::onDestroy(std::ranlux24_base &rng)
{
}

EarthThornBoltProjectile::EarthThornBoltProjectile()
{
	hitStats.damage = 5.0f;
	hitStats.pushBack = 3.0f;
	element = Elements::Earth;
	timeAlieve = 6.0f;
	physics.transform.size = {PIXEL_SIZE * 6.0f, PIXEL_SIZE * 6.0f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 80;
}

bool EarthThornBoltProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		particleEmmision = getBasicMagicMissleParticleEmision(element, 1.1f);
		particleSystem.emitParticles(particleEmmision.create, physics.getPos(), rng, physics.getPos());
	}

	trailTimer -= deltaTime;
	if (trailTimer <= 0.0f)
	{
		trailTimer += trailInterval;
		particleSystem.emitParticles(particleEmmision.sustain, physics.getPos(), rng, physics.getPos());
		if (spawnedThorns < maxThorns)
		{
			auto thorn = std::make_unique<ThornProjectile>();
			thorn->element = element;
			getProjectileHolder().addProjectileDeferredAsPtr(std::move(thorn), physics.getPos());
			spawnedThorns++;
		}
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void EarthThornBoltProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	physics.renderCollider(renderer);
}

void EarthThornBoltProjectile::onDestroy(std::ranlux24_base &rng)
{
}

EarthWaterThornBoltProjectile::EarthWaterThornBoltProjectile()
{
	hitStats.damage = 30.0f;
	hitStats.pushBack = 3.0f;
	element = Elements::Earth;
	timeAlieve = 6.0f;
	baseColliderSize = {PIXEL_SIZE * 7.5f, PIXEL_SIZE * 7.5f};
	physics.transform.size = baseColliderSize;
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 120;
	storedDamage = maxStoredDamage;
}

void EarthWaterThornBoltProjectile::updateVisualScale()
{
	float ratio = maxStoredDamage > 0 ? (float)storedDamage / (float)maxStoredDamage : 0.0f;
	ratio = glm::clamp(ratio, 0.0f, 1.0f);
	float scale = minScale + (1.0f - minScale) * ratio;

	physics.transform.size = baseColliderSize * scale;

	particleEmmision = baseEmmision;
	particleEmmision.sustain.createApearence.size *= scale;
	particleEmmision.sustain.endApearence.size *= scale;
	particleEmmision.release.createApearence.size *= scale;
	particleEmmision.release.endApearence.size *= scale;
	particleEmmision.create.createApearence.size *= scale;
	particleEmmision.create.endApearence.size *= scale;
}

bool EarthWaterThornBoltProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		baseEmmision = getBasicMagicMissleParticleEmision(element, particleSizeBias);
		particleEmmision = baseEmmision;
		updateVisualScale();
		particleSystem.emitParticles(particleEmmision.create, physics.getPos(), rng, physics.getPos());
		orbitParticle = getOrbitParticle(elementToSecondaryColor(element), elementToColor(element));
		orbitParticle.onCreateCount = 2;
		orbitParticle.particleLifeTime = {0.45f, 0.7f};
		orbitParticle.createApearence.size = glm::vec2{2.4f, 3.4f} * PIXEL_SIZE;
		orbitParticle.endApearence.size = glm::vec2{1.4f, 2.4f} * PIXEL_SIZE;
		orbitParticle.animationScaleX = {PIXEL_SIZE * 5.0f, PIXEL_SIZE * 11.0f};
		orbitParticle.animationScaleY = {PIXEL_SIZE * 5.0f, PIXEL_SIZE * 11.0f};
		orbitParticle.folowParent = true;
		orbitTimer = getRandomFloat(rng, 0.0f, orbitInterval);
	}

	trailTimer -= deltaTime;
	if (trailTimer <= 0.0f)
	{
		trailTimer += trailInterval;
		particleSystem.emitParticles(particleEmmision.sustain, physics.getPos(), rng, physics.getPos());

		if (storedDamage > 0)
		{
			auto thorn = std::make_unique<ThornProjectile>();
			thorn->element = element;
			glm::vec2 offset = {getRandomFloat(rng, -spawnOffset, spawnOffset),
				getRandomFloat(rng, -spawnOffset, spawnOffset)};
			getProjectileHolder().addProjectileDeferredAsPtr(std::move(thorn), physics.getPos() + offset);
			storedDamage = std::max(0, storedDamage - 1);
			hitStats.damage = (float)storedDamage;
			updateVisualScale();

			if (storedDamage <= 0)
			{
				particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
				return false;
			}
		}
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void EarthWaterThornBoltProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	physics.renderCollider(renderer);
}

void EarthWaterThornBoltProjectile::onDestroy(std::ranlux24_base &rng)
{
}

RicochetProjectile::RicochetProjectile()
{
	hitStats.damage = 5.0f;
	hitStats.pushBack = 2.6f;
	element = Elements::Earth;
	timeAlieve = 18.0f;
	physics.transform.size = {PIXEL_SIZE * 7.0f, PIXEL_SIZE * 7.0f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 40;
}

bool RicochetProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		auto startColor = elementToSecondaryColor(element); startColor.a = 0.7f;
		auto endColor = elementToColor(element); endColor.a = 0.7f;

		bodyParticle = getBasicMagicMissleParticle(startColor, endColor);
		bodyParticle.onCreateCount = 2;
		bodyParticle.folowParent = true;
		bodyParticle.particleLifeTime = {timeAlieve + 0.2f, timeAlieve + 0.2f};
		bodyParticle.velocityX = {0.0f, 0.0f};
		bodyParticle.velocityY = {0.0f, 0.0f};
		bodyParticle.dragX = {0.0f, 0.0f};
		bodyParticle.dragY = {0.0f, 0.0f};
		bodyParticle.rotationSpeed = {0.0f, 0.0f};
		bodyParticle.createApearence.size = glm::vec2{4.5f, 6.5f} * PIXEL_SIZE;
		bodyParticle.endApearence.size = glm::vec2{4.0f, 6.0f} * PIXEL_SIZE;
		bodyParticle.positionX = glm::vec2{-1.5f, 1.5f} * PIXEL_SIZE;
		bodyParticle.positionY = glm::vec2{-1.5f, 1.5f} * PIXEL_SIZE;

		particleSystem.emitParticles(bodyParticle, physics.getPos(), rng, physics.getPos());
		orbitParticle = getOrbitParticle(startColor, endColor);
		orbitParticle.onCreateCount = 2;
		orbitParticle.particleLifeTime = {0.35f, 0.6f};
		orbitParticle.createApearence.size = glm::vec2{2.0f, 2.6f} * PIXEL_SIZE;
		orbitParticle.endApearence.size = glm::vec2{1.0f, 1.8f} * PIXEL_SIZE;
		orbitParticle.animationScaleX = {PIXEL_SIZE * 2.5f, PIXEL_SIZE * 6.0f};
		orbitParticle.animationScaleY = {PIXEL_SIZE * 2.5f, PIXEL_SIZE * 6.0f};
		orbitParticle.folowParent = true;
		orbitTimer = getRandomFloat(rng, 0.0f, orbitInterval);
	}

	orbitTimer -= deltaTime;
	if (orbitTimer <= 0.0f)
	{
		orbitTimer += orbitInterval;
		particleSystem.emitParticles(orbitParticle, physics.getPos(), rng, physics.getPos());
	}

	hitCooldown = std::max(0.0f, hitCooldown - deltaTime);

	glm::vec2 prevVelocity = physics.velocity;
	physics.updateForces(deltaTime, 0.0f);
	physics.resolveConstrains(map);

	bool bouncedX = physics.leftTouch || physics.rightTouch;
	bool bouncedY = physics.upTouch || physics.downTouch;
	if (bouncedX) { physics.velocity.x = -prevVelocity.x; }
	if (bouncedY) { physics.velocity.y = -prevVelocity.y; }
	if (bouncedX || bouncedY)
	{
		physics.velocity *= 0.98f;
	}

	physics.updateMove();

	if (hitCooldown <= 0.0f)
	{
		auto projectile = physics.transform;
		for (auto &e : entityHolder.entities)
		{
			if (e->dying) continue; // skip dying entities
			if (projectile.intersectTransform(e->physics.transform))
			{
				glm::vec2 pushBack = {};
				e->life.computeHit(hitStats, element, e->element, physics.velocity, pushBack);
				e->physics.velocity += pushBack;
				addStatusEffectFromElement(e->statusEffects, e->statusImmunities, element, 2.0f);

				glm::vec2 damagePos = e->physics.getPos();
				damagePos.y -= e->physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(hitStats.damage, damagePos);

				hitCooldown = hitCooldownDuration;
				break;
			}
		}
	}

	particleSystem.update(deltaTime);
	return true;
}

void RicochetProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
}

void RicochetProjectile::onDestroy(std::ranlux24_base &rng)
{
}

FastMagicBoltProjectile::FastMagicBoltProjectile()
{
	hitStats.damage = 22.0f;
	hitStats.pushBack = 4.0f;
	element = Elements::Fire;
	timeAlieve = 3.5f;
	physics.transform.size = {PIXEL_SIZE * 6.5f, PIXEL_SIZE * 6.5f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 120;
}

void FastMagicBoltProjectile::setupParticles(std::ranlux24_base &rng)
{
	auto startColor = elementToSecondaryColor(element); startColor.a = 0.7f;
	auto endColor = elementToColor(element); endColor.a = 0.7f;

	coreParticle = getBasicMagicMissleParticle(startColor, endColor);
	coreParticle.onCreateCount = 2;
	coreParticle.folowParent = true;
	coreParticle.particleLifeTime = {timeAlieve + 0.1f, timeAlieve + 0.2f};
	coreParticle.velocityX = {0.0f, 0.0f};
	coreParticle.velocityY = {0.0f, 0.0f};
	coreParticle.dragX = {0.0f, 0.0f};
	coreParticle.dragY = {0.0f, 0.0f};
	coreParticle.rotationSpeed = {0.0f, 0.0f};
	coreParticle.createApearence.size = glm::vec2{5.2f, 7.4f} * PIXEL_SIZE;
	coreParticle.endApearence.size = glm::vec2{4.8f, 7.0f} * PIXEL_SIZE;
	coreParticle.positionX = glm::vec2{-2.0f, 2.0f} * PIXEL_SIZE;
	coreParticle.positionY = glm::vec2{-2.0f, 2.0f} * PIXEL_SIZE;

	trailParticle = getSpiralParticle(startColor, endColor);
	trailParticle.onCreateCount = 2;
	trailParticle.folowParent = false;
	trailParticle.particleLifeTime = {0.24f, 0.45f};
	trailParticle.createApearence.size *= 0.7f;
	trailParticle.endApearence.size *= 0.6f;
	trailParticle.velocityX = {0.0f, 0.0f};
	trailParticle.velocityY = {0.0f, 0.0f};
	trailParticle.animationSpeed = {-8.0f, 8.0f};
	trailParticle.animationScaleX = {PIXEL_SIZE * 6.0f, PIXEL_SIZE * 10.0f};
	trailParticle.animationScaleY = {PIXEL_SIZE * 6.0f, PIXEL_SIZE * 10.0f};

	particleSystem.emitParticles(coreParticle, physics.getPos(), rng, physics.getPos());
}

void FastMagicBoltProjectile::explode(EntityHolder &entityHolder, std::ranlux24_base &rng)
{
	if (exploded)
	{
		return;
	}

	exploded = true;
	float statusAmount = 0.0f;
	if (element == Elements::Ice) { statusAmount = 5.0f; }
	if (element == Elements::Fire) { statusAmount = 4.0f; }

	glm::vec4 startColor = elementToSecondaryColor(element); startColor.a = 0.8f;
	glm::vec4 endColor = elementToColor(element); endColor.a = 0.8f;
	ParticleSettings burstParticle = element == Elements::Ice
		? getFrostShardParticle(startColor, endColor)
		: getSparkBurstParticle(startColor, endColor);
	burstParticle.onCreateCount = 14;
	burstParticle.folowParent = false;
	burstParticle.particleLifeTime = {0.25f, 0.45f};
	burstParticle.velocityX = {-0.9f, 0.9f};
	burstParticle.velocityY = {-0.9f, 0.9f};
	particleSystem.emitParticles(burstParticle, physics.getPos(), rng, physics.getPos());

	float radius2 = explosionRadius * explosionRadius;
	for (auto &e : entityHolder.entities)
	{
		if (e->dying) continue; // skip dying entities
		glm::vec2 diff = e->physics.getPos() - physics.getPos();
		if (glm::dot(diff, diff) <= radius2)
		{
			if (statusAmount > 0.0f)
			{
				addStatusEffectFromElement(e->statusEffects, e->statusImmunities, element, statusAmount);
			}
		}
	}
}

bool FastMagicBoltProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		float len = glm::length(physics.velocity);
		if (len <= 0.0001f)
		{
			moveDir = {1.0f, 0.0f};
		}
		else
		{
			moveDir = physics.velocity / len;
		}

		setupParticles(rng);
	}

	accelTimer += deltaTime;
	float speed = slowSpeed;
	if (accelTimer > slowHoldTime)
	{
		float t = (accelTimer - slowHoldTime) / accelDuration;
		t = glm::clamp(t, 0.0f, 1.0f);
		speed = slowSpeed + (maxSpeed - slowSpeed) * t;
	}
	physics.velocity = moveDir * speed;

	physics.updateForces(deltaTime, 0.0f);
	physics.resolveConstrains(map);
	physics.updateMove();

	trailTimer -= deltaTime;
	if (trailTimer <= 0.0f)
	{
		trailTimer += trailInterval;
		particleSystem.emitParticles(trailParticle, physics.getPos(), rng, physics.getPos());
	}

	if (physics.leftTouch || physics.rightTouch || physics.upTouch || physics.downTouch)
	{
		explode(entityHolder, rng);
		return false;
	}

	auto projectile = physics.transform;
	for (auto &e : entityHolder.entities)
	{
		if (e->dying) continue; // skip dying entities
		if (projectile.intersectTransform(e->physics.transform))
		{
			glm::vec2 pushBack = {};
			e->life.computeHit(hitStats, element, e->element, physics.velocity, pushBack);
			e->physics.velocity += pushBack;

			glm::vec2 damagePos = e->physics.getPos();
			damagePos.y -= e->physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(hitStats.damage, damagePos);

			explode(entityHolder, rng);
			return false;
		}
	}

	particleSystem.update(deltaTime);
	return true;
}

void FastMagicBoltProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
}

void FastMagicBoltProjectile::onDestroy(std::ranlux24_base &rng)
{
	if (!exploded)
	{
		return;
	}

	for (auto &p : particleSystem.particles)
	{
		if (p.durationRemaining > 0.25f)
		{
			p.durationRemaining = 0.2f;
			p.durationTotal = 0.2f;
		}
	}
}

BoulderProjectile::BoulderProjectile()
{
	hitStats.damage = 10.0f;
	hitStats.pushBack = 8.0f;
	element = Elements::NoneElement;
	physics.transform.size = {PIXEL_SIZE * 12.0f, PIXEL_SIZE * 12.0f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 120;
}

bool BoulderProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		glm::vec4 startColor = {0.55f, 0.55f, 0.55f, 0.9f};
		glm::vec4 endColor = {0.3f, 0.3f, 0.3f, 0.9f};
		bigParticle = getSmallSquareParticle(startColor, endColor);
		bigParticle.onCreateCount = 1;
		bigParticle.folowParent = true;
		bigParticle.particleLifeTime = {timeAlieve + 0.2f, timeAlieve + 0.2f};
		bigParticle.createApearence.size = {0.7f, 0.85f};
		bigParticle.endApearence.size = {0.7f, 0.85f};
		bigParticle.velocityX = {0.0f, 0.0f};
		bigParticle.velocityY = {0.0f, 0.0f};

		trailParticle = getSmallSquareParticle(startColor, endColor);
		trailParticle.onCreateCount = 1;
		trailParticle.folowParent = false;
		trailParticle.particleLifeTime = {0.25f, 0.45f};
		trailParticle.createApearence.size = {0.12f, 0.2f};
		trailParticle.endApearence.size = {0.05f, 0.12f};
		trailParticle.velocityX = {0.0f, 0.0f};
		trailParticle.velocityY = {0.0f, 0.0f};

		particleSystem.emitParticles(bigParticle, physics.getPos(), rng, physics.getPos());
	}

	trailTimer -= deltaTime;
	if (trailTimer <= 0.0f)
	{
		trailTimer += trailInterval;
		particleSystem.emitParticles(trailParticle, physics.getPos(), rng, physics.getPos());
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats))
	{
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void BoulderProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	glm::vec4 aabb = physics.getAABB();
	renderer.renderRectangle(aabb, {0.6f, 0.6f, 0.6f, 1.0f});
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
}

void BoulderProjectile::onDestroy(std::ranlux24_base &rng)
{
	for (auto &p : particleSystem.particles)
	{
		if (p.durationRemaining > 0.4f)
		{
			p.durationRemaining = 0.2f;
			p.durationTotal = 0.2f;
		}
	}

	glm::vec4 startColor = {0.55f, 0.55f, 0.55f, 0.9f};
	glm::vec4 endColor = {0.3f, 0.3f, 0.3f, 0.9f};
	auto burstParticle = getSmallSquareParticle(startColor, endColor);
	burstParticle.onCreateCount = 6;
	burstParticle.folowParent = false;
	burstParticle.particleLifeTime = {0.25f, 0.35f};
	burstParticle.createApearence.size = {0.3f, 0.45f};
	burstParticle.endApearence.size = {0.1f, 0.28f};
	burstParticle.velocityX = {-0.6f, 0.6f};
	burstParticle.velocityY = {-0.6f, 0.6f};
	particleSystem.emitParticles(burstParticle, physics.getPos(), rng, physics.getPos());
}

BigIceBlockProjectile::BigIceBlockProjectile()
{
	hitStats.damage = 16.0f;
	hitStats.pushBack = 6.0f;
	element = Elements::Ice;
	timeAlieve = 6.0f;
	physics.transform.size = {PIXEL_SIZE * 10.0f, PIXEL_SIZE * 10.0f};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 160;
}

bool BigIceBlockProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		bigParticle = getBasicMagicMissleParticle(bigStartColor, bigEndColor);
		bigParticle.onCreateCount = 3;
		bigParticle.folowParent = true;
		bigParticle.particleLifeTime = {timeAlieve + 0.2f, timeAlieve + 0.2f};
		bigParticle.createApearence.size = glm::vec2{7.0f, 10.5f} * PIXEL_SIZE;
		bigParticle.endApearence.size = glm::vec2{6.5f, 9.5f} * PIXEL_SIZE;
		bigParticle.velocityX = {0.0f, 0.0f};
		bigParticle.velocityY = {0.0f, 0.0f};
		bigParticle.dragX = {0.0f, 0.0f};
		bigParticle.dragY = {0.0f, 0.0f};
		bigParticle.rotationSpeed = {0.0f, 0.0f};
		bigParticle.positionX = glm::vec2{-2.5f, 2.5f} * PIXEL_SIZE;
		bigParticle.positionY = glm::vec2{-2.5f, 2.5f} * PIXEL_SIZE;

		trailParticle = getFrostShardParticle(bigStartColor, bigEndColor);
		trailParticle.onCreateCount = 1;
		trailParticle.folowParent = false;
		trailParticle.particleLifeTime = {0.2f, 0.35f};
		trailParticle.createApearence.size = glm::vec2{0.12f, 0.2f};
		trailParticle.endApearence.size = glm::vec2{0.06f, 0.16f};
		trailParticle.velocityX = {0.0f, 0.0f};
		trailParticle.velocityY = {0.0f, 0.0f};

		particleSystem.emitParticles(bigParticle, physics.getPos(), rng, physics.getPos());
		orbitParticle = getOrbitParticle(bigStartColor, bigEndColor);
		orbitParticle.onCreateCount = 2;
		orbitParticle.particleLifeTime = {0.35f, 0.6f};
		orbitParticle.createApearence.size = glm::vec2{2.0f, 2.8f} * PIXEL_SIZE;
		orbitParticle.endApearence.size = glm::vec2{1.0f, 2.0f} * PIXEL_SIZE;
		orbitParticle.animationScaleX = {PIXEL_SIZE * 7.0f, PIXEL_SIZE * 14.0f};
		orbitParticle.animationScaleY = {PIXEL_SIZE * 7.0f, PIXEL_SIZE * 14.0f};
		orbitParticle.folowParent = true;
		orbitTimer = getRandomFloat(rng, 0.0f, orbitInterval);
	}

	trailTimer -= deltaTime;
	if (trailTimer <= 0.0f)
	{
		trailTimer += trailInterval;
		particleSystem.emitParticles(trailParticle, physics.getPos(), rng, physics.getPos());
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats))
	{
		shouldBurst = true;
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		shouldBurst = true;
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void BigIceBlockProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	glm::vec2 center = physics.getPos();
	float size = physics.transform.size.x * 1.2f;
	glm::vec2 offsets[] = {
		{0.0f, 0.0f},
		{PIXEL_SIZE * 1.2f, -PIXEL_SIZE * 0.6f},
		{-PIXEL_SIZE * 1.0f, PIXEL_SIZE * 0.8f}
	};
	for (auto &offset : offsets)
	{
		glm::vec2 drawPos = center + offset;
		glm::vec4 rect = {drawPos.x - size * 0.5f, drawPos.y - size * 0.5f, size, size};
		renderer.renderRectangle(rect, assetManager.particleCircle, bigStartColor);
	}
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
}

void BigIceBlockProjectile::onDestroy(std::ranlux24_base &rng)
{
	if (!shouldBurst)
	{
		return;
	}

	for (auto &p : particleSystem.particles)
	{
		if (p.durationRemaining > 0.35f)
		{
			p.durationRemaining = 0.25f;
			p.durationTotal = 0.25f;
		}
	}

	glm::vec4 startColor = {0.75f, 0.9f, 1.0f, 0.9f};
	glm::vec4 endColor = {0.4f, 0.7f, 1.0f, 0.9f};
	auto burstParticle = getFrostShardParticle(startColor, endColor);
	burstParticle.onCreateCount = 10;
	burstParticle.folowParent = false;
	burstParticle.particleLifeTime = {0.35f, 0.55f};
	burstParticle.createApearence.size = {0.18f, 0.3f};
	burstParticle.endApearence.size = {0.08f, 0.2f};
	burstParticle.velocityX = {-0.7f, 0.7f};
	burstParticle.velocityY = {-0.7f, 0.7f};
	particleSystem.emitParticles(burstParticle, physics.getPos(), rng, physics.getPos());

	HitStats shardHit;
	shardHit.damage = 1.0f;
	shardHit.pushBack = 2.0f;
	const int shardCount = 14;
	const float shardSpeed = 7.0f;
	const float twoPi = 6.2831853f;

	for (int i = 0; i < shardCount; i++)
	{
		float angle = (twoPi / (float)shardCount) * i + getRandomFloat(rng, -0.15f, 0.15f);
		glm::vec2 dir = {std::cos(angle), std::sin(angle)};

		auto shard = std::make_unique<BasicMagicMissle>(shardHit, 0.6f);
		shard->element = Elements::Ice;
		shard->timeAlieve = 1.2f;
		shard->physics.transform.size = {PIXEL_SIZE * 6.0f, PIXEL_SIZE * 6.0f};
		shard->physics.velocity = dir * shardSpeed;
		getProjectileHolder().addProjectileDeferredAsPtr(std::move(shard), physics.getPos());
	}
}

ElementWallProjectile::ElementWallProjectile()
{
	particleSystem.maxCount = 200;
	setElementType(Elements::Fire);
}

ElementWallProjectile::ElementWallProjectile(int elementType)
{
	particleSystem.maxCount = 200;
	setElementType(elementType);
}

void ElementWallProjectile::setElementType(int elementType)
{
	element = elementType;
	physics.transform.isCircleCollider = false;
	if (elementType == Elements::Ice)
	{
		hitStats.damage = 0.12f;
		hitStats.pushBack = 1.4f;
		timeAlieve = 18.0f;
		hitTimerPenalty = 0.08f;
	}
	else
	{
		hitStats.damage = 0.8f;
		hitStats.pushBack = 0.4f;
		timeAlieve = 12.0f;
		hitTimerPenalty = 0.15f;
	}
}

void ElementWallProjectile::setupWall(glm::vec2 aimDir)
{
	float len = glm::length(aimDir);
	if (len <= 0.0001f)
	{
		aimDir = {1.0f, 0.0f};
		len = 1.0f;
	}
	wallNormal = aimDir / len;
	physics.transform.isCircleCollider = false;
	physics.transform.size = {wallLength, wallLength};
}

bool ElementWallProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		flameParticle = getSmallSquareParticle(elementToSecondaryColor(element), elementToColor(element));
		flameParticle.onCreateCount = 3;
		flameParticle.particleLifeTime = {0.45f, 1.1f};
		flameParticle.createApearence.size = {0.2f, 0.32f};
		flameParticle.endApearence.size = {0.1f, 0.22f};
		flameParticle.folowParent = false;
	}

	particleTimer -= deltaTime;
	while (particleTimer <= 0.0f)
	{
		particleTimer += particleInterval;
		glm::vec2 axis = glm::vec2(-wallNormal.y, wallNormal.x);
		float along = getRandomFloat(rng, -wallLength * 0.5f, wallLength * 0.5f);
		float across = getRandomFloat(rng, -wallThickness * 0.4f, wallThickness * 0.4f);
		glm::vec2 spawnPos = physics.getPos() + axis * along + wallNormal * across;
		particleSystem.emitParticles(flameParticle, spawnPos, rng, physics.getPos());
	}

	tickTimer -= deltaTime;
	if (tickTimer <= 0.0f)
	{
		tickTimer += tickInterval;
		int hitCount = 0;
		glm::vec2 axis = glm::vec2(-wallNormal.y, wallNormal.x);
		for (auto &e : entityHolder.entities)
		{
			if (e->dying) continue; // skip dying entities
			glm::vec2 diff = e->physics.getPos() - physics.getPos();
			float along = glm::dot(diff, axis);
			float across = glm::dot(diff, wallNormal);
			if (std::abs(along) > wallLength * 0.5f + e->physics.transform.size.y * 0.5f)
			{
				continue;
			}
			if (std::abs(across) > wallThickness * 0.5f + e->physics.transform.size.x * 0.5f)
			{
				continue;
			}

			glm::vec2 pushBack = {};
			glm::vec2 hitDir = wallNormal;
			if (glm::length(hitDir) <= 0.0001f)
			{
				hitDir = e->physics.getPos() - physics.getPos();
			}
			e->life.computeHit(hitStats, element, e->element, {hitDir}, pushBack);
			e->physics.velocity += pushBack;
			glm::vec2 damagePos = e->physics.getPos();
			damagePos.y -= e->physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(hitStats.damage, damagePos);
			hitCount++;
		}

		if (hitCount > 0)
		{
			timeAlieve -= hitTimerPenalty * hitCount;
		}
	}

	particleSystem.update(deltaTime);
	return true;
}

void ElementWallProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	glm::vec2 axis = glm::vec2(-wallNormal.y, wallNormal.x);
	float axisLen = glm::length(axis);
	if (axisLen <= 0.0001f)
	{
		axis = {1.0f, 0.0f};
		axisLen = 1.0f;
	}
	axis /= axisLen;

	float segmentLength = wallLength / (float)segmentCount;
	for (int i = 0; i < segmentCount; i++)
	{
		float t = (i + 0.5f) / (float)segmentCount;
		float along = (t - 0.5f) * wallLength;
		glm::vec2 center = physics.getPos() + axis * along;
		glm::vec4 rect = {center.x - segmentLength * 0.5f, center.y - wallThickness * 0.5f,
			segmentLength, wallThickness};
		renderer.renderRectangleOutline(rect, Colors_Blue, 0.02f);
	}
}

void ElementWallProjectile::onDestroy(std::ranlux24_base &rng)
{
}

HomingMagicMissle::HomingMagicMissle()
{
	hitStats.damage = 2;
	hitStats.pushBack = 5.2f;
}

HomingMagicMissle::HomingMagicMissle(HitStats hitStats)
{
	this->hitStats = hitStats;
}

HomingMagicMissle::HomingMagicMissle(HitStats hitStats, float particleSizeBias)
{
	this->hitStats = hitStats;
	this->particleSizeBias = particleSizeBias;
}

bool HomingMagicMissle::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	auto safeNormalize = [](glm::vec2 v)
	{
		float len = glm::length(v);
		if (len <= 0.00001f) { return glm::vec2(0.0f); }
		return v / len;
	};

	if (firstTime)
	{
		firstTime = 0;
		travelSpeed = glm::length(physics.velocity);
		if (travelSpeed <= 0.00001f) { travelSpeed = 0.00001f; }

		glm::vec4 startColor = elementToColor(element);
		startColor.a = 0.3f;
		glm::vec4 endColor = {0.7f, 0.3f, 0.95f, 0.3f};

		particleEmmision.sustain = getBasicMagicMissleParticle(startColor, endColor);
		particleEmmision.release = getBasicMagicMissleParticle(startColor, endColor);
		particleEmmision.release.particleLifeTime *= 2.0f;
		particleEmmision.emitTimer = 0.01f;

		particleEmmision.sustain.createApearence.size *= particleSizeBias;
		particleEmmision.sustain.endApearence.size *= particleSizeBias;
		particleEmmision.release.createApearence.size *= particleSizeBias;
		particleEmmision.release.endApearence.size *= particleSizeBias;
		particleEmmision.create = particleEmmision.sustain;

		particleEmmision.sustain.folowParent = false;
		particleEmmision.release.folowParent = false;
		particleEmmision.create.folowParent = false;

		particleSystem.emitParticles(particleEmmision.create, physics.getPos(), rng, physics.getPos());
	}

	glm::vec2 toTarget = {};
	float bestDist2 = homingRange * homingRange;
	bool hasTarget = false;

	for (auto &e : entityHolder.entities)
	{
		if (e->dying) continue; // skip dying entities
		glm::vec2 diff = e->physics.getPos() - physics.getPos();
		float dist2 = glm::dot(diff, diff);
		if (dist2 < bestDist2)
		{
			bestDist2 = dist2;
			toTarget = diff;
			hasTarget = true;
		}
	}

	if (hasTarget)
	{
		glm::vec2 desiredDir = safeNormalize(toTarget);
		float currentSpeed = glm::length(physics.velocity);
		float speed = currentSpeed > 0.00001f ? currentSpeed : travelSpeed;
		glm::vec2 currentDir = currentSpeed > 0.00001f ? (physics.velocity / currentSpeed) : desiredDir;

		float turn = glm::clamp(homingTurnRate * deltaTime, 0.0f, 1.0f);
		glm::vec2 newDir = safeNormalize(glm::mix(currentDir, desiredDir, turn));
		if (glm::length(newDir) > 0.00001f)
		{
			physics.velocity = newDir * speed;
		}
	}

	particleTimer -= deltaTime;
	if (particleTimer < 0)
	{
		particleTimer += particleEmmision.emitTimer;
		particleSystem.emitParticles(particleEmmision.sustain, physics.getPos(), rng, physics.getPos());
	}

	if (basicProjectileHitEntitiesLogic(physics, physics.velocity,
		element, entityHolder, hitStats))
	{
		return false;
	}

	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
		return false;
	}

	particleSystem.update(deltaTime);

	return true;
}

void HomingMagicMissle::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	physics.renderCollider(renderer);
}

void HomingMagicMissle::onDestroy(std::ranlux24_base &rng)
{
}

// Enemy orb projectile - hits player and summons
EnemyOrbProjectile::EnemyOrbProjectile()
{
	physics.transform.size = {PIXEL_SIZE * 10, PIXEL_SIZE * 10};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 100;
	timeAlieve = 8.0f;
	setupParticles();
}

void EnemyOrbProjectile::setDirection(glm::vec2 dir)
{
	if (glm::length(dir) > 0.0001f)
	{
		moveDir = glm::normalize(dir);
	}
	physics.velocity = moveDir * speed;
}

void EnemyOrbProjectile::setupParticles()
{
	// Core particle - bright center, stays close
	coreParticle = {};
	coreParticle.onCreateCount = 1;
	coreParticle.positionX = {-PIXEL_SIZE * 2, PIXEL_SIZE * 2};
	coreParticle.positionY = {-PIXEL_SIZE * 2, PIXEL_SIZE * 2};
	coreParticle.particleLifeTime = {0.3f, 0.5f};
	coreParticle.velocityX = {-0.4f, 0.4f};
	coreParticle.velocityY = {-0.4f, 0.4f};
	coreParticle.dragX = {0.8f, 1.2f};
	coreParticle.dragY = {0.8f, 1.2f};
	// Start bright orange
	coreParticle.createApearence.color1 = {0.9f, 0.7f, 0.2f, 1.f};
	coreParticle.createApearence.color2 = {0.9f, 0.6f, 0.1f, 1.f};
	coreParticle.createApearence.size = {PIXEL_SIZE * 7, PIXEL_SIZE * 7};
	// Fade to bright white
	coreParticle.endApearence.color1 = {1.0f, 1.0f, 1.0f, 0.6f};
	coreParticle.endApearence.color2 = {1.0f, 1.0f, 0.95f, 0.6f};
	coreParticle.endApearence.size = {PIXEL_SIZE * 4, PIXEL_SIZE * 4};
	coreParticle.tranzitionType = ParticleSettings::linear;
	coreParticle.folowParent = true;
	coreParticle.texture = getAssetManager().particleCircle;

	// Glow particle - outer glow, moves outward slowly
	glowParticle = {};
	glowParticle.animationType = glowParticle.animationAtom;
	glowParticle.onCreateCount = 2;
	//glowParticle.animationSpeed = {1.f,1.f};
	//glowParticle.animationAcceleration = {1.f,1.f};
	//glowParticle.animationScaleX = {0.2f,0.4f};
	//glowParticle.animationScaleY = {0.2f,0.4f};
	glowParticle.positionX = {-PIXEL_SIZE * 1.5f, PIXEL_SIZE * 1.5f};
	glowParticle.positionY = {-PIXEL_SIZE * 1.5f, PIXEL_SIZE * 1.5f};
	glowParticle.particleLifeTime = {0.4f, 0.7f};
	glowParticle.velocityX = {-0.6f, 0.6f};
	glowParticle.velocityY = {-0.6f, 0.6f};
	glowParticle.dragX = {0.5f, 0.8f};
	glowParticle.dragY = {0.5f, 0.8f};
	// Start bright orange
	glowParticle.createApearence.color1 = {1.0f, 0.65f, 0.15f, 0.8f};
	glowParticle.createApearence.color2 = {1.0f, 0.55f, 0.1f, 0.8f};
	glowParticle.createApearence.size = {PIXEL_SIZE * 5, PIXEL_SIZE * 5};
	// Fade to bright white
	glowParticle.endApearence.color1 = {1.0f, 1.0f, 1.0f, 0.6f};
	glowParticle.endApearence.color2 = {1.0f, 1.0f, 0.9f, 0.6f};
	glowParticle.endApearence.size = {PIXEL_SIZE * 3, PIXEL_SIZE * 3};
	glowParticle.tranzitionType = ParticleSettings::linear;
	glowParticle.folowParent = true;
	glowParticle.texture = getAssetManager().particleCircle;

}

bool EnemyOrbProjectile::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, EntityHolder &entityHolder)
{
	if (firstTime)
	{
		firstTime = false;
		// Emit initial burst
		ParticleSettings burst = coreParticle;
		burst.onCreateCount = 8;
		particleSystem.emitParticles(burst, physics.getPos(), rng, physics.getPos());
	}

	// Emit particles continuously
	particleTimer -= deltaTime;
	while (particleTimer <= 0.0f)
	{
		particleTimer += particleInterval;
		particleSystem.emitParticles(coreParticle, physics.getPos(), rng, physics.getPos());
		particleSystem.emitParticles(glowParticle, physics.getPos(), rng, physics.getPos());
	}

	// Check collision with player
	if (targetPlayer)
	{
		if (physics.transform.intersectTransform(targetPlayer->physics.transform))
		{
			targetPlayer->life -= damage;
			glm::vec2 damagePos = targetPlayer->physics.getPos();
			damagePos.y -= targetPlayer->physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(damage, damagePos);
			return false;
		}
	}

	// Check collision with summons
	if (targetSummons)
	{
		for (auto &summon : targetSummons->summons)
		{
			if (summon->isDying()) continue;
			if (physics.transform.intersectTransform(summon->physics.transform))
			{
				summon->life -= damage;
				glm::vec2 damagePos = summon->physics.getPos();
				damagePos.y -= summon->physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(damage, damagePos);
				return false;
			}
		}
	}

	// Physics and wall collision
	if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
	{
		return false;
	}

	particleSystem.update(deltaTime);
	return true;
}

void EnemyOrbProjectile::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	if (showCollider)
	{
		physics.renderCollider(renderer);
	}
}

void EnemyOrbProjectile::onDestroy(std::ranlux24_base &rng)
{
	// Burst of particles on destroy
	ParticleSettings burst = glowParticle;
	burst.onCreateCount = 12;
	burst.velocityX = {-1.5f, 1.5f};
	burst.velocityY = {-1.5f, 1.5f};
	burst.folowParent = false;
	particleSystem.emitParticles(burst, physics.getPos(), rng, physics.getPos());
}
