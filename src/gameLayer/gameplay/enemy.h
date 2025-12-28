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
	glm::vec2 idleDir = glm::vec2(0.0f);

	bool wanderWhenIdle = false;          // option requested
	bool chasing = false;
	float timeSinceSeen = 0.0f;
	glm::ivec2 lastGoalTile = {-9999,-9999};
	std::vector<glm::ivec2> path;
	glm::vec2 moveDir = glm::vec2(1,0);


	glm::vec2 wanderTargetWorld = glm::vec2(0.0f);

	// Tuning
	float speed = 2.2f;
	float chaseAcquireRange = 13.0f;      // start chasing if within this distance
	float chaseLoseRange    = 15.0f;     // keep chasing until beyond this distance
	float turnRate          = 14.0f;     // higher = snappier steering
	float noLOSTimer = 0.0f;
	float forgetAfterNoLOS = 2.0f;     // seconds with no LOS before forgetting
	float repathInterval = 0.25f;      // how often to rebuild A* when needed
	bool seeThroughWalls = false;

	glm::vec2 lastSeenPlayerPos = glm::vec2(0.0f);
	glm::ivec2 lastSeenPlayerTile = glm::ivec2(0);
	bool hasLastSeen = false;

	glm::ivec2 lastPathGoalTile = glm::ivec2(999999); // forces first rebuild


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

		const glm::vec2 enemyPos = physics.getPos();
		const glm::vec2 playerPos = player.physics.getPos();

		const glm::vec2 toPlayer = playerPos - enemyPos;
		const float dist2 = glm::length2(toPlayer);

		// LOS-based aggro/forget (with optional distance cap)
		const bool hasLOS = seeThroughWalls
			? true
			: HasLineOfSightTiles(map, WorldToTile(enemyPos), WorldToTile(playerPos));

		const bool withinAggro = (dist2 <= chaseAcquireRange * chaseAcquireRange);

		// Can "see" player either via LOS+range, or just range if seeThroughWalls
		const bool canSeePlayer = seeThroughWalls ? withinAggro : (hasLOS && withinAggro);

		if (canSeePlayer)
		{
			chasing = true;
			noLOSTimer = 0.0f;

			// Remember last seen position/tile
			lastSeenPlayerPos = playerPos;
			lastSeenPlayerTile = WorldToTile(playerPos);
			hasLastSeen = true;
		}
		else
		{
			if (chasing)
			{
				noLOSTimer += deltaTime;
				if (noLOSTimer >= forgetAfterNoLOS)
				{
					chasing = false;
					pathTiles.clear();
					pathIndex = 0;
					hasLastSeen = false;
				}
			}
		}

		glm::vec2 moveDir(0.0f);

		if (chasing)
		{
			// During grace period, chase last seen target if we can't currently see the player
			const bool useLastSeen = (!canSeePlayer && hasLastSeen);
			const glm::vec2 chaseTargetPos = useLastSeen ? lastSeenPlayerPos : playerPos;
			const glm::ivec2 chaseTargetTile = useLastSeen ? lastSeenPlayerTile : WorldToTile(playerPos);

			const glm::vec2 toTarget = chaseTargetPos - enemyPos;
			const float distTarget2 = glm::length2(toTarget);

			// 1) If direct chase is possible (or walls ignored), go straight for smoothness
			if (seeThroughWalls || CanChaseDirect(map, enemyPos, chaseTargetPos))
			{
				if (distTarget2 > 0.0001f)
					moveDir = glm::normalize(toTarget);

				// When going direct, drop the current path (prevents robotic stepping)
				pathTiles.clear();
				pathIndex = 0;
				repathTimer = 0.0f;
				lastPathGoalTile = glm::ivec2(999999); // force rebuild next time we need A*
			}
			else
			{
				// 2) Otherwise fall back to your old A* path
				repathTimer -= deltaTime;

				// Rebuild if timer elapsed OR path empty OR finished OR target tile changed
				if (repathTimer <= 0.0f ||
					pathTiles.empty() ||
					pathIndex >= (int)pathTiles.size() ||
					chaseTargetTile != lastPathGoalTile)
				{
					repathTimer = repathInterval;

					glm::ivec2 startT = WorldToTile(enemyPos);
					glm::ivec2 goalT = chaseTargetTile;

					lastPathGoalTile = goalT;

					// Call YOUR old A* here:
					// pathTiles = FindPathAStar8_Old(map, startT, goalT);
					// pathIndex = 0;

					// If A* fails, don't freeze: just keep trying to move roughly toward target
					// (still blocked by collisions later)
					if (pathTiles.empty())
					{
						if (distTarget2 > 0.0001f)
							moveDir = glm::normalize(toTarget);
					}
				}

				// Follow the path
				if (!pathTiles.empty() && pathIndex < (int)pathTiles.size())
				{
					// Skip current tile if included
					glm::ivec2 currT = WorldToTile(enemyPos);
					while (pathIndex < (int)pathTiles.size() && pathTiles[pathIndex] == currT)
						pathIndex++;

					if (pathIndex < (int)pathTiles.size())
					{
						const glm::vec2 nextCenter = glm::vec2(pathTiles[pathIndex]) + glm::vec2(0.5f);
						glm::vec2 toNext = nextCenter - enemyPos;

						// Advance node when close
						if (glm::length2(toNext) < 0.05f * 0.05f)
						{
							pathIndex++;
						}
						else
						{
							moveDir = glm::normalize(toNext);
						}
					}
				}
			}
		}
		else
		{
			// Idle behavior
			if (wanderWhenIdle)
			{
				wanderTimer -= deltaTime;
				if (wanderTimer <= 0.0f)
				{
					wanderTimer = getRandomFloat(rng, 0.6f, 1.4f);

					// random 8-dir
					static const glm::vec2 dirs[8] = {
						{ 1, 0},{-1, 0},{ 0, 1},{ 0,-1},
						{ 1, 1},{ 1,-1},{-1, 1},{-1,-1}
					};
					idleDir = glm::normalize(dirs[getRandomInt(rng, 0, 7)]);
				}

				// optional: small chance to stand still even in wander mode
				if (getRandomChance(rng, 0.10f))
					moveDir = glm::vec2(0.0f);
				else
					moveDir = idleDir;
			}
			else
			{
				// stay put
				moveDir = glm::vec2(0.0f);
			}
		}

		// Apply movement
		if (glm::dot(moveDir, moveDir) > 0.0f)
		{
			physics.getPos() += moveDir * speed * deltaTime;
		}

		animator.setAnimationBasedOnMovement(moveDir);

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