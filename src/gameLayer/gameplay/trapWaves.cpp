#include "gameplay/trapWaves.h"

#include "gameplay/entities/enemyTypes.h"
#include "worldGen/floorGen.h"
#include "randomStuff.h"
#include <algorithm>

namespace
{
	struct WeightedEnemy
	{
		TrapEnemyType type = TrapEnemyType::GoblinArcher;
		int weight = 1;
	};

	struct DifficultyPools
	{
		std::vector<WeightedEnemy> base;
		std::vector<WeightedEnemy> elite;
		float eliteChanceSmall = 0.0f;
		float eliteChanceMedium = 0.0f;
		float eliteChanceLarge = 0.0f;
	};

	int clampDifficulty(int difficulty)
	{
		return std::max(0, std::min(difficulty, 3));
	}

	int pickRoomSizeTier(const FloorRoom &room)
	{
		int spawnCount = (int)room.enemySpawnPositions.size();
		if (spawnCount <= 3) { return 0; }
		if (spawnCount <= 6) { return 1; }
		return 2;
	}

	int pickBaseCount(int roomTier, std::ranlux24_base &rng)
	{
		switch (roomTier)
		{
			case 0: return getRandomInt(rng, 2, 3);
			case 1: return getRandomInt(rng, 3, 5);
			default: return getRandomInt(rng, 5, 8);
		}
	}

	int pickWaveCount(int roomTier, std::ranlux24_base &rng)
	{
		int waves = 1;
		if (roomTier == 2)
		{
			if (getRandomChance(rng, 0.38f)) { waves = 2; }
			if (waves == 2 && getRandomChance(rng, 0.25f)) { waves = 3; }
		}
		else if (roomTier == 1)
		{
			if (getRandomChance(rng, 0.22f)) { waves = 2; }
		}
		return waves;
	}

	TrapEnemyType pickWeighted(std::ranlux24_base &rng,
		const std::vector<WeightedEnemy> &pool)
	{
		int total = 0;
		for (const auto &entry : pool) { total += std::max(0, entry.weight); }
		if (total <= 0) { return pool.empty() ? TrapEnemyType::GoblinArcher : pool[0].type; }
		int roll = getRandomInt(rng, 1, total);
		for (const auto &entry : pool)
		{
			int w = std::max(0, entry.weight);
			if (roll <= w)
			{
				return entry.type;
			}
			roll -= w;
		}
		return pool.back().type;
	}

	std::vector<glm::vec2> pickSpawnPositions(const std::vector<glm::vec2> &spawnPositions,
		int count, std::ranlux24_base &rng)
	{
		std::vector<glm::vec2> positions = spawnPositions;
		std::vector<glm::vec2> result;
		if (positions.empty() || count <= 0) { return result; }
		count = std::min(count, (int)positions.size());
		result.reserve(count);
		for (int i = 0; i < count; i++)
		{
			int index = getRandomInt(rng, 0, (int)positions.size() - 1);
			result.push_back(positions[index]);
			positions[index] = positions.back();
			positions.pop_back();
		}
		return result;
	}

	DifficultyPools getDifficultyPools(int difficulty)
	{
		switch (clampDifficulty(difficulty))
		{
		case 0:
			return {
				{
					{TrapEnemyType::GoblinArcher, 2},
					{TrapEnemyType::GoblinSpearman, 3},
					{TrapEnemyType::GoblinThief, 6}
				},
				{
					{TrapEnemyType::GoblinHeavy, 1}
				},
				0.12f,
				0.2f,
				0.3f
			};
			case 1:
				return {
					{
						{TrapEnemyType::GoblinArcher, 3},
						{TrapEnemyType::GoblinSpearman, 3},
						{TrapEnemyType::GoblinThief, 2},
						{TrapEnemyType::OrcArcher, 2}
					},
					{
						{TrapEnemyType::GoblinHeavy, 2},
						{TrapEnemyType::Skeleton, 1}
					},
					0.22f,
					0.35f,
					0.48f
				};
			case 2:
				return {
					{
						{TrapEnemyType::GoblinArcher, 2},
						{TrapEnemyType::GoblinSpearman, 2},
						{TrapEnemyType::GoblinThief, 1},
						{TrapEnemyType::OrcArcher, 2},
						{TrapEnemyType::Skeleton, 2}
					},
					{
						{TrapEnemyType::Templar, 3}
					},
					0.30f,
					0.46f,
					0.62f
				};
			default:
				return {
					{
						{TrapEnemyType::Templar, 5},
						{TrapEnemyType::Skeleton, 1},
						{TrapEnemyType::OrcArcher, 1}
					},
					{
						{TrapEnemyType::DarkAngel, 2},
						{TrapEnemyType::Templar, 1}
					},
					0.36f,
					0.52f,
					0.72f
				};
		}
	}
}

