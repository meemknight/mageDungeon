#pragma once




#include "Physics.h"
#include "elements.h"
#include <map>
#include <memory>
#include "particleSystem.h"
#include <random>
#include <particles/particleCreator.h>
#include <gameplay/characterAnimator.h>

#include <gameplay/player.h>
#include <gameplay/aStar.h>

struct Enemy
{

	PhysicalEntity physics{glm::vec2{12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE}};
	CharacterAnimator animator{glm::vec2(48.f * PIXEL_SIZE,48.f * PIXEL_SIZE)};

	int element = 0;

	Enemy()
	{
	}

	void basicPhysicsAndCollisionsCheck(float deltaTime, Map &map)
	{
		physics.updateForces(deltaTime);
		physics.resolveConstrains(map);
		physics.updateMove();

	}


	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player) = 0;

	virtual void render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer) = 0;


};

struct BasicMeleEnemy : public Enemy
{

	TileSet tileSet;

	enum class AIState { Wander, Chase, Investigate };

	AIState aiState = AIState::Wander;

	std::vector<glm::ivec2> pathTiles;
	int pathIndex = 0;

	float repathTimer = 0.0f;
	float forgetTimer = 0.0f;
	float wanderTimer = 0.0f;

	glm::vec2 wanderTargetWorld = glm::vec2(0.0f);

	// Tuning
	float moveSpeed = 2.2f;

	float aggroRange = 9.0f;      // start chasing if within this
	float attackRange = 0.2f;      // “close enough” (no attack yet; just stop)
	float loseRange = 14.0f;      // if beyond this, start forgetting
	float forgetTime = 2.5f;       // seconds beyond loseRange before giving up

