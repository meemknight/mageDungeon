#pragma once

#include "gameplay/Physics.h"
#include "gameplay/elements.h"
#include <map>
#include <memory>
#include "gameplay/particleSystem.h"
#include <random>
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
				mainParticleSystem.copyParticles(
					p.particleSystem, rng, p.physics.getPos());
				it = projectiles.erase(it);
				continue;
			}

			if (!p.update(deltaTime, map, mainParticleSystem, rng, entityHolder))
			{
				mainParticleSystem.copyParticles(
					p.particleSystem, rng, p.physics.getPos());
				it = projectiles.erase(it);
				continue;
			}

			++it;
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