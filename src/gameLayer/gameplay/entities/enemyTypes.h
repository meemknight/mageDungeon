#include "entity.h"
#include "entity.h"
#include "gameplay/assetsManager.h"

namespace EnemyTypes
{
	// Configures the base enemy visuals and behavior defaults.
	inline void setupEnemy(BasicMeleEnemy &enemy, TileSet &tileSet, int cellPixels)
	{
		enemy.tileSet = tileSet;
		int size = int(cellPixels * PIXEL_SIZE);
		enemy.animator.textureSize = {size, size};
		enemy.behavior.shootPatterns = ShootPattern_None;
	}

	struct SkeletonEnemy : public BasicMeleEnemy
	{
		SkeletonEnemy() { setupEnemy(*this, getAssetManager().skeleton, 48); }
	};

	struct TemplarOriginalEnemy : public BasicMeleEnemy
	{
		TemplarOriginalEnemy() { setupEnemy(*this, getAssetManager().templarOriginal, 48); }
	};

	struct EarthTemplarEnemy : public BasicMeleEnemy
	{
		EarthTemplarEnemy() { setupEnemy(*this, getAssetManager().earthTemplar, 48); }
	};

	struct FireTemplarEnemy : public BasicMeleEnemy
	{
		FireTemplarEnemy() { setupEnemy(*this, getAssetManager().fireTemplar, 48); }
	};

	struct IceTemplarEnemy : public BasicMeleEnemy
	{
		IceTemplarEnemy() { setupEnemy(*this, getAssetManager().iceTemplar, 48); }
	};

	struct WaterTemplarEnemy : public BasicMeleEnemy
	{
		WaterTemplarEnemy() { setupEnemy(*this, getAssetManager().waterTemplar, 48); }
	};

	struct GoblinArcherEnemy : public BasicMeleEnemy
	{
		GoblinArcherEnemy() { setupEnemy(*this, getAssetManager().goblinArcher, 48); }
	};

	struct GoblinSpearmanEnemy : public BasicMeleEnemy
	{
		GoblinSpearmanEnemy() { setupEnemy(*this, getAssetManager().goblinSpearman, 48); }
	};

	struct GoblinHeavyEnemy : public BasicMeleEnemy
	{
		GoblinHeavyEnemy() { setupEnemy(*this, getAssetManager().goblinHeavy, 32); }
	};

	struct GoblinThiefEnemy : public BasicMeleEnemy
	{
		GoblinThiefEnemy() { setupEnemy(*this, getAssetManager().goblinThief, 32); }
	};

	struct OrcArcherEnemy : public BasicMeleEnemy
	{
		OrcArcherEnemy() { setupEnemy(*this, getAssetManager().orcArcher, 48); }
	};

	struct DarkAngelEnemy : public BasicMeleEnemy
	{
		DarkAngelEnemy() { setupEnemy(*this, getAssetManager().darkAngel, 64); }
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
