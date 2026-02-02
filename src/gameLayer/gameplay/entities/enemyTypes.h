#include "entity.h"
#include "entity.h"
#include "gameplay/assetsManager.h"

namespace EnemyTypes
{
	constexpr float enemyLifeScale = 2.0f;

	// Configures the base enemy visuals and behavior defaults.
	inline void setupEnemy(BasicMeleEnemy &enemy, TileSet &tileSet, int cellPixels)
	{
		enemy.tileSet = tileSet;
		int size = int(cellPixels * PIXEL_SIZE);
		enemy.animator.textureSize = {size, size};
		enemy.behavior.shootPatterns = ShootPattern_None;
	}

	inline void setupRanged(BasicMeleEnemy &enemy, unsigned char pattern,
		float life, float damage)
	{
		enemy.life.setLifeAndMaxLife(life * enemyLifeScale);
		enemy.behavior.shootPatterns = pattern;
		enemy.behavior.meleeRange = 1.5f;
		enemy.behavior.shootRange = 8.0f;
		enemy.behavior.shootCooldown = 2.0f;
		enemy.behavior.projectileSpeed = 4.0f;
		enemy.behavior.spreadAngle = 30.0f;
		enemy.behavior.projectileDamage = damage;
		enemy.behavior.sideProjectileDamage = 1.0f;
		enemy.behavior.burstCount = 3;
		enemy.behavior.burstInterval = 0.18f;
	}

	// Configures a melee-only enemy (life + no shooting).
	inline void setupMele(BasicMeleEnemy &enemy, float life)
	{
		enemy.life.setLifeAndMaxLife(life * enemyLifeScale);
		enemy.behavior.shootPatterns = ShootPattern_None;
	}

	struct SkeletonEnemy : public BasicMeleEnemy
	{
		SkeletonEnemy()
		{
			setupEnemy(*this, getAssetManager().skeleton, 48);
			setupMele(*this, 18.0f);
		}
	};

	struct TemplarOriginalEnemy : public BasicMeleEnemy
	{
		TemplarOriginalEnemy()
		{
			setupEnemy(*this, getAssetManager().templarOriginal, 48);
			setupMele(*this, 22.0f);
			behavior.specialShootPatterns = ShootPattern_RotatingCross;
			behavior.specialShootChance = 0.18f;
			renderOffsetY = PIXEL_SIZE * 4.0f;
		}
	};

	struct EarthTemplarEnemy : public BasicMeleEnemy
	{
		EarthTemplarEnemy()
		{
			setupEnemy(*this, getAssetManager().earthTemplar, 48);
			setupMele(*this, 22.0f);
			behavior.specialShootPatterns = ShootPattern_RotatingCross;
			behavior.specialShootChance = 0.18f;
			element = Elements::Earth;
			renderOffsetY = PIXEL_SIZE * 4.0f;
		}
	};

	struct FireTemplarEnemy : public BasicMeleEnemy
	{
		FireTemplarEnemy()
		{
			setupEnemy(*this, getAssetManager().fireTemplar, 48);
			setupMele(*this, 22.0f);
			behavior.specialShootPatterns = ShootPattern_RotatingCross;
			behavior.specialShootChance = 0.18f;
			element = Elements::Fire;
			renderOffsetY = PIXEL_SIZE * 4.0f;
		}
	};

	struct IceTemplarEnemy : public BasicMeleEnemy
	{
		IceTemplarEnemy()
		{
			setupEnemy(*this, getAssetManager().iceTemplar, 48);
			setupMele(*this, 22.0f);
			behavior.specialShootPatterns = ShootPattern_RotatingCross;
			behavior.specialShootChance = 0.18f;
			element = Elements::Ice;
			renderOffsetY = PIXEL_SIZE * 4.0f;
		}
	};

	struct WaterTemplarEnemy : public BasicMeleEnemy
	{
		WaterTemplarEnemy()
		{
			setupEnemy(*this, getAssetManager().waterTemplar, 48);
			setupMele(*this, 22.0f);
			behavior.specialShootPatterns = ShootPattern_RotatingCross;
			behavior.specialShootChance = 0.18f;
			element = Elements::Water;
			renderOffsetY = PIXEL_SIZE * 4.0f;
		}
	};

