#pragma once

#include "Physics.h"
#include "elements.h"
#include <map>
#include <memory>
#include "particleSystem.h"
#include <random>

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

	float particleTimer = 0.1;

	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng)
	{

		if (!basicPhysicsAndCollisionsCheck(deltaTime, map)) { return 0; }

		particleTimer -= deltaTime;
		if (particleTimer < 0)
		{
			particleTimer += 0.01;


			ParticleSettings fireParticle;

			fireParticle.onCreateCount = 1;
			fireParticle.particleLifeTime = {0.3, 0.6};
			fireParticle.velocityX = glm::vec2{-8,8} * PIXEL_SIZE;
			fireParticle.velocityY = glm::vec2{-8,-12} * PIXEL_SIZE;
			fireParticle.createApearence.size = glm::vec2{5, 7} *PIXEL_SIZE;
			fireParticle.endApearence.size = glm::vec2{3, 5} *PIXEL_SIZE;

			fireParticle.dragX = glm::vec2{-5,5} * PIXEL_SIZE;
			fireParticle.dragY = glm::vec2{-50,-80} *PIXEL_SIZE;
			fireParticle.rotation = {0, 360};
			fireParticle.rotationSpeed = {0, 10};
			fireParticle.rotationDrag = {0, 100};
			fireParticle.createApearence.color1 = glm::vec4{255, 223, 135, 80} / 255.f;
			fireParticle.createApearence.color2 = glm::vec4{255, 205, 69, 100} / 255.f; 
			fireParticle.endApearence.color1 = {0.8, 0.1, 0.1, 0.4};
			fireParticle.endApearence.color2 = {0.9, 0.2, 0.2, 0.5};

			fireParticle.tranzitionType = ParticleSettings::TRANZITION_TYPES::abruptCurbe;
			fireParticle.positionX = glm::vec2{-2,2} * PIXEL_SIZE;
			fireParticle.positionY = glm::vec2{-2,2} * PIXEL_SIZE;

			particleSystem.emitParticles(fireParticle, physics.getPos(), rng, physics.getPos());

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

