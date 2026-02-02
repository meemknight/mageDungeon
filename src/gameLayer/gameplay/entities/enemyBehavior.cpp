#include "enemyBehavior.h"
#include "gameplay/summons.h"
#include "gameplay/projectiles/projectiles.h"

glm::vec2 EnemyBehavior::update(float deltaTime, Map &map, std::ranlux24_base &rng,
	glm::vec2 enemyPos, glm::vec2 playerPos, SummonHolder &summons)
{
	// Reset output flags
	wantsToMelee = false;
	wantsToShoot = false;

	// Update shoot cooldown
	if (shootTimer > 0.0f)
	{
		shootTimer -= deltaTime;
	}

	// Find best target (player or nearby summon)
	glm::vec2 targetPos = playerPos;
	glm::ivec2 targetTile = WorldToTile(playerPos);

	float bestSummonDist2 = summonAggroRange * summonAggroRange;
	for (auto &summon : summons.summons)
	{
		if (!summon->canBeTargeted()) { continue; }
		glm::vec2 summonPos = summon->physics.getPos();
		float distToSummon2 = glm::length2(summonPos - enemyPos);
		if (distToSummon2 > bestSummonDist2) { continue; }
		if (!HasLineOfSightTiles(map, WorldToTile(enemyPos), WorldToTile(summonPos))) { continue; }
		bestSummonDist2 = distToSummon2;
		targetPos = summonPos;
		targetTile = WorldToTile(summonPos);
	}

	currentTargetPos = targetPos;

	const glm::vec2 toTarget = targetPos - enemyPos;
	const float dist2 = glm::length2(toTarget);
	distanceToTarget = std::sqrt(dist2);

	// Calculate direction to target
	if (distanceToTarget > 0.0001f)
	{
		directionToTarget = toTarget / distanceToTarget;
	}
	else
	{
		directionToTarget = glm::vec2(1.0f, 0.0f);
	}

	// LOS-based aggro/forget
	const bool hasLOS = seeThroughWalls
		? true
		: HasLineOfSightTiles(map, WorldToTile(enemyPos), targetTile);

	const bool withinAggro = (dist2 <= chaseAcquireRange * chaseAcquireRange);
	const bool canSeeTarget = seeThroughWalls ? withinAggro : (hasLOS && withinAggro);

	if (canSeeTarget)
	{
		chasing = true;
		noLOSTimer = 0.0f;
		lastSeenTargetPos = targetPos;
		lastSeenTargetTile = targetTile;
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

	moveDir = glm::vec2(0.0f);

	if (chasing)
	{
		// Chase last seen position if we can't currently see the target
		const bool useLastSeen = (!canSeeTarget && hasLastSeen);
		const glm::vec2 chaseTargetPos = useLastSeen ? lastSeenTargetPos : targetPos;
		const glm::ivec2 chaseTargetTile = useLastSeen ? lastSeenTargetTile : targetTile;

		const glm::vec2 toChaseTarget = chaseTargetPos - enemyPos;
		const float distTarget2 = glm::length2(toChaseTarget);

		// Direct chase if possible
		if (seeThroughWalls || CanChaseDirect(map, enemyPos, chaseTargetPos))
		{
			if (distTarget2 > 0.0001f)
				moveDir = glm::normalize(toChaseTarget);

			pathTiles.clear();
			pathIndex = 0;
			repathTimer = 0.0f;
			lastPathGoalTile = glm::ivec2(999999);
		}
		else
		{
			// A* pathfinding
			repathTimer -= deltaTime;

			if (repathTimer <= 0.0f ||
				pathTiles.empty() ||
				pathIndex >= (int)pathTiles.size() ||
				chaseTargetTile != lastPathGoalTile)
			{
				repathTimer = repathInterval;
				glm::ivec2 startT = WorldToTile(enemyPos);
				glm::ivec2 goalT = chaseTargetTile;
				lastPathGoalTile = goalT;

				pathTiles = findPathAStar8(map, startT, goalT);
				pathIndex = 0;

				if (pathTiles.empty() && distTarget2 > 0.0001f)
				{
					moveDir = glm::normalize(toChaseTarget);
				}
			}

			// Follow the path
			if (!pathTiles.empty() && pathIndex < (int)pathTiles.size())
			{
				glm::ivec2 currT = WorldToTile(enemyPos);
				while (pathIndex < (int)pathTiles.size() && pathTiles[pathIndex] == currT)
					pathIndex++;

				if (pathIndex < (int)pathTiles.size())
				{
					const glm::vec2 nextCenter = glm::vec2(pathTiles[pathIndex]) + glm::vec2(0.5f);
					glm::vec2 toNext = nextCenter - enemyPos;

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

				static const glm::vec2 dirs[8] = {
					{ 1, 0},{-1, 0},{ 0, 1},{ 0,-1},
					{ 1, 1},{ 1,-1},{-1, 1},{-1,-1}
				};
				idleDir = glm::normalize(dirs[getRandomInt(rng, 0, 7)]);
			}

			if (getRandomChance(rng, 0.10f))
				moveDir = glm::vec2(0.0f);
			else
				moveDir = idleDir;
		}
	}

	// Determine if we should melee or shoot
	if (chasing && canSeeTarget)
	{
		if (distanceToTarget <= meleeRange)
		{
			// Close range - prefer melee
			wantsToMelee = true;
		}
		else if (shootPatterns != ShootPattern_None &&
			distanceToTarget <= shootRange &&
			shootTimer <= 0.0f)
		{
			// In shooting range and cooldown ready
			wantsToShoot = true;
			shootTimer = shootCooldown;
		}
	}

	return moveDir;
}

// Rotate a 2D vector by angle in degrees
static glm::vec2 rotateVec2(glm::vec2 v, float angleDegrees)
{
	float rad = glm::radians(angleDegrees);
	float c = std::cos(rad);
	float s = std::sin(rad);
	return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

void EnemyBehavior::shoot(glm::vec2 enemyPos, ProjectileHolder &projectiles,
	Player &player, SummonHolder &summons, std::ranlux24_base &rng)
{
	if (shootPatterns & ShootPattern_TripleSpread)
	{
		// Shoot 3 projectiles: center, +spreadAngle, -spreadAngle
		glm::vec2 dirs[3] = {
			directionToTarget,
			rotateVec2(directionToTarget, spreadAngle),
			rotateVec2(directionToTarget, -spreadAngle)
		};

		for (int i = 0; i < 3; i++)
		{
			EnemyOrbProjectile orb;
			orb.targetPlayer = &player;
			orb.targetSummons = &summons;
			orb.speed = projectileSpeed;
			orb.damage = 1.0f;
			orb.setDirection(dirs[i]);
			projectiles.addProjectile(orb, enemyPos);
		}
	}
}
