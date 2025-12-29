#pragma once

#include "Physics.h"
#include "elements.h"
#include <map>
#include <memory>
#include "particleSystem.h"
#include <random>
#include <particles/particleCreator.h>

struct Projectile
{

	PhysicalEntity physics;
	ParticleSystem particleSystem;
	int element = 0;
	int type = 0;
	float timeAlieve = 10;

	Projectile()
	{
		//basic size
		physics.transform.size = {PIXEL_SIZE * 8, PIXEL_SIZE * 8};
	}

	enum ProjectileTypes
	{
		none,
		fireBolt,


	};

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
		std::ranlux24_base &rng) = 0;

	virtual void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) = 0;


};

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

	void update(float deltaTime,
		Map &map,
		ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng)
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

			if (!p.update(deltaTime, map, mainParticleSystem, rng))
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



struct BasicMagicMissle: Projectile
{


	float particleTimer = 0.0;
	bool firstTime = 1;

	ParticleEmissionSettings fireEmision;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng) override
	{

		if (firstTime)
		{
			firstTime = 0;
			fireEmision = getBasicMagicMissleParticleEmision(element);

			particleSystem.emitParticles(fireEmision.create, physics.getPos(), rng, physics.getPos());
		}



		particleTimer -= deltaTime;
		if (particleTimer < 0)
		{
			particleTimer += fireEmision.emitTimer;
			particleSystem.emitParticles(fireEmision.sustain, physics.getPos(), rng, physics.getPos());

		}

		//have chance to emit one particle at least so we keep this last
		if (!basicPhysicsAndCollisionsCheck(deltaTime, map))
		{
			particleSystem.emitParticles(fireEmision.release, physics.getPos(), rng, physics.getPos());
			return 0;
		}


		particleSystem.update(deltaTime);


		return 1;
	}

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer) override
	{
		glm::vec4 aabb = physics.getAABB();

		particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());

		renderer.renderRectangleOutline(aabb, {0,0,1,0.8}, 0.02);

	}



};

