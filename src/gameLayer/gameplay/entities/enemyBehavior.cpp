#include "enemyBehavior.h"
#include "gameplay/summons.h"
#include "gameplay/projectiles/projectiles.h"
#include <cmath>

// Looser LOS check to allow shots past tight wall corners.
static bool HasLooseLineOfSightTiles(Map &map, glm::ivec2 from, glm::ivec2 to)
{
	if (HasLineOfSightTiles(map, from, to))
	{
		return true;
	}

	static const glm::ivec2 offsets[8] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1},
		{1, 1}, {1, -1}, {-1, 1}, {-1, -1}
	};

	for (const auto &off : offsets)
	{
		glm::ivec2 f = from + off;
		if (!IsBlockedTileLineOfSight(map, f.x, f.y) && HasLineOfSightTiles(map, f, to))
		{
			return true;
		}
	}

	for (const auto &off : offsets)
	{
		glm::ivec2 t = to + off;
		if (!IsBlockedTileLineOfSight(map, t.x, t.y) && HasLineOfSightTiles(map, from, t))
		{
			return true;
		}
	}

	return false;
}

glm::vec2 EnemyBehavior::update(float deltaTime, Map &map, std::ranlux24_base &rng,
	glm::vec2 enemyPos, glm::vec2 playerPos, SummonHolder &summons)
{
	// Reset output flags
	wantsToMelee = false;
	wantsToShoot = false;
	firingBurstShot = false;
	useSpecialShot = false;

	// Update shoot cooldown
	if (shootTimer > 0.0f)
	{
		shootTimer -= deltaTime;
	}

	if (hoverMeleeCooldownTimer > 0.0f)
	{
		hoverMeleeCooldownTimer -= deltaTime;
	}

	if (dashCooldownTimer > 0.0f)
	{
		dashCooldownTimer -= deltaTime;
		if (dashCooldownTimer < 0.0f) { dashCooldownTimer = 0.0f; }
	}

	if (dashHitCooldownTimer > 0.0f)
	{
		dashHitCooldownTimer -= deltaTime;
		if (dashHitCooldownTimer < 0.0f) { dashHitCooldownTimer = 0.0f; }
	}

	if (targetSwitchTimer > 0.0f)
	{
		targetSwitchTimer -= deltaTime;
		if (targetSwitchTimer < 0.0f) { targetSwitchTimer = 0.0f; }
	}

	if (burstRemaining > 0)
	{
		burstTimer -= deltaTime;
		if (burstTimer <= 0.0f)
		{
			wantsToShoot = true;
			firingBurstShot = true;
		}
	}

	// Find best target (player or nearby summon)
	glm::vec2 targetPos = playerPos;
	glm::ivec2 targetTile = WorldToTile(playerPos);
	const float playerDist2 = glm::length2(playerPos - enemyPos);

	SummonEntity *bestSummon = nullptr;
	glm::vec2 bestSummonPos = {};
	float bestSummonDist2 = 999999.0f;
	for (auto &summon : summons.summons)
	{
		if (!summon->canBeTargeted()) { continue; }
		glm::vec2 summonPos = summon->physics.getPos();
		if (!seeThroughWalls &&
			!HasLooseLineOfSightTiles(map, WorldToTile(enemyPos), WorldToTile(summonPos)))
		{
			continue;
		}
		float distToSummon2 = glm::length2(summonPos - enemyPos);
		if (distToSummon2 >= bestSummonDist2) { continue; }
		bestSummonDist2 = distToSummon2;
		bestSummonPos = summonPos;
		bestSummon = summon.get();
	}

	SummonEntity *currentSummon = nullptr;
	glm::vec2 currentSummonPos = {};
	float currentSummonDist2 = 0.0f;
	if (targetIsSummon && targetSummon)
	{
		for (auto &summon : summons.summons)
		{
			if (summon.get() != targetSummon) { continue; }
			if (!summon->canBeTargeted()) { break; }
			currentSummon = targetSummon;
			currentSummonPos = summon->physics.getPos();
			currentSummonDist2 = glm::length2(currentSummonPos - enemyPos);
			break;
		}
	}

	if (!currentSummon)
	{
		targetIsSummon = false;
		targetSummon = nullptr;
	}

	bool desiredIsSummon = targetIsSummon;
	SummonEntity *desiredSummon = currentSummon;
	glm::vec2 desiredSummonPos = currentSummonPos;
	float desiredSummonDist2 = currentSummonDist2;

	const float closeRange = meleeRange + closeTargetExtraRange;
	const float closeRange2 = closeRange * closeRange;
	const float summonTargetRange = std::max(summonAggroRange, chaseAcquireRange);
	const float summonTargetRange2 = summonTargetRange * summonTargetRange;
	const bool playerClose = playerDist2 <= closeRange2;
	const bool summonClose = bestSummon && bestSummonDist2 <= closeRange2;

	if (targetSwitchTimer <= 0.0f)
	{
		bool switched = false;
		if (summonClose || playerClose)
		{
			if (summonClose && playerClose)
			{
				if (bestSummonDist2 + targetSwitchMargin < playerDist2)
				{
					desiredIsSummon = true;
					desiredSummon = bestSummon;
					desiredSummonPos = bestSummonPos;
					desiredSummonDist2 = bestSummonDist2;
					switched = !targetIsSummon || targetSummon != bestSummon;
				}
				else if (playerDist2 + targetSwitchMargin < bestSummonDist2)
				{
					desiredIsSummon = false;
					desiredSummon = nullptr;
					switched = targetIsSummon;
				}
			}
			else if (summonClose)
			{
				desiredIsSummon = true;
				desiredSummon = bestSummon;
				desiredSummonPos = bestSummonPos;
				desiredSummonDist2 = bestSummonDist2;
				switched = !targetIsSummon || targetSummon != bestSummon;
			}
			else
			{
				desiredIsSummon = false;
				desiredSummon = nullptr;
				switched = targetIsSummon;
			}
		}
		else
		{
			if (bestSummon && bestSummonDist2 <= summonTargetRange2 &&
				bestSummonDist2 + targetSwitchMargin < playerDist2)
			{
				desiredIsSummon = true;
				desiredSummon = bestSummon;
				desiredSummonPos = bestSummonPos;
				desiredSummonDist2 = bestSummonDist2;
				switched = !targetIsSummon || targetSummon != bestSummon;
			}
			else if (targetIsSummon && currentSummonDist2 > summonTargetRange2)
			{
				desiredIsSummon = false;
				desiredSummon = nullptr;
				switched = true;
			}
		}

		if (switched)
		{
			targetSwitchTimer = targetSwitchCooldown;
			targetIsSummon = desiredIsSummon;
			targetSummon = desiredSummon;
		}
	}

	if (targetIsSummon && desiredSummon)
	{
		targetPos = desiredSummonPos;
		targetTile = WorldToTile(targetPos);
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
		: HasLooseLineOfSightTiles(map, WorldToTile(enemyPos), targetTile);

	const bool withinAggro = (dist2 <= chaseAcquireRange * chaseAcquireRange);
	const bool canSeeTarget = seeThroughWalls ? withinAggro : (hasLOS && withinAggro);

	if (canSeeTarget)
	{
		chasing = true;
		noLOSTimer = 0.0f;
		lastSeenTargetPos = targetPos;
		lastSeenTargetTile = targetTile;
		hasLastSeen = true;
		patrolActive = false;
		patrolRequested = false;
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
				if (patrolEnabled)
				{
					patrolActive = true;
					patrolRequested = false;
					patrolTimer = getRandomFloat(rng, patrolAfterLoseMin, patrolAfterLoseMax);
					patrolDirTimer = 0.0f;
				}
			}
		}
	}

	if (patrolRequested && patrolEnabled && !chasing)
	{
		patrolRequested = false;
		patrolActive = true;
		patrolTimer = getRandomFloat(rng, patrolHitDurationMin, patrolHitDurationMax);
		patrolDirTimer = 0.0f;
	}

	// Trigger a dash strike when close and in LOS
	if (!dashActive && !hoverMeleeActive && dashEnabled && chasing && canSeeTarget &&
		distanceToTarget <= dashRange && dashCooldownTimer <= 0.0f)
	{
		float chance = dashChance * deltaTime;
		if (chance > 1.0f) { chance = 1.0f; }
		if (getRandomChance(rng, chance))
		{
			dashActive = true;
			dashTimer = 0.0f;
			dashDir = directionToTarget;
			float effectiveSpeed = speed * dashSpeedMultiplier;
			if (effectiveSpeed < 0.1f) { effectiveSpeed = 0.1f; }
			dashDuration = std::max(dashMinDuration, distanceToTarget / effectiveSpeed);
			hoverMeleeActive = false;
		}
	}

	// Trigger a hover melee swoop when close and in LOS
	if (!hoverMeleeActive && !dashActive && hoverMeleeEnabled && chasing && canSeeTarget &&
		distanceToTarget <= hoverMeleeRange && hoverMeleeCooldownTimer <= 0.0f)
	{
		float chance = hoverMeleeChance * deltaTime;
		if (chance > 1.0f) { chance = 1.0f; }
		if (getRandomChance(rng, chance))
		{
			hoverMeleeActive = true;
			hoverMeleeTimer = 0.0f;
			hoverMeleeArcSign = getRandomChance(rng, 0.5f) ? 1.0f : -1.0f;
			float effectiveSpeed = speed * hoverMeleeSpeedMultiplier;
			if (effectiveSpeed < 0.1f) { effectiveSpeed = 0.1f; }
			hoverMeleeActiveDuration = std::max(hoverMeleeMinDuration,
				distanceToTarget / effectiveSpeed);
		}
	}

	moveDir = glm::vec2(0.0f);

	if (chasing && !hoverMeleeActive && !dashActive)
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
	else if (patrolActive)
	{
		// Patrol: random search movement after damage or losing LOS.
		patrolTimer -= deltaTime;
		if (patrolTimer <= 0.0f)
		{
			patrolActive = false;
		}
		else
		{
			patrolDirTimer -= deltaTime;
			if (patrolDirTimer <= 0.0f)
			{
				patrolDirTimer = getRandomFloat(rng, patrolDirChangeMin, patrolDirChangeMax);

				static const glm::vec2 dirs[8] = {
					{ 1, 0},{-1, 0},{ 0, 1},{ 0,-1},
					{ 1, 1},{ 1,-1},{-1, 1},{-1,-1}
				};
				patrolDir = glm::normalize(dirs[getRandomInt(rng, 0, 7)]);
			}

			if (getRandomChance(rng, patrolStopChance))
				moveDir = glm::vec2(0.0f);
			else
				moveDir = patrolDir;
		}
	}
	else if (!chasing)
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

	// Orbit movement when close to the target (used for keep-distance strafing)
	bool orbiting = false;
	const bool allowOrbit = orbitEnabled || (stopChaseRange > 0.0f);
	const float orbitDistance = orbitEnabled ? orbitRange : stopChaseRange;
	if (!hoverMeleeActive && !dashActive && allowOrbit && chasing && canSeeTarget &&
		distanceToTarget <= orbitDistance)
	{
		orbiting = true;
		if (!orbitActive)
		{
			orbitActive = true;
			orbitSign = getRandomChance(rng, 0.5f) ? 1.0f : -1.0f;
			orbitDirectionTimer = getRandomFloat(rng, orbitDirectionChangeMin, orbitDirectionChangeMax);
			orbitRadialTimer = getRandomFloat(rng, orbitRadialChangeMin, orbitRadialChangeMax);
			orbitRadialOffset = 0.0f;
		}
		else
		{
			orbitDirectionTimer -= deltaTime;
			orbitRadialTimer -= deltaTime;
			if (orbitDirectionTimer <= 0.0f)
			{
				orbitDirectionTimer = getRandomFloat(rng, orbitDirectionChangeMin, orbitDirectionChangeMax);
				orbitSign = getRandomChance(rng, 0.5f) ? 1.0f : -1.0f;
			}
			if (orbitRadialTimer <= 0.0f)
			{
				orbitRadialTimer = getRandomFloat(rng, orbitRadialChangeMin, orbitRadialChangeMax);
				if (getRandomChance(rng, orbitRadialZeroChance))
				{
					orbitRadialOffset = 0.0f;
				}
				else
				{
					orbitRadialOffset = getRandomFloat(rng, -orbitRadialStrength, orbitRadialStrength);
				}
			}
		}

		glm::vec2 tangential = glm::vec2(-directionToTarget.y, directionToTarget.x) * orbitSign;
		glm::vec2 orbitDir = tangential + directionToTarget * orbitRadialOffset;
		if (glm::length2(orbitDir) > 0.0001f)
		{
			moveDir = glm::normalize(orbitDir);
		}
	}
	else if (orbitActive)
	{
		orbitActive = false;
		orbitRadialOffset = 0.0f;
	}

	// Stop near target for ranged enemies
	if (!hoverMeleeActive && !dashActive && !orbiting && stopChaseRange > 0.0f && chasing && canSeeTarget &&
		distanceToTarget <= stopChaseRange)
	{
		moveDir = glm::vec2(0.0f);
	}

	// If we're close enough to nearly melee, push in for contact
	if (chasing && canSeeTarget && !hoverMeleeActive && !dashActive &&
		distanceToTarget <= (meleeRange + 1.0f))
	{
		orbitActive = false;
		orbitRadialOffset = 0.0f;
		moveDir = directionToTarget;
	}

	// Hover melee swoop movement with a curved trajectory
	if (hoverMeleeActive)
	{
		hoverMeleeTimer += deltaTime;
		float duration = std::max(0.0001f, hoverMeleeActiveDuration);
		float t = hoverMeleeTimer / duration;
		if (t >= 1.0f)
		{
			hoverMeleeActive = false;
			hoverMeleeCooldownTimer = hoverMeleeCooldown;
		}
		else
		{
			float arc = std::sin(t * 3.14159265f) * hoverMeleeArcStrength;
			glm::vec2 dir = directionToTarget;
			glm::vec2 perp = glm::vec2(-dir.y, dir.x) * hoverMeleeArcSign;
			glm::vec2 curvedDir = dir + perp * arc;
			if (glm::length2(curvedDir) > 0.0001f)
			{
				moveDir = glm::normalize(curvedDir);
			}
		}
	}

	// Dash strike movement
	if (dashActive)
	{
		dashTimer += deltaTime;
		if (dashTimer >= dashDuration)
		{
			dashActive = false;
			dashCooldownTimer = dashCooldown;
		}
		else
		{
			orbitActive = false;
			orbitRadialOffset = 0.0f;
			moveDir = dashDir;
		}
	}

	// Determine if we should melee or shoot
	if (!firingBurstShot && chasing && canSeeTarget && !hoverMeleeActive && !dashActive)
	{
		if (distanceToTarget <= meleeRange)
		{
			// Close range - prefer melee
			wantsToMelee = true;
		}
		else if (burstRemaining == 0 && distanceToTarget <= shootRange && shootTimer <= 0.0f)
		{
			const bool hasNormalShot = shootPatterns != ShootPattern_None;
			const bool hasSpecialShot = specialShootPatterns != ShootPattern_None;
			if (hasNormalShot || hasSpecialShot)
			{
				bool pickSpecial = false;
				if (hasSpecialShot)
				{
					float chance = specialShootChance;
					if (!hasNormalShot)
					{
						chance *= deltaTime;
					}
					if (chance < 0.0f) { chance = 0.0f; }
					if (chance > 1.0f) { chance = 1.0f; }
					if (getRandomChance(rng, chance))
					{
						pickSpecial = true;
					}
				}

				if (pickSpecial || hasNormalShot)
				{
					wantsToShoot = true;
					useSpecialShot = pickSpecial && hasSpecialShot;
					if (!hasNormalShot && hasSpecialShot)
					{
						useSpecialShot = true;
					}
					shootTimer = shootCooldown;
				}
			}
		}
	}

	if (hoverMeleeActive || dashActive)
	{
		wantsToShoot = false;
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
	auto spawnOrb = [&](glm::vec2 dir, float dmg)
	{
		EnemyOrbProjectile orb;
		orb.targetPlayer = &player;
		orb.targetSummons = &summons;
		orb.speed = projectileSpeed;
		orb.setDamage(dmg);
		orb.setDirection(dir);
		projectiles.addProjectile(orb, enemyPos);
	};

	glm::vec2 shootDir = directionToTarget;
	if (firingBurstShot && burstRemaining > 0)
	{
		shootDir = burstDir;
	}

	unsigned char patterns = shootPatterns;
	if (useSpecialShot && specialShootPatterns != ShootPattern_None)
	{
		patterns = specialShootPatterns;
	}

	auto spawnCross = [&](glm::vec2 dir)
	{
		EnemyOrbProjectile center;
		center.targetPlayer = &player;
		center.targetSummons = &summons;
		center.speed = projectileSpeed;
		center.setDamage(crossShotDamage);
		center.setDirection(dir);
		projectiles.addProjectile(center, enemyPos);

		const float baseAngle = std::atan2(dir.y, dir.x);
		const float angleStep = 1.5707963f;
		for (int i = 0; i < 4; i++)
		{
			EnemyOrbProjectile orb;
			orb.targetPlayer = &player;
			orb.targetSummons = &summons;
			orb.speed = projectileSpeed;
			orb.setDamage(crossShotDamage);
			orb.setDirection(dir);
			orb.enableOrbit(crossShotOrbitRadius, crossShotOrbitSpeed, baseAngle + angleStep * (float)i);
			projectiles.addProjectile(orb, enemyPos);
		}
	};

	if (patterns & ShootPattern_RotatingCross)
	{
		spawnCross(shootDir);
		return;
	}

	if (patterns & ShootPattern_HeavyVolley)
	{
		spawnOrb(shootDir, projectileDamage);
		float a = spreadAngle;
		spawnOrb(rotateVec2(shootDir, a), sideProjectileDamage);
		spawnOrb(rotateVec2(shootDir, a * 2.0f), sideProjectileDamage);
		spawnOrb(rotateVec2(shootDir, -a), sideProjectileDamage);
		spawnOrb(rotateVec2(shootDir, -a * 2.0f), sideProjectileDamage);
		return;
	}

	if (patterns & ShootPattern_Spread5)
	{
		glm::vec2 dirs[5] = {
			rotateVec2(shootDir, -spreadAngle * 2.0f),
			rotateVec2(shootDir, -spreadAngle),
			shootDir,
			rotateVec2(shootDir, spreadAngle),
			rotateVec2(shootDir, spreadAngle * 2.0f)
		};
		for (int i = 0; i < 5; i++)
		{
			spawnOrb(dirs[i], projectileDamage);
		}
		return;
	}

	if (patterns & ShootPattern_TripleSpread)
	{
		glm::vec2 dirs[3] = {
			shootDir,
			rotateVec2(shootDir, spreadAngle),
			rotateVec2(shootDir, -spreadAngle)
		};
		for (int i = 0; i < 3; i++)
		{
			spawnOrb(dirs[i], projectileDamage);
		}
		return;
	}

	if (patterns & ShootPattern_BurstForward)
	{
		if (!firingBurstShot)
		{
			burstRemaining = std::max(0, burstCount - 1);
			burstDir = directionToTarget;
			burstDamage = projectileDamage;
		}
		else
		{
			burstRemaining = std::max(0, burstRemaining - 1);
		}

		spawnOrb(burstDir, burstDamage);
		if (burstRemaining > 0)
		{
			burstTimer = burstInterval;
		}
		return;
	}

	if (patterns & ShootPattern_Single)
	{
		spawnOrb(shootDir, projectileDamage);
		return;
	}
}
