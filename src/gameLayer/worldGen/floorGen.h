#pragma once
#include <FastNoiseSIMD.h>
#include <gameplay/map.h>
#include <gameplay/doors.h>
#include <glm/vec2.hpp>
#include <random>
#include <randomStuff.h>
#include <algorithm>
#include <cmath>
#include <climits>
#include <optional>
#include <vector>

struct FloorConnection
{
	enum class Side
	{
		North,
		South,
		West,
		East
	};

	Side side = Side::North;
	int offset = 0;
	int type = 0;
	bool required = true;
};

struct FloorRoom
{
	glm::ivec2 pos = {};
	glm::ivec2 size = {};
	std::vector<glm::vec2> enemySpawnPositions;
	std::vector<glm::vec2> wandSpawnPositions; // optional item spawn points
	std::vector<glm::ivec2> doorPositions; // tile positions for future door placement
	bool isSpawnRoom = false;
	bool isBigRoom = false;
	bool isEmptyRoom = false;
	bool isExitRoom = false;

	glm::ivec2 center() const { return {pos.x + size.x / 2, pos.y + size.y / 2}; }
	int area() const { return size.x * size.y; }
};

struct FloorInfo
{
	std::vector<FloorRoom> rooms;
	std::vector<glm::vec2> enemySpawnPositions;
	std::optional<glm::vec2> playerSpawnPos;
	std::optional<int> spawnRoomIndex;
	std::optional<int> exitRoomIndex;
	std::optional<glm::vec2> exitPos;
};

struct FloorGenerator
{
	FastNoiseSIMD *grassDecorNoise = nullptr;

	FastNoiseSIMD *dirtNoise = nullptr;
	FastNoiseSIMD *dirtDecorNoise = nullptr;

	// Cosmetic tuning values for world generation.
	struct CosmeticTuning
	{
		float caveRoomChance = 0.18f;
		float woodRoomChance = 0.18f;
		float grassRoomChance = 0.20f;
		int caveRoomRadiusMin = 5;
		int caveRoomRadiusMax = 9;
		int caveRoomExtentPadding = 2;
		float caveRoomSetpieceChance = 0.85f;
		int caveRoomWallClustersMin = 2;
		int caveRoomWallClustersMax = 4;
		int caveRoomWallClusterSizeMin = 2;
		int caveRoomWallClusterSizeMax = 4;
		int caveDoorClearRadius = 3;
		// Damaged wooden plank patches inside cave rooms.
		float caveRoomWoodFloorChance = 0.35f;
		float caveRoomWoodDamageBase = 0.15f;
		float caveRoomWoodDamageDoorBoost = 0.7f;
		int caveRoomWoodDamageRadius = 6;
		// Bigger cave rooms with maze-like interiors.
		float caveMazeRoomChance = 0.35f;
		int caveMazeRoomRadiusMin = 9;
		int caveMazeRoomRadiusMax = 14;
		int caveMazeRoomExtentPadding = 4;
		// Carpet roads linking entrances inside wooden rooms.
		float woodRoomCarpetRoadChance = 0.35f;
		// Dungeon wall decorations.
		float wallDecorChance = 0.08f;
		// If true, rooms can stay empty (flagged for future use).
		bool allowEmptyRooms = true;
		float bigRoomChance = 0.12f;
		int bigRoomMinSize = 22;
		int bigRoomMaxSize = 30;
		int bigRoomAreaThreshold = 520;
		float bigRoomSetpieceChance = 0.95f;
		int bigRoomSpawnMargin = 4;
		int doorSpawnMinDist = 4;
		int bigRoomDoorMinDist = 6;

		float floor2Chance = 0.12f;
		float grassRoomDirtSpotChance = 0.08f;
		float roadDirtDecorChance = 0.18f;
		float grassRoomDirtExpandChance = 0.30f;

		float grassDecorNoiseFrequency = 0.1f;
		int grassDecorNoiseOctaves = 3;
		float grassDecorNoiseLacunarity = 2.0f;
		float grassDecorNoiseGain = 0.5f;

		float dirtNoiseFrequency = 0.08f;
		int dirtNoiseOctaves = 2;
		float dirtNoiseLacunarity = 2.0f;
		float dirtNoiseGain = 0.5f;

		float dirtDecorNoiseFrequency = 0.75f;
		float dirtCutoffBase = 0.95f;
		float dirtCutoffScale = 0.45f;
		float dirtDecorCutoff = 0.80f;

		float plainsDirtThreshold = 0.4f;
		float grassRoomDirtThreshold = 0.16f;

		float grassDecorThresholdPlains = 0.48f;
		float grassDecorThresholdDungeon = 0.55f;
		float grassFlowerPatchThreshold = 0.80f;
		float grassFlowerPatchChance = 0.85f;
		float grassMushroomChance = 0.02f;
		float grassStoneChance = 0.25f;
		float grassFlowerChance = 0.45f;

		float grassRoomRoadChance = 0.5f;
		float grassRoomTreeChance = 0.85f;
		int grassRoomTreeAreaDiv = 55;
		int grassRoomTreeMin = 1;
		int grassRoomTreeMax = 6;