TrapWavePlan buildTrapRoomWavePlan(const FloorRoom &room, int difficulty,
	std::ranlux24_base &rng, const std::vector<glm::vec2> *spawnPositions)
{
	TrapWavePlan plan = {};
	const auto &roomSpawns = spawnPositions ? *spawnPositions : room.enemySpawnPositions;
	if (roomSpawns.empty()) { return plan; }

	int roomTier = pickRoomSizeTier(room);
	int baseCount = pickBaseCount(roomTier, rng);
	baseCount += clampDifficulty(difficulty);
	baseCount = std::max(1, std::min(baseCount, (int)roomSpawns.size()));

	int waveCount = pickWaveCount(roomTier, rng);
	DifficultyPools pools = getDifficultyPools(difficulty);

	plan.waves.reserve(waveCount);
	for (int waveIndex = 0; waveIndex < waveCount; waveIndex++)
	{
		int waveCountLocal = baseCount;
		if (waveIndex > 0)
		{
			waveCountLocal = std::max(2, baseCount - 1);
		}
		waveCountLocal = std::min(waveCountLocal, (int)roomSpawns.size());

		float eliteChance = pools.eliteChanceSmall;
		if (roomTier == 1) { eliteChance = pools.eliteChanceMedium; }
		if (roomTier == 2) { eliteChance = pools.eliteChanceLarge; }
		eliteChance += 0.12f * waveIndex;

		int eliteCount = 0;
		if (!pools.elite.empty() && getRandomChance(rng, eliteChance))
		{
			eliteCount = 1;
			if (roomTier == 2 && getRandomChance(rng, eliteChance * 0.45f))
			{
				eliteCount = 2;
			}
		}
		eliteCount = std::min(eliteCount, std::max(0, waveCountLocal - 1));

		auto positions = pickSpawnPositions(roomSpawns, waveCountLocal, rng);
		std::vector<TrapWaveSpawn> wave;
		wave.reserve(positions.size());
		for (size_t i = 0; i < positions.size(); i++)
		{
			TrapEnemyType type = TrapEnemyType::GoblinArcher;
			if ((int)i < eliteCount)
			{
				type = pickWeighted(rng, pools.elite);
			}
			else
			{
				type = pickWeighted(rng, pools.base);
			}
			wave.push_back({type, positions[i]});
		}
		if (!wave.empty())
		{
			plan.waves.push_back(std::move(wave));
		}
	}

	return plan;
}

void spawnTrapWaveEnemy(EntityHolder &entityHolder, TrapEnemyType type, glm::vec2 pos,
	std::ranlux24_base &rng)
{
	switch (type)
	{
		case TrapEnemyType::GoblinArcher:
			entityHolder.addEntity(EnemyTypes::getGoblinArcherEnemy(), pos);
			break;
		case TrapEnemyType::GoblinSpearman:
			entityHolder.addEntity(EnemyTypes::getGoblinSpearmanEnemy(), pos);
			break;
		case TrapEnemyType::GoblinThief:
			entityHolder.addEntity(EnemyTypes::getGoblinThiefEnemy(), pos);
			break;
		case TrapEnemyType::GoblinHeavy:
			entityHolder.addEntity(EnemyTypes::getGoblinHeavyEnemy(), pos);
			break;
		case TrapEnemyType::OrcArcher:
			entityHolder.addEntity(EnemyTypes::getOrcArcherEnemy(), pos);
			break;
		case TrapEnemyType::Skeleton:
			entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), pos);
			break;
		case TrapEnemyType::Templar:
		{
			int pick = getRandomInt(rng, 0, 4);
			switch (pick)
			{
				case 0: entityHolder.addEntity(EnemyTypes::getTemplarOriginalEnemy(), pos); break;
				case 1: entityHolder.addEntity(EnemyTypes::getEarthTemplarEnemy(), pos); break;
				case 2: entityHolder.addEntity(EnemyTypes::getFireTemplarEnemy(), pos); break;
				case 3: entityHolder.addEntity(EnemyTypes::getIceTemplarEnemy(), pos); break;
				default: entityHolder.addEntity(EnemyTypes::getWaterTemplarEnemy(), pos); break;
			}
		} break;
		case TrapEnemyType::DarkAngel:
			entityHolder.addEntity(EnemyTypes::getDarkAngelEnemy(), pos);
			break;
	}
}
