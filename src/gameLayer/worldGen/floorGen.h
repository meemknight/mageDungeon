#pragma once
#include <FastNoiseSIMD.h>
#include <gameplay/map.h>
#include <glm/vec2.hpp>
#include <random>
#include <randomStuff.h>
#include <algorithm>
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

			int x2() const { return x + w; }
			int y2() const { return y + h; }
			glm::ivec2 center() const { return {x + w / 2, y + h / 2}; }
		};

		auto randRange = [&](int minVal, int maxVal)
		{
			return getRandomInt(rng, minVal, maxVal);
		};

		std::vector<Rect> rooms;
		int attempts = std::max(40, (sizeX * sizeY) / 60);
		int minRoomSize = 10;
		int maxRoomSize = 18;
		int padding = 2;

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
			int roomType = getRandomInt(rng, 0, 4);
			int w = randRange(minRoomSize, maxRoomSize);
			int h = randRange(minRoomSize, maxRoomSize);

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

			if (w >= sizeX - 4 || h >= sizeY - 4)
			{
				continue;
			}

			Rect room;
			room.x = randRange(2, sizeX - w - 2);
			room.y = randRange(2, sizeY - h - 2);
			room.w = w;
			room.h = h;

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
			carveRoom(room);
			carveRoomObstacles(room);
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

		if (!rooms.empty())
		{
			std::vector<Rect> remaining = rooms;
			std::vector<Rect> connected;
			connected.push_back(remaining.back());
			remaining.pop_back();

			while (!remaining.empty())
			{
				int bestRemaining = 0;
				int bestConnected = 0;
				int bestDist = INT_MAX;

				for (int i = 0; i < (int)remaining.size(); i++)
				{
					for (int j = 0; j < (int)connected.size(); j++)
					{
						glm::ivec2 a = remaining[i].center();
						glm::ivec2 b = connected[j].center();
						int dist = std::abs(a.x - b.x) + std::abs(a.y - b.y);
						if (dist < bestDist)
						{
							bestDist = dist;
							bestRemaining = i;
							bestConnected = j;
						}
					}
				}

				glm::ivec2 a = remaining[bestRemaining].center();
				glm::ivec2 b = connected[bestConnected].center();

				if (getRandomChance(rng, 0.3f))
				{
					carveMazeCorridor(a, b);
				}
				else
				{
					carveCorridor(a, b);
				}

				connected.push_back(remaining[bestRemaining]);
				remaining.erase(remaining.begin() + bestRemaining);
			}
		}

		if (rooms.size() > 2)
		{
			int extraLinks = std::max(1, (int)rooms.size() / 3);
			for (int i = 0; i < extraLinks; i++)
			{
				int a = getRandomInt(rng, 0, (int)rooms.size() - 1);
				int b = getRandomInt(rng, 0, (int)rooms.size() - 1);
				if (a == b) { continue; }

				glm::ivec2 start = rooms[a].center();
				glm::ivec2 end = rooms[b].center();
				if (getRandomChance(rng, 0.4f))
				{
					carveMazeCorridor(start, end);
				}
				else
				{
					carveCorridor(start, end);
				}
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

				carveCorridor(end, nearest);
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
					if (tile.type == Blocks::floor1 || tile.type == Blocks::floor2)
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
