#pragma once
#include "gameplay/Physics.h"
#include "gameplay/map.h"
#include "gameplay/player.h"
#include "gameplay/aStar.h"
#include <randomStuff.h>
#include <random>
#include <vector>

struct SummonHolder;
struct Summon;
struct ProjectileHolder;

// Shooting pattern flags - can be combined
enum ShootPattern : unsigned char
{
	ShootPattern_None = 0,
	ShootPattern_TripleSpread = 1 << 0,  // 3 projectiles: center + ±30 degrees
	ShootPattern_Single = 1 << 1,        // 1 projectile forward
	ShootPattern_BurstForward = 1 << 2,  // 3 projectiles in sequence forward
	ShootPattern_Spread5 = 1 << 3,       // 5 projectiles spread
	ShootPattern_HeavyVolley = 1 << 4,   // 1 forward + 4 side shots
	// Add more patterns here as needed
};

// Reusable enemy movement/AI behavior. Handles chasing, pathfinding, wandering, shooting.
// Use via composition in any enemy struct.
struct EnemyBehavior
{
	// Movement configuration
	float speed = 2.2f;
	float chaseAcquireRange = 13.0f;      // start chasing if within this distance
	float summonAggroRange = 5.0f;        // switch to summon target if very close
	float forgetAfterNoLOS = 2.0f;        // seconds with no LOS before forgetting
	float repathInterval = 0.25f;         // how often to rebuild A* path
	bool seeThroughWalls = false;         // ignore LOS checks
	bool wanderWhenIdle = false;          // wander randomly when not chasing

	// Shooting configuration
	unsigned char shootPatterns = ShootPattern_None;  // which patterns this enemy uses
	float meleeRange = 1.5f;              // prefer melee attack if closer than this
	float shootRange = 10.0f;             // can shoot if within this range
	float shootCooldown = 1.5f;           // time between shots
	float projectileSpeed = 4.0f;         // speed of fired projectiles
	float spreadAngle = 30.0f;            // degrees for spread patterns
	float projectileDamage = 1.0f;
	float sideProjectileDamage = 1.0f;    // used by heavy volley
	int burstCount = 3;                   // shots per burst
	float burstInterval = 0.18f;          // time between burst shots

	// Movement state
	std::vector<glm::ivec2> pathTiles;
	int pathIndex = 0;
	float repathTimer = 0.0f;
	float wanderTimer = 0.0f;
	float noLOSTimer = 0.0f;
	glm::vec2 idleDir = glm::vec2(0.0f);
	bool chasing = false;
	glm::vec2 lastSeenTargetPos = glm::vec2(0.0f);
	glm::ivec2 lastSeenTargetTile = glm::ivec2(0);
	bool hasLastSeen = false;
	glm::ivec2 lastPathGoalTile = glm::ivec2(999999);

	// Shooting state
	float shootTimer = 0.0f;
	int burstRemaining = 0;
	float burstTimer = 0.0f;
	bool firingBurstShot = false;
	glm::vec2 burstDir = glm::vec2(1.0f, 0.0f);
	float burstDamage = 1.0f;

	// Output from last update
	glm::vec2 moveDir = glm::vec2(0.0f);
	glm::vec2 currentTargetPos = glm::vec2(0.0f);
	glm::vec2 directionToTarget = glm::vec2(0.0f);
	float distanceToTarget = 0.0f;
	bool wantsToMelee = false;            // true if in melee range and should attack
	bool wantsToShoot = false;            // true if should shoot this frame

	// Update the behavior and compute movement direction.
	// Returns the movement direction (not normalized if zero).
	glm::vec2 update(float deltaTime, Map &map, std::ranlux24_base &rng,
		glm::vec2 enemyPos, glm::vec2 playerPos, SummonHolder &summons);

	// Try to shoot at the target. Call this after update() if wantsToShoot is true.
	// Spawns projectiles into the holder.
	void shoot(glm::vec2 enemyPos, ProjectileHolder &projectiles,
		Player &player, SummonHolder &summons, std::ranlux24_base &rng);

	// Reset state when enemy spawns or needs to forget everything
	void reset()
	{
		pathTiles.clear();
		pathIndex = 0;
		repathTimer = 0.0f;
		wanderTimer = 0.0f;
		noLOSTimer = 0.0f;
		chasing = false;
		hasLastSeen = false;
		lastPathGoalTile = glm::ivec2(999999);
		moveDir = glm::vec2(0.0f);
		shootTimer = 0.0f;
		wantsToMelee = false;
		wantsToShoot = false;
		burstRemaining = 0;
		burstTimer = 0.0f;
		firingBurstShot = false;
		burstDir = glm::vec2(1.0f, 0.0f);
		burstDamage = 1.0f;
	}

	// Called when enemy hits a wall, forces repath
	void onWallHit()
	{
		repathTimer = 0.0f;
		lastPathGoalTile = glm::ivec2(999999);
		if (!pathTiles.empty() && pathIndex < (int)pathTiles.size())
		{
			pathIndex++;
		}
	}
};
