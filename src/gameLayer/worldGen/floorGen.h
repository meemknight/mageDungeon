#pragma once
#include <FastNoiseSIMD.h>
#include <gameplay/map.h>
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
		float grassRoomDirtThreshold = 0.18f;

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
		bool createASpawnRoom, FloorInfo &outInfo)
	{
		map.create(sizeX, sizeY);
		outInfo = {};

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
		int minRoomSize = 12;
		int maxRoomSize = 20;
		int padding = 4;
		int maxRoomConnections = 2;

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

		auto carveCaveRoom = [&](const Rect &room)
		{
			glm::ivec2 center = room.center();
			int baseRadius = std::max(4, room.caveRadius);
			int lumps = getRandomInt(rng, 3, 5);
			std::vector<glm::ivec2> centers;
			centers.reserve(lumps);
			centers.push_back(center);

			for (int i = 1; i < lumps; i++)
			{
				glm::ivec2 offset = {
					getRandomInt(rng, -baseRadius / 2, baseRadius / 2),
					getRandomInt(rng, -baseRadius / 2, baseRadius / 2)
				};
				centers.push_back(center + offset);
			}

			for (auto c : centers)
			{
				int r = baseRadius + getRandomInt(rng, -2, 2);
				paintCircle(c, r, Blocks::cobbleStoneWall, Blocks::dungeonWall);
			}

			int innerRadius = std::max(3, baseRadius - 2);
			for (auto c : centers)
			{
				int r = innerRadius + getRandomInt(rng, -1, 1);
				paintCircle(c, r, Blocks::caveFloor, Blocks::cobbleStoneWall);
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
			int width = getRandomChance(rng, 0.25f) ? 3 : 2;
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
			int w = randRange(minRoomSize, maxRoomSize);
			int h = randRange(minRoomSize, maxRoomSize);
			int caveRadius = 0;

			if (isCave)
			{
				caveRadius = randRange(cosmetics.caveRoomRadiusMin, cosmetics.caveRoomRadiusMax);
				int caveExtent = caveRadius + cosmetics.caveRoomExtentPadding;
				w = caveExtent * 2 + 1;
				h = caveExtent * 2 + 1;
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
			glm::ivec2 center = rooms[spawnIndex].center();
			outInfo.playerSpawnPos = {center.x + 0.5f, center.y + 0.5f};
		}

		std::vector<int> roomConnections(rooms.size(), 0);
		std::vector<std::vector<char>> roomLinks(rooms.size(),
			std::vector<char>(rooms.size(), 0));

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

			if (room.isCave)
			{
				glm::ivec2 pos = center;
				glm::ivec2 last = center;
				for (int i = 0; i < room.w + room.h; i++)
				{
					if (pos.x < room.x || pos.x >= room.x2() || pos.y < room.y || pos.y >= room.y2())
					{
						break;
					}
					auto &b = map.firstLayer.getBlockUnsafe(pos.x, pos.y);
					if (b.type == Blocks::caveFloor || b.type == Blocks::floor2)
					{
						last = pos;
					}
					else if (b.type == Blocks::cobbleStoneWall)
					{
						break;
					}
					pos += dir;
				}
				return clampDoorTopLeft(last);
			}

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
			registerDoor(a, doorA);
			registerDoor(b, doorB);
			carveDoorOpening(doorA, style);
			carveDoorOpening(doorB, style);

			if (allowMaze && getRandomChance(rng, 0.15f))
			{
				carveMazeCorridorWithStyle(doorA, doorB, style);
			}
			else
			{
				carveCorridorWithStyle(doorA, doorB, style);
			}

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
			int pattern = getRandomInt(rng, 0, 4);
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
				default:
				{
					int size = std::min(3, std::min(right - left + 1, bottom - top + 1));
					placeWallRect(center.x - size / 2, center.y - size / 2, size, size);
					break;
				}
			}
		};

		// Extra cover layouts for large rooms.
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
			int pattern = getRandomInt(rng, 0, 3);
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
			if (!getRandomChance(rng, cosmetics.caveRoomSetpieceChance)) { return; }

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
					paintCircle({x, y}, r, Blocks::cobbleStoneWall, Blocks::caveFloor);
					break;
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
			int extraLinks = std::max(1, (int)rooms.size() / 6);
			for (int i = 0; i < extraLinks; i++)
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

			if (nearestIndex >= 0)
			{
				glm::ivec2 door = pickDoorPos(nearestIndex, end);
				registerDoor(nearestIndex, door);
				carveDoorOpening(door, style);
				if (nearestIndex >= 0 && nearestIndex < (int)roomConnections.size())
				{
					roomConnections[nearestIndex]++;
				}
				carveCorridorWithStyle(end, door, style);
			}
		}

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			blendCaveEntrance(roomIndex);
			applyRoomSetpiece(roomIndex);
			applyBigRoomSetpiece(roomIndex);
			applyCaveRoomSetpiece(roomIndex);
			decorateDungeonRoomFloor(roomIndex);
		}

		bool hasGrassRooms = false;
		for (const auto &room : rooms)
		{
			if (room.isGrassRoom)
			{
				hasGrassRooms = true;
				paintGrassRoom(room);
			}
			else if (room.isWoodRoom)
			{
				paintWoodRoom(room);
				paintWoodRoomWalls(room);
			}
		}

		if (hasGrassRooms)
		{
			placeRandomDirtSpots(map, seed + 140, cosmetics.grassRoomDirtThreshold);
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
				return type == Blocks::caveFloor || type == Blocks::floor2;
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
			int margin = room.isBigRoom ? cosmetics.bigRoomSpawnMargin : 2;
			int minDist = room.isBigRoom ? cosmetics.bigRoomDoorMinDist : cosmetics.doorSpawnMinDist;
			int attempts = room.isBigRoom ? 12 : 6;
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

				if (room.isBigRoom)
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
				return spawn;
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

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			clearDoorTilesForRoom(roomIndex);
		}

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			if (createASpawnRoom && outInfo.spawnRoomIndex && *outInfo.spawnRoomIndex == roomIndex)
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
					int minDist = room.isBigRoom ? cosmetics.bigRoomDoorMinDist : cosmetics.doorSpawnMinDist;
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
					glm::vec2 spawnPos = {fallback.x + 0.5f, fallback.y + 0.5f};
					outRoom.enemySpawnPositions.push_back(spawnPos);
					outInfo.enemySpawnPositions.push_back(spawnPos);
				}
			}
		}
	}
};
