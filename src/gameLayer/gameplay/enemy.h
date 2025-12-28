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

	bool wanderWhenIdle = false;          // option requested
	bool chasing = false;
	float timeSinceSeen = 0.0f;
	glm::ivec2 lastGoalTile = {-9999,-9999};
	std::vector<glm::ivec2> path;
	glm::vec2 moveDir = glm::vec2(1,0);


	glm::vec2 wanderTargetWorld = glm::vec2(0.0f);

	// Tuning
	float speed = 2.2f;
	float chaseAcquireRange = 9.0f;      // start chasing if within this distance
	float chaseLoseRange    = 11.0f;     // keep chasing until beyond this distance
	float forgetAfterSec    = 2.5f;      // forget if far for this long
	float repathEverySec    = 0.25f;
	float turnRate          = 14.0f;     // higher = snappier steering

	static inline glm::ivec2 worldToTile(glm::vec2 p) { return glm::ivec2((int)std::floor(p.x), (int)std::floor(p.y)); }
	static inline glm::vec2  tileCenter(glm::ivec2 t) { return glm::vec2(t) + glm::vec2(0.5f, 0.5f); }

	static inline bool isCornerCutBlocked(Map &map, glm::ivec2 from, glm::ivec2 to)
	{
		glm::ivec2 d = to - from;
		if (std::abs(d.x) == 1 && std::abs(d.y) == 1)
		{
			// If moving diagonally, disallow squeezing through a corner:
			// both adjacent orthogonal tiles must be free.
			if (IsBlockedTile(map, from.x + d.x, from.y) || IsBlockedTile(map, from.x, from.y + d.y))
				return true;
		}
		return false;
	}

	// Supercover line walk on grid (good for "direct chase" feel). Returns false if any tile along the line is blocked.
	// Also checks the "no corner cutting" rule for diagonal steps.
	static bool hasClearGridLine8(Map &map, glm::ivec2 start, glm::ivec2 goal)
	{
		int x0 = start.x, y0 = start.y;
		int x1 = goal.x, y1 = goal.y;

		int dx = std::abs(x1 - x0);
		int dy = std::abs(y1 - y0);
		int sx = (x0 < x1) ? 1 : -1;
		int sy = (y0 < y1) ? 1 : -1;

		int err = dx - dy;

		glm::ivec2 prev(x0, y0);

		// Check start (optional)
		if (IsBlockedTile(map, prev.x, prev.y)) return false;

		while (!(x0 == x1 && y0 == y1))
		{
			int e2 = err * 2;

			int nx = x0;
			int ny = y0;

			if (e2 > -dy) { err -= dy; nx += sx; }
			if (e2 < dx) { err += dx; ny += sy; }

			glm::ivec2 curr(nx, ny);

			if (IsBlockedTile(map, curr.x, curr.y)) return false;
			if (isCornerCutBlocked(map, prev, curr)) return false;

			prev = curr;
			x0 = nx; y0 = ny;
		}
		return true;
	}

	static inline glm::vec2 safeNormalize(glm::vec2 v)
	{
		float len2 = glm::dot(v, v);
		if (len2 <= 1e-8f) return glm::vec2(0, 0);
		return v * (1.0f / std::sqrt(len2));
	}

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player) override
	{
		animator.update(deltaTime, 0.12, 6);

		const glm::vec2 enemyPos = physics.getPos();   // center
		const glm::vec2 playerPos = player.physics.getPos();

		const float dist = glm::length(playerPos - enemyPos);

		// --- Chase state machine (acquire/lose + forget timer) ---
		if (!chasing)
		{
			if (dist <= chaseAcquireRange)
			{
				chasing = true;
				timeSinceSeen = 0.0f;
				repathTimer = 0.0f;
				path.clear();
				pathIndex = 0;
				lastGoalTile = {-9999,-9999};
			}
		}
		else
		{
			// keep chasing while reasonably close
			if (dist <= chaseLoseRange)
			{
				timeSinceSeen = 0.0f;
			}
			else
			{
				timeSinceSeen += deltaTime;
				if (timeSinceSeen >= forgetAfterSec)
				{
					chasing = false;
					path.clear();
					pathIndex = 0;
					lastGoalTile = {-9999,-9999};
				}
			}
		}

		glm::vec2 desiredMove(0.0f);

		// --- Movement when chasing ---
		if (chasing)
		{
			glm::ivec2 eTile = worldToTile(enemyPos);
			glm::ivec2 pTile = worldToTile(playerPos);

			// 1) "Go directly" if the grid line is clear (this removes the rigid A* “stair step” feel).
			bool directOk = hasClearGridLine8(map, eTile, pTile);

			glm::vec2 targetPos = playerPos;

			if (!directOk)
			{
				// 2) Otherwise fall back to your old A* (unchanged) and follow path.
				repathTimer -= deltaTime;

				// Repath if timer elapsed OR goal tile changed OR path finished.
				if (repathTimer <= 0.0f || pTile != lastGoalTile || pathIndex >= (int)path.size())
				{
					repathTimer = repathEverySec;
					lastGoalTile = pTile;

					path = findPathAStar8(map, eTile, pTile);
					pathIndex = 0;
				}

				// If we have a path, target the next step center.
				if (!path.empty() && pathIndex < (int)path.size())
				{
					// Skip nodes we already reached (prevents tiny left-then-diagonal jitters near tile borders)
					while (pathIndex < (int)path.size())
					{
						glm::vec2 c = tileCenter(path[pathIndex]);
						if (glm::length(c - enemyPos) > 0.15f) break;
						++pathIndex;
					}

					if (pathIndex < (int)path.size())
					{
						// Also skip a diagonal node if it would corner-cut in practice.
						// (If blocked, just wait for repath to resolve.)
						if (pathIndex > 0)
						{
							glm::ivec2 from = (pathIndex == 0) ? eTile : path[pathIndex - 1];
							glm::ivec2 to = path[pathIndex];
							if (!isCornerCutBlocked(map, from, to))
								targetPos = tileCenter(to);
						}
						else
						{
							targetPos = tileCenter(path[pathIndex]);
						}
					}
				}
			}

			// Steering (smooth turns so it feels less robotic even when path corners)
			glm::vec2 wantDir = safeNormalize(targetPos - enemyPos);
			float blend = 1.0f - std::exp(-turnRate * deltaTime); // framerate-independent
			glm::vec2 blended = safeNormalize(glm::mix(moveDir, wantDir, blend));
			if (glm::dot(blended, blended) > 0.0f) moveDir = blended;

			desiredMove = moveDir * speed;
		}
		else
		{
			// --- Not chasing: wander OR stay put (requested bool option) ---
			if (wanderWhenIdle)
			{
				wanderTimer -= deltaTime;
				if (wanderTimer <= 0.0f)
				{
					wanderTimer = getRandomFloat(rng, 0.6f, 1.6f);

					// pick one of 8 directions
					static const glm::vec2 dirs[8] = {
						{ 1, 0}, {-1, 0}, {0, 1}, {0,-1},
						{ 1, 1}, { 1,-1}, {-1, 1}, {-1,-1}
					};
					moveDir = safeNormalize(dirs[getRandomInt(rng, 0, 7)]);
				}

				// simple “don’t walk into walls” check using next tile
				glm::vec2 probe = enemyPos + moveDir * 0.35f;
				glm::ivec2 t = worldToTile(probe);
				if (!IsBlockedTile(map, t.x, t.y))
					desiredMove = moveDir * (speed * 0.35f); // slower wander
				else
					desiredMove = glm::vec2(0.0f);
			}
			else
			{
				desiredMove = glm::vec2(0.0f);
			}
		}

		// Apply desired movement (you said “just add to the position to move * deltaTime * speed”)
		if (glm::dot(desiredMove, desiredMove) > 0.0f)
		{
			physics.getPos() += desiredMove * deltaTime;
		}

		animator.setAnimationBasedOnMovement(desiredMove);

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