#pragma once
#include "gameplay/Physics.h"
#include "gameplay/map.h"
#include "gameplay/player.h"
#include "gameplay/aStar.h"
#include <randomStuff.h>
#include <random>
#include <vector>

struct SummonHolder;
struct SummonEntity;
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
	ShootPattern_RotatingCross = 1 << 5, // 5 projectiles: cross with rotating outer bullets
	// Add more patterns here as needed
};

// Reusable enemy movement/AI behavior. Handles chasing, pathfinding, wandering, shooting.
// Use via composition in any enemy struct.
struct EnemyBehavior
{
	// Movement configuration
	float speed = 2.2f;
	float chaseAcquireRange = 13.0f;      // start chasing if within this distance
	float summonAggroRange = 5.0f;        // minimum range to prefer summons
	float forgetAfterNoLOS = 2.0f;        // seconds with no LOS before forgetting
	float repathInterval = 0.25f;         // how often to rebuild A* path
	bool seeThroughWalls = false;         // ignore LOS checks
	bool wanderWhenIdle = false;          // wander randomly when not chasing
	float stopChaseRange = 0.0f;          // stop moving when in LOS and within this distance
	bool patrolEnabled = true;            // patrol when damaged or after losing LOS
	float patrolHitDurationMin = 1.6f;    // min patrol time after damage
	float patrolHitDurationMax = 3.2f;    // max patrol time after damage
	float patrolAfterLoseMin = 2.0f;      // min patrol time after losing LOS
	float patrolAfterLoseMax = 3.0f;      // max patrol time after losing LOS
	float patrolDirChangeMin = 0.5f;      // min seconds before turning
	float patrolDirChangeMax = 1.2f;      // max seconds before turning
	float patrolStopChance = 0.08f;       // chance to pause while patrolling
	float closeTargetExtraRange = 1.0f;   // extra distance beyond melee to switch targets
	float targetSwitchCooldown = 1.0f;    // seconds between target switches
	float targetSwitchMargin = 0.2f;      // distance margin before switching targets

	// Shooting configuration
	unsigned char shootPatterns = ShootPattern_None;  // which patterns this enemy uses
	float meleeRange = 1.5f;              // prefer melee attack if closer than this
	float shootRange = 10.0f;             // can shoot if within this range
	float shootCooldown = 1.5f;           // time between shots
	float projectileSpeed = 4.6f;         // speed of fired projectiles
	float spreadAngle = 30.0f;            // degrees for spread patterns
	float projectileDamage = 1.0f;
	float sideProjectileDamage = 1.0f;    // used by heavy volley
	int burstCount = 3;                   // shots per burst
	float burstInterval = 0.36f;          // time between burst shots
	unsigned char specialShootPatterns = ShootPattern_None; // alternative patterns
	float specialShootChance = 0.0f;      // chance to use special pattern per shot
	float crossShotDamage = 1.0f;         // damage per rotating cross projectile
	float crossShotOrbitRadius = PIXEL_SIZE * 13.0f; // distance from center
	float crossShotOrbitSpeed = 3.6f;     // radians per second

	// Hover melee (flying swoop) configuration
	bool hoverMeleeEnabled = false;       // enable curved melee swoop
	float hoverMeleeRange = 5.0f;         // trigger range for swoop
	float hoverMeleeChance = 0.30f;       // chance per second while in range
	float hoverMeleeCooldown = 2.4f;      // seconds between swoops
	float hoverMeleeArcStrength = 0.7f;   // sideways curve strength
	float hoverMeleeMinDuration = 0.45f;  // minimum swoop time
	float hoverMeleeSpeedMultiplier = 1.35f; // movement speed boost during swoop

	// Dash attack configuration
	bool dashEnabled = false;             // enable dash strike
	float dashRange = 6.5f;               // trigger range for dash
	float dashChance = 0.30f;             // chance per second while in range
	float dashCooldown = 2.4f;            // seconds between dashes
	float dashMinDuration = 0.28f;        // minimum dash time
	float dashSpeedMultiplier = 3.0f;     // movement speed boost during dash
	float dashContactDamage = 1.0f;       // damage on dash contact
	float dashHitCooldown = 0.35f;        // summon hit cooldown during dash

	// Orbit movement (close-range circling) configuration
	bool orbitEnabled = false;            // circle target when in range
	float orbitRange = 5.0f;              // start orbiting within this distance
	float orbitDirectionChangeMin = 1.0f; // min seconds before direction change
	float orbitDirectionChangeMax = 2.0f; // max seconds before direction change
	float orbitRadialChangeMin = 0.35f;   // min seconds before radial tweak
	float orbitRadialChangeMax = 0.9f;    // max seconds before radial tweak
	float orbitRadialStrength = 0.45f;    // radial push/pull amount
	float orbitRadialZeroChance = 0.45f;  // chance to keep pure circle on tweak

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
	bool useSpecialShot = false;

	// Hover melee state
	bool hoverMeleeActive = false;
	float hoverMeleeTimer = 0.0f;
	float hoverMeleeActiveDuration = 0.0f;
	float hoverMeleeCooldownTimer = 0.0f;
	float hoverMeleeArcSign = 1.0f;

	// Dash state
	bool dashActive = false;
	float dashTimer = 0.0f;
	float dashDuration = 0.0f;
	float dashCooldownTimer = 0.0f;
	float dashHitCooldownTimer = 0.0f;
	glm::vec2 dashDir = glm::vec2(1.0f, 0.0f);

	// Orbit state
	bool orbitActive = false;
	float orbitDirectionTimer = 0.0f;
	float orbitRadialTimer = 0.0f;
	float orbitRadialOffset = 0.0f;
	float orbitSign = 1.0f;

	// Patrol state
	bool patrolActive = false;
	bool patrolRequested = false;
	float patrolTimer = 0.0f;
	float patrolDirTimer = 0.0f;
	glm::vec2 patrolDir = glm::vec2(0.0f);
	float targetSwitchTimer = 0.0f;
	bool targetIsSummon = false;
	SummonEntity *targetSummon = nullptr;

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

	// Called when the enemy takes damage and should start patrolling.
	void requestPatrol() { patrolRequested = true; }

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
		useSpecialShot = false;
		hoverMeleeActive = false;
			hoverMeleeTimer = 0.0f;
			hoverMeleeActiveDuration = 0.0f;
			hoverMeleeCooldownTimer = 0.0f;
			hoverMeleeArcSign = 1.0f;
			dashActive = false;
			dashTimer = 0.0f;
			dashDuration = 0.0f;
			dashCooldownTimer = 0.0f;
			dashHitCooldownTimer = 0.0f;
			dashDir = glm::vec2(1.0f, 0.0f);
		orbitActive = false;
		orbitDirectionTimer = 0.0f;
		orbitRadialTimer = 0.0f;
		orbitRadialOffset = 0.0f;
		orbitSign = 1.0f;
		patrolActive = false;
		patrolRequested = false;
		patrolTimer = 0.0f;
		patrolDirTimer = 0.0f;
		patrolDir = glm::vec2(0.0f);
		targetSwitchTimer = 0.0f;
		targetIsSummon = false;
		targetSummon = nullptr;
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