	float repathInterval = 0.25f;  // how often to recompute path during chase
	float arriveEps = 0.08f;       // waypoint arrival threshold


	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player) override
	{
		animator.update(deltaTime, 0.12, 6);

		const glm::vec2 myPos = physics.getPos();
		const glm::vec2 plPos = player.physics.getPos();

		const float distToPlayer = glm::length(plPos - myPos);

		// --- State transitions ---
		if (aiState != AIState::Chase)
		{
			// acquire aggro
			if (distToPlayer <= aggroRange)
			{
				aiState = AIState::Chase;
				forgetTimer = 0.0f;
				repathTimer = 0.0f;
				pathTiles.clear();
				pathIndex = 0;
			}
		}
		else
		{
			// already chasing: forget logic
			if (distToPlayer > loseRange)
			{
				forgetTimer += deltaTime;
				if (forgetTimer >= forgetTime)
				{
					aiState = AIState::Wander;
					pathTiles.clear();
					pathIndex = 0;
					wanderTimer = 0.0f;
				}
			}
			else
			{
				forgetTimer = 0.0f;
			}
		}

		// --- Behaviour ---
		glm::vec2 desiredMove(0.0f);

		auto worldToTile = [](const glm::vec2 &p) -> glm::ivec2
		{
			return glm::ivec2(int(std::floor(p.x)), int(std::floor(p.y)));
		};
		auto tileToWorldCenter = [](const glm::ivec2 &t) -> glm::vec2
		{
			return glm::vec2(float(t.x) + 0.5f, float(t.y) + 0.5f);
		};

		if (aiState == AIState::Chase)
		{
			// If close enough: stop (this is where you'd attack later)
			if (distToPlayer <= attackRange)
			{
				desiredMove = glm::vec2(0.0f);
				pathTiles.clear();
				pathIndex = 0;
			}
			else
			{
				// Repath periodically (or if we have no path)
				repathTimer -= deltaTime;
				if (repathTimer <= 0.0f || pathTiles.empty() || pathIndex >= (int)pathTiles.size())
				{
					repathTimer = repathInterval;

					const glm::ivec2 start = worldToTile(myPos);
					const glm::ivec2 goal = worldToTile(plPos);

					pathTiles = findPathAStar8(map, start, goal);
					pathIndex = 0;
				}

				// Follow path
				if (!pathTiles.empty() && pathIndex < (int)pathTiles.size())
				{
					glm::vec2 waypoint = tileToWorldCenter(pathTiles[pathIndex]);
					glm::vec2 toWp = waypoint - myPos;
					float d = glm::length(toWp);

					if (d <= arriveEps)
					{
						pathIndex++;
						if (pathIndex < (int)pathTiles.size())
						{
							waypoint = tileToWorldCenter(pathTiles[pathIndex]);
							toWp = waypoint - myPos;
							d = glm::length(toWp);
						}
					}

					if (d > 1e-5f)
						desiredMove = toWp / d; // normalized
				}
				else
				{
					// Fallback: no path -> simple direct move (still obeys your collision step)
					glm::vec2 dir = plPos - myPos;
					float d = glm::length(dir);
					if (d > 1e-5f) desiredMove = dir / d;
				}
			}
		}
		else // Wander / random behaviour
		{
			wanderTimer -= deltaTime;

			// Pick a new wander target sometimes (or if reached)
			const float reached = glm::length(wanderTargetWorld - myPos);
			if (wanderTimer <= 0.0f || reached < 0.3f)
			{
				wanderTimer = getRandomFloat(rng, 0.8f, 2.2f);

				// Choose a random nearby tile that isn't collidable
				const glm::ivec2 base = worldToTile(myPos);
				const int r = getRandomInt(rng, 2, 6);

				for (int tries = 0; tries < 12; ++tries)
				{
					const int ox = getRandomInt(rng, -r, r);
					const int oy = getRandomInt(rng, -r, r);
					glm::ivec2 t = base + glm::ivec2(ox, oy);

					if (t.x < 0 || t.y < 0 || t.x >= map.size.x || t.y >= map.size.y) continue;
					if (map.isCollidableAtPosSafe(t.x, t.y)) continue;

					wanderTargetWorld = tileToWorldCenter(t);
					break;
				}
			}

			glm::vec2 dir = wanderTargetWorld - myPos;
			float d = glm::length(dir);
			if (d > 1e-5f)
			{
				desiredMove = dir / d;

				// Add a little “messiness” sometimes
				if (getRandomChance(rng, 0.06f))
				{
					desiredMove += glm::vec2(getRandomFloat(rng, -0.6f, 0.6f),
						getRandomFloat(rng, -0.6f, 0.6f));
					float m = glm::length(desiredMove);
					if (m > 1e-5f) desiredMove /= m;
				}
			}
		}

		// --- Apply movement (simple) ---
		// You said: "just add to the position to move * deltaTime * speed"
		// If your collision helper expects velocity instead, swap this to set velocity.
		if (glm::length(desiredMove) > 1e-5f)
		{
			glm::vec2 newPos = myPos + desiredMove * (deltaTime * moveSpeed);
			physics.getPos() = newPos;
		}

		basicPhysicsAndCollisionsCheck(deltaTime, map);
		return true;
	}


	void render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer) override
	{
		glm::vec4 aabb = physics.getAABB();

		auto renderPos = aabb;
		renderPos.z = animator.textureSize.x;
		renderPos.w = animator.textureSize.y;

		renderPos.y -= (renderPos.w - physics.transform.size.y);
		renderPos.x -= (renderPos.z - physics.transform.size.x) / 2;

		renderPos.y += PIXEL_SIZE * 10;

		renderer.renderRectangle(renderPos, tileSet.texture,
			Colors_White, {}, {}, tileSet.atlas.get(animator.positionX, animator.positionY,
			animator.flipX));

		renderer.renderRectangleOutline(aabb, Colors_Blue, 0.02);


	}



};

inline BasicMeleEnemy getSkeletonEnemy(glm::vec2 pos)
{

	BasicMeleEnemy ret;
	ret.physics.teleport(pos);

	ret.tileSet = getAssetManager().skeleton;

	return ret;
}