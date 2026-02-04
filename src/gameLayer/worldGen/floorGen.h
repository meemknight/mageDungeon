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

	glm::ivec2 center() const { return {pos.x + size.x / 2, pos.y + size.y / 2}; }
};

struct FloorInfo
{
	std::vector<FloorRoom> rooms;
	std::vector<glm::vec2> enemySpawnPositions;
	std::optional<glm::vec2> playerSpawnPos;
	std::optional<int> spawnRoomIndex;
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

	void init()
	{
		grassDecorNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		grassDecorNoise->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
		grassDecorNoise->SetFrequency(cosmetics.grassDecorNoiseFrequency);
		grassDecorNoise->SetFractalOctaves(cosmetics.grassDecorNoiseOctaves);
		grassDecorNoise->SetFractalLacunarity(cosmetics.grassDecorNoiseLacunarity);
		grassDecorNoise->SetFractalGain(cosmetics.grassDecorNoiseGain);

		// --- Dirt blobs (small circular-ish patches) ---
		dirtNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		dirtNoise->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
		dirtNoise->SetFrequency(cosmetics.dirtNoiseFrequency);        // higher than grass -> smaller blobs
		dirtNoise->SetFractalOctaves(cosmetics.dirtNoiseOctaves);       // less detail -> fewer weird tendrils
		dirtNoise->SetFractalLacunarity(cosmetics.dirtNoiseLacunarity);
		dirtNoise->SetFractalGain(cosmetics.dirtNoiseGain);

		// --- Dirt decoration (sparse random dots, not clumps) ---
		dirtDecorNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		dirtDecorNoise->SetNoiseType(FastNoiseSIMD::NoiseType::Simplex); // no fractal -> less clumping
		dirtDecorNoise->SetFrequency(cosmetics.dirtDecorNoiseFrequency);   // high freq -> small isolated hits
	}

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

	void generateDungeonFloor(int sizeX, int sizeY, Map &map, int seed,
		const std::vector<FloorConnection> &connections,
		bool createASpawnRoom, FloorInfo &outInfo, DoorHolder &doorHolder)
	{
		map.create(sizeX, sizeY);
		outInfo = {};
		doorHolder.clear();

		std::ranlux24_base rng(seed);

		for (int y = 0; y < sizeY; y++)
		{
			for (int x = 0; x < sizeX; x++)
			{
				map.firstLayer.getBlockUnsafe(x, y).type = Blocks::dungeonWall;
				map.secondLayer.getBlockUnsafe(x, y).type = Blocks::none;
			}
		}

		struct Rect
		{
			int x = 0;
			int y = 0;
			int w = 0;
			int h = 0;
			bool isCave = false;
			bool isCaveMaze = false;
			int caveRadius = 0;
			bool isGrassRoom = false;
			bool isWoodRoom = false;
			bool isBigRoom = false;

			int x2() const { return x + w; }
			int y2() const { return y + h; }
			glm::ivec2 center() const { return {x + w / 2, y + h / 2}; }
		};

		auto randRange = [&](int minVal, int maxVal)
		{
			return getRandomInt(rng, minVal, maxVal);
		};

		std::vector<Rect> rooms;
		int attempts = std::max(28, (sizeX * sizeY) / 140);
		int minRoomSize = 16;
		int maxRoomSize = 26;
		int padding = 4;
		int maxRoomConnections = 3;

		auto overlaps = [&](const Rect &a, const Rect &b)
		{
			return !(a.x2() + padding <= b.x || b.x2() + padding <= a.x ||
				a.y2() + padding <= b.y || b.y2() + padding <= a.y);
		};

		auto carveFloor = [&](int x, int y)
		{
			if (x < 0 || y < 0 || x >= sizeX || y >= sizeY) { return; }
			auto &b = map.firstLayer.getBlockUnsafe(x, y);
			b.type = Blocks::floor1;
			if (getRandomFloat(rng, 0.0f, 1.0f) < cosmetics.floor2Chance)
			{
				b.type = Blocks::floor2;
			}
		};

		auto carveRoom = [&](const Rect &room)
		{
			for (int y = room.y; y < room.y2(); y++)
			{
				for (int x = room.x; x < room.x2(); x++)
				{
					carveFloor(x, y);
				}
			}
		};

		auto carveRoomObstacles = [&](const Rect &room)
		{
			int obstacleCount = getRandomInt(rng, 1, 3);
			for (int i = 0; i < obstacleCount; i++)
			{
				int ox = randRange(room.x + 2, room.x2() - 3);
				int oy = randRange(room.y + 2, room.y2() - 3);
				int ow = randRange(2, 4);
				int oh = randRange(2, 4);
				for (int y = oy; y < oy + oh; y++)
				{
					for (int x = ox; x < ox + ow; x++)
					{
						if (x <= room.x + 1 || x >= room.x2() - 2 || y <= room.y + 1 || y >= room.y2() - 2)
						{
							continue;
						}
						map.firstLayer.getBlockUnsafe(x, y).type = Blocks::dungeonWall;
					}
				}
			}
		};

		// Irregular, rounded cave rooms carved out of stone walls.
		auto paintCircle = [&](glm::ivec2 center, int radius, BlockType newType, BlockType requiredType)
		{
			int minX = std::max(2, center.x - radius - 2);
			int maxX = std::min(sizeX - 3, center.x + radius + 2);
			int minY = std::max(2, center.y - radius - 2);
			int maxY = std::min(sizeY - 3, center.y + radius + 2);

			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					float dx = (x + 0.5f) - (float)center.x;
					float dy = (y + 0.5f) - (float)center.y;
					float dist = std::sqrt(dx * dx + dy * dy);
					float jitter = getRandomFloat(rng, -0.65f, 0.65f);

					if (dist <= radius + jitter)
					{
						auto &b = map.firstLayer.getBlockUnsafe(x, y);
						if (b.type == requiredType)
						{
							b.type = newType;
						}
					}
				}
			}
		};

		auto paintCircleInRoom = [&](const Rect &room, glm::ivec2 center, int radius,
			BlockType newType, BlockType requiredType)
		{
			int minX = std::max(room.x, std::max(2, center.x - radius - 2));
			int maxX = std::min(room.x2() - 1, std::min(sizeX - 3, center.x + radius + 2));
			int minY = std::max(room.y, std::max(2, center.y - radius - 2));
			int maxY = std::min(room.y2() - 1, std::min(sizeY - 3, center.y + radius + 2));

			if (minX > maxX || minY > maxY) { return; }

			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					float dx = (x + 0.5f) - (float)center.x;
					float dy = (y + 0.5f) - (float)center.y;
					float dist = std::sqrt(dx * dx + dy * dy);
					float jitter = getRandomFloat(rng, -0.65f, 0.65f);

					if (dist <= radius + jitter)
					{
						auto &b = map.firstLayer.getBlockUnsafe(x, y);
						if (b.type == requiredType)
						{
							b.type = newType;
						}
					}
				}
			}
		};

		auto paintCaveCircle = [&](const Rect &room, glm::ivec2 center, int radius,
			BlockType newType, BlockType requiredType)
		{
			// Keep cave carving inside the room bounds.
			paintCircleInRoom(room, center, radius, newType, requiredType);
		};

		auto carveCaveRoom = [&](const Rect &room)
		{
			glm::ivec2 center = room.center();
			int baseRadius = std::max(4, room.caveRadius);
			int lumps = room.isCaveMaze ? getRandomInt(rng, 5, 8) : getRandomInt(rng, 3, 5);
			int scatter = std::max(2, baseRadius / 2);
			if (room.isCaveMaze)
			{
				scatter = std::max(3, baseRadius / 2 + 1);
			}
			std::vector<glm::ivec2> centers;
			centers.reserve(lumps);
			centers.push_back(center);

			for (int i = 1; i < lumps; i++)
			{
				glm::ivec2 offset = {
					getRandomInt(rng, -scatter, scatter),
					getRandomInt(rng, -scatter, scatter)
				};
				centers.push_back(center + offset);
			}

			for (auto c : centers)
			{
				int r = baseRadius + getRandomInt(rng, -2, 2);
				paintCaveCircle(room, c, r, Blocks::cobbleStoneWall, Blocks::dungeonWall);
			}

			int innerRadius = std::max(3, baseRadius - 2);
			for (auto c : centers)
			{
				int r = innerRadius + getRandomInt(rng, -1, 1);
				paintCaveCircle(room, c, r, Blocks::caveFloor, Blocks::cobbleStoneWall);
			}
		};

		// Converts a room into grassy ground without breaking surrounding walls.
		auto paintGrassRoom = [&](const Rect &room)
		{
			for (int y = room.y; y < room.y2(); y++)
			{
				for (int x = room.x; x < room.x2(); x++)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::floor1 || b.type == Blocks::floor2)
					{
						b.type = Blocks::grass;
						if (getRandomChance(rng, cosmetics.grassRoomDirtSpotChance))
						{
							b.type = Blocks::dirt;
						}
					}
				}
			}
		};

		// Converts a room into wooden floor tiles.
		auto paintWoodRoom = [&](const Rect &room)
		{
			for (int y = room.y; y < room.y2(); y++)
			{
				for (int x = room.x; x < room.x2(); x++)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::floor1 || b.type == Blocks::floor2
						|| b.type == Blocks::floorPatern1
						|| b.type == Blocks::floorBigTileTopLeft
						|| b.type == Blocks::floorBigTileTopRight
						|| b.type == Blocks::floorBigTileBottomLeft
						|| b.type == Blocks::floorBigTileBottomRight)
					{
						b.type = Blocks::woodenFloor;
					}
				}
			}
		};

		// Shared floor pattern painter for carpets and cave planks.
		auto placeRoomPattern = [&](const Rect &room, const std::vector<glm::ivec2> &doors,
			auto &&isBaseTile, auto &&applyTile, float roadChance)
		{
			int cx = room.x + room.w / 2;
			int cy = room.y + room.h / 2;
			int innerMinX = room.x + 1;
			int innerMaxX = room.x2() - 2;
			int innerMinY = room.y + 1;
			int innerMaxY = room.y2() - 2;

			auto canPlaceTile = [&](int x, int y)
			{
				if (x < innerMinX || x > innerMaxX || y < innerMinY || y > innerMaxY)
				{
					return false;
				}
				return isBaseTile(x, y);
			};

			auto placeTile = [&](int x, int y)
			{
				if (!canPlaceTile(x, y)) { return; }
				applyTile(x, y);
			};

			auto clampPatternPos = [&](glm::ivec2 pos)
			{
				pos.x = std::clamp(pos.x, innerMinX, innerMaxX);
				pos.y = std::clamp(pos.y, innerMinY, innerMaxY);
				return pos;
			};

			auto canPaintHorizontal = [&](int x0, int x1, int y, int side)
			{
				int start = std::min(x0, x1);
				int end = std::max(x0, x1);
				for (int x = start; x <= end; x++)
				{
					if (!canPlaceTile(x, y)) { return false; }
					if (side != 0 && !canPlaceTile(x, y + side)) { return false; }
				}
				return true;
			};

			auto canPaintVertical = [&](int y0, int y1, int x, int side)
			{
				int start = std::min(y0, y1);
				int end = std::max(y0, y1);
				for (int y = start; y <= end; y++)
				{
					if (!canPlaceTile(x, y)) { return false; }
					if (side != 0 && !canPlaceTile(x + side, y)) { return false; }
				}
				return true;
			};

			auto pickHorizontalSide = [&](int x0, int x1, int y) -> std::optional<int>
			{
				int primary = getRandomChance(rng, 0.5f) ? 1 : -1;
				int secondary = -primary;
				if (canPaintHorizontal(x0, x1, y, primary)) { return primary; }
				if (canPaintHorizontal(x0, x1, y, secondary)) { return secondary; }
				if (canPaintHorizontal(x0, x1, y, 0)) { return 0; }
				return {};
			};

			auto pickVerticalSide = [&](int y0, int y1, int x) -> std::optional<int>
			{
				int primary = getRandomChance(rng, 0.5f) ? 1 : -1;
				int secondary = -primary;
				if (canPaintVertical(y0, y1, x, primary)) { return primary; }
				if (canPaintVertical(y0, y1, x, secondary)) { return secondary; }
				if (canPaintVertical(y0, y1, x, 0)) { return 0; }
				return {};
			};

			auto paintHorizontal = [&](int x0, int x1, int y, int side)
			{
				int start = std::min(x0, x1);
				int end = std::max(x0, x1);
				for (int x = start; x <= end; x++)
				{
					placeTile(x, y);
					if (side != 0)
					{
						placeTile(x, y + side);
					}
				}
			};

			auto paintVertical = [&](int y0, int y1, int x, int side)
			{
				int start = std::min(y0, y1);
				int end = std::max(y0, y1);
				for (int y = start; y <= end; y++)
				{
					placeTile(x, y);
					if (side != 0)
					{
						placeTile(x + side, y);
					}
				}
			};

			// Optional straight floor road between two entrances.
			if (doors.size() >= 2 && getRandomChance(rng, roadChance))
			{
				int aIndex = getRandomInt(rng, 0, (int)doors.size() - 1);
				int bIndex = getRandomInt(rng, 0, (int)doors.size() - 1);
				if (bIndex == aIndex)
				{
					bIndex = (bIndex + 1) % (int)doors.size();
				}

				glm::ivec2 start = clampPatternPos(doors[aIndex]);
				glm::ivec2 end = clampPatternPos(doors[bIndex]);
				if (start != end)
				{
					auto tryPaintRoad = [&](bool horizontalFirst)
					{
						if (horizontalFirst)
						{
							glm::ivec2 mid = {end.x, start.y};
							auto sideH = pickHorizontalSide(start.x, mid.x, start.y);
							if (!sideH) { return false; }
							auto sideV = pickVerticalSide(mid.y, end.y, mid.x);
							if (!sideV) { return false; }
							paintHorizontal(start.x, mid.x, start.y, *sideH);
							paintVertical(mid.y, end.y, mid.x, *sideV);
							return true;
						}
						else
						{
							glm::ivec2 mid = {start.x, end.y};
							auto sideV = pickVerticalSide(start.y, mid.y, start.x);
							if (!sideV) { return false; }
							auto sideH = pickHorizontalSide(mid.x, end.x, mid.y);
							if (!sideH) { return false; }
							paintVertical(start.y, mid.y, start.x, *sideV);
							paintHorizontal(mid.x, end.x, mid.y, *sideH);
							return true;
						}
					};

					bool horizontalFirst = getRandomChance(rng, 0.5f);
					if (tryPaintRoad(horizontalFirst) || tryPaintRoad(!horizontalFirst))
					{
						return;
					}
				}
			}

			int pattern = getRandomInt(rng, 0, 5);
			switch (pattern)
			{
				case 0:
				{
					// center patch
					int rugW = std::max(3, room.w / 3);
					int rugH = std::max(3, room.h / 3);
					int startX = cx - rugW / 2;
					int startY = cy - rugH / 2;
					for (int y = startY; y < startY + rugH; y++)
					{
						for (int x = startX; x < startX + rugW; x++)
						{
							placeTile(x, y);
						}
					}
					break;
				}
				case 1:
				{
					// entry path
					for (int y = room.y + 1; y < room.y2() - 1; y++)
					{
						placeTile(cx, y);
						placeTile(cx + 1, y);
					}
					break;
				}
				case 2:
				{
					// border strip
					for (int x = room.x + 1; x < room.x2() - 1; x++)
					{
						placeTile(x, room.y + 1);
						placeTile(x, room.y2() - 2);
					}
					for (int y = room.y + 2; y < room.y2() - 2; y++)
					{
						placeTile(room.x + 1, y);
						placeTile(room.x2() - 2, y);
					}
					break;
				}
				case 3:
				{
					// margin lanes
					for (int y = room.y + 2; y < room.y2() - 2; y++)
					{
						placeTile(room.x + 2, y);
						placeTile(room.x2() - 3, y);
					}
					break;
				}
				case 4:
				{
					// centered rectangle
					int rugW = std::max(4, room.w / 2);
					int rugH = std::max(4, room.h / 2);
					int startX = cx - rugW / 2;
					int startY = cy - rugH / 2;
					for (int y = startY; y < startY + rugH; y++)
					{
						for (int x = startX; x < startX + rugW; x++)
						{
							placeTile(x, y);
						}
					}
					break;
				}
				case 5:
				{
					// centered square
					int size = std::max(3, std::min(room.w, room.h) / 3);
					int startX = cx - size / 2;
					int startY = cy - size / 2;
					for (int y = startY; y < startY + size; y++)
					{
						for (int x = startX; x < startX + size; x++)
						{
							placeTile(x, y);
						}
					}
					break;
				}
				default:
				{
					int rugW = std::max(3, room.w / 3);
					int rugH = std::max(3, room.h / 3);
					int startX = cx - rugW / 2;
					int startY = cy - rugH / 2;
					for (int y = startY; y < startY + rugH; y++)
					{
						for (int x = startX; x < startX + rugW; x++)
						{
							placeTile(x, y);
						}
					}
					break;
				}
			}
		};

		// Places carpet decals inside wooden rooms.
		auto placeCarpetsInRoom = [&](const Rect &room, const std::vector<glm::ivec2> &doors)
		{
			auto isWoodTile = [&](int x, int y)
			{
				return map.firstLayer.getBlockUnsafe(x, y).type == Blocks::woodenFloor;
			};

		auto applyCarpetTile = [&](int x, int y)
		{
			auto &base = map.firstLayer.getBlockUnsafe(x, y);
			base.type = Blocks::carpetFloor;
			//auto &over = map.secondLayer.getBlockUnsafe(x, y);
			//over.type = Blocks::carpetFloor;
		};

			placeRoomPattern(room, doors, isWoodTile, applyCarpetTile, cosmetics.woodRoomCarpetRoadChance);
		};

		// Places damaged wooden planks inside cave rooms.
		auto placeDamagedWoodInCaveRoom = [&](const Rect &room, const std::vector<glm::ivec2> &doors)
		{
			if (!getRandomChance(rng, cosmetics.caveRoomWoodFloorChance)) { return; }

			auto isCaveTile = [&](int x, int y)
			{
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				return base.type == Blocks::caveFloor || base.type == Blocks::floor2;
			};

			auto nearestDoorDist = [&](int x, int y)
			{
				int best = INT_MAX;
				for (auto d : doors)
				{
					for (int dy = 0; dy <= 1; dy++)
					{
						for (int dx = 0; dx <= 1; dx++)
						{
							int dist = std::abs((d.x + dx) - x) + std::abs((d.y + dy) - y);
							if (dist < best) { best = dist; }
						}
					}
				}
				return best;
			};

			auto woodDamageChance = [&](int x, int y)
			{
				float chance = cosmetics.caveRoomWoodDamageBase;
				if (!doors.empty())
				{
					int dist = nearestDoorDist(x, y);
					if (dist <= 1)
					{
						return 1.0f;
					}
					float radius = (float)std::max(1, cosmetics.caveRoomWoodDamageRadius);
					float t = 1.0f - std::clamp(dist / radius, 0.0f, 1.0f);
					chance += t * cosmetics.caveRoomWoodDamageDoorBoost;
				}
				return std::clamp(chance, 0.0f, 1.0f);
			};

		auto applyWoodTile = [&](int x, int y)
		{
			if (getRandomChance(rng, woodDamageChance(x, y))) { return; }
			auto &base = map.firstLayer.getBlockUnsafe(x, y);
			base.type = Blocks::woodenFloor;
		};

			placeRoomPattern(room, doors, isCaveTile, applyWoodTile, cosmetics.woodRoomCarpetRoadChance);
		};

		// Converts the surrounding walls of a wooden room to wooden walls.
		auto paintWoodRoomWalls = [&](const Rect &room)
		{
			int minX = std::max(1, room.x - 1);
			int maxX = std::min(sizeX - 2, room.x2());
			int minY = std::max(1, room.y - 1);
			int maxY = std::min(sizeY - 2, room.y2());

			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					if (x >= room.x && x < room.x2() && y >= room.y && y < room.y2())
					{
						continue;
					}
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::dungeonWall)
					{
						b.type = Blocks::woodenWall;
					}
				}
			}
		};

		// Adds a few trees to grassy rooms.
		auto isDoorNearbyLocal = [&](const std::vector<glm::ivec2> &doors, int x, int y, int dist)
		{
			for (auto d : doors)
			{
				for (int dy = 0; dy <= 1; dy++)
				{
					for (int dx = 0; dx <= 1; dx++)
					{
						int distValue = std::abs((d.x + dx) - x) + std::abs((d.y + dy) - y);
						if (distValue <= dist)
						{
							return true;
						}
					}
				}
			}
			return false;
		};

		auto placeTreesInRoom = [&](const Rect &room, const std::vector<glm::ivec2> &doors)
		{
			if (!getRandomChance(rng, cosmetics.grassRoomTreeChance)) { return; }
			int area = room.w * room.h;
			int maxTrees = std::clamp(area / cosmetics.grassRoomTreeAreaDiv,
				cosmetics.grassRoomTreeMin, cosmetics.grassRoomTreeMax);
			int treeCount = getRandomInt(rng, cosmetics.grassRoomTreeMin, maxTrees);
			for (int i = 0; i < treeCount; i++)
			{
				bool placed = false;
				for (int attempt = 0; attempt < 10; attempt++)
				{
					int x = getRandomInt(rng, room.x + 1, room.x2() - 2);
					int y = getRandomInt(rng, room.y + 1, room.y2() - 2);
					if (isDoorNearbyLocal(doors, x, y, 2)) { continue; }
					auto &base = map.firstLayer.getBlockUnsafe(x, y);
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					if (over.type != Blocks::none) { continue; }
					if (base.type == Blocks::grass || base.type == Blocks::grassDecoration
						|| base.type == Blocks::grassDecorationStones
						|| base.type == Blocks::grassDecorationFlowers
						|| base.type == Blocks::grassDecorationMushrooms)
					{
						over.type = Blocks::smallTree;
						placed = true;
						break;
					}
				}
				if (!placed) { break; }
			}
		};

		// Slightly thickens dirt blobs inside grass rooms.
		auto expandGrassRoomDirtPatches = [&](const Rect &room)
		{
			if (cosmetics.grassRoomDirtExpandChance <= 0.0f) { return; }
			const int offsets[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
			std::vector<glm::ivec2> toDirt;
			for (int y = room.y + 1; y < room.y2() - 1; y++)
			{
				for (int x = room.x + 1; x < room.x2() - 1; x++)
				{
					auto &base = map.firstLayer.getBlockUnsafe(x, y);
					if (base.type != Blocks::dirt && base.type != Blocks::dirtDecoration) { continue; }
					for (auto &o : offsets)
					{
						int nx = x + o[0];
						int ny = y + o[1];
						if (nx < room.x + 1 || nx >= room.x2() - 1 || ny < room.y + 1 || ny >= room.y2() - 1)
						{
							continue;
						}
						auto &nb = map.firstLayer.getBlockUnsafe(nx, ny);
						if (nb.type == Blocks::grass || nb.type == Blocks::grassDecoration
							|| nb.type == Blocks::grassDecorationStones
							|| nb.type == Blocks::grassDecorationFlowers
							|| nb.type == Blocks::grassDecorationMushrooms)
						{
							if (getRandomChance(rng, cosmetics.grassRoomDirtExpandChance))
							{
								toDirt.push_back({nx, ny});
							}
						}
					}
				}
			}

			for (auto pos : toDirt)
			{
				auto &tile = map.firstLayer.getBlockUnsafe(pos.x, pos.y);
				if (tile.type == Blocks::grass || tile.type == Blocks::grassDecoration
					|| tile.type == Blocks::grassDecorationStones
					|| tile.type == Blocks::grassDecorationFlowers
					|| tile.type == Blocks::grassDecorationMushrooms)
				{
					tile.type = Blocks::dirt;
				}
			}
		};

		// Organic dirt roads that connect corridor exits inside a grass room.
		auto carveGrassRoomRoads = [&](int roomIndex, const Rect &room)
		{
			if (!getRandomChance(rng, cosmetics.grassRoomRoadChance)) { return; }

			std::vector<glm::ivec2> exits;
			exits.reserve(6);

			auto addExit = [&](int x, int y)
			{
				for (auto e : exits)
				{
					if (e.x == x && e.y == y) { return; }
				}
				exits.push_back({x, y});
			};

			if (roomIndex >= 0 && roomIndex < (int)outInfo.rooms.size())
			{
				for (auto d : outInfo.rooms[roomIndex].doorPositions)
				{
					addExit(d.x, d.y);
				}
			}

			if (exits.size() < 2)
			{
				for (int x = room.x; x < room.x2(); x++)
				{
					if (room.y > 1)
					{
						auto &outside = map.firstLayer.getBlockUnsafe(x, room.y - 1);
						if (outside.type == Blocks::floor1 || outside.type == Blocks::floor2)
						{
							addExit(x, room.y);
						}
					}
					if (room.y2() < sizeY - 1)
					{
						auto &outside = map.firstLayer.getBlockUnsafe(x, room.y2());
						if (outside.type == Blocks::floor1 || outside.type == Blocks::floor2)
						{
							addExit(x, room.y2() - 1);
						}
					}
				}

				for (int y = room.y; y < room.y2(); y++)
				{
					if (room.x > 1)
					{
						auto &outside = map.firstLayer.getBlockUnsafe(room.x - 1, y);
						if (outside.type == Blocks::floor1 || outside.type == Blocks::floor2)
						{
							addExit(room.x, y);
						}
					}
					if (room.x2() < sizeX - 1)
					{
						auto &outside = map.firstLayer.getBlockUnsafe(room.x2(), y);
						if (outside.type == Blocks::floor1 || outside.type == Blocks::floor2)
						{
							addExit(room.x2() - 1, y);
						}
					}
				}
			}

			if (exits.size() < 2) { return; }
			std::shuffle(exits.begin(), exits.end(), rng);

			auto paintRoadTile = [&](int x, int y)
			{
				if (x < room.x || x >= room.x2() || y < room.y || y >= room.y2()) { return; }
				auto &b = map.firstLayer.getBlockUnsafe(x, y);
				if (b.type == Blocks::grass || b.type == Blocks::grassDecoration
					|| b.type == Blocks::grassDecorationStones
					|| b.type == Blocks::grassDecorationFlowers
					|| b.type == Blocks::grassDecorationMushrooms
					|| b.type == Blocks::dirt || b.type == Blocks::dirtDecoration)
				{
					b.type = Blocks::dirt;
					if (getRandomChance(rng, cosmetics.roadDirtDecorChance))
					{
						b.type = Blocks::dirtDecoration;
					}
				}
			};

			auto paintRoadWidth = [&](glm::ivec2 pos, glm::ivec2 dir)
			{
				paintRoadTile(pos.x, pos.y);
				if (dir.x != 0)
				{
					int side = 0;
					if (pos.y + 1 < room.y2() && pos.y - 1 >= room.y)
					{
						side = getRandomChance(rng, 0.5f) ? 1 : -1;
					}
					else
					{
						side = (pos.y + 1 < room.y2()) ? 1 : -1;
					}
					paintRoadTile(pos.x, pos.y + side);
				}
				else if (dir.y != 0)
				{
					int side = 0;
					if (pos.x + 1 < room.x2() && pos.x - 1 >= room.x)
					{
						side = getRandomChance(rng, 0.5f) ? 1 : -1;
					}
					else
					{
						side = (pos.x + 1 < room.x2()) ? 1 : -1;
					}
					paintRoadTile(pos.x + side, pos.y);
				}
			};

			auto carveRoad = [&](glm::ivec2 start, glm::ivec2 end)
			{
				glm::ivec2 current = start;
				glm::ivec2 lastDir = {};
				int steps = 0;
				int maxSteps = room.w * room.h * 2;

				int dx0 = end.x - current.x;
				int dy0 = end.y - current.y;
				glm::ivec2 firstDir = (std::abs(dx0) >= std::abs(dy0))
					? glm::ivec2{(dx0 > 0 ? 1 : (dx0 < 0 ? -1 : 0)), 0}
					: glm::ivec2{0, (dy0 > 0 ? 1 : (dy0 < 0 ? -1 : 0))};
				paintRoadWidth(current, firstDir);

				while (current != end && steps < maxSteps)
				{
					steps++;
					int dx = end.x - current.x;
					int dy = end.y - current.y;
					if (dx == 0 && dy == 0) { break; }

					glm::ivec2 primary = (std::abs(dx) >= std::abs(dy))
						? glm::ivec2{(dx > 0 ? 1 : -1), 0}
						: glm::ivec2{0, (dy > 0 ? 1 : -1)};
					glm::ivec2 secondary = (primary.x != 0)
						? glm::ivec2{0, (dy > 0 ? 1 : (dy < 0 ? -1 : 0))}
						: glm::ivec2{(dx > 0 ? 1 : (dx < 0 ? -1 : 0)), 0};

					glm::ivec2 dir = primary;
					if (secondary != glm::ivec2{} && getRandomChance(rng, 0.35f))
					{
						dir = secondary;
					}
					else if (lastDir != glm::ivec2{} && getRandomChance(rng, 0.2f))
					{
						dir = lastDir;
					}

					glm::ivec2 next = current + dir;
					if (next.x < room.x || next.x >= room.x2() || next.y < room.y || next.y >= room.y2())
					{
						glm::ivec2 alt = (dir.x != 0)
							? glm::ivec2{0, (dy > 0 ? 1 : (dy < 0 ? -1 : 0))}
							: glm::ivec2{(dx > 0 ? 1 : (dx < 0 ? -1 : 0)), 0};
						next = current + alt;
						if (alt == glm::ivec2{} || next.x < room.x || next.x >= room.x2()
							|| next.y < room.y || next.y >= room.y2())
						{
							break;
						}
						dir = alt;
					}

					current = next;
					paintRoadWidth(current, dir);
					lastDir = dir;
				}
			};

			for (int i = 1; i < (int)exits.size(); i++)
			{
				carveRoad(exits[i - 1], exits[i]);
			}
		};

		enum CorridorStyle
		{
			Corridor_Dungeon = 0,
			Corridor_Cave = 1,
			Corridor_Wood = 2
		};

		struct CorridorLink
		{
			glm::ivec2 from = {};
			glm::ivec2 to = {};
			int style = Corridor_Dungeon;
		};

		auto findRoomIndexAt = [&](int x, int y)
		{
			for (int i = 0; i < (int)rooms.size(); i++)
			{
				const Rect &room = rooms[i];
				if (x >= room.x && x < room.x2() && y >= room.y && y < room.y2())
				{
					return i;
				}
			}
			return -1;
		};

		auto isDoorTileForRoom = [&](int roomIndex, int x, int y)
		{
			if (roomIndex < 0 || roomIndex >= (int)outInfo.rooms.size()) { return false; }
			for (auto d : outInfo.rooms[roomIndex].doorPositions)
			{
				if (x >= d.x && x <= d.x + 1 && y >= d.y && y <= d.y + 1)
				{
					return true;
				}
			}
			return false;
		};



		auto paintCorridorTile = [&](int x, int y, int style)
		{
			if (x < 0 || y < 0 || x >= sizeX || y >= sizeY) { return; }
			auto &b = map.firstLayer.getBlockUnsafe(x, y);
			if (!isWall(b.type) && b.type != Blocks::none)
			{
				return;
			}

			auto applyWoodCorridorWalls = [&](int cx, int cy)
			{
				const int offsets[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
				for (auto &o : offsets)
				{
					int nx = cx + o[0];
					int ny = cy + o[1];
					if (nx < 0 || ny < 0 || nx >= sizeX || ny >= sizeY) { continue; }
					auto &nb = map.firstLayer.getBlockUnsafe(nx, ny);
					if (nb.type == Blocks::dungeonWall)
					{
						nb.type = Blocks::woodenWall;
					}
				}
			};

			switch (style)
			{
				case Corridor_Cave:
					b.type = Blocks::caveFloor;
					break;
				case Corridor_Wood:
					b.type = Blocks::woodenFloor;
					applyWoodCorridorWalls(x, y);
					break;
				default:
					b.type = Blocks::floor1;
					if (getRandomFloat(rng, 0.0f, 1.0f) < cosmetics.floor2Chance)
					{
						b.type = Blocks::floor2;
					}
					break;
			}
		};

		auto carveDoorOpening = [&](glm::ivec2 pos, int style)
		{
			for (int y = pos.y; y <= pos.y + 1; y++)
			{
				for (int x = pos.x; x <= pos.x + 1; x++)
				{
					paintCorridorTile(x, y, style);
				}
			}
		};

		auto carveCorridorWithStyle = [&](glm::ivec2 from, glm::ivec2 to, int style)
		{
			int width = 2;
			auto canCarveCorridorTile = [&](int x, int y)
			{
				int roomIndex = findRoomIndexAt(x, y);
				if (roomIndex < 0) { return true; }
				if (!rooms[roomIndex].isCave) { return true; }
				return isDoorTileForRoom(roomIndex, x, y);
			};
			auto carveLine = [&](glm::ivec2 start, glm::ivec2 end)
			{
				if (start.x == end.x)
				{
					int y0 = std::min(start.y, end.y);
					int y1 = std::max(start.y, end.y);
					for (int y = y0; y <= y1; y++)
					{
						for (int x = start.x; x < start.x + width; x++)
						{
							if (!canCarveCorridorTile(x, y)) { continue; }
							paintCorridorTile(x, y, style);
						}
					}
				}
				else
				{
					int x0 = std::min(start.x, end.x);
					int x1 = std::max(start.x, end.x);
					for (int x = x0; x <= x1; x++)
					{
						for (int y = start.y; y < start.y + width; y++)
						{
							if (!canCarveCorridorTile(x, y)) { continue; }
							paintCorridorTile(x, y, style);
						}
					}
				}
			};

			if (getRandomChance(rng, 0.6f))
			{
				carveLine(from, {to.x, from.y});
				carveLine({to.x, from.y}, to);
			}
			else
			{
				carveLine(from, {from.x, to.y});
				carveLine({from.x, to.y}, to);
			}
		};

		auto carveCorridor = [&](glm::ivec2 from, glm::ivec2 to)
		{
			carveCorridorWithStyle(from, to, Corridor_Dungeon);
		};

		auto carveMazeCorridorWithStyle = [&](glm::ivec2 from, glm::ivec2 to, int style)
		{
			glm::ivec2 current = from;
			int steps = 0;
			while (current != to && steps < sizeX * sizeY)
			{
				steps++;
				int dx = to.x - current.x;
				int dy = to.y - current.y;
				glm::ivec2 next = current;

				if (getRandomChance(rng, 0.4f))
				{
					if (std::abs(dx) > std::abs(dy))
					{
						next.x += (dx > 0 ? 1 : -1);
					}
					else
					{
						next.y += (dy > 0 ? 1 : -1);
					}
				}
				else
				{
					if (getRandomChance(rng, 0.5f))
					{
						next.x += (dx >= 0 ? 1 : -1);
					}
					else
					{
						next.y += (dy >= 0 ? 1 : -1);
					}
				}

				next.x = std::clamp(next.x, 2, sizeX - 3);
				next.y = std::clamp(next.y, 2, sizeY - 3);

				carveCorridorWithStyle(current, next, style);
				current = next;
			}
		};

		auto carveMazeCorridor = [&](glm::ivec2 from, glm::ivec2 to)
		{
			carveMazeCorridorWithStyle(from, to, Corridor_Dungeon);
		};

		for (int i = 0; i < attempts; i++)
		{
			bool isCave = getRandomChance(rng, cosmetics.caveRoomChance);
			bool isCaveMaze = false;
			bool isGrassRoom = false;
			bool isWoodRoom = false;
			bool isBigRoom = false;
			if (!isCave)
			{
				float typeRoll = getRandomFloat(rng, 0.0f, 1.0f);
				if (typeRoll < cosmetics.woodRoomChance)
				{
					isWoodRoom = true;
				}
				else if (typeRoll < cosmetics.woodRoomChance + cosmetics.grassRoomChance)
				{
					isGrassRoom = true;
				}
			}
			else
			{
				isCaveMaze = getRandomChance(rng, cosmetics.caveMazeRoomChance);
			}
			int w = randRange(minRoomSize, maxRoomSize);
			int h = randRange(minRoomSize, maxRoomSize);
			int caveRadius = 0;

			if (isCave)
			{
				if (isCaveMaze)
				{
					caveRadius = randRange(cosmetics.caveMazeRoomRadiusMin, cosmetics.caveMazeRoomRadiusMax);
					int caveExtent = caveRadius + cosmetics.caveMazeRoomExtentPadding;
					w = caveExtent * 2 + 1;
					h = caveExtent * 2 + 1;
				}
				else
				{
					caveRadius = randRange(cosmetics.caveRoomRadiusMin, cosmetics.caveRoomRadiusMax);
					int caveExtent = caveRadius + cosmetics.caveRoomExtentPadding;
					w = caveExtent * 2 + 1;
					h = caveExtent * 2 + 1;
				}
			}
			else
			{
				int roomType = getRandomInt(rng, 0, 4);
				if (roomType == 1)
				{
					w = randRange(14, 22);
					h = randRange(12, 16);
				}
				else if (roomType == 2)
				{
					w = randRange(12, 16);
					h = randRange(14, 22);
				}
				else if (roomType == 3)
				{
					w = randRange(16, 24);
					h = randRange(12, 14);
				}
				else if (roomType == 4)
				{
					w = randRange(12, 18);
					h = randRange(12, 18);
				}
			}

			if (!isCave && !isGrassRoom && !isWoodRoom && getRandomChance(rng, cosmetics.bigRoomChance))
			{
				w = randRange(cosmetics.bigRoomMinSize, cosmetics.bigRoomMaxSize);
				h = randRange(cosmetics.bigRoomMinSize, cosmetics.bigRoomMaxSize);
				isBigRoom = true;
			}

			if (w >= sizeX - 4 || h >= sizeY - 4)
			{
				continue;
			}

			Rect room;
			room.x = randRange(2, sizeX - w - 2);
			room.y = randRange(2, sizeY - h - 2);
			room.w = w;
			room.h = h;
			room.isCave = isCave;
			room.isCaveMaze = isCaveMaze;
			room.caveRadius = caveRadius;
			room.isGrassRoom = isGrassRoom;
			room.isWoodRoom = isWoodRoom;
			room.isBigRoom = (!room.isCave && !room.isGrassRoom && !room.isWoodRoom)
				&& (isBigRoom || (room.w * room.h >= cosmetics.bigRoomAreaThreshold));

			bool valid = true;
			for (const auto &other : rooms)
			{
				if (overlaps(room, other))
				{
					valid = false;
					break;
				}
			}

			if (!valid)
			{
				continue;
			}

			rooms.push_back(room);
			if (room.isCave)
			{
				carveCaveRoom(room);
			}
			else
			{
				carveRoom(room);
				// room setpieces are placed after corridors for door awareness
			}
		}

		outInfo.rooms.reserve(rooms.size());
		for (const auto &room : rooms)
		{
			FloorRoom outRoom;
			outRoom.pos = {room.x, room.y};
			outRoom.size = {room.w, room.h};
			outRoom.isBigRoom = room.isBigRoom;
			outRoom.isEmptyRoom = false;
			outInfo.rooms.push_back(std::move(outRoom));
		}

		if (createASpawnRoom && !rooms.empty())
		{
			int spawnIndex = 0;
			int bestArea = -1;
			for (int i = 0; i < (int)rooms.size(); i++)
			{
				int area = rooms[i].w * rooms[i].h;
				if (area > bestArea)
				{
					bestArea = area;
					spawnIndex = i;
				}
			}

			outInfo.spawnRoomIndex = spawnIndex;
			outInfo.rooms[spawnIndex].isSpawnRoom = true;
		}

		std::vector<int> roomConnections(rooms.size(), 0);
		std::vector<std::vector<char>> roomLinks(rooms.size(),
			std::vector<char>(rooms.size(), 0));
		std::vector<CorridorLink> corridorLinks;
		corridorLinks.reserve(rooms.size() * 2);

		auto registerDoor = [&](int roomIndex, glm::ivec2 pos)
		{
			if (roomIndex < 0 || roomIndex >= (int)outInfo.rooms.size()) { return; }
			auto &doors = outInfo.rooms[roomIndex].doorPositions;
			for (auto d : doors)
			{
				if (d == pos) { return; }
			}
			doors.push_back(pos);
		};

		auto snapDoorToEdge = [&](int roomIndex, glm::ivec2 pos, glm::ivec2 targetCenter)
		{
			const Rect &room = rooms[roomIndex];
			glm::ivec2 center = room.center();
			int dx = targetCenter.x - center.x;
			int dy = targetCenter.y - center.y;
			if (std::abs(dx) >= std::abs(dy))
			{
				pos.x = (dx >= 0) ? (room.x2() - 2) : room.x;
				pos.y = std::clamp(pos.y, room.y, room.y2() - 2);
			}
			else
			{
				pos.y = (dy >= 0) ? (room.y2() - 2) : room.y;
				pos.x = std::clamp(pos.x, room.x, room.x2() - 2);
			}
			return pos;
		};

		auto getDoorOutsidePos = [&](int roomIndex, glm::ivec2 doorPos)
		{
			const Rect &room = rooms[roomIndex];
			if (doorPos.y == room.y)
			{
				return glm::ivec2{doorPos.x, doorPos.y - 1};
			}
			if (doorPos.y == room.y2() - 2)
			{
				return glm::ivec2{doorPos.x, doorPos.y + 2};
			}
			if (doorPos.x == room.x)
			{
				return glm::ivec2{doorPos.x - 1, doorPos.y};
			}
			if (doorPos.x == room.x2() - 2)
			{
				return glm::ivec2{doorPos.x + 2, doorPos.y};
			}
			return doorPos;
		};


		auto pickDoorPos = [&](int roomIndex, glm::ivec2 targetCenter)
		{
			const Rect &room = rooms[roomIndex];
			glm::ivec2 center = room.center();
			int dx = targetCenter.x - center.x;
			int dy = targetCenter.y - center.y;
			glm::ivec2 dir = (std::abs(dx) >= std::abs(dy))
				? glm::ivec2{(dx >= 0 ? 1 : -1), 0}
				: glm::ivec2{0, (dy >= 0 ? 1 : -1)};

			auto clampDoorTopLeft = [&](glm::ivec2 pos)
			{
				pos.x = std::clamp(pos.x, room.x, room.x2() - 2);
				pos.y = std::clamp(pos.y, room.y, room.y2() - 2);
				return pos;
			};

			glm::ivec2 best = center;
			int tries = 6;
			for (int i = 0; i < tries; i++)
			{
				if (dir.x != 0)
				{
					int x = dir.x > 0 ? room.x2() - 2 : room.x;
					int offset = getRandomInt(rng, -room.h / 3, room.h / 3);
					int y = std::clamp(center.y + offset, room.y, room.y2() - 2);
					best = {x, y};
				}
				else
				{
					int y = dir.y > 0 ? room.y2() - 2 : room.y;
					int offset = getRandomInt(rng, -room.w / 3, room.w / 3);
					int x = std::clamp(center.x + offset, room.x, room.x2() - 2);
					best = {x, y};
				}

				best = clampDoorTopLeft(best);

				bool occupied = false;
				for (auto d : outInfo.rooms[roomIndex].doorPositions)
				{
					if (std::abs(d.x - best.x) + std::abs(d.y - best.y) < 3)
					{
						occupied = true;
						break;
					}
				}
				if (!occupied) { break; }
			}

			return best;
		};


		auto linkRooms = [&](int a, int b, bool allowMaze)
		{
			if (a == b) { return; }
			if (roomLinks[a][b]) { return; }
			int style = Corridor_Dungeon;
			if (rooms[a].isCave || rooms[b].isCave)
			{
				style = Corridor_Cave;
			}
			else if (rooms[a].isWoodRoom || rooms[b].isWoodRoom)
			{
				style = Corridor_Wood;
			}

			glm::ivec2 doorA = pickDoorPos(a, rooms[b].center());
			glm::ivec2 doorB = pickDoorPos(b, rooms[a].center());
			doorA = snapDoorToEdge(a, doorA, rooms[b].center());
			doorB = snapDoorToEdge(b, doorB, rooms[a].center());
			registerDoor(a, doorA);
			registerDoor(b, doorB);
			carveDoorOpening(doorA, style);
			carveDoorOpening(doorB, style);
			glm::ivec2 outA = getDoorOutsidePos(a, doorA);
			glm::ivec2 outB = getDoorOutsidePos(b, doorB);
			if (outA != doorA)
			{
				carveCorridorWithStyle(doorA, outA, style);
				corridorLinks.push_back({doorA, outA, style});
			}
			if (outB != doorB)
			{
				carveCorridorWithStyle(doorB, outB, style);
				corridorLinks.push_back({doorB, outB, style});
			}

			if (allowMaze && getRandomChance(rng, 0.15f))
			{
				carveMazeCorridorWithStyle(outA, outB, style);
			}
			else
			{
				carveCorridorWithStyle(outA, outB, style);
			}
			corridorLinks.push_back({outA, outB, style});

			roomConnections[a]++;
			roomConnections[b]++;
			roomLinks[a][b] = 1;
			roomLinks[b][a] = 1;
		};

		// Gungeon-style room setpieces: cover walls, pillars, and split rooms.
		auto isDoorNearby = [&](int roomIndex, int x, int y, int dist)
		{
			if (roomIndex < 0 || roomIndex >= (int)outInfo.rooms.size()) { return false; }
			for (auto d : outInfo.rooms[roomIndex].doorPositions)
			{
				for (int dy = 0; dy <= 1; dy++)
				{
					for (int dx = 0; dx <= 1; dx++)
					{
						int distValue = std::abs((d.x + dx) - x) + std::abs((d.y + dy) - y);
						if (distValue <= dist)
						{
							return true;
						}
					}
				}
			}
			return false;
		};

		auto applyRoomSetpiece = [&](int roomIndex)
		{
			const Rect &room = rooms[roomIndex];
			if (room.isCave || room.isGrassRoom || room.isBigRoom) { return; }
			if (!getRandomChance(rng, 0.7f)) { return; }

			int left = room.x + 2;
			int right = room.x2() - 3;
			int top = room.y + 2;
			int bottom = room.y2() - 3;
			if (left > right || top > bottom) { return; }
			BlockType innerWallType = room.isWoodRoom ? Blocks::woodenWall : Blocks::dungeonWall;

			auto placeWall = [&](int x, int y)
			{
				if (x < left || x > right || y < top || y > bottom) { return; }
				if (isDoorNearby(roomIndex, x, y, 2)) { return; }
				auto &b = map.firstLayer.getBlockUnsafe(x, y);
				if (b.type == Blocks::floor1 || b.type == Blocks::floor2)
				{
					// TODO: replace some inner walls with decorative walls later.
					b.type = innerWallType;
				}
			};

			auto placeWallRect = [&](int x0, int y0, int w, int h)
			{
				for (int y = y0; y < y0 + h; y++)
				{
					for (int x = x0; x < x0 + w; x++)
					{
						placeWall(x, y);
					}
				}
			};

			glm::ivec2 center = room.center();
			int pattern = getRandomInt(rng, 0, 7);
			switch (pattern)
			{
				case 0:
				{
					bool horizontal = (room.w >= room.h)
						? getRandomChance(rng, 0.7f)
						: getRandomChance(rng, 0.3f);
					int inset = std::max(1, (right - left) / 8);
					if (horizontal)
					{
						int y = std::clamp(center.y + getRandomInt(rng, -1, 1), top, bottom);
						for (int x = left + inset; x <= right - inset; x++)
						{
							if (std::abs(x - center.x) <= 1) { continue; }
							placeWall(x, y);
						}
					}
					else
					{
						int x = std::clamp(center.x + getRandomInt(rng, -1, 1), left, right);
						for (int y = top + inset; y <= bottom - inset; y++)
						{
							if (std::abs(y - center.y) <= 1) { continue; }
							placeWall(x, y);
						}
					}
					break;
				}
				case 1:
				{
					bool horizontal = room.w >= room.h;
					int offset = std::max(1, std::min(2, (horizontal ? (bottom - top) : (right - left)) / 3));
					int len = (horizontal ? (right - left) : (bottom - top)) - 2;
					len = std::max(4, len);
					if (horizontal)
					{
						int y1 = std::clamp(center.y - offset, top, bottom);
						int y2 = std::clamp(center.y + offset, top, bottom);
						if (y1 == y2) { y2 = std::min(bottom, y1 + 1); }
						int startX = std::clamp(center.x - len / 2, left, right - 1);
						int endX = std::clamp(startX + len, left, right);
						for (int x = startX; x <= endX; x++)
						{
							placeWall(x, y1);
							placeWall(x, y2);
						}
					}
					else
					{
						int x1 = std::clamp(center.x - offset, left, right);
						int x2 = std::clamp(center.x + offset, left, right);
						if (x1 == x2) { x2 = std::min(right, x1 + 1); }
						int startY = std::clamp(center.y - len / 2, top, bottom - 1);
						int endY = std::clamp(startY + len, top, bottom);
						for (int y = startY; y <= endY; y++)
						{
							placeWall(x1, y);
							placeWall(x2, y);
						}
					}
					break;
				}
				case 2:
				{
					int lenX = std::min(6, right - left + 1);
					int lenY = std::min(6, bottom - top + 1);
					int startX = std::clamp(center.x + getRandomInt(rng, -2, 2), left, right);
					int startY = std::clamp(center.y + getRandomInt(rng, -2, 2), top, bottom);
					int dirX = getRandomChance(rng, 0.5f) ? 1 : -1;
					int dirY = getRandomChance(rng, 0.5f) ? 1 : -1;
					for (int i = 0; i < lenX; i++)
					{
						placeWall(startX + i * dirX, startY);
					}
					for (int i = 0; i < lenY; i++)
					{
						placeWall(startX, startY + i * dirY);
					}
					break;
				}
				case 3:
				{
					int dx = std::max(2, (right - left) / 3);
					int dy = std::max(2, (bottom - top) / 3);
					int px1 = std::clamp(left + dx, left, right);
					int px2 = std::clamp(right - dx, left, right);
					int py1 = std::clamp(top + dy, top, bottom);
					int py2 = std::clamp(bottom - dy, top, bottom);
					placeWallRect(px1 - 1, py1 - 1, 2, 2);
					placeWallRect(px2 - 1, py1 - 1, 2, 2);
					placeWallRect(px1 - 1, py2 - 1, 2, 2);
					placeWallRect(px2 - 1, py2 - 1, 2, 2);
					break;
				}
				case 4:
				{
					// Inner ring with small openings for cover lanes.
					int ringInsetX = std::max(2, (right - left) / 4);
					int ringInsetY = std::max(2, (bottom - top) / 4);
					int ringLeft = std::clamp(left + ringInsetX, left, right);
					int ringRight = std::clamp(right - ringInsetX, left, right);
					int ringTop = std::clamp(top + ringInsetY, top, bottom);
					int ringBottom = std::clamp(bottom - ringInsetY, top, bottom);
					if (ringLeft + 2 <= ringRight && ringTop + 2 <= ringBottom)
					{
						for (int x = ringLeft; x <= ringRight; x++)
						{
							if (std::abs(x - center.x) <= 1) { continue; }
							placeWall(x, ringTop);
							placeWall(x, ringBottom);
						}
						for (int y = ringTop; y <= ringBottom; y++)
						{
							if (std::abs(y - center.y) <= 1) { continue; }
							placeWall(ringLeft, y);
							placeWall(ringRight, y);
						}
					}
					else
					{
						int size = std::min(3, std::min(right - left + 1, bottom - top + 1));
						placeWallRect(center.x - size / 2, center.y - size / 2, size, size);
					}
					break;
				}
				case 5:
				{
					// U-shaped bunker for hard cover.
					int inset = std::max(1, std::min((right - left) / 4, (bottom - top) / 4));
					int uLeft = std::clamp(left + inset, left, right);
					int uRight = std::clamp(right - inset, left, right);
					int uTop = std::clamp(top + inset, top, bottom);
					int uBottom = std::clamp(bottom - inset, top, bottom);
					if (uLeft + 2 <= uRight && uTop + 2 <= uBottom)
					{
						int orientation = getRandomInt(rng, 0, 3);
						if (orientation == 0)
						{
							for (int y = uTop; y <= uBottom; y++)
							{
								placeWall(uLeft, y);
								placeWall(uRight, y);
							}
							for (int x = uLeft; x <= uRight; x++)
							{
								placeWall(x, uBottom);
							}
						}
						else if (orientation == 1)
						{
							for (int y = uTop; y <= uBottom; y++)
							{
								placeWall(uLeft, y);
								placeWall(uRight, y);
							}
							for (int x = uLeft; x <= uRight; x++)
							{
								placeWall(x, uTop);
							}
						}
						else if (orientation == 2)
						{
							for (int x = uLeft; x <= uRight; x++)
							{
								placeWall(x, uTop);
								placeWall(x, uBottom);
							}
							for (int y = uTop; y <= uBottom; y++)
							{
								placeWall(uRight, y);
							}
						}
						else
						{
							for (int x = uLeft; x <= uRight; x++)
							{
								placeWall(x, uTop);
								placeWall(x, uBottom);
							}
							for (int y = uTop; y <= uBottom; y++)
							{
								placeWall(uLeft, y);
							}
						}
					}
					break;
				}
				case 6:
				{
					// Four offset cover islands.
					int blockSize = std::min(3, std::min(right - left + 1, bottom - top + 1));
					int offsetX = std::max(2, (right - left) / 4);
					int offsetY = std::max(2, (bottom - top) / 4);
					placeWallRect(center.x - offsetX - blockSize / 2, center.y - offsetY - blockSize / 2,
						blockSize, blockSize);
					placeWallRect(center.x + offsetX - blockSize / 2, center.y - offsetY - blockSize / 2,
						blockSize, blockSize);
					placeWallRect(center.x - offsetX - blockSize / 2, center.y + offsetY - blockSize / 2,
						blockSize, blockSize);
					placeWallRect(center.x + offsetX - blockSize / 2, center.y + offsetY - blockSize / 2,
						blockSize, blockSize);
					break;
				}
				case 7:
				{
					// T-junction wall for choke points.
					bool barHorizontal = getRandomChance(rng, 0.5f);
					if (barHorizontal)
					{
						int barLen = std::max(4, (right - left) / 2);
						int barY = std::clamp(center.y + getRandomInt(rng, -2, 2), top + 1, bottom - 1);
						int barStartX = std::clamp(center.x - barLen / 2, left, right);
						int barEndX = std::clamp(barStartX + barLen, left, right);
						for (int x = barStartX; x <= barEndX; x++)
						{
							placeWall(x, barY);
						}

						bool stemDown = getRandomChance(rng, 0.5f);
						int stemX = std::clamp(center.x + getRandomInt(rng, -1, 1), barStartX, barEndX);
						int stemStart = stemDown ? barY + 1 : barY - 1;
						int stemEnd = stemDown ? (bottom - 1) : (top + 1);
						int step = stemDown ? 1 : -1;
						for (int y = stemStart; stemDown ? (y <= stemEnd) : (y >= stemEnd); y += step)
						{
							placeWall(stemX, y);
						}
					}
					else
					{
						int barLen = std::max(4, (bottom - top) / 2);
						int barX = std::clamp(center.x + getRandomInt(rng, -2, 2), left + 1, right - 1);
						int barStartY = std::clamp(center.y - barLen / 2, top, bottom);
						int barEndY = std::clamp(barStartY + barLen, top, bottom);
						for (int y = barStartY; y <= barEndY; y++)
						{
							placeWall(barX, y);
						}

						bool stemRight = getRandomChance(rng, 0.5f);
						int stemY = std::clamp(center.y + getRandomInt(rng, -1, 1), barStartY, barEndY);
						int stemStart = stemRight ? barX + 1 : barX - 1;
						int stemEnd = stemRight ? (right - 1) : (left + 1);
						int step = stemRight ? 1 : -1;
						for (int x = stemStart; stemRight ? (x <= stemEnd) : (x >= stemEnd); x += step)
						{
							placeWall(x, stemY);
						}
					}
					break;
				}
				default:
				{
					int size = std::min(3, std::min(right - left + 1, bottom - top + 1));
					placeWallRect(center.x - size / 2, center.y - size / 2, size, size);
					break;
				}
			}
		};

		// Extra cover layouts for large rooms (splits, pits, corridors).
		auto applyBigRoomSetpiece = [&](int roomIndex)
		{
			const Rect &room = rooms[roomIndex];
			if (!room.isBigRoom || room.isCave || room.isGrassRoom || room.isWoodRoom) { return; }
			if (!getRandomChance(rng, cosmetics.bigRoomSetpieceChance)) { return; }

			int left = room.x + 2;
			int right = room.x2() - 3;
			int top = room.y + 2;
			int bottom = room.y2() - 3;
			if (left > right || top > bottom) { return; }
			BlockType innerWallType = Blocks::dungeonWall;

			auto placeWall = [&](int x, int y)
			{
				if (x < left || x > right || y < top || y > bottom) { return; }
				if (isDoorNearby(roomIndex, x, y, 2)) { return; }
				auto &b = map.firstLayer.getBlockUnsafe(x, y);
				if (b.type == Blocks::floor1 || b.type == Blocks::floor2
					|| b.type == Blocks::floorPatern1
					|| b.type == Blocks::floorBigTileTopLeft
					|| b.type == Blocks::floorBigTileTopRight
					|| b.type == Blocks::floorBigTileBottomLeft
					|| b.type == Blocks::floorBigTileBottomRight)
				{
					// TODO: replace some inner walls with decorative walls later.
					b.type = innerWallType;
				}
			};

			auto placeWallRect = [&](int x0, int y0, int w, int h)
			{
				for (int y = y0; y < y0 + h; y++)
				{
					for (int x = x0; x < x0 + w; x++)
					{
						placeWall(x, y);
					}
				}
			};

			glm::ivec2 center = room.center();
			int pattern = getRandomInt(rng, 0, 8);
			switch (pattern)
			{
				case 0:
				{
					bool horizontal = room.w >= room.h;
					int gap = 2;
					if (horizontal)
					{
						int y1 = std::clamp(center.y - gap, top, bottom);
						int y2 = std::clamp(center.y + gap, top, bottom);
						for (int x = left + 1; x <= right - 1; x++)
						{
							placeWall(x, y1);
							placeWall(x, y2);
						}
					}
					else
					{
						int x1 = std::clamp(center.x - gap, left, right);
						int x2 = std::clamp(center.x + gap, left, right);
						for (int y = top + 1; y <= bottom - 1; y++)
						{
							placeWall(x1, y);
							placeWall(x2, y);
						}
					}
					break;
				}
				case 1:
				{
					for (int x = left + 1; x <= right - 1; x++)
					{
						if (std::abs(x - center.x) <= 1) { continue; }
						placeWall(x, center.y);
					}
					for (int y = top + 1; y <= bottom - 1; y++)
					{
						if (std::abs(y - center.y) <= 1) { continue; }
						placeWall(center.x, y);
					}
					break;
				}
				case 2:
				{
					int pillar = 3;
					int px1 = left + 2;
					int px2 = right - 2 - (pillar - 1);
					int py1 = top + 2;
					int py2 = bottom - 2 - (pillar - 1);
					placeWallRect(px1, py1, pillar, pillar);
					placeWallRect(px2, py1, pillar, pillar);
					placeWallRect(px1, py2, pillar, pillar);
					placeWallRect(px2, py2, pillar, pillar);
					break;
				}
				case 3:
				{
					// Large inner ring with wide openings.
					int ringInset = std::max(3, std::min((right - left) / 4, (bottom - top) / 4));
					int ringLeft = std::clamp(left + ringInset, left, right);
					int ringRight = std::clamp(right - ringInset, left, right);
					int ringTop = std::clamp(top + ringInset, top, bottom);
					int ringBottom = std::clamp(bottom - ringInset, top, bottom);
					if (ringLeft + 3 <= ringRight && ringTop + 3 <= ringBottom)
					{
						for (int x = ringLeft; x <= ringRight; x++)
						{
							if (std::abs(x - center.x) <= 2) { continue; }
							placeWall(x, ringTop);
							placeWall(x, ringBottom);
						}
						for (int y = ringTop; y <= ringBottom; y++)
						{
							if (std::abs(y - center.y) <= 2) { continue; }
							placeWall(ringLeft, y);
							placeWall(ringRight, y);
						}
					}
					break;
				}
				case 4:
				{
					// Staggered cover columns.
					int block = 3;
					int colOffset = std::max(3, (right - left) / 4);
					int colX1 = std::clamp(center.x - colOffset, left, right);
					int colX2 = std::clamp(center.x + colOffset, left, right);
					int startY = top + 1;
					int endY = bottom - block;
					if (endY < startY) { break; }
					int step = block + 2;
					for (int y = startY; y <= endY; y += step)
					{
						placeWallRect(colX1, y, block, block);
						placeWallRect(colX2, y + 1, block, block);
					}
					break;
				}
				case 5:
				{
					// Split wall with a single passage.
					bool horizontal = (room.w >= room.h)
						? getRandomChance(rng, 0.6f)
						: getRandomChance(rng, 0.4f);
					int gapSize = getRandomInt(rng, 2, 3);
					if (horizontal)
					{
						int y = std::clamp(center.y + getRandomInt(rng, -2, 2), top + 1, bottom - 1);
						int gapX = std::clamp(center.x + getRandomInt(rng, -room.w / 6, room.w / 6),
							left + 2, right - gapSize - 1);
						for (int x = left + 1; x <= right - 1; x++)
						{
							if (x >= gapX && x < gapX + gapSize) { continue; }
							placeWall(x, y);
						}
					}
					else
					{
						int x = std::clamp(center.x + getRandomInt(rng, -2, 2), left + 1, right - 1);
						int gapY = std::clamp(center.y + getRandomInt(rng, -room.h / 6, room.h / 6),
							top + 2, bottom - gapSize - 1);
						for (int y = top + 1; y <= bottom - 1; y++)
						{
							if (y >= gapY && y < gapY + gapSize) { continue; }
							placeWall(x, y);
						}
					}
					break;
				}
				case 6:
				{
					// Large inner pit that creates a wraparound path.
					int pitInsetX = std::max(3, (right - left) / 4);
					int pitInsetY = std::max(3, (bottom - top) / 4);
					int pitLeft = std::clamp(left + pitInsetX, left, right);
					int pitRight = std::clamp(right - pitInsetX, left, right);
					int pitTop = std::clamp(top + pitInsetY, top, bottom);
					int pitBottom = std::clamp(bottom - pitInsetY, top, bottom);
					if (pitLeft + 2 <= pitRight && pitTop + 2 <= pitBottom)
					{
						placeWallRect(pitLeft, pitTop, pitRight - pitLeft + 1, pitBottom - pitTop + 1);
					}
					break;
				}
				case 7:
				{
					// Upper corridor band with entry gaps.
					int bandY = std::clamp(top + getRandomInt(rng, 1, 2), top + 1, bottom - 2);
					if (bandY >= bottom - 1) { break; }
					int gapA = std::clamp(center.x + getRandomInt(rng, -room.w / 5, room.w / 5), left + 2, right - 2);
					int gapB = -999;
					if (getRandomChance(rng, 0.4f))
					{
						gapB = std::clamp(center.x + getRandomInt(rng, -room.w / 4, room.w / 4), left + 2, right - 2);
						if (std::abs(gapB - gapA) < 4) { gapB = -999; }
					}
					int gapHalf = 1;
					for (int x = left + 1; x <= right - 1; x++)
					{
						bool isGap = (std::abs(x - gapA) <= gapHalf)
							|| (gapB != -999 && std::abs(x - gapB) <= gapHalf);
						if (isGap) { continue; }
						placeWall(x, bandY);
					}
					break;
				}
				case 8:
				{
					// Large T-junction divider.
					bool barHorizontal = getRandomChance(rng, 0.5f);
					if (barHorizontal)
					{
						int barLen = std::max(6, (right - left) - 4);
						int barY = std::clamp(center.y + getRandomInt(rng, -2, 2), top + 1, bottom - 1);
						int barStartX = std::clamp(center.x - barLen / 2, left + 1, right - 1);
						int barEndX = std::clamp(barStartX + barLen, left + 1, right - 1);
						for (int x = barStartX; x <= barEndX; x++)
						{
							placeWall(x, barY);
						}

						bool stemDown = getRandomChance(rng, 0.5f);
						int stemX = std::clamp(center.x + getRandomInt(rng, -2, 2), barStartX, barEndX);
						int stemStart = stemDown ? barY + 1 : barY - 1;
						int stemEnd = stemDown ? (bottom - 1) : (top + 1);
						int step = stemDown ? 1 : -1;
						for (int y = stemStart; stemDown ? (y <= stemEnd) : (y >= stemEnd); y += step)
						{
							placeWall(stemX, y);
						}
					}
					else
					{
						int barLen = std::max(6, (bottom - top) - 4);
						int barX = std::clamp(center.x + getRandomInt(rng, -2, 2), left + 1, right - 1);
						int barStartY = std::clamp(center.y - barLen / 2, top + 1, bottom - 1);
						int barEndY = std::clamp(barStartY + barLen, top + 1, bottom - 1);
						for (int y = barStartY; y <= barEndY; y++)
						{
							placeWall(barX, y);
						}

						bool stemRight = getRandomChance(rng, 0.5f);
						int stemY = std::clamp(center.y + getRandomInt(rng, -2, 2), barStartY, barEndY);
						int stemStart = stemRight ? barX + 1 : barX - 1;
						int stemEnd = stemRight ? (right - 1) : (left + 1);
						int step = stemRight ? 1 : -1;
						for (int x = stemStart; stemRight ? (x <= stemEnd) : (x >= stemEnd); x += step)
						{
							placeWall(x, stemY);
						}
					}
					break;
				}
				default:
				{
					int len = std::min(8, right - left);
					int startX = std::clamp(center.x - len / 2, left + 1, right - 1);
					int startY = std::clamp(center.y - len / 2, top + 1, bottom - 1);
					for (int i = 0; i < len; i++)
					{
						placeWall(startX + i, startY + (i % 2));
						placeWall(startX + i, startY + 3 + (i % 2));
					}
					break;
				}
			}
		};

		// Rock clusters inside caves for cover.
		auto applyCaveRoomSetpiece = [&](int roomIndex)
		{
			const Rect &room = rooms[roomIndex];
			if (!room.isCave) { return; }

			auto clearCaveDoorArea = [&]()
			{
				int radius = cosmetics.caveDoorClearRadius;
				for (auto d : outInfo.rooms[roomIndex].doorPositions)
				{
					int minX = std::max(room.x, d.x - radius);
					int maxX = std::min(room.x2() - 1, d.x + 1 + radius);
					int minY = std::max(room.y, d.y - radius);
					int maxY = std::min(room.y2() - 1, d.y + 1 + radius);

					for (int y = minY; y <= maxY; y++)
					{
						for (int x = minX; x <= maxX; x++)
						{
							auto &tile = map.firstLayer.getBlockUnsafe(x, y);
							if (tile.type == Blocks::cobbleStoneWall)
							{
								tile.type = Blocks::caveFloor;
							}
						}
					}
				}
			};

			if (room.isCaveMaze)
			{
				// Larger cave rooms get maze-like wall ribs for mini-chambers.
				glm::ivec2 center = room.center();
				int area = room.w * room.h;
				int centerClear = std::max(3, room.caveRadius / 3);
				int wallBudget = std::clamp(area / 7, 18, 160);
				int ribs = std::clamp(area / 90, 5, 14);
				int ribMin = std::max(4, room.caveRadius / 2);
				int ribMax = std::max(ribMin + 2, room.caveRadius + 1);

				auto isProtected = [&](int x, int y)
				{
					if (isDoorNearby(roomIndex, x, y, cosmetics.caveDoorClearRadius + 1)) { return true; }
					int dist = std::abs(x - center.x) + std::abs(y - center.y);
					return dist <= centerClear;
				};

				auto tryPlaceWall = [&](int x, int y)
				{
					if (x < room.x + 2 || x >= room.x2() - 2 || y < room.y + 2 || y >= room.y2() - 2)
					{
						return false;
					}
					if (isProtected(x, y)) { return false; }
					auto &tile = map.firstLayer.getBlockUnsafe(x, y);
					if (tile.type != Blocks::caveFloor) { return false; }
					tile.type = Blocks::cobbleStoneWall;
					return true;
				};

				auto randomCardinal = [&]() -> glm::ivec2
				{
					if (getRandomChance(rng, 0.5f))
					{
						return {getRandomChance(rng, 0.5f) ? 1 : -1, 0};
					}
					return {0, getRandomChance(rng, 0.5f) ? 1 : -1};
				};

				int placedWalls = 0;
				for (int i = 0; i < ribs && placedWalls < wallBudget; i++)
				{
					glm::ivec2 pos = {
						getRandomInt(rng, room.x + 2, room.x2() - 3),
						getRandomInt(rng, room.y + 2, room.y2() - 3)
					};
					if (isProtected(pos.x, pos.y)) { continue; }
					glm::ivec2 dir = randomCardinal();
					int len = getRandomInt(rng, ribMin, ribMax);

					for (int step = 0; step < len && placedWalls < wallBudget; step++)
					{
						if (getRandomChance(rng, 0.18f))
						{
							pos += dir;
							continue;
						}

						if (tryPlaceWall(pos.x, pos.y))
						{
							placedWalls++;
						}

						if (getRandomChance(rng, 0.35f))
						{
							dir = (dir.x != 0)
								? glm::ivec2{0, getRandomChance(rng, 0.5f) ? 1 : -1}
								: glm::ivec2{getRandomChance(rng, 0.5f) ? 1 : -1, 0};
						}

						pos += dir;
						if (pos.x < room.x + 2 || pos.x >= room.x2() - 2
							|| pos.y < room.y + 2 || pos.y >= room.y2() - 2)
						{
							break;
						}
					}
				}

				int clusters = getRandomInt(rng, cosmetics.caveRoomWallClustersMin + 2,
					cosmetics.caveRoomWallClustersMax + 6);
				for (int i = 0; i < clusters; i++)
				{
					for (int attempt = 0; attempt < 8; attempt++)
					{
						int x = getRandomInt(rng, room.x + 2, room.x2() - 3);
						int y = getRandomInt(rng, room.y + 2, room.y2() - 3);
						if (isProtected(x, y)) { continue; }
						int r = getRandomInt(rng, cosmetics.caveRoomWallClusterSizeMin,
							cosmetics.caveRoomWallClusterSizeMax + 1);
						paintCircleInRoom(room, {x, y}, r, Blocks::cobbleStoneWall, Blocks::caveFloor);
						break;
					}
				}

				clearCaveDoorArea();
				return;
			}

			if (!getRandomChance(rng, cosmetics.caveRoomSetpieceChance)) { return; }

			int clusters = getRandomInt(rng, cosmetics.caveRoomWallClustersMin,
				cosmetics.caveRoomWallClustersMax);
			for (int i = 0; i < clusters; i++)
			{
				for (int attempt = 0; attempt < 10; attempt++)
				{
					int x = getRandomInt(rng, room.x + 2, room.x2() - 3);
					int y = getRandomInt(rng, room.y + 2, room.y2() - 3);
					if (isDoorNearby(roomIndex, x, y, 3)) { continue; }
					int r = getRandomInt(rng, cosmetics.caveRoomWallClusterSizeMin,
						cosmetics.caveRoomWallClusterSizeMax);
					paintCircleInRoom(room, {x, y}, r, Blocks::cobbleStoneWall, Blocks::caveFloor);
					break;
				}
			}

			// Occasional extra wall blobs to make cave rooms less empty.
			if (getRandomChance(rng, 0.6f))
			{
				int area = room.w * room.h;
				int extraBlobs = std::clamp(area / 180, 2, 3);
				int margin = std::clamp(std::min(room.w, room.h) / 4, 2, 5);
				int minX = room.x + margin;
				int maxX = room.x2() - 1 - margin;
				int minY = room.y + margin;
				int maxY = room.y2() - 1 - margin;
				if (minX <= maxX && minY <= maxY)
				{
					for (int i = 0; i < extraBlobs; i++)
					{
						for (int attempt = 0; attempt < 8; attempt++)
						{
							int x = getRandomInt(rng, minX, maxX);
							int y = getRandomInt(rng, minY, maxY);
							if (isDoorNearby(roomIndex, x, y, 3)) { continue; }
							int r = getRandomInt(rng, 1, 2);
							paintCircleInRoom(room, {x, y}, r, Blocks::cobbleStoneWall, Blocks::caveFloor);
							break;
						}
					}
				}
			}

			clearCaveDoorArea();
		};

		// Softly blends dungeon floors into cave entrances.
		auto blendCaveEntrance = [&](int roomIndex)
		{
			if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return; }
			const Rect &room = rooms[roomIndex];
			if (!room.isCave) { return; }

			auto &doors = outInfo.rooms[roomIndex].doorPositions;
			if (doors.empty()) { return; }

			int radius = 3;
			for (auto door : doors)
			{
				for (int y = door.y - radius; y <= door.y + radius; y++)
				{
					for (int x = door.x - radius; x <= door.x + radius; x++)
					{
						if (x < room.x || x >= room.x2() || y < room.y || y >= room.y2()) { continue; }
						auto &tile = map.firstLayer.getBlockUnsafe(x, y);
						if (tile.type != Blocks::caveFloor) { continue; }

						float dx = (float)(x - door.x);
						float dy = (float)(y - door.y);
						float dist = std::sqrt(dx * dx + dy * dy);
						float strength = 1.0f - dist / (radius + 0.5f);
						if (strength <= 0.0f) { continue; }

						if (getRandomFloat(rng, 0.0f, 1.0f) < strength * 0.65f)
						{
							tile.type = Blocks::floor2;
						}
					}
				}
			}
		};

		auto isDungeonFloorTile = [&](BlockType type)
		{
			return type == Blocks::floor1 || type == Blocks::floor2
				|| type == Blocks::floorPatern1
				|| type == Blocks::floorBigTileTopLeft
				|| type == Blocks::floorBigTileTopRight
				|| type == Blocks::floorBigTileBottomLeft
				|| type == Blocks::floorBigTileBottomRight;
		};

		auto tryPlaceBigFloorTile = [&](int roomIndex, const Rect &room, int x, int y)
		{
			if (x < room.x + 2 || y < room.y + 2 || x + 1 >= room.x2() - 2 || y + 1 >= room.y2() - 2)
			{
				return false;
			}
			if (isDoorNearby(roomIndex, x, y, 2) || isDoorNearby(roomIndex, x + 1, y + 1, 2))
			{
				return false;
			}

			for (int yy = y - 1; yy <= y + 2; yy++)
			{
				for (int xx = x - 1; xx <= x + 2; xx++)
				{
					if (xx < room.x + 1 || xx >= room.x2() - 1 || yy < room.y + 1 || yy >= room.y2() - 1)
					{
						return false;
					}
					if (isWall(map.firstLayer.getBlockUnsafe(xx, yy).type))
					{
						return false;
					}
				}
			}

			for (int yy = y; yy <= y + 1; yy++)
			{
				for (int xx = x; xx <= x + 1; xx++)
				{
					auto &tile = map.firstLayer.getBlockUnsafe(xx, yy);
					if (!isDungeonFloorTile(tile.type))
					{
						return false;
					}
				}
			}

			map.firstLayer.getBlockUnsafe(x, y).type = Blocks::floorBigTileTopLeft;
			map.firstLayer.getBlockUnsafe(x + 1, y).type = Blocks::floorBigTileTopRight;
			map.firstLayer.getBlockUnsafe(x, y + 1).type = Blocks::floorBigTileBottomLeft;
			map.firstLayer.getBlockUnsafe(x + 1, y + 1).type = Blocks::floorBigTileBottomRight;
			return true;
		};

		// Adds floor patterns and big tiles for larger rooms.
		auto decorateDungeonRoomFloor = [&](int roomIndex)
		{
			const Rect &room = rooms[roomIndex];
			if (room.isCave || room.isGrassRoom || room.isWoodRoom) { return; }

			for (int y = room.y + 1; y < room.y2() - 1; y++)
			{
				for (int x = room.x + 1; x < room.x2() - 1; x++)
				{
					if (isDoorNearby(roomIndex, x, y, 1)) { continue; }
					auto &tile = map.firstLayer.getBlockUnsafe(x, y);
					if (tile.type == Blocks::floor1 || tile.type == Blocks::floor2)
					{
					if (getRandomChance(rng, cosmetics.dungeonFloorPatternChance))
					{
						tile.type = Blocks::floorPatern1;
					}
					}
				}
			}

			if (room.w < 6 || room.h < 6) { return; }

			int area = room.w * room.h;
			int maxBigTiles = std::clamp(area / cosmetics.bigTileAreaDiv, 0, cosmetics.bigTileMax);
			int bigTiles = getRandomInt(rng, 0, maxBigTiles);
			for (int i = 0; i < bigTiles; i++)
			{
				bool placed = false;
				for (int attempt = 0; attempt < cosmetics.bigTileAttempts; attempt++)
				{
					int x = getRandomInt(rng, room.x + 2, room.x2() - 3);
					int y = getRandomInt(rng, room.y + 2, room.y2() - 3);
					if (tryPlaceBigFloorTile(roomIndex, room, x, y))
					{
						placed = true;
						break;
					}
				}
				if (!placed) { break; }
			}
		};

		if (!rooms.empty())
		{
			std::vector<int> remaining;
			std::vector<int> connected;
			remaining.reserve(rooms.size());
			for (int i = 0; i < (int)rooms.size(); i++)
			{
				remaining.push_back(i);
			}
			connected.push_back(remaining.back());
			remaining.pop_back();

			while (!remaining.empty())
			{
				int bestRemaining = -1;
				int bestConnected = -1;
				int bestDist = INT_MAX;
				bool found = false;

				for (int i = 0; i < (int)remaining.size(); i++)
				{
					int rIndex = remaining[i];
					for (int j = 0; j < (int)connected.size(); j++)
					{
						int cIndex = connected[j];
						if (roomConnections[rIndex] >= maxRoomConnections
							|| roomConnections[cIndex] >= maxRoomConnections)
						{
							continue;
						}
						glm::ivec2 a = rooms[rIndex].center();
						glm::ivec2 b = rooms[cIndex].center();
						int dist = std::abs(a.x - b.x) + std::abs(a.y - b.y);
						if (dist < bestDist)
						{
							bestDist = dist;
							bestRemaining = i;
							bestConnected = j;
							found = true;
						}
					}
				}

				if (!found)
				{
					for (int i = 0; i < (int)remaining.size(); i++)
					{
						int rIndex = remaining[i];
						for (int j = 0; j < (int)connected.size(); j++)
						{
							int cIndex = connected[j];
							glm::ivec2 a = rooms[rIndex].center();
							glm::ivec2 b = rooms[cIndex].center();
							int dist = std::abs(a.x - b.x) + std::abs(a.y - b.y);
							if (dist < bestDist)
							{
								bestDist = dist;
								bestRemaining = i;
								bestConnected = j;
								found = true;
							}
						}
					}
				}

				if (bestRemaining < 0 || bestConnected < 0)
				{
					break;
				}

				int roomA = remaining[bestRemaining];
				int roomB = connected[bestConnected];
				linkRooms(roomA, roomB, true);

				connected.push_back(roomA);
				remaining.erase(remaining.begin() + bestRemaining);
			}
		}

		if (rooms.size() > 2)
		{
			int extraLinksTarget = std::max(2, (int)rooms.size() / 3);
			int extraAdded = 0;
			int extraAttempts = extraLinksTarget * 3;
			for (int i = 0; i < extraAttempts && extraAdded < extraLinksTarget; i++)
			{
				int a = getRandomInt(rng, 0, (int)rooms.size() - 1);
				int b = getRandomInt(rng, 0, (int)rooms.size() - 1);
				if (a == b) { continue; }
				if (roomConnections[a] >= maxRoomConnections
					|| roomConnections[b] >= maxRoomConnections)
				{
					continue;
				}

				linkRooms(a, b, false);
				extraAdded++;
			}
		}

		for (const auto &conn : connections)
		{
			glm::ivec2 start = {};
			if (conn.side == FloorConnection::Side::North)
			{
				int x = std::clamp(conn.offset, 2, sizeX - 3);
				start = {x, 1};
			}
			else if (conn.side == FloorConnection::Side::South)
			{
				int x = std::clamp(conn.offset, 2, sizeX - 3);
				start = {x, sizeY - 2};
			}
			else if (conn.side == FloorConnection::Side::West)
			{
				int y = std::clamp(conn.offset, 2, sizeY - 3);
				start = {1, y};
			}
			else if (conn.side == FloorConnection::Side::East)
			{
				int y = std::clamp(conn.offset, 2, sizeY - 3);
				start = {sizeX - 2, y};
			}

			if (start.x < 2 || start.y < 2 || start.x >= sizeX - 2 || start.y >= sizeY - 2)
			{
				continue;
			}

			glm::ivec2 end = start;
			if (conn.side == FloorConnection::Side::North) { end.y += 6; }
			if (conn.side == FloorConnection::Side::South) { end.y -= 6; }
			if (conn.side == FloorConnection::Side::West) { end.x += 6; }
			if (conn.side == FloorConnection::Side::East) { end.x -= 6; }

			int style = Corridor_Dungeon;
			int nearestIndex = -1;
			if (!rooms.empty())
			{
				glm::ivec2 nearest = rooms.front().center();
				int bestDist = INT_MAX;
				for (const auto &room : rooms)
				{
					glm::ivec2 center = room.center();
					int dist = std::abs(center.x - end.x) + std::abs(center.y - end.y);
					if (dist < bestDist)
					{
						bestDist = dist;
						nearest = center;
					}
				}

				for (int i = 0; i < (int)rooms.size(); i++)
				{
					if (rooms[i].center() == nearest)
					{
						nearestIndex = i;
						break;
					}
				}

				if (nearestIndex >= 0)
				{
					if (rooms[nearestIndex].isCave)
					{
						style = Corridor_Cave;
					}
					else if (rooms[nearestIndex].isWoodRoom)
					{
						style = Corridor_Wood;
					}
				}
			}

			carveCorridorWithStyle(start, end, style);
			corridorLinks.push_back({start, end, style});

			if (nearestIndex >= 0)
			{
				glm::ivec2 door = pickDoorPos(nearestIndex, end);
				door = snapDoorToEdge(nearestIndex, door, end);
				registerDoor(nearestIndex, door);
				carveDoorOpening(door, style);
				if (nearestIndex >= 0 && nearestIndex < (int)roomConnections.size())
				{
					roomConnections[nearestIndex]++;
				}
				glm::ivec2 doorOut = getDoorOutsidePos(nearestIndex, door);
				if (doorOut != door)
				{
					carveCorridorWithStyle(door, doorOut, style);
					corridorLinks.push_back({door, doorOut, style});
				}
				carveCorridorWithStyle(end, doorOut, style);
				corridorLinks.push_back({end, doorOut, style});
			}
		}

		auto enforceRoomPerimeters = [&]()
		{
			doorHolder.clear();

			for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
			{
				const Rect &room = rooms[roomIndex];
				auto &outRoom = outInfo.rooms[roomIndex];

				BlockType wallType = Blocks::dungeonWall;
				BlockType floorType = Blocks::floor1;
				if (room.isWoodRoom)
				{
					wallType = Blocks::woodenWall;
					floorType = Blocks::woodenFloor;
				}
				else if (room.isGrassRoom)
				{
					floorType = Blocks::grass;
				}
				else if (room.isCave)
				{
					wallType = Blocks::cobbleStoneWall;
					floorType = Blocks::caveFloor;
				}

				std::vector<glm::ivec2> uniqueDoors;
				uniqueDoors.reserve(outRoom.doorPositions.size());
				for (auto d : outRoom.doorPositions)
				{
					bool exists = false;
					for (auto u : uniqueDoors)
					{
						if (u == d) { exists = true; break; }
					}
					if (!exists) { uniqueDoors.push_back(d); }
				}

		auto openDoorTiles = [&](glm::ivec2 d)
		{
			for (int y = d.y; y <= d.y + 1; y++)
			{
				for (int x = d.x; x <= d.x + 1; x++)
				{
					map.firstLayer.getBlockUnsafe(x, y).type = floorType;
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					if (over.type != Blocks::none && isBlockColidable(over.type))
					{
						over.type = Blocks::none;
					}
				}
			}
		};

				auto isOpenTile = [&](int x, int y)
				{
					if (x < 0 || y < 0 || x >= sizeX || y >= sizeY) { return false; }
					auto &tile = map.firstLayer.getBlockUnsafe(x, y);
					return tile.type != Blocks::none && !isWall(tile.type);
				};

				auto addDoorSprite = [&](glm::ivec2 d, bool onNorth, bool onSouth, bool corridorUp, bool corridorDown)
				{
					bool addNorth = onNorth || (room.isCave && corridorUp);
					bool addSouth = onSouth || (room.isCave && corridorDown);
					if (!addNorth && !addSouth) { return; }
					int anchorY = addNorth ? d.y : (d.y + 1);
					glm::ivec2 anchor = {d.x, anchorY};
					if (anchor.x >= 0 && anchor.y >= 0 && anchor.x < sizeX && anchor.y < sizeY)
					{
						doorHolder.addDoor(anchor, Door::Orientation::Horizontal);
					}
				};

				for (int x = room.x; x < room.x2(); x++)
				{
					map.firstLayer.getBlockUnsafe(x, room.y).type = wallType;
					map.firstLayer.getBlockUnsafe(x, room.y2() - 1).type = wallType;
				}
				for (int y = room.y; y < room.y2(); y++)
				{
					map.firstLayer.getBlockUnsafe(room.x, y).type = wallType;
					map.firstLayer.getBlockUnsafe(room.x2() - 1, y).type = wallType;
				}

				for (auto d : uniqueDoors)
				{
					if (d.x < room.x || d.y < room.y) { continue; }
					if (d.x + 1 >= room.x2() || d.y + 1 >= room.y2()) { continue; }
				bool onNorth = d.y == room.y;
				bool onSouth = d.y == room.y2() - 2;
				bool onWest = d.x == room.x;
				bool onEast = d.x == room.x2() - 2;
				bool isOnEdge = onNorth || onSouth || onWest || onEast;
				if (!isOnEdge) { continue; }
				openDoorTiles(d);
				bool corridorUp = isOpenTile(d.x, d.y - 1) && isOpenTile(d.x + 1, d.y - 1);
				bool corridorDown = isOpenTile(d.x, d.y + 2) && isOpenTile(d.x + 1, d.y + 2);
				if (!onWest && !onEast)
				{
					addDoorSprite(d, onNorth, onSouth, corridorUp, corridorDown);
				}
				if (onWest || onEast)
				{
					int anchorX = onWest ? d.x : (d.x + 1);
					int anchorY = d.y + 1;
					glm::ivec2 anchor = {anchorX, anchorY};
					if (anchor.x >= 0 && anchor.y >= 0 && anchor.x < sizeX && anchor.y < sizeY)
					{
						doorHolder.addDoor(anchor, Door::Orientation::Vertical);
					}
				}
			}
			}
		};

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			blendCaveEntrance(roomIndex);
			applyRoomSetpiece(roomIndex);
			applyBigRoomSetpiece(roomIndex);
			applyCaveRoomSetpiece(roomIndex);
			decorateDungeonRoomFloor(roomIndex);
		}

		bool hasGrassRooms = false;
		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			const auto &room = rooms[roomIndex];
			if (room.isGrassRoom)
			{
				hasGrassRooms = true;
				paintGrassRoom(room);
			}
			else if (room.isWoodRoom)
			{
				paintWoodRoom(room);
				paintWoodRoomWalls(room);
				placeCarpetsInRoom(room, outInfo.rooms[roomIndex].doorPositions);
			}
			else if (room.isCave)
			{
				placeDamagedWoodInCaveRoom(room, outInfo.rooms[roomIndex].doorPositions);
			}
		}

		if (hasGrassRooms)
		{
			placeRandomDirtSpots(map, seed + 140, cosmetics.grassRoomDirtThreshold);
			for (const auto &room : rooms)
			{
				if (room.isGrassRoom)
				{
					expandGrassRoomDirtPatches(room);
				}
			}
			decorateGrassPatches(map, seed + 303);
			for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
			{
				const auto &room = rooms[roomIndex];
				if (room.isGrassRoom)
				{
					carveGrassRoomRoads(roomIndex, room);
					placeTreesInRoom(room, outInfo.rooms[roomIndex].doorPositions);
				}
			}
		}

		// Ensures rooms are not fully empty by placing a small cover block.
		auto ensureRoomHasCover = [&](int roomIndex)
		{
			if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return; }
			const Rect &room = rooms[roomIndex];
			if (room.isCave) { return; }

			int innerMinX = room.x + 1;
			int innerMaxX = room.x2() - 2;
			int innerMinY = room.y + 1;
			int innerMaxY = room.y2() - 2;
			if (innerMinX > innerMaxX || innerMinY > innerMaxY) { return; }

			auto hasCover = [&]()
			{
				for (int y = innerMinY; y <= innerMaxY; y++)
				{
					for (int x = innerMinX; x <= innerMaxX; x++)
					{
						auto &base = map.firstLayer.getBlockUnsafe(x, y);
						auto &over = map.secondLayer.getBlockUnsafe(x, y);
						if (isWall(base.type) || (over.type != Blocks::none && isBlockColidable(over.type)))
						{
							return true;
						}
					}
				}
				return false;
			};

			if (hasCover()) { return; }

			BlockType wallType = Blocks::dungeonWall;
			if (room.isWoodRoom)
			{
				wallType = Blocks::woodenWall;
			}
			else if (room.isGrassRoom)
			{
				wallType = Blocks::cobbleStoneWall;
			}

			int placeMinX = room.x + 1;
			int placeMaxX = room.x2() - 3;
			int placeMinY = room.y + 1;
			int placeMaxY = room.y2() - 3;
			if (placeMinX > placeMaxX || placeMinY > placeMaxY) { return; }

			auto canPlaceTile = [&](int x, int y)
			{
				if (x < innerMinX || x > innerMaxX || y < innerMinY || y > innerMaxY) { return false; }
				if (isDoorNearby(roomIndex, x, y, 2)) { return false; }
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				if (isWall(base.type) || base.type == Blocks::none) { return false; }
				if (room.isWoodRoom)
				{
					return base.type == Blocks::woodenFloor || base.type == Blocks::carpetFloor;
				}
				if (room.isGrassRoom)
				{
					return base.type == Blocks::grass || base.type == Blocks::grassDecoration
						|| base.type == Blocks::grassDecorationStones
						|| base.type == Blocks::grassDecorationFlowers
						|| base.type == Blocks::grassDecorationMushrooms
						|| base.type == Blocks::dirt || base.type == Blocks::dirtDecoration;
				}
				return isDungeonFloorTile(base.type);
			};

			auto placeWallTile = [&](int x, int y)
			{
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				auto &over = map.secondLayer.getBlockUnsafe(x, y);
				base.type = wallType;
				if (over.type != Blocks::none && isBlockColidable(over.type))
				{
					over.type = Blocks::none;
				}
			};

			auto tryPlaceBlock = [&](int x, int y)
			{
				if (!canPlaceTile(x, y) || !canPlaceTile(x + 1, y)
					|| !canPlaceTile(x, y + 1) || !canPlaceTile(x + 1, y + 1))
				{
					return false;
				}
				placeWallTile(x, y);
				placeWallTile(x + 1, y);
				placeWallTile(x, y + 1);
				placeWallTile(x + 1, y + 1);
				return true;
			};

			for (int attempt = 0; attempt < 10; attempt++)
			{
				int x = getRandomInt(rng, placeMinX, placeMaxX);
				int y = getRandomInt(rng, placeMinY, placeMaxY);
				if (tryPlaceBlock(x, y)) { return; }
			}

			glm::ivec2 center = room.center();
			int cx = std::clamp(center.x, placeMinX, placeMaxX);
			int cy = std::clamp(center.y, placeMinY, placeMaxY);
			tryPlaceBlock(cx, cy);
		};

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			ensureRoomHasCover(roomIndex);
		}

		// Adds extra corner cover for big rooms.
		auto ensureBigRoomCover = [&](int roomIndex)
		{
			if (cosmetics.allowEmptyRooms) { return; }
			if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return; }
			const Rect &room = rooms[roomIndex];
			if (!room.isBigRoom || room.isCave || room.isGrassRoom || room.isWoodRoom) { return; }

			int innerMinX = room.x + 1;
			int innerMaxX = room.x2() - 2;
			int innerMinY = room.y + 1;
			int innerMaxY = room.y2() - 2;
			if (innerMinX > innerMaxX || innerMinY > innerMaxY) { return; }

			auto canPlaceTile = [&](int x, int y)
			{
				if (x < innerMinX || x > innerMaxX || y < innerMinY || y > innerMaxY) { return false; }
				if (isDoorNearby(roomIndex, x, y, 3)) { return false; }
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				if (isWall(base.type) || base.type == Blocks::none) { return false; }
				return isDungeonFloorTile(base.type);
			};

			auto placeWallTile = [&](int x, int y)
			{
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				auto &over = map.secondLayer.getBlockUnsafe(x, y);
				base.type = Blocks::dungeonWall;
				if (over.type != Blocks::none && isBlockColidable(over.type))
				{
					over.type = Blocks::none;
				}
			};

			auto tryPlaceBlock = [&](int x, int y)
			{
				if (!canPlaceTile(x, y) || !canPlaceTile(x + 1, y)
					|| !canPlaceTile(x, y + 1) || !canPlaceTile(x + 1, y + 1))
				{
					return false;
				}
				placeWallTile(x, y);
				placeWallTile(x + 1, y);
				placeWallTile(x, y + 1);
				placeWallTile(x + 1, y + 1);
				return true;
			};

			int left = room.x + 2;
			int right = room.x2() - 4;
			int top = room.y + 2;
			int bottom = room.y2() - 4;
			if (left > right || top > bottom) { return; }

			glm::ivec2 corners[4] = {
				{left, top},
				{right, top},
				{left, bottom},
				{right, bottom}
			};

			int placed = 0;
			for (auto pos : corners)
			{
				if (tryPlaceBlock(pos.x, pos.y)) { placed++; }
			}

			for (int attempt = 0; attempt < 8 && placed < 2; attempt++)
			{
				int x = getRandomInt(rng, left, right);
				int y = getRandomInt(rng, top, bottom);
				if (tryPlaceBlock(x, y)) { placed++; }
			}
		};

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			ensureBigRoomCover(roomIndex);
		}

		// Repairs cave corridor blockages by reopening the stored corridor links.
		auto isPassableTile = [&](int x, int y)
		{
			if (x < 0 || y < 0 || x >= sizeX || y >= sizeY) { return false; }
			auto &base = map.firstLayer.getBlockUnsafe(x, y);
			if (isWall(base.type) || base.type == Blocks::none) { return false; }
			auto &over = map.secondLayer.getBlockUnsafe(x, y);
			if (over.type != Blocks::none && isBlockColidable(over.type)) { return false; }
			return true;
		};

		auto isCorridorLinkBlocked = [&](const CorridorLink &link)
		{
			if (link.from == link.to) { return false; }
			if (!isPassableTile(link.from.x, link.from.y)) { return true; }
			if (!isPassableTile(link.to.x, link.to.y)) { return true; }

			int margin = 6;
			int minX = std::clamp(std::min(link.from.x, link.to.x) - margin, 0, sizeX - 1);
			int maxX = std::clamp(std::max(link.from.x, link.to.x) + margin, 0, sizeX - 1);
			int minY = std::clamp(std::min(link.from.y, link.to.y) - margin, 0, sizeY - 1);
			int maxY = std::clamp(std::max(link.from.y, link.to.y) + margin, 0, sizeY - 1);
			int width = maxX - minX + 1;
			int height = maxY - minY + 1;
			if (width <= 0 || height <= 0) { return true; }

			auto indexOf = [&](int x, int y)
			{
				return (x - minX) + (y - minY) * width;
			};

			std::vector<char> visited(width * height, 0);
			std::vector<glm::ivec2> queue;
			queue.reserve(width * height / 2);

			int startIndex = indexOf(link.from.x, link.from.y);
			visited[startIndex] = 1;
			queue.push_back(link.from);

			const int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
			for (size_t i = 0; i < queue.size(); i++)
			{
				auto pos = queue[i];
				if (pos == link.to) { return false; }
				for (auto &dir : dirs)
				{
					int nx = pos.x + dir[0];
					int ny = pos.y + dir[1];
					if (nx < minX || nx > maxX || ny < minY || ny > maxY) { continue; }
					int idx = indexOf(nx, ny);
					if (visited[idx]) { continue; }
					if (!isPassableTile(nx, ny)) { continue; }
					visited[idx] = 1;
					queue.push_back({nx, ny});
				}
			}

			return true;
		};

		auto repairCorridorTile = [&](int x, int y, int style)
		{
			if (x < 0 || y < 0 || x >= sizeX || y >= sizeY) { return; }
			auto &base = map.firstLayer.getBlockUnsafe(x, y);
			if (isWall(base.type) || base.type == Blocks::none)
			{
				paintCorridorTile(x, y, style);
			}
			auto &over = map.secondLayer.getBlockUnsafe(x, y);
			if (over.type != Blocks::none && isBlockColidable(over.type))
			{
				over.type = Blocks::none;
			}
		};

		auto repairCorridorWithStyle = [&](const CorridorLink &link)
		{
			int width = 2;
			auto countBlockedLine = [&](glm::ivec2 start, glm::ivec2 end)
			{
				int blocked = 0;
				if (start.x == end.x)
				{
					int y0 = std::min(start.y, end.y);
					int y1 = std::max(start.y, end.y);
					for (int y = y0; y <= y1; y++)
					{
						for (int x = start.x; x < start.x + width; x++)
						{
							if (!isPassableTile(x, y)) { blocked++; }
						}
					}
				}
				else
				{
					int x0 = std::min(start.x, end.x);
					int x1 = std::max(start.x, end.x);
					for (int x = x0; x <= x1; x++)
					{
						for (int y = start.y; y < start.y + width; y++)
						{
							if (!isPassableTile(x, y)) { blocked++; }
						}
					}
				}
				return blocked;
			};

			auto repairLine = [&](glm::ivec2 start, glm::ivec2 end)
			{
				if (start.x == end.x)
				{
					int y0 = std::min(start.y, end.y);
					int y1 = std::max(start.y, end.y);
					for (int y = y0; y <= y1; y++)
					{
						for (int x = start.x; x < start.x + width; x++)
						{
							repairCorridorTile(x, y, link.style);
						}
					}
				}
				else
				{
					int x0 = std::min(start.x, end.x);
					int x1 = std::max(start.x, end.x);
					for (int x = x0; x <= x1; x++)
					{
						for (int y = start.y; y < start.y + width; y++)
						{
							repairCorridorTile(x, y, link.style);
						}
					}
				}
			};

			glm::ivec2 midH = {link.to.x, link.from.y};
			glm::ivec2 midV = {link.from.x, link.to.y};
			int blockedH = countBlockedLine(link.from, midH) + countBlockedLine(midH, link.to);
			int blockedV = countBlockedLine(link.from, midV) + countBlockedLine(midV, link.to);
			bool horizontalFirst = blockedH <= blockedV;
			if (horizontalFirst)
			{
				repairLine(link.from, midH);
				repairLine(midH, link.to);
			}
			else
			{
				repairLine(link.from, midV);
				repairLine(midV, link.to);
			}
		};

		for (const auto &link : corridorLinks)
		{
			if (link.style != Corridor_Cave) { continue; }
			if (isCorridorLinkBlocked(link))
			{
				repairCorridorWithStyle(link);
			}
		}

		// Flags rooms that end up without cover so they can stay empty.
		auto roomHasCover = [&](int roomIndex)
		{
			if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return true; }
			const Rect &room = rooms[roomIndex];
			for (int y = room.y + 1; y < room.y2() - 1; y++)
			{
				for (int x = room.x + 1; x < room.x2() - 1; x++)
				{
					auto &base = map.firstLayer.getBlockUnsafe(x, y);
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					if (isWall(base.type) || (over.type != Blocks::none && isBlockColidable(over.type)))
					{
						return true;
					}
				}
			}
			return false;
		};

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			bool empty = !roomHasCover(roomIndex);
			if (!cosmetics.allowEmptyRooms)
			{
				empty = false;
			}
			outInfo.rooms[roomIndex].isEmptyRoom = empty;
		}

		auto canSpawnOnTile = [&](const Rect &room, BlockType type)
		{
			if (room.isGrassRoom)
			{
				return type == Blocks::grass || type == Blocks::grassDecoration
					|| type == Blocks::grassDecorationStones
					|| type == Blocks::grassDecorationFlowers
					|| type == Blocks::grassDecorationMushrooms
					|| type == Blocks::dirt || type == Blocks::dirtDecoration;
			}
		if (room.isCave)
		{
			return type == Blocks::caveFloor || type == Blocks::floor2
				|| type == Blocks::woodenFloor;
		}
			if (room.isWoodRoom)
			{
				return type == Blocks::woodenFloor;
			}
			return type == Blocks::floor1 || type == Blocks::floor2
				|| type == Blocks::floorPatern1
				|| type == Blocks::floorBigTileTopLeft
				|| type == Blocks::floorBigTileTopRight
				|| type == Blocks::floorBigTileBottomLeft
				|| type == Blocks::floorBigTileBottomRight;
		};

		auto isDoorTooClose = [&](int roomIndex, glm::ivec2 pos, int minDist)
		{
			if (roomIndex < 0 || roomIndex >= (int)outInfo.rooms.size()) { return false; }
			for (auto d : outInfo.rooms[roomIndex].doorPositions)
			{
				for (int dy = 0; dy <= 1; dy++)
				{
					for (int dx = 0; dx <= 1; dx++)
					{
						int dist = std::abs(pos.x - (d.x + dx)) + std::abs(pos.y - (d.y + dy));
						if (dist < minDist)
						{
							return true;
						}
					}
				}
			}
			return false;
		};

		auto pickSpawnPosition = [&](int roomIndex, const Rect &room) -> std::optional<glm::ivec2>
		{
			if (roomIndex >= 0 && roomIndex < (int)outInfo.rooms.size())
			{
				if (outInfo.rooms[roomIndex].isEmptyRoom) { return {}; }
			}
			bool largeRoom = room.isBigRoom || room.isCaveMaze;
			int margin = largeRoom ? cosmetics.bigRoomSpawnMargin : 2;
			int minDist = largeRoom ? cosmetics.bigRoomDoorMinDist : cosmetics.doorSpawnMinDist;
			int attempts = largeRoom ? 12 : 6;
			glm::ivec2 center = room.center();

			int minX = room.x + margin;
			int maxX = room.x2() - margin - 1;
			int minY = room.y + margin;
			int maxY = room.y2() - margin - 1;
			if (minX > maxX || minY > maxY) { return {}; }

			for (int attempt = 0; attempt < attempts; attempt++)
			{
				glm::ivec2 spawn = {
					getRandomInt(rng, minX, maxX),
					getRandomInt(rng, minY, maxY)
				};

				if (largeRoom)
				{
					if (std::abs(spawn.x - center.x) > room.w / 3
						|| std::abs(spawn.y - center.y) > room.h / 3)
					{
						continue;
					}
				}

				if (isDoorTooClose(roomIndex, spawn, minDist)) { continue; }
				auto &tile = map.firstLayer.getBlockUnsafe(spawn.x, spawn.y);
				if (!canSpawnOnTile(room, tile.type)) { continue; }
				if (isWall(tile.type) || tile.type == Blocks::none) { continue; }
				auto &over = map.secondLayer.getBlockUnsafe(spawn.x, spawn.y);
				if (over.type != Blocks::none && isBlockColidable(over.type)) { continue; }
				return spawn;
			}

			return {};
		};

		auto pickPlayerSpawnPosition = [&](int roomIndex, const Rect &room) -> std::optional<glm::ivec2>
		{
			if (roomIndex < 0 || roomIndex >= (int)outInfo.rooms.size()) { return {}; }
			int minX = room.x + 1;
			int maxX = room.x2() - 2;
			int minY = room.y + 1;
			int maxY = room.y2() - 2;
			if (minX > maxX || minY > maxY) { return {}; }

			glm::ivec2 center = room.center();
			glm::ivec2 candidate = {
				std::clamp(center.x, minX, maxX),
				std::clamp(center.y, minY, maxY)
			};
			auto &centerTile = map.firstLayer.getBlockUnsafe(candidate.x, candidate.y);
			auto &centerOver = map.secondLayer.getBlockUnsafe(candidate.x, candidate.y);
			if (!isWall(centerTile.type) && centerTile.type != Blocks::none
				&& (centerOver.type == Blocks::none || !isBlockColidable(centerOver.type))
				&& canSpawnOnTile(room, centerTile.type))
			{
				return candidate;
			}

			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					auto &tile = map.firstLayer.getBlockUnsafe(x, y);
					if (isWall(tile.type) || tile.type == Blocks::none) { continue; }
					if (!canSpawnOnTile(room, tile.type)) { continue; }
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					if (over.type != Blocks::none && isBlockColidable(over.type)) { continue; }
					return glm::ivec2{x, y};
				}
			}

			return {};
		};

		auto clearDoorTilesForRoom = [&](int roomIndex)
		{
			if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return; }
			const auto &room = rooms[roomIndex];
			BlockType doorFloor = Blocks::floor1;
			if (room.isGrassRoom) { doorFloor = Blocks::grass; }
			else if (room.isCave) { doorFloor = Blocks::caveFloor; }
			else if (room.isWoodRoom) { doorFloor = Blocks::woodenFloor; }

			for (auto d : outInfo.rooms[roomIndex].doorPositions)
			{
				for (int dy = 0; dy <= 1; dy++)
				{
					for (int dx = 0; dx <= 1; dx++)
					{
						int x = d.x + dx;
						int y = d.y + dy;
						if (x < room.x || x >= room.x2() || y < room.y || y >= room.y2()) { continue; }
						auto &tile = map.firstLayer.getBlockUnsafe(x, y);
						if (isWall(tile.type) || tile.type == Blocks::none)
						{
							tile.type = doorFloor;
						}
						auto &over = map.secondLayer.getBlockUnsafe(x, y);
						if (over.type != Blocks::none && isBlockColidable(over.type))
						{
							over.type = Blocks::none;
						}
					}
				}
			}
		};


		// Flood fill cave floors and connect any isolated pockets.
		auto ensureCaveRoomConnectivity = [&](int roomIndex)
		{
			if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return; }
			const Rect &room = rooms[roomIndex];
			if (!room.isCave) { return; }

			auto isInsideRoom = [&](int x, int y)
			{
				return x >= room.x && x < room.x2() && y >= room.y && y < room.y2();
			};

		auto isCaveFloorTile = [&](BlockType type)
		{
			return type == Blocks::caveFloor || type == Blocks::floor2
				|| type == Blocks::woodenFloor;
		};

			int roomW = room.w;
			int roomH = room.h;
			if (roomW <= 0 || roomH <= 0) { return; }

			auto indexOf = [&](int x, int y)
			{
				return (x - room.x) + (y - room.y) * roomW;
			};

			auto isDoorTile = [&](int x, int y)
			{
				return isDoorTileForRoom(roomIndex, x, y);
			};

			auto isInteriorTile = [&](int x, int y)
			{
				return x > room.x && x < room.x2() - 1
					&& y > room.y && y < room.y2() - 1;
			};

			auto carveCaveTile = [&](int x, int y)
			{
				if (!isInsideRoom(x, y)) { return; }
				if (!isInteriorTile(x, y) && !isDoorTile(x, y)) { return; }
				auto &tile = map.firstLayer.getBlockUnsafe(x, y);
				tile.type = Blocks::caveFloor;
				auto &over = map.secondLayer.getBlockUnsafe(x, y);
				if (over.type != Blocks::none && isBlockColidable(over.type))
				{
					over.type = Blocks::none;
				}
			};

			for (int pass = 0; pass < 2; pass++)
			{
				std::vector<char> visited(roomW * roomH, 0);
				std::vector<glm::ivec2> queue;
				queue.reserve(roomW * roomH);
				std::vector<glm::ivec2> reachable;
				reachable.reserve(roomW * roomH);

				std::vector<glm::ivec2> seeds;
				for (auto d : outInfo.rooms[roomIndex].doorPositions)
				{
					for (int dy = 0; dy <= 1; dy++)
					{
						for (int dx = 0; dx <= 1; dx++)
						{
							int x = d.x + dx;
							int y = d.y + dy;
							if (!isInsideRoom(x, y)) { continue; }
							map.firstLayer.getBlockUnsafe(x, y).type = Blocks::caveFloor;
							seeds.push_back({x, y});
						}
					}
				}

				if (seeds.empty())
				{
					glm::ivec2 center = room.center();
					if (isInsideRoom(center.x, center.y))
					{
						carveCaveTile(center.x, center.y);
						seeds.push_back(center);
					}
					else
					{
						for (int y = room.y; y < room.y2(); y++)
						{
							for (int x = room.x; x < room.x2(); x++)
							{
								auto &tile = map.firstLayer.getBlockUnsafe(x, y);
								if (isCaveFloorTile(tile.type))
								{
									seeds.push_back({x, y});
									break;
								}
							}
							if (!seeds.empty()) { break; }
						}
					}
				}

				if (seeds.empty()) { return; }

				for (auto s : seeds)
				{
					int idx = indexOf(s.x, s.y);
					if (!visited[idx])
					{
						visited[idx] = 1;
						queue.push_back(s);
						reachable.push_back(s);
					}
				}

				const int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
				for (size_t qi = 0; qi < queue.size(); qi++)
				{
					auto pos = queue[qi];
					for (auto &dir : dirs)
					{
						int nx = pos.x + dir[0];
						int ny = pos.y + dir[1];
						if (!isInsideRoom(nx, ny)) { continue; }
						int nIdx = indexOf(nx, ny);
						if (visited[nIdx]) { continue; }
						auto &tile = map.firstLayer.getBlockUnsafe(nx, ny);
						if (!isCaveFloorTile(tile.type)) { continue; }
						visited[nIdx] = 1;
						queue.push_back({nx, ny});
						reachable.push_back({nx, ny});
					}
				}

				std::vector<glm::ivec2> unreachable;
				for (int y = room.y; y < room.y2(); y++)
				{
					for (int x = room.x; x < room.x2(); x++)
					{
						auto &tile = map.firstLayer.getBlockUnsafe(x, y);
						if (!isCaveFloorTile(tile.type)) { continue; }
						if (!visited[indexOf(x, y)])
						{
							unreachable.push_back({x, y});
						}
					}
				}

				if (unreachable.empty()) { return; }

				for (auto target : unreachable)
				{
					int bestDist = INT_MAX;
					glm::ivec2 best = target;
					for (auto src : reachable)
					{
						int dist = std::abs(target.x - src.x) + std::abs(target.y - src.y);
						if (dist < bestDist)
						{
							bestDist = dist;
							best = src;
						}
					}
					if (bestDist == INT_MAX) { continue; }

					bool horizontalFirst = getRandomChance(rng, 0.5f);
					if (horizontalFirst)
					{
						int stepX = (best.x >= target.x) ? 1 : -1;
						for (int x = target.x; x != best.x; x += stepX)
						{
							carveCaveTile(x, target.y);
						}
						int stepY = (best.y >= target.y) ? 1 : -1;
						for (int y = target.y; y != best.y; y += stepY)
						{
							carveCaveTile(best.x, y);
						}
					}
					else
					{
						int stepY = (best.y >= target.y) ? 1 : -1;
						for (int y = target.y; y != best.y; y += stepY)
						{
							carveCaveTile(target.x, y);
						}
						int stepX = (best.x >= target.x) ? 1 : -1;
						for (int x = target.x; x != best.x; x += stepX)
						{
							carveCaveTile(x, best.y);
						}
					}
					carveCaveTile(best.x, best.y);
					reachable.push_back(target);
				}
			}
		};

		// Restores 2x2 cave doorway edges after carving.
		auto restoreCaveDoorEdges = [&](int roomIndex)
		{
			if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return; }
			const Rect &room = rooms[roomIndex];
			if (!room.isCave) { return; }

			auto isInsideRoom = [&](int x, int y)
			{
				return x >= room.x && x < room.x2() && y >= room.y && y < room.y2();
			};

			auto setWallIfFloor = [&](int x, int y)
			{
				if (!isInsideRoom(x, y)) { return; }
				auto &tile = map.firstLayer.getBlockUnsafe(x, y);
				if (tile.type == Blocks::caveFloor || tile.type == Blocks::floor2
					|| tile.type == Blocks::woodenFloor)
				{
					tile.type = Blocks::cobbleStoneWall;
				}
			};

			for (auto d : outInfo.rooms[roomIndex].doorPositions)
			{
				for (int dy = 0; dy <= 1; dy++)
				{
					for (int dx = 0; dx <= 1; dx++)
					{
						int x = d.x + dx;
						int y = d.y + dy;
						if (!isInsideRoom(x, y)) { continue; }
						map.firstLayer.getBlockUnsafe(x, y).type = Blocks::caveFloor;
					}
				}

				bool onNorth = d.y == room.y;
				bool onSouth = d.y == room.y2() - 2;
				bool onWest = d.x == room.x;
				bool onEast = d.x == room.x2() - 2;

				if (onNorth || onSouth)
				{
					int xLeft = d.x - 1;
					int xRight = d.x + 2;
					for (int y = d.y; y <= d.y + 1; y++)
					{
						setWallIfFloor(xLeft, y);
						setWallIfFloor(xRight, y);
					}
				}
				else if (onWest || onEast)
				{
					int yTop = d.y - 1;
					int yBottom = d.y + 2;
					for (int x = d.x; x <= d.x + 1; x++)
					{
						setWallIfFloor(x, yTop);
						setWallIfFloor(x, yBottom);
					}
				}
			}
		};

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			clearDoorTilesForRoom(roomIndex);
		}

		enforceRoomPerimeters();

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			if (rooms[roomIndex].isCave)
			{
				ensureCaveRoomConnectivity(roomIndex);
				restoreCaveDoorEdges(roomIndex);
			}
		}

		if (outInfo.spawnRoomIndex && *outInfo.spawnRoomIndex >= 0
			&& *outInfo.spawnRoomIndex < (int)rooms.size())
		{
			int spawnIndex = *outInfo.spawnRoomIndex;
			auto spawnPick = pickPlayerSpawnPosition(spawnIndex, rooms[spawnIndex]);
			if (spawnPick)
			{
				outInfo.playerSpawnPos = {spawnPick->x + 0.5f, spawnPick->y + 0.5f};
			}
			else
			{
				const Rect &room = rooms[spawnIndex];
				glm::ivec2 center = room.center();
				BlockType floorType = Blocks::floor1;
				if (room.isWoodRoom) { floorType = Blocks::woodenFloor; }
				else if (room.isGrassRoom) { floorType = Blocks::grass; }
				else if (room.isCave) { floorType = Blocks::caveFloor; }
				auto &tile = map.firstLayer.getBlockUnsafe(center.x, center.y);
				if (isWall(tile.type) || tile.type == Blocks::none)
				{
					tile.type = floorType;
				}
				outInfo.playerSpawnPos = {center.x + 0.5f, center.y + 0.5f};
			}
		}

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			if (createASpawnRoom && outInfo.spawnRoomIndex && *outInfo.spawnRoomIndex == roomIndex)
			{
				continue;
			}
			if (roomIndex < (int)outInfo.rooms.size() && outInfo.rooms[roomIndex].isEmptyRoom)
			{
				continue;
			}

			const auto &room = rooms[roomIndex];
			auto &outRoom = outInfo.rooms[roomIndex];
			int count = getRandomInt(rng, 1, 3);
			for (int i = 0; i < count; i++)
			{
				bool placed = false;
				auto spawnPick = pickSpawnPosition(roomIndex, room);
				if (spawnPick)
				{
					glm::vec2 spawnPos = {spawnPick->x + 0.5f, spawnPick->y + 0.5f};
					outRoom.enemySpawnPositions.push_back(spawnPos);
					outInfo.enemySpawnPositions.push_back(spawnPos);
					placed = true;
				}

				if (!placed)
				{
					glm::ivec2 fallback = room.center();
					bool largeRoom = room.isBigRoom || room.isCaveMaze;
					int minDist = largeRoom ? cosmetics.bigRoomDoorMinDist : cosmetics.doorSpawnMinDist;
					if (isDoorTooClose(roomIndex, fallback, minDist))
					{
						glm::ivec2 away = fallback;
						int bestDist = INT_MAX;
						glm::ivec2 nearest = fallback;
						for (auto d : outInfo.rooms[roomIndex].doorPositions)
						{
							for (int dy = 0; dy <= 1; dy++)
							{
								for (int dx = 0; dx <= 1; dx++)
								{
									int dist = std::abs(fallback.x - (d.x + dx)) + std::abs(fallback.y - (d.y + dy));
									if (dist < bestDist)
									{
										bestDist = dist;
										nearest = {d.x + dx, d.y + dy};
									}
								}
							}
						}
						glm::ivec2 dir = fallback - nearest;
						if (dir.x == 0 && dir.y == 0) { dir = {1, 0}; }
						if (std::abs(dir.x) >= std::abs(dir.y))
						{
							away.x += (dir.x >= 0 ? minDist : -minDist);
						}
						else
						{
							away.y += (dir.y >= 0 ? minDist : -minDist);
						}
						away.x = std::clamp(away.x, room.x + 1, room.x2() - 2);
						away.y = std::clamp(away.y, room.y + 1, room.y2() - 2);
						fallback = away;
					}
					for (int attempt = 0; attempt < 8; attempt++)
					{
						int ox = getRandomInt(rng, -2, 2);
						int oy = getRandomInt(rng, -2, 2);
						glm::ivec2 candidate = {
							std::clamp(fallback.x + ox, room.x + 1, room.x2() - 2),
							std::clamp(fallback.y + oy, room.y + 1, room.y2() - 2)
						};
						auto &tile = map.firstLayer.getBlockUnsafe(candidate.x, candidate.y);
						if (!canSpawnOnTile(room, tile.type)) { continue; }
						if (isWall(tile.type) || tile.type == Blocks::none) { continue; }
						auto &over = map.secondLayer.getBlockUnsafe(candidate.x, candidate.y);
						if (over.type != Blocks::none && isBlockColidable(over.type)) { continue; }
						fallback = candidate;
						break;
					}
					glm::vec2 spawnPos = {fallback.x + 0.5f, fallback.y + 0.5f};
					outRoom.enemySpawnPositions.push_back(spawnPos);
					outInfo.enemySpawnPositions.push_back(spawnPos);
				}
			}
		}
	}
};
