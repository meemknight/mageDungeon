#pragma once

#include <glm/vec2.hpp>
#include <random>
#include <vector>

struct FloorRoom;
struct EntityHolder;

// Trap room wave builder and spawn helpers.
// Generates small, room-sized encounter waves from difficulty tiers.

// Enemy archetypes used by trap-room encounter waves.
enum class TrapEnemyType
{
	GoblinArcher,
	GoblinSpearman,
	GoblinThief,
	GoblinHeavy,
	OrcArcher,
	Skeleton,
	Templar,
	DarkAngel
};

struct TrapWaveSpawn
{
	TrapEnemyType type = TrapEnemyType::GoblinArcher;
	glm::vec2 pos = {};
};

struct TrapWavePlan
{
	std::vector<std::vector<TrapWaveSpawn>> waves;
};

TrapWavePlan buildTrapRoomWavePlan(const FloorRoom &room, int difficulty,
	std::ranlux24_base &rng, const std::vector<glm::vec2> *spawnPositions = nullptr);

void spawnTrapWaveEnemy(EntityHolder &entityHolder, TrapEnemyType type, glm::vec2 pos,
	std::ranlux24_base &rng);
