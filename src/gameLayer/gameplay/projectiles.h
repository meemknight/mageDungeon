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

	float particleTimer = 0.0;
	bool firstTime = 1;

	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng)
	{
		static ParticleEmissionSettings fireEmision = getBasicMagicMissleParticleEmision(Elements::Earth);

		if (firstTime)
		{
			firstTime = 0;

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

	virtual void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer)
	{
		glm::vec4 aabb = physics.getAABB();

		particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());

		renderer.renderRectangleOutline(aabb, {0,0,1,0.8}, 0.02);

	}


};

struct ProjectileHolder
{

	std::vector<Projectile> projectiles;

	void update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem, std::ranlux24_base &rng)
	{

		for (auto it = projectiles.begin(); it != projectiles.end(); )
		{
			if (!it->runTimer(deltaTime))
			{
				mainParticleSystem.copyParticles(it->particleSystem, rng, it->physics.getPos());
				it = projectiles.erase(it);
				break;
			}

			if (!it->update(deltaTime, map, mainParticleSystem, rng))
			{
				mainParticleSystem.copyParticles(it->particleSystem, rng, it->physics.getPos());
				it = projectiles.erase(it);
				break;
			}

			++it;
		}

	}

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager, ParticlePostProcessRenderer &particlePostProcessRenderer)
	{

		for (auto &el : projectiles)
		{
			el.render(renderer, assetManager, particlePostProcessRenderer);
		}

	}


};



struct BasicMagicMissle: Projectile
{

	TileSet tileSet;



};

