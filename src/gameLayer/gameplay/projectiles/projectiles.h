#pragma once

#include "gameplay/Physics.h"
#include "gameplay/elements.h"
#include <glm/glm.hpp>
#include <map>
#include <cmath>
#include <memory>
#include "gameplay/particleSystem.h"
#include <gameplay/damageViewerSystem.h>
#include <random>
#include <randomStuff.h>
#include <particles/particleCreator.h>
#include <gameplay/entities/entity.h>

struct Projectile
{

	PhysicalEntity physics;
	ParticleSystem particleSystem;
	int element = 0;
	int type = 0;
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
	EntityHolder &entities, HitStats hitStats);

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
	HitStats hitStats;

	BasicMagicMissle()
	{
		hitStats.damage = 2;
		hitStats.pushBack = 5.2;
	}

	BasicMagicMissle(HitStats hitStats)
	{
		this->hitStats = hitStats;
	}

	BasicMagicMissle(HitStats hitStats, float particleSizeBias)
	{
		this->hitStats = hitStats;
		this->particleSizeBias = particleSizeBias;
	}

	float particleTimer = 0.0;
	bool firstTime = 1;
	float particleSizeBias = 1;

	ParticleEmissionSettings particleEmmision;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override
	{

		if (firstTime)
		{
			firstTime = 0;
			particleEmmision = getBasicMagicMissleParticleEmision(element, particleSizeBias);

			particleSystem.emitParticles(particleEmmision.create, physics.getPos(), rng, physics.getPos());
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
			//we hit an enemy
			return 0;
		}

		//have chance to emit one particle at least so we keep this last
		if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
		{
			particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
			return 0;
		}


		particleSystem.update(deltaTime);


		return 1;
	}

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) override
	{
		glm::vec4 aabb = physics.getAABB();

		particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());

		physics.renderCollider(renderer);
	}

	void onDestroy(std::ranlux24_base &rng) override {}


};


struct TrapProjectile: public CloneableProjectile<TrapProjectile>
{

	HitStats hitStats;

	TrapProjectile()
	{
		timeAlieve = 20;
	}

	TrapProjectile(HitStats hitStats): hitStats(hitStats)
	{
		timeAlieve = 20;
	}


	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override
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

	};

	void onDestroy(std::ranlux24_base &rng) override {}

	float particleTimer = 0;
	float trapRadious = 1.5;
	constexpr static float activateRadiousMultiplier = 0.8;
	bool triggered = 0;
	float triggerTimer = 0.4;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder)
	{

		physics.transform.size = glm::vec2(trapRadious * 2);

		particleTimer -= deltaTime;
		if (particleTimer < 0)
		{
			particleTimer += 0.20;
			//particleTimer = 1000000;

			auto particle = getSmallSquareParticle(elementToColor(element),
				elementToSecondaryColor(element));
			particle.folowParent = false;
			//particle.particleLifeTime.x = timeAlieve + 0.2;
			//particle.particleLifeTime.y = timeAlieve + 0.2;

			for (float i = 0; i < 3.14159 * 2; i += 0.3)
			{

				float dist = trapRadious;
				glm::vec2 p = getPos() + glm::vec2(std::cos(i), std::sin(i)) * dist;

				if (HasLineOfSightGrid(map, getPos(), p))
				{
					particleSystem.emitParticles(particle, p, rng, physics.getPos());
				}
			}

		}

		//try hit enemies
		auto projectile = physics.transform;

		auto smallerTransform = physics.transform;
		smallerTransform.size *= activateRadiousMultiplier;

		if(!triggered)
		for (auto &e : entityHolder.entities)
		{

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

					if (projectile.intersectTransform(e->physics.transform))
					{
						//hit enemy
						glm::vec2 pushBack = {};

						e->life.computeHit(hitStats, element, e->element, {e->physics.getPos() - getPos()}, pushBack);
						e->physics.velocity += pushBack;
					}


				}
				return false;
			};
		}


		particleSystem.update(deltaTime);

		return true;
	};



};

struct BoulderProjectile: public CloneableProjectile<BoulderProjectile>
{
	HitStats hitStats;
	bool firstTime = true;
	float trailTimer = 0.0f;
	float trailInterval = 0.05f;
	ParticleSettings bigParticle;
	ParticleSettings trailParticle;

	BoulderProjectile()
	{
		hitStats.damage = 10.0f;
		hitStats.pushBack = 8.0f;
		element = Elements::NoneElement;
		physics.transform.size = {PIXEL_SIZE * 12.0f, PIXEL_SIZE * 12.0f};
		physics.transform.isCircleCollider = true;
		particleSystem.maxCount = 120;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override
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

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) override
	{
		glm::vec4 aabb = physics.getAABB();
		renderer.renderRectangle(aabb, {0.6f, 0.6f, 0.6f, 1.0f});
		particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	}