		float dungeonFloorPatternChance = 0.32f;
		int bigTileAreaDiv = 24;
		int bigTileMax = 6;
		int bigTileAttempts = 20;
	};

	CosmeticTuning cosmetics = {};

	// Stable hash used for per-tile decoration choices.
	static unsigned int hashPosition(int x, int y, int seed)
	{
		unsigned int h = 2166136261u;
		h = (h ^ (unsigned int)(x + seed * 31)) * 16777619u;
		h = (h ^ (unsigned int)(y + seed * 131)) * 16777619u;
		h = (h ^ (unsigned int)seed) * 16777619u;
		return h;
	}

	static float hashToFloat01(int x, int y, int seed)
	{
		unsigned int h = hashPosition(x, y, seed);
		return (h & 0x00FFFFFFu) / 16777216.0f;
	}

	BlockType pickGrassDecorationType(int x, int y, int seed)
	{
		float roll = hashToFloat01(x, y, seed + 17);
		float patch = hashToFloat01(x, y, seed + 911);
		if (patch > cosmetics.grassFlowerPatchThreshold && roll < cosmetics.grassFlowerPatchChance)
		{
			return Blocks::grassDecorationFlowers;
		}
		if (roll < cosmetics.grassMushroomChance)
		{
			return Blocks::grassDecorationMushrooms;
		}
		if (roll < cosmetics.grassStoneChance)
		{
			return Blocks::grassDecorationStones;
		}
		if (roll < cosmetics.grassFlowerChance)
		{
			return Blocks::grassDecorationFlowers;
		}
		return Blocks::grassDecoration;
	}

	void init();