	struct GoblinArcherEnemy : public BasicMeleEnemy
	{
		GoblinArcherEnemy()
		{
			setupEnemy(*this, getAssetManager().goblinArcher, 48);
			setupRanged(*this, ShootPattern_TripleSpread, 9.0f, 1.0f);
			behavior.stopChaseRange = 5.0f;
		}
	};

	struct GoblinSpearmanEnemy : public BasicMeleEnemy
	{
		GoblinSpearmanEnemy()
		{
			setupEnemy(*this, getAssetManager().goblinSpearman, 48);
			setupRanged(*this, ShootPattern_BurstForward, 9.0f, 1.0f);
			behavior.orbitEnabled = true;
			behavior.orbitRange = 5.0f;
		}
	};

	struct GoblinHeavyEnemy : public BasicMeleEnemy
	{
		GoblinHeavyEnemy()
		{
			setupEnemy(*this, getAssetManager().goblinHeavy, 32);
			setupRanged(*this, ShootPattern_HeavyVolley, 16.0f, 2.0f);
			behavior.sideProjectileDamage = 1.0f;
			behavior.orbitEnabled = true;
			behavior.orbitRange = 5.0f;
		}
	};

	struct GoblinThiefEnemy : public BasicMeleEnemy
	{
		GoblinThiefEnemy()
		{
			setupEnemy(*this, getAssetManager().goblinThief, 32);
			setupRanged(*this, ShootPattern_Single, 7.0f, 1.0f);
			behavior.orbitEnabled = true;
			behavior.orbitRange = 5.0f;
		}
	};

	struct OrcArcherEnemy : public BasicMeleEnemy
	{
		OrcArcherEnemy()
		{
			setupEnemy(*this, getAssetManager().orcArcher, 48);
			setupRanged(*this, ShootPattern_TripleSpread, 15.0f, 2.0f);
			behavior.stopChaseRange = 5.0f;
		}
	};

	struct DarkAngelEnemy : public BasicMeleEnemy
	{
		DarkAngelEnemy()
		{
			setupEnemy(*this, getAssetManager().darkAngel, 64);
			setupRanged(*this, ShootPattern_Single, 40.0f, 2.0f);
			behavior.shootRange = 10.0f;
			behavior.shootCooldown = 1.7f;
			behavior.projectileSpeed = 4.6f;
			behavior.stopChaseRange = 5.0f;
			behavior.hoverMeleeEnabled = true;
			behavior.hoverMeleeRange = 6.0f;
			behavior.hoverMeleeChance = 0.75f;
			behavior.hoverMeleeCooldown = 1.6f;
			behavior.hoverMeleeArcStrength = 0.85f;
			behavior.hoverMeleeSpeedMultiplier = 2.5f;
			behavior.specialShootPatterns = ShootPattern_RotatingCross;
			behavior.specialShootChance = 0.45f;
			hoverEnabled = true;
			hoverHeight = PIXEL_SIZE * 2.0f;
			hoverSpeed = 2.3f;
			renderOffsetY = PIXEL_SIZE * 18.0f;
		}
	};

	inline SkeletonEnemy getSkeletonEnemy()
	{
		return SkeletonEnemy{};
	}

	inline TemplarOriginalEnemy getTemplarOriginalEnemy() { return TemplarOriginalEnemy{}; }
	inline EarthTemplarEnemy getEarthTemplarEnemy() { return EarthTemplarEnemy{}; }
	inline FireTemplarEnemy getFireTemplarEnemy() { return FireTemplarEnemy{}; }
	inline IceTemplarEnemy getIceTemplarEnemy() { return IceTemplarEnemy{}; }
	inline WaterTemplarEnemy getWaterTemplarEnemy() { return WaterTemplarEnemy{}; }
	inline GoblinArcherEnemy getGoblinArcherEnemy() { return GoblinArcherEnemy{}; }
	inline GoblinSpearmanEnemy getGoblinSpearmanEnemy() { return GoblinSpearmanEnemy{}; }
	inline GoblinHeavyEnemy getGoblinHeavyEnemy() { return GoblinHeavyEnemy{}; }
	inline GoblinThiefEnemy getGoblinThiefEnemy() { return GoblinThiefEnemy{}; }
	inline OrcArcherEnemy getOrcArcherEnemy() { return OrcArcherEnemy{}; }
	inline DarkAngelEnemy getDarkAngelEnemy() { return DarkAngelEnemy{}; }


};