	void onDestroy(std::ranlux24_base &rng) override
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
};

// Heavy ice block projectile that bursts into icy shards on impact.
struct BigIceBlockProjectile: public CloneableProjectile<BigIceBlockProjectile>
{
	HitStats hitStats;
	bool firstTime = true;
	float trailTimer = 0.0f;
	float trailInterval = 0.07f;
	ParticleSettings bigParticle;
	ParticleSettings trailParticle;
	bool shouldBurst = false;

	BigIceBlockProjectile()
	{
		hitStats.damage = 14.0f;
		hitStats.pushBack = 6.0f;
		element = Elements::Ice;
		timeAlieve = 6.0f;
		physics.transform.size = {PIXEL_SIZE * 10.0f, PIXEL_SIZE * 10.0f};
		physics.transform.isCircleCollider = true;
		particleSystem.maxCount = 160;
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override
	{
		if (firstTime)
		{
			firstTime = false;
			glm::vec4 startColor = {0.65f, 0.85f, 1.0f, 0.95f};
			glm::vec4 endColor = {0.35f, 0.65f, 1.0f, 0.9f};

			bigParticle = getSmallSquareParticle(startColor, endColor);
			bigParticle.onCreateCount = 1;
			bigParticle.folowParent = true;
			bigParticle.particleLifeTime = {timeAlieve + 0.2f, timeAlieve + 0.2f};
			bigParticle.createApearence.size = {0.85f, 1.05f};
			bigParticle.endApearence.size = {0.85f, 1.05f};
			bigParticle.velocityX = {0.0f, 0.0f};
			bigParticle.velocityY = {0.0f, 0.0f};

			trailParticle = getFrostShardParticle(startColor, endColor);
			trailParticle.onCreateCount = 1;
			trailParticle.folowParent = false;
			trailParticle.particleLifeTime = {0.25f, 0.4f};
			trailParticle.createApearence.size = {0.12f, 0.22f};
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

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		ParticlePostProcessRenderer &particlePostProcessRenderer) override
	{
		glm::vec4 aabb = physics.getAABB();
		renderer.renderRectangle(aabb, {0.6f, 0.8f, 1.0f, 0.9f});
		particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	}

	void onDestroy(std::ranlux24_base &rng) override
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
		const int shardCount = 12;
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
};

struct ElementWallProjectile: public CloneableProjectile<ElementWallProjectile>
{
	HitStats hitStats;
	bool firstTime = true;
	float tickTimer = 0.0f;
	float tickInterval = 0.2f;
	float hitTimerPenalty = 0.15f;
	float wallLength = 6.0f;
	float wallThickness = 0.6f;
	float particleTimer = 0.0f;
	float particleInterval = 0.02f;
	glm::vec2 wallNormal = {1.0f, 0.0f};
	int segmentCount = 8;
	float segmentRadius = 0.45f;
	float segmentSpacing = 0.75f;
	ParticleSettings flameParticle;

	ElementWallProjectile()
	{
		particleSystem.maxCount = 200;
		setElementType(Elements::Fire);
	}

	ElementWallProjectile(int elementType)
	{
		particleSystem.maxCount = 200;
		setElementType(elementType);
	}

	void setElementType(int elementType)
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

	void setupWall(glm::vec2 aimDir)
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

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override
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

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) override
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

	void onDestroy(std::ranlux24_base &rng) override {}
};

struct HomingMagicMissle: public CloneableProjectile<HomingMagicMissle>
{
	HitStats hitStats;

	HomingMagicMissle()
	{
		hitStats.damage = 2;
		hitStats.pushBack = 5.2f;
	}

	HomingMagicMissle(HitStats hitStats)
	{
		this->hitStats = hitStats;
	}

	HomingMagicMissle(HitStats hitStats, float particleSizeBias)
	{
		this->hitStats = hitStats;
		this->particleSizeBias = particleSizeBias;
	}

	float particleTimer = 0.0f;
	bool firstTime = 1;
	float particleSizeBias = 1.4f;
	float homingRange = 6.0f;
	float homingTurnRate = 4.0f;
	float travelSpeed = 0.0f;

	ParticleEmissionSettings particleEmmision;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, EntityHolder &entityHolder) override
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
			return 0;
		}

		if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
		{
			particleSystem.emitParticles(particleEmmision.release, physics.getPos(), rng, physics.getPos());
			return 0;
		}

		particleSystem.update(deltaTime);

		return 1;
	}

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) override
	{
		particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
		physics.renderCollider(renderer);
	}

	void onDestroy(std::ranlux24_base &rng) override {}
};