	void placeGrassLayer(Map &map, int seed)
	{
		for (int y = 0; y < map.size.y; y++)
			for (int x = 0; x < map.size.x; x++)
				map.firstLayer.getBlockUnsafe(x, y).type = Blocks::grass;

		if (!grassDecorNoise) return;

		grassDecorNoise->SetSeed(seed);

		const int sx = map.size.x;
		const int sy = map.size.y;

		float *n = grassDecorNoise->GetNoiseSet(0, 0, 0, sx, sy, 1);

		const float threshold = cosmetics.grassDecorThresholdPlains;

		for (int y = 0; y < sy; y++)
		{
			for (int x = 0; x < sx; x++)
			{
				const int idx = x + y * sx;
				const float v = (n[idx] + 1.f) * 0.5f;

				if (v > threshold)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::grass)
						b.type = pickGrassDecorationType(x, y, seed);
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(n);
	}

	// tresshold 0..1
	// low tresshold => few small patches (as you expect with 0.2f)
	void placeRandomDirtSpots(Map &map, int seed, float tresshold)
	{
		if (!dirtNoise || !dirtDecorNoise) return;

		// clamp
		if (tresshold < 0.f) tresshold = 0.f;
		if (tresshold > 1.f) tresshold = 1.f;

		const int sx = map.size.x;
		const int sy = map.size.y;

		// ---- Dirt blobs ----
		dirtNoise->SetSeed(seed);

		// Make low tresshold => HIGH cutoff => fewer hits.
		// 0.2 -> about 0.90 (few patches)
		const float dirtCutoff = cosmetics.dirtCutoffBase - (tresshold * cosmetics.dirtCutoffScale);

		float *dn = dirtNoise->GetNoiseSet(0, 0, 0, sx, sy, 1);

		for (int y = 0; y < sy; y++)
		{
			for (int x = 0; x < sx; x++)
			{
				const int idx = x + y * sx;
				const float v = (dn[idx] + 1.f) * 0.5f;

				if (v > dirtCutoff)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);

					// only paint dirt on top of grass layer types
					if (b.type == Blocks::grass || b.type == Blocks::grassDecoration
						|| b.type == Blocks::grassDecorationStones
						|| b.type == Blocks::grassDecorationFlowers
						|| b.type == Blocks::grassDecorationMushrooms)
						b.type = Blocks::dirt;
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(dn);

		// ---- Dirt decoration (sparse dots, not clumps) ----
		// Use a different seed so it doesn't correlate with blobs too much
		dirtDecorNoise->SetSeed(seed + 1337);

		const float decorCutoff = cosmetics.dirtDecorCutoff;

		float *ddn = dirtDecorNoise->GetNoiseSet(0, 0, 0, sx, sy, 1);

		for (int y = 0; y < sy; y++)
		{
			for (int x = 0; x < sx; x++)
			{
				const int idx = x + y * sx;
				const float v = (ddn[idx] + 1.f) * 0.5f;

				if (v > decorCutoff)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::dirt)
						b.type = Blocks::dirtDecoration;
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(ddn);
	}

	// Adds subtle grass variation without overwriting non-grass tiles.
	void decorateGrassPatches(Map &map, int seed)
	{
		if (!grassDecorNoise) return;

		grassDecorNoise->SetSeed(seed);

		const int sx = map.size.x;
		const int sy = map.size.y;

		float *n = grassDecorNoise->GetNoiseSet(0, 0, 0, sx, sy, 1);

		const float threshold = cosmetics.grassDecorThresholdDungeon;

		for (int y = 0; y < sy; y++)
		{
			for (int x = 0; x < sx; x++)
			{
				const int idx = x + y * sx;
				const float v = (n[idx] + 1.f) * 0.5f;

				if (v > threshold)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::grass)
						b.type = pickGrassDecorationType(x, y, seed);
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(n);
	}

	// Sprinkles rare trees on top of grass/dirt tiles.
	void placeRareTrees(Map &map, std::ranlux24_base &rng, float chance)
	{
		chance = std::clamp(chance, 0.0f, 1.0f);
		for (int y = 2; y < map.size.y - 2; y++)
		{
			for (int x = 2; x < map.size.x - 2; x++)
			{
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				auto &over = map.secondLayer.getBlockUnsafe(x, y);

				if (over.type != Blocks::none) { continue; }
				if (base.type != Blocks::grass && base.type != Blocks::grassDecoration
					&& base.type != Blocks::grassDecorationStones
					&& base.type != Blocks::grassDecorationFlowers
					&& base.type != Blocks::grassDecorationMushrooms
					&& base.type != Blocks::dirt && base.type != Blocks::dirtDecoration)
				{
					continue;
				}

				if (getRandomChance(rng, chance))
				{
					over.type = Blocks::smallTree;
				}
			}
		}
	}

	void clear()
	{
		delete grassDecorNoise;
		delete dirtNoise;
		delete dirtDecorNoise;
		*this = {};
	}

	void generatePlainsFloor(int sizeX, int sizeY, Map &map, int seed)
	{
		map.create(sizeX, sizeY);

		placeGrassLayer(map, seed);

		placeRandomDirtSpots(map, seed + 10, cosmetics.plainsDirtThreshold);


		//map.secondLayer.getBlockUnsafe(32, 30).type = Blocks::smallTree;
		//map.secondLayer.getBlockUnsafe(34, 30).type = Blocks::smallTree;
		//map.secondLayer.getBlockUnsafe(35, 30).type = Blocks::smallTree;
		//map.secondLayer.getBlockUnsafe(36, 30).type = Blocks::smallTree;
		//
		//map.firstLayer.getBlockUnsafe(5, 3).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(5, 7).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(4, 7).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(6, 7).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(8, 5).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(8, 6).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(8, 7).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(12, 5).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(12, 6).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(12, 7).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(11, 6).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(13, 6).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(14, 6).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(14, 5).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(3, 11).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(4, 11).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(6, 11).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(7, 11).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(3, 12).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(7, 12).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(3, 15).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(7, 15).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(3, 16).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(4, 16).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(6, 16).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(7, 16).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(16, 16).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(17, 16).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(16, 17).type = Blocks::cobbleStoneWall;
		//map.firstLayer.getBlockUnsafe(17, 17).type = Blocks::cobbleStoneWall;

		//for (int y = 0; y < 15; y++)
		//{
		//	for (int x = 15; x < 30; x++)
		//	{
		//		map.firstLayer.getBlockUnsafe(x,y).type = Blocks::floor1;
		//	}
		//}
		//
		//for (int y = 15; y < 30; y++)
		//{
		//	for (int x = 15; x < 30; x++)
		//	{
		//		map.firstLayer.getBlockUnsafe(x, y).type = Blocks::floor2;
		//	}
		//}
		//
		//map.firstLayer.getBlockUnsafe(5 + 16, 3).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(5 + 16, 7).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(4 + 16, 7).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(6 + 16, 7).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(8 + 16, 5).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(8 + 16, 6).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(8 + 16, 7).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(12 + 16, 5).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(12 + 16, 6).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(12 + 16, 7).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(11 + 16, 6).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(13 + 16, 6).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(14 + 16, 6).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(14 + 16, 5).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(3 + 16, 11).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(4 + 16, 11).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(6 + 16, 11).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(7 + 16, 11).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(3 + 16, 12).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(7 + 16, 12).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(3 + 16, 15).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(7 + 16, 15).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(3 + 16, 16).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(4 + 16, 16).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(6 + 16, 16).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(7 + 16, 16).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(16 + 16, 16).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(17 + 16, 16).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(16 + 16, 17).type = Blocks::dungeonWall;
		//map.firstLayer.getBlockUnsafe(17 + 16, 17).type = Blocks::dungeonWall;
		//
		//
		//for (int i = 0; i < 10; i++)
		//	map.firstLayer.getBlockUnsafe(13, 19 + i).type = Blocks::cobbleStoneWall;

	}

	void generateCaveFloor(int sizeX, int sizeY, Map &map, int seed)
	{
		generatePlainsFloor(sizeX, sizeY, map, seed);
	}

	void generateTutorialFloor(int sizeX, int sizeY, Map &map, FloorInfo &outInfo, DoorHolder &doorHolder);

	void generateDungeonFloor(int sizeX, int sizeY, Map &map, int seed,
		const std::vector<FloorConnection> &connections,
		bool createASpawnRoom, FloorInfo &outInfo, DoorHolder &doorHolder);
};
