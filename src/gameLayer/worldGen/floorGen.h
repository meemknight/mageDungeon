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

	void init()
	{
		grassDecorNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		grassDecorNoise->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
		grassDecorNoise->SetFrequency(0.1f);
		grassDecorNoise->SetFractalOctaves(3);
		grassDecorNoise->SetFractalLacunarity(2.0f);
		grassDecorNoise->SetFractalGain(0.5f);

		// --- Dirt blobs (small circular-ish patches) ---
		dirtNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		dirtNoise->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
		dirtNoise->SetFrequency(0.08f);        // higher than grass -> smaller blobs
		dirtNoise->SetFractalOctaves(2);       // less detail -> fewer weird tendrils
		dirtNoise->SetFractalLacunarity(2.0f);
		dirtNoise->SetFractalGain(0.5f);

		// --- Dirt decoration (sparse random dots, not clumps) ---
		dirtDecorNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		dirtDecorNoise->SetNoiseType(FastNoiseSIMD::NoiseType::Simplex); // no fractal -> less clumping
		dirtDecorNoise->SetFrequency(0.75f);   // high freq -> small isolated hits
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

		const float threshold = 0.6f;

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
						b.type = Blocks::grassDecoration;
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
		const float dirtCutoff = 0.95f - (tresshold * 0.45f); // range ~[0.95 .. 0.50]

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
					if (b.type == Blocks::grass || b.type == Blocks::grassDecoration)
						b.type = Blocks::dirt;
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(dn);

		// ---- Dirt decoration (sparse dots, not clumps) ----
		// Use a different seed so it doesn't correlate with blobs too much
		dirtDecorNoise->SetSeed(seed + 1337);

		const float decorCutoff = 0.80f;

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

		const float threshold = 0.72f;

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
						b.type = Blocks::grassDecoration;
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

		placeRandomDirtSpots(map, seed + 10, 0.4f);


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
		int minRoomSize = 10;
		int maxRoomSize = 18;
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
			if (getRandomFloat(rng, 0.0f, 1.0f) < 0.12f)
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
				paintCircle(c, r, Blocks::floor2, Blocks::cobbleStoneWall);
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
						if (getRandomChance(rng, 0.08f))
						{
							b.type = Blocks::dirt;
						}
					}
				}
			}
		};

		// Adds a few trees to grassy rooms.
		auto placeTreesInRoom = [&](const Rect &room)
		{
			int area = room.w * room.h;
			int maxTrees = std::clamp(area / 70, 1, 4);
			int treeCount = getRandomInt(rng, 0, maxTrees);
			for (int i = 0; i < treeCount; i++)
			{
				bool placed = false;
				for (int attempt = 0; attempt < 10; attempt++)
				{
					int x = getRandomInt(rng, room.x + 1, room.x2() - 2);
					int y = getRandomInt(rng, room.y + 1, room.y2() - 2);
					auto &base = map.firstLayer.getBlockUnsafe(x, y);
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					if (over.type != Blocks::none) { continue; }
					if (base.type == Blocks::grass || base.type == Blocks::grassDecoration)
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
			if (!getRandomChance(rng, 0.5f)) { return; }

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
					|| b.type == Blocks::dirt || b.type == Blocks::dirtDecoration)
				{
					b.type = Blocks::dirt;
					if (getRandomChance(rng, 0.18f))
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

		auto carveCorridor = [&](glm::ivec2 from, glm::ivec2 to)
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
							carveFloor(x, y);
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
							carveFloor(x, y);
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

		auto carveMazeCorridor = [&](glm::ivec2 from, glm::ivec2 to)
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

				carveCorridor(current, next);
				current = next;
			}
		};

		for (int i = 0; i < attempts; i++)
		{
			bool isCave = getRandomChance(rng, 0.18f);
			bool isGrassRoom = (!isCave) && getRandomChance(rng, 0.2f);
			int w = randRange(minRoomSize, maxRoomSize);
			int h = randRange(minRoomSize, maxRoomSize);
			int caveRadius = 0;

			if (isCave)
			{
				caveRadius = randRange(6, 11);
				int caveExtent = caveRadius + caveRadius / 2 + 2;
				w = caveExtent * 2 + 1;
				h = caveExtent * 2 + 1;
			}
			else
			{
				int roomType = getRandomInt(rng, 0, 4);
				if (roomType == 1)
				{
					w = randRange(12, 20);
					h = randRange(10, 14);
				}
				else if (roomType == 2)
				{
					w = randRange(10, 14);
					h = randRange(12, 20);
				}
				else if (roomType == 3)
				{
					w = randRange(14, 22);
					h = randRange(8, 12);
				}
				else if (roomType == 4)
				{
					w = randRange(10, 16);
					h = randRange(10, 16);
				}
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
					if (b.type == Blocks::floor2)
					{
						last = pos;
					}
					else if (b.type == Blocks::cobbleStoneWall)
					{
						break;
					}
					pos += dir;
				}
				return last;
			}

			glm::ivec2 best = center;
			int tries = 6;
			for (int i = 0; i < tries; i++)
			{
				if (dir.x != 0)
				{
					int x = dir.x > 0 ? room.x2() - 1 : room.x;
					int offset = getRandomInt(rng, -room.h / 3, room.h / 3);
					int y = std::clamp(center.y + offset, room.y + 1, room.y2() - 2);
					best = {x, y};
				}
				else
				{
					int y = dir.y > 0 ? room.y2() - 1 : room.y;
					int offset = getRandomInt(rng, -room.w / 3, room.w / 3);
					int x = std::clamp(center.x + offset, room.x + 1, room.x2() - 2);
					best = {x, y};
				}

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

			glm::ivec2 doorA = pickDoorPos(a, rooms[b].center());
			glm::ivec2 doorB = pickDoorPos(b, rooms[a].center());
			registerDoor(a, doorA);
			registerDoor(b, doorB);

			if (allowMaze && getRandomChance(rng, 0.15f))
			{
				carveMazeCorridor(doorA, doorB);
			}
			else
			{
				carveCorridor(doorA, doorB);
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
				if (std::abs(d.x - x) + std::abs(d.y - y) <= dist)
				{
					return true;
				}
			}
			return false;
		};

		auto applyRoomSetpiece = [&](int roomIndex)
		{
			const Rect &room = rooms[roomIndex];
			if (room.isCave || room.isGrassRoom) { return; }
			if (!getRandomChance(rng, 0.7f)) { return; }

			int left = room.x + 2;
			int right = room.x2() - 3;
			int top = room.y + 2;
			int bottom = room.y2() - 3;
			if (left > right || top > bottom) { return; }

			auto placeWall = [&](int x, int y)
			{
				if (x < left || x > right || y < top || y > bottom) { return; }
				if (isDoorNearby(roomIndex, x, y, 2)) { return; }
				auto &b = map.firstLayer.getBlockUnsafe(x, y);
				if (b.type == Blocks::floor1 || b.type == Blocks::floor2)
				{
					b.type = Blocks::dungeonWall;
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
							placeWall(x, y);
						}
					}
					else
					{
						int x = std::clamp(center.x + getRandomInt(rng, -1, 1), left, right);
						for (int y = top + inset; y <= bottom - inset; y++)
						{
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

			carveCorridor(start, end);

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

				int nearestIndex = 0;
				for (int i = 0; i < (int)rooms.size(); i++)
				{
					if (rooms[i].center() == nearest)
					{
						nearestIndex = i;
						break;
					}
				}

				glm::ivec2 door = pickDoorPos(nearestIndex, end);
				registerDoor(nearestIndex, door);
				carveCorridor(end, door);
			}
		}

		bool hasGrassRooms = false;
		for (const auto &room : rooms)
		{
			if (room.isGrassRoom)
			{
				hasGrassRooms = true;
				paintGrassRoom(room);
			}
		}

		if (hasGrassRooms)
		{
			placeRandomDirtSpots(map, seed + 140, 0.18f);
			decorateGrassPatches(map, seed + 303);
			for (const auto &room : rooms)
			{
				if (room.isGrassRoom)
				{
					//NOTE FOR AGENT, FINISH IMPLEMENTING THIS LINE !!!!!
					//carveGrassRoomRoads(room); 
					placeTreesInRoom(room);
				}
			}
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
				for (int attempt = 0; attempt < 6; attempt++)
				{
					glm::ivec2 spawn = {
						room.x + 2 + getRandomInt(rng, 0, room.w - 4),
						room.y + 2 + getRandomInt(rng, 0, room.h - 4)
					};

					auto &tile = map.firstLayer.getBlockUnsafe(spawn.x, spawn.y);
				bool canSpawn = false;
				if (room.isGrassRoom)
				{
					canSpawn = tile.type == Blocks::grass || tile.type == Blocks::grassDecoration
						|| tile.type == Blocks::dirt || tile.type == Blocks::dirtDecoration;
				}
				else
				{
					canSpawn = tile.type == Blocks::floor1 || tile.type == Blocks::floor2;
				}

				if (canSpawn)
				{
					glm::vec2 spawnPos = {spawn.x + 0.5f, spawn.y + 0.5f};
					outRoom.enemySpawnPositions.push_back(spawnPos);
						outInfo.enemySpawnPositions.push_back(spawnPos);
						placed = true;
						break;
					}
				}

				if (!placed)
				{
					glm::vec2 spawnPos = {room.center().x + 0.5f, room.center().y + 0.5f};
					outRoom.enemySpawnPositions.push_back(spawnPos);
					outInfo.enemySpawnPositions.push_back(spawnPos);
				}
			}
		}
	}
};
