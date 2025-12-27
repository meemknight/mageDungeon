#pragma once

#include "Physics.h"
#include "elements.h"
#include <map>
#include <memory>

struct Projectile
{

	PhysicalEntity physics;
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

	virtual bool update(float deltaTime, Map &map)
	{


		if (!basicPhysicsAndCollisionsCheck(deltaTime, map)) { return 0; }


		return 1;
	}

	virtual void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager)
	{
		glm::vec4 aabb = physics.getAABB();

		renderer.renderRectangle(aabb, Colors_Red);
	}


};

struct ProjectileHolder
{

	std::vector<Projectile> projectiles;

	void update(float deltaTime, Map &map)
	{

		for (auto it = projectiles.begin(); it != projectiles.end(); )
		{
			if (!it->runTimer(deltaTime))
			{
				it = projectiles.erase(it);
				break;
			}

			if (!it->update(deltaTime, map))
			{
				it = projectiles.erase(it);
				break;
			}

			++it;
		}

	}

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager)
	{

		for (auto &el : projectiles)
		{
			el.render(renderer, assetManager);
		}

	}


};





struct BasicMagicMissle: Projectile
{

};

