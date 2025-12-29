#include "entity.h"


bool BasicMeleEnemy::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, Player &player)
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

void BasicMeleEnemy::render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer)
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

	physics.renderCollider(renderer);


}
