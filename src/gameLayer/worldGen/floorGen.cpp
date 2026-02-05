#include "floorGen.h"
#include <string>

void FloorGenerator::init()
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

void FloorGenerator::generateTutorialFloor(int sizeX, int sizeY, Map &map, FloorInfo &outInfo, DoorHolder &doorHolder)
{
	map.create(sizeX, sizeY);
	outInfo = {};
	doorHolder.clear();
	std::ranlux24_base rng{std::random_device{}()};

	for (int y = 0; y < sizeY; y++)
	{
		for (int x = 0; x < sizeX; x++)
		{
			map.firstLayer.getBlockUnsafe(x, y).type = Blocks::dungeonWall;
			map.secondLayer.getBlockUnsafe(x, y).type = Blocks::none;
		}
	}

	int gap = 4;
	int roomW = 14;
	int roomH = 14;
	int totalW = roomW * 3 + gap * 2;
	if (totalW > sizeX - 4)
	{
		roomW = std::max(6, (sizeX - 4 - gap * 2) / 3);
		totalW = roomW * 3 + gap * 2;
	}
	roomH = std::min(roomH, sizeY - 6);
	if (roomH < 6) { roomH = std::max(5, sizeY - 4); }

	int startX = std::clamp((sizeX - totalW) / 2, 1, std::max(1, sizeX - totalW - 1));
	int startY = std::clamp((sizeY - roomH) / 2, 1, std::max(1, sizeY - roomH - 1));
	int corridorY = std::clamp(startY + roomH / 2 - 1, 1, sizeY - 3);

	outInfo.rooms.clear();
	outInfo.rooms.reserve(3);

	for (int i = 0; i < 3; i++)
	{
		int x0 = startX + i * (roomW + gap);
		int y0 = startY;

		FloorRoom room = {};
		room.pos = {x0, y0};
		room.size = {roomW, roomH};
		room.isSpawnRoom = (i == 0);
		room.isExitRoom = (i == 2);
		room.isTutorialRoom = true;
		outInfo.rooms.push_back(room);

		for (int y = y0; y < y0 + roomH; y++)
		{
			for (int x = x0; x < x0 + roomW; x++)
			{
				if (x <= 0 || y <= 0 || x >= sizeX - 1 || y >= sizeY - 1) { continue; }
				map.firstLayer.getBlockUnsafe(x, y).type = Blocks::floor1;
			}
		}

		glm::vec2 annotationPos = {
			x0 + roomW * 0.5f,
			y0 + roomH * 0.5f - 3
		};
		std::string annotationText = "tutorial_room_" + std::to_string(i + 1);
		if (i == 0) { annotationText = "Shoot {shoot_button}"; }
		if (i == 1) { annotationText = "Press {select_spell} than {up_spell}\nto load a better spell"; }
		if (i == 2) { annotationText = "Good luck!"; }
		map.textAnnotations[annotationPos] = annotationText;
	}

	for (int i = 0; i < 2; i++)
	{
		const auto &leftRoom = outInfo.rooms[i];
		const auto &rightRoom = outInfo.rooms[i + 1];
		int xStart = leftRoom.pos.x + leftRoom.size.x;
		int xEnd = rightRoom.pos.x;
		for (int x = xStart; x < xEnd; x++)
		{
			for (int dy = 0; dy <= 1; dy++)
			{
				int y = corridorY + dy;
				if (x <= 0 || y <= 0 || x >= sizeX - 1 || y >= sizeY - 1) { continue; }
				map.firstLayer.getBlockUnsafe(x, y).type = Blocks::floor1;
			}
		}
	}

	outInfo.spawnRoomIndex = 0;
	if (!outInfo.rooms.empty())
	{
		glm::ivec2 startCenter = outInfo.rooms.front().center();
		outInfo.playerSpawnPos = {startCenter.x + 0.5f, startCenter.y + 0.5f};
	}

	outInfo.exitRoomIndex = 2;
	if (outInfo.rooms.size() >= 3)
	{
		glm::ivec2 exitCenter = outInfo.rooms.back().center();
		auto isExitSpotValid = [&](glm::ivec2 pos)
		{
			if (pos.x <= 0 || pos.y <= 0 || pos.x >= sizeX - 1 || pos.y >= sizeY - 1) { return false; }
			int belowY = pos.y + 1;
			if (belowY <= 0 || belowY >= sizeY - 1) { return false; }
			if (map.isCollidableAtPosSafe(pos.x, belowY)) { return false; }
			auto &tile = map.firstLayer.getBlockUnsafe(pos.x, pos.y);
			if (isWall(tile.type) || tile.type == Blocks::none) { return false; }
			auto &over = map.secondLayer.getBlockUnsafe(pos.x, pos.y);
			if (over.type != Blocks::none) { return false; }
			return true;
		};

		glm::ivec2 exitPos = exitCenter;
		if (!isExitSpotValid(exitPos))
		{
			int attempts = 8;
			for (int attempt = 0; attempt < attempts; attempt++)
			{
				glm::ivec2 pos = {
					exitCenter.x + getRandomInt(rng, -2, 2),
					exitCenter.y + getRandomInt(rng, -2, 2)
				};
				if (isExitSpotValid(pos))
				{
					exitPos = pos;
					break;
				}
			}
		}

		if (isExitSpotValid(exitPos))
		{
			map.secondLayer.getBlockUnsafe(exitPos.x, exitPos.y).type = Blocks::exit;
			outInfo.exitPos = {exitPos.x + 0.5f, exitPos.y + 0.5f};
		}
	}
}

void FloorGenerator::generateDungeonFloor(int sizeX, int sizeY, Map &map, int seed, const std::vector<FloorConnection> &connections, bool createASpawnRoom, FloorInfo &outInfo, DoorHolder &doorHolder)
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
		const int offsets[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
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

	struct DoorConnection
	{
		int roomIndex = -1;
		glm::ivec2 doorPos = {};
		glm::ivec2 doorOut = {};
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
			const int offsets[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
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
			return isDoorTileForRoom(roomIndex, x, y);
		};
		auto canCarveLine = [&](glm::ivec2 start, glm::ivec2 end)
		{
			if (start.x == end.x)
			{
				int y0 = std::min(start.y, end.y);
				int y1 = std::max(start.y, end.y);
				for (int y = y0; y <= y1; y++)
				{
					for (int x = start.x; x < start.x + width; x++)
					{
						if (!canCarveCorridorTile(x, y)) { return false; }
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
						if (!canCarveCorridorTile(x, y)) { return false; }
					}
				}
			}
			return true;
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
		auto carvePathBfs = [&]()
		{
			const int maxNodes = sizeX * sizeY;
			std::vector<int> prev(maxNodes, -1);
			std::vector<char> visited(maxNodes, 0);
			auto indexOf = [&](int x, int y) { return y * sizeX + x; };
			auto inBounds = [&](int x, int y)
			{
				return x >= 1 && y >= 1 && x < sizeX - 1 && y < sizeY - 1;
			};
			std::vector<glm::ivec2> queue;
			queue.reserve(maxNodes / 2);
			if (!inBounds(from.x, from.y) || !inBounds(to.x, to.y)) { return false; }
			if (!canCarveCorridorTile(from.x, from.y)) { return false; }
			if (!canCarveCorridorTile(to.x, to.y)) { return false; }
			int startIndex = indexOf(from.x, from.y);
			int goalIndex = indexOf(to.x, to.y);
			queue.push_back(from);
			visited[startIndex] = 1;
			int head = 0;
			while (head < (int)queue.size())
			{
				glm::ivec2 current = queue[head++];
				if (current.x == to.x && current.y == to.y) { break; }
				glm::ivec2 neighbors[4] = {
					{current.x + 1, current.y},
					{current.x - 1, current.y},
					{current.x, current.y + 1},
					{current.x, current.y - 1}
				};
				for (auto n : neighbors)
				{
					if (!inBounds(n.x, n.y)) { continue; }
					int idx = indexOf(n.x, n.y);
					if (visited[idx]) { continue; }
					if (!canCarveCorridorTile(n.x, n.y)) { continue; }
					visited[idx] = 1;
					prev[idx] = indexOf(current.x, current.y);
					queue.push_back(n);
				}
			}
			if (!visited[goalIndex]) { return false; }
			std::vector<glm::ivec2> path;
			int cur = goalIndex;
			while (cur != -1)
			{
				int x = cur % sizeX;
				int y = cur / sizeX;
				path.push_back({x, y});
				if (cur == startIndex) { break; }
				cur = prev[cur];
			}
			if (path.empty()) { return false; }
			std::reverse(path.begin(), path.end());
			glm::ivec2 lastDir = {};
			for (size_t i = 0; i < path.size(); i++)
			{
				glm::ivec2 current = path[i];
				if (i + 1 < path.size())
				{
					lastDir = path[i + 1] - current;
				}
				else if (i > 0)
				{
					lastDir = current - path[i - 1];
				}
				paintCorridorTile(current.x, current.y, style);
				if (lastDir.x != 0)
				{
					paintCorridorTile(current.x, current.y + 1, style);
				}
				else if (lastDir.y != 0)
				{
					paintCorridorTile(current.x + 1, current.y, style);
				}
			}
			return true;
		};
		auto tryCarve = [&](bool horizontalFirst)
		{
			glm::ivec2 mid = horizontalFirst
				? glm::ivec2{to.x, from.y}
				: glm::ivec2{from.x, to.y};
			if (!canCarveLine(from, mid) || !canCarveLine(mid, to)) { return false; }
			carveLine(from, mid);
			carveLine(mid, to);
			return true;
		};

		if (getRandomChance(rng, 0.6f))
		{
			if (!tryCarve(true))
			{
				if (!tryCarve(false))
				{
					carvePathBfs();
				}
			}
		}
		else
		{
			if (!tryCarve(false))
			{
				if (!tryCarve(true))
				{
					carvePathBfs();
				}
			}
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
	std::vector<DoorConnection> doorConnections;
	doorConnections.reserve(rooms.size() * 2);

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
		doorConnections.push_back({a, doorA, outA, style});
		doorConnections.push_back({b, doorB, outB, style});
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
			doorConnections.push_back({nearestIndex, door, doorOut, style});
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

		const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
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

	// Places wall decorations on dungeon walls.
	auto placeWallDecorations = [&]()
	{
		for (int y = 1; y < sizeY - 1; y++)
		{
			for (int x = 1; x < sizeX - 1; x++)
			{
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				if (base.type != Blocks::dungeonWall) { continue; }
				auto &over = map.secondLayer.getBlockUnsafe(x, y);
				if (over.type != Blocks::none) { continue; }
				auto &below = map.firstLayer.getBlockUnsafe(x, y + 1);
				if (isWall(below.type)) { continue; }
				if (!getRandomChance(rng, cosmetics.wallDecorChance)) { continue; }
				over.type = Blocks::wallDecorations;
			}
		}
	};

	// Places breakable wooden decorations (cover) on the second layer.
	auto placeWoodDecorations = [&]()
	{
		auto isDoorTooCloseLocal = [&](int roomIndex, glm::ivec2 pos, int minDist)
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

		auto canPlaceDecorTile = [&](int x, int y)
		{
			if (x < 0 || y < 0 || x >= sizeX || y >= sizeY) { return false; }
			auto &base = map.firstLayer.getBlockUnsafe(x, y);
			if (isWall(base.type) || base.type == Blocks::none) { return false; }
			auto &over = map.secondLayer.getBlockUnsafe(x, y);
			if (over.type != Blocks::none) { return false; }
			return true;
		};

		auto canPlaceDecorInRoom = [&](int roomIndex, int x, int y)
		{
			const Rect &room = rooms[roomIndex];
			if (x <= room.x || x >= room.x2() - 1 || y <= room.y || y >= room.y2() - 1)
			{
				return false;
			}
			if (isDoorTileForRoom(roomIndex, x, y)) { return false; }
			if (isDoorTooCloseLocal(roomIndex, {x, y}, 2)) { return false; }
			return canPlaceDecorTile(x, y);
		};

		auto placeDecorTile = [&](int x, int y)
		{
			map.secondLayer.getBlockUnsafe(x, y).type = Blocks::woodenDecorations;
		};

		auto tryPlaceDecorBlock = [&](int roomIndex, int x, int y)
		{
			if (!canPlaceDecorInRoom(roomIndex, x, y)
				|| !canPlaceDecorInRoom(roomIndex, x + 1, y)
				|| !canPlaceDecorInRoom(roomIndex, x, y + 1)
				|| !canPlaceDecorInRoom(roomIndex, x + 1, y + 1))
			{
				return false;
			}
			placeDecorTile(x, y);
			placeDecorTile(x + 1, y);
			placeDecorTile(x, y + 1);
			placeDecorTile(x + 1, y + 1);
			return true;
		};

		auto tryPlaceDecorLine = [&](int roomIndex, glm::ivec2 start, int length, bool horizontal)
		{
			for (int i = 0; i < length; i++)
			{
				int x = start.x + (horizontal ? i : 0);
				int y = start.y + (horizontal ? 0 : i);
				if (!canPlaceDecorInRoom(roomIndex, x, y)) { return false; }
			}
			for (int i = 0; i < length; i++)
			{
				int x = start.x + (horizontal ? i : 0);
				int y = start.y + (horizontal ? 0 : i);
				placeDecorTile(x, y);
			}
			return true;
		};

		auto roomCoverRatio = [&](int roomIndex)
		{
			const Rect &room = rooms[roomIndex];
			int coverCount = 0;
			int area = 0;
			for (int y = room.y + 1; y < room.y2() - 1; y++)
			{
				for (int x = room.x + 1; x < room.x2() - 1; x++)
				{
					auto &base = map.firstLayer.getBlockUnsafe(x, y);
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					area++;
					if (isWall(base.type)
						|| (over.type != Blocks::none && (isBlockColidable(over.type)
						|| over.type == Blocks::woodenDecorations)))
					{
						coverCount++;
					}
				}
			}
			if (area <= 0) { return 1.0f; }
			return (float)coverCount / (float)area;
		};

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			const Rect &room = rooms[roomIndex];
			float coverRatio = roomCoverRatio(roomIndex);
			int budget = room.isBigRoom ? 4 : 2;
			if (room.w * room.h > 260) { budget++; }
			if (room.isCave || room.isGrassRoom) { budget = std::min(budget, 2); }
			if (room.isWoodRoom) { budget++; }
			if (coverRatio > 0.12f) { budget = 0; }
			else if (coverRatio > 0.08f) { budget = std::min(budget, 1); }
			if (budget <= 0) { continue; }

			int attempts = budget * 4 + 4;
			for (int attempt = 0; attempt < attempts && budget > 0; attempt++)
			{
				int minX = room.x + 2;
				int maxX = room.x2() - 3;
				int minY = room.y + 2;
				int maxY = room.y2() - 3;
				if (minX > maxX || minY > maxY) { break; }
				int x = getRandomInt(rng, minX, maxX);
				int y = getRandomInt(rng, minY, maxY);
				int shape = getRandomInt(rng, 0, 2);
				bool placed = false;
				if (shape == 0)
				{
					placed = tryPlaceDecorBlock(roomIndex, x, y);
				}
				else
				{
					int length = getRandomInt(rng, 3, 5);
					bool horizontal = getRandomChance(rng, 0.5f);
					placed = tryPlaceDecorLine(roomIndex, {x, y}, length, horizontal);
				}
				if (placed) { budget--; }
			}
		}

		auto isDoorOutTile = [&](int x, int y)
		{
			for (auto &entry : doorConnections)
			{
				if (entry.doorOut.x == x && entry.doorOut.y == y) { return true; }
			}
			return false;
		};

		// Sparse corridor cover.
		for (int y = 1; y < sizeY - 1; y++)
		{
			for (int x = 1; x < sizeX - 1; x++)
			{
				if (findRoomIndexAt(x, y) >= 0) { continue; }
				if (isDoorOutTile(x, y)) { continue; }
				if (!canPlaceDecorTile(x, y)) { continue; }
				if (getRandomChance(rng, 0.008f))
				{
					placeDecorTile(x, y);
				}
			}
		}

		// Very rare cover right outside doors.
		for (auto &entry : doorConnections)
		{
			if (!getRandomChance(rng, 0.003f)) { continue; }
			int x = entry.doorOut.x;
			int y = entry.doorOut.y;
			if (!canPlaceDecorTile(x, y)) { continue; }
			placeDecorTile(x, y);
		}
	};

	placeWallDecorations();
	placeWoodDecorations();

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

	// Place spike traps on the second layer with simple patterns.
	auto placeSpikeTraps = [&]()
	{
		auto isDoorTooCloseLocal = [&](int roomIndex, glm::ivec2 pos, int minDist)
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

		auto canPlaceSpike = [&](int roomIndex, int x, int y)
		{
			const Rect &room = rooms[roomIndex];
			if (x <= room.x || x >= room.x2() - 1 || y <= room.y || y >= room.y2() - 1)
			{
				return false;
			}
			if (isDoorTooCloseLocal(roomIndex, {x, y}, 2)) { return false; }
			if (isDoorTileForRoom(roomIndex, x, y)) { return false; }
				auto &base = map.firstLayer.getBlockUnsafe(x, y);
				if (isWall(base.type) || base.type == Blocks::none) { return false; }
				auto &over = map.secondLayer.getBlockUnsafe(x, y);
			if (over.type != Blocks::none) { return false; }
			if (map.isCollidableAtPosSafe(x, y)) { return false; }
			int belowY = y + 1;
			if (belowY < 0 || belowY >= sizeY) { return false; }
			if (map.isCollidableAtPosSafe(x, belowY)) { return false; }
			return true;
		};

		auto placeSpike = [&](int x, int y)
		{
			map.secondLayer.getBlockUnsafe(x, y).type = Blocks::spikeTrap;
		};

		auto tryPlaceLine = [&](int roomIndex, glm::ivec2 start, int length, bool horizontal)
		{
			for (int i = 0; i < length; i++)
			{
				int x = start.x + (horizontal ? i : 0);
				int y = start.y + (horizontal ? 0 : i);
				if (!canPlaceSpike(roomIndex, x, y)) { return false; }
			}
			for (int i = 0; i < length; i++)
			{
				int x = start.x + (horizontal ? i : 0);
				int y = start.y + (horizontal ? 0 : i);
				placeSpike(x, y);
			}
			return true;
		};

		auto tryPlaceRectOutline = [&](int roomIndex, glm::ivec2 start, int w, int h)
		{
			for (int dx = 0; dx < w; dx++)
			{
				int x = start.x + dx;
				int yTop = start.y;
				int yBottom = start.y + h - 1;
				if (!canPlaceSpike(roomIndex, x, yTop) || !canPlaceSpike(roomIndex, x, yBottom))
				{
					return false;
				}
			}
			for (int dy = 1; dy < h - 1; dy++)
			{
				int y = start.y + dy;
				int xLeft = start.x;
				int xRight = start.x + w - 1;
				if (!canPlaceSpike(roomIndex, xLeft, y) || !canPlaceSpike(roomIndex, xRight, y))
				{
					return false;
				}
			}
			for (int dx = 0; dx < w; dx++)
			{
				int x = start.x + dx;
				placeSpike(x, start.y);
				placeSpike(x, start.y + h - 1);
			}
			for (int dy = 1; dy < h - 1; dy++)
			{
				int y = start.y + dy;
				placeSpike(start.x, y);
				placeSpike(start.x + w - 1, y);
			}
			return true;
		};

		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			const Rect &room = rooms[roomIndex];
			auto &outRoom = outInfo.rooms[roomIndex];
			if (outRoom.isSpawnRoom || outRoom.isExitRoom || outRoom.isEmptyRoom)
			{
				continue;
			}
			int area = room.w * room.h;
			bool isSmallRoom = !room.isBigRoom && area <= 220;
			float chance = 0.24f;
			if (isSmallRoom) { chance *= 0.35f; }
			if (!getRandomChance(rng, chance)) { continue; }
			int patterns = room.isBigRoom ? 2 : 1;
			if (isSmallRoom) { patterns = 1; }
			int attempts = 8;
			int minX = room.x + 2;
			int maxX = room.x2() - 3;
			int minY = room.y + 2;
			int maxY = room.y2() - 3;
			if (minX > maxX || minY > maxY) { continue; }
			for (int p = 0; p < patterns; p++)
			{
				bool placed = false;
				for (int attempt = 0; attempt < attempts && !placed; attempt++)
				{
					int pattern = getRandomInt(rng, 0, 2);
					if (pattern == 0)
					{
						int length = getRandomInt(rng, 3, 6);
						int x = getRandomInt(rng, minX, std::max(minX, maxX - length + 1));
						int y = getRandomInt(rng, minY, maxY);
						placed = tryPlaceLine(roomIndex, {x, y}, length, true);
					}
					else if (pattern == 1)
					{
						int length = getRandomInt(rng, 3, 6);
						int x = getRandomInt(rng, minX, maxX);
						int y = getRandomInt(rng, minY, std::max(minY, maxY - length + 1));
						placed = tryPlaceLine(roomIndex, {x, y}, length, false);
					}
					else
					{
						int w = getRandomInt(rng, 3, 6);
						int h = getRandomInt(rng, 3, 6);
						int x = getRandomInt(rng, minX, std::max(minX, maxX - w + 1));
						int y = getRandomInt(rng, minY, std::max(minY, maxY - h + 1));
						placed = tryPlaceRectOutline(roomIndex, {x, y}, w, h);
					}
				}
			}
		}
	};

	placeSpikeTraps();

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
				if (isWall(base.type)
					|| (over.type != Blocks::none && (isBlockColidable(over.type)
					|| over.type == Blocks::woodenDecorations
					|| over.type == Blocks::wallDecorations)))
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

	auto pickSpawnPosition = [&](int roomIndex, const Rect &room) -> std::optional<glm::ivec2>
	{
		auto isDoorTooCloseLocal = [&](glm::ivec2 pos, int minDist)
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

		auto canSpawnOnTileLocal = [&](BlockType type)
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

			if (isDoorTooCloseLocal(spawn, minDist)) { continue; }
			auto &tile = map.firstLayer.getBlockUnsafe(spawn.x, spawn.y);
			if (!canSpawnOnTileLocal(tile.type)) { continue; }
			if (isWall(tile.type) || tile.type == Blocks::none) { continue; }
			auto &over = map.secondLayer.getBlockUnsafe(spawn.x, spawn.y);
			if (over.type != Blocks::none && (isBlockColidable(over.type)
				|| over.type == Blocks::woodenDecorations
				|| over.type == Blocks::wallDecorations
				|| over.type == Blocks::exit
				|| over.type == Blocks::spikeTrap))
			{
				continue;
			}
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
			&& (centerOver.type == Blocks::none || (!isBlockColidable(centerOver.type)
			&& centerOver.type != Blocks::woodenDecorations
			&& centerOver.type != Blocks::wallDecorations
			&& centerOver.type != Blocks::exit
			&& centerOver.type != Blocks::spikeTrap))
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
				if (over.type != Blocks::none && (isBlockColidable(over.type)
					|| over.type == Blocks::woodenDecorations
					|| over.type == Blocks::wallDecorations
					|| over.type == Blocks::exit
					|| over.type == Blocks::spikeTrap))
				{
					continue;
				}
				return glm::ivec2{x, y};
			}
		}

		return {};
	};

	auto pickExitRoomIndex = [&]() -> int
	{
		if (rooms.empty()) { return -1; }
		int spawnIndex = outInfo.spawnRoomIndex.value_or(0);
		spawnIndex = std::clamp(spawnIndex, 0, (int)rooms.size() - 1);

		std::vector<int> dist(rooms.size(), -1);
		std::vector<int> queue;
		queue.reserve(rooms.size());
		dist[spawnIndex] = 0;
		queue.push_back(spawnIndex);

		for (size_t i = 0; i < queue.size(); i++)
		{
			int current = queue[i];
			for (int j = 0; j < (int)rooms.size(); j++)
			{
				if (!roomLinks[current][j]) { continue; }
				if (dist[j] != -1) { continue; }
				dist[j] = dist[current] + 1;
				queue.push_back(j);
			}
		}

		int bestIndex = -1;
		int bestDist = -1;
		int minRoomDistance = 3;
		for (int i = 0; i < (int)rooms.size(); i++)
		{
			if (i == spawnIndex) { continue; }
			if (outInfo.rooms[i].isEmptyRoom) { continue; }
			int d = dist[i];
			if (d < 0) { continue; }
			if (d < minRoomDistance) { continue; }
			if (d > bestDist)
			{
				bestDist = d;
				bestIndex = i;
			}
		}

		if (bestIndex == -1)
		{
			for (int i = 0; i < (int)rooms.size(); i++)
			{
				if (i == spawnIndex) { continue; }
				if (outInfo.rooms[i].isEmptyRoom) { continue; }
				int d = dist[i];
				if (d > bestDist)
				{
					bestDist = d;
					bestIndex = i;
				}
			}
		}

		if (bestIndex == -1)
		{
			bestIndex = spawnIndex;
		}
		return bestIndex;
	};

	auto pickExitPosition = [&](int roomIndex, const Rect &room) -> std::optional<glm::ivec2>
	{
		int minX = room.x + 2;
		int maxX = room.x2() - 3;
		int minY = room.y + 2;
		int maxY = room.y2() - 3;
		if (minX > maxX || minY > maxY) { return {}; }

		auto isExitSpotValid = [&](glm::ivec2 pos)
		{
			if (pos.x < minX || pos.x > maxX || pos.y < minY || pos.y > maxY) { return false; }
			if (isDoorTooClose(roomIndex, pos, 3)) { return false; }
			int belowY = pos.y + 1;
			if (belowY <= 0 || belowY >= map.size.y - 1) { return false; }
			if (map.isCollidableAtPosSafe(pos.x, belowY)) { return false; }
			auto &tile = map.firstLayer.getBlockUnsafe(pos.x, pos.y);
			if (!canSpawnOnTile(room, tile.type)) { return false; }
			if (isWall(tile.type) || tile.type == Blocks::none) { return false; }
			auto &over = map.secondLayer.getBlockUnsafe(pos.x, pos.y);
			if (over.type != Blocks::none) { return false; }
			return true;
		};

		glm::ivec2 center = room.center();
		if (isExitSpotValid(center)) { return center; }

		int centerAttempts = 8;
		for (int attempt = 0; attempt < centerAttempts; attempt++)
		{
			glm::ivec2 pos = {
				center.x + getRandomInt(rng, -2, 2),
				center.y + getRandomInt(rng, -2, 2)
			};
			if (isExitSpotValid(pos)) { return pos; }
		}

		int attempts = 12;
		for (int attempt = 0; attempt < attempts; attempt++)
		{
			glm::ivec2 pos = {
				getRandomInt(rng, minX, maxX),
				getRandomInt(rng, minY, maxY)
			};
			if (isExitSpotValid(pos)) { return pos; }
		}
		return {};
	};

	int exitRoomIndex = pickExitRoomIndex();
	if (exitRoomIndex >= 0 && exitRoomIndex < (int)rooms.size())
	{
		outInfo.exitRoomIndex = exitRoomIndex;
		outInfo.rooms[exitRoomIndex].isExitRoom = true;
		auto exitPick = pickExitPosition(exitRoomIndex, rooms[exitRoomIndex]);
		if (exitPick)
		{
			glm::ivec2 pos = *exitPick;
			auto &over = map.secondLayer.getBlockUnsafe(pos.x, pos.y);
			over.type = Blocks::exit;
			outInfo.exitPos = {pos.x + 0.5f, pos.y + 0.5f};
		}
	}

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


	// Flood fill cave floors and connect isolated pockets inside the room interior.
	auto ensureCaveRoomConnectivity = [&](int roomIndex)
	{
		if (roomIndex < 0 || roomIndex >= (int)rooms.size()) { return; }
		const Rect &room = rooms[roomIndex];
		if (!room.isCave) { return; }

		int minX = room.x + 1;
		int maxX = room.x2() - 2;
		int minY = room.y + 1;
		int maxY = room.y2() - 2;
		if (minX > maxX || minY > maxY) { return; }

		int roomW = room.w;
		int roomH = room.h;
		auto indexOf = [&](int x, int y)
		{
			return (x - room.x) + (y - room.y) * roomW;
		};

		auto isInterior = [&](int x, int y)
		{
			return x >= minX && x <= maxX && y >= minY && y <= maxY;
		};

		auto isWalkable = [&](int x, int y)
		{
			if (!isInterior(x, y)) { return false; }
			auto &tile = map.firstLayer.getBlockUnsafe(x, y);
			if (tile.type == Blocks::none || isWall(tile.type)) { return false; }
			if (map.isCollidableAtPosSafe(x, y)) { return false; }
			return true;
		};

		auto carveCaveTile = [&](int x, int y)
		{
			if (!isInterior(x, y)) { return; }
			auto &tile = map.firstLayer.getBlockUnsafe(x, y);
			tile.type = Blocks::caveFloor;
			auto &over = map.secondLayer.getBlockUnsafe(x, y);
			if (over.type != Blocks::none && isBlockColidable(over.type))
			{
				over.type = Blocks::none;
			}
		};

		std::vector<glm::ivec2> entrySeeds;
		auto addEntrySeed = [&](int x, int y)
		{
			if (!isInterior(x, y)) { return; }
			if (!isWalkable(x, y))
			{
				carveCaveTile(x, y);
			}
			for (auto s : entrySeeds)
			{
				if (s.x == x && s.y == y) { return; }
			}
			entrySeeds.push_back({x, y});
		};

		for (auto d : outInfo.rooms[roomIndex].doorPositions)
		{
			bool onNorth = d.y == room.y;
			bool onSouth = d.y == room.y2() - 2;
			bool onWest = d.x == room.x;
			bool onEast = d.x == room.x2() - 2;
			if (onNorth || onSouth)
			{
				int baseY = onNorth ? (d.y + 1) : d.y;
				int dirY = onNorth ? 1 : -1;
				for (int depth = 0; depth <= 1; depth++)
				{
					int y = baseY + dirY * depth;
					addEntrySeed(d.x, y);
					addEntrySeed(d.x + 1, y);
				}
			}
			if (onWest || onEast)
			{
				int baseX = onWest ? (d.x + 1) : d.x;
				int dirX = onWest ? 1 : -1;
				for (int depth = 0; depth <= 1; depth++)
				{
					int x = baseX + dirX * depth;
					addEntrySeed(x, d.y);
					addEntrySeed(x, d.y + 1);
				}
			}
		}

		std::vector<int> zoneId(roomW * roomH, -1);
		std::vector<glm::ivec2> zoneSeeds;
		std::vector<int> zoneSizes;

		const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				if (!isWalkable(x, y)) { continue; }
				int idx = indexOf(x, y);
				if (zoneId[idx] != -1) { continue; }

				int zoneIndex = (int)zoneSeeds.size();
				zoneSeeds.push_back({x, y});
				zoneSizes.push_back(0);
				std::vector<glm::ivec2> queue;
				queue.reserve(roomW * roomH / 2);
				queue.push_back({x, y});
				zoneId[idx] = zoneIndex;
				for (size_t i = 0; i < queue.size(); i++)
				{
					glm::ivec2 pos = queue[i];
					zoneSizes[zoneIndex]++;
					for (auto &dir : dirs)
					{
						int nx = pos.x + dir[0];
						int ny = pos.y + dir[1];
						if (!isWalkable(nx, ny)) { continue; }
						int nIdx = indexOf(nx, ny);
						if (zoneId[nIdx] != -1) { continue; }
						zoneId[nIdx] = zoneIndex;
						queue.push_back({nx, ny});
					}
				}
			}
		}

		if (zoneSeeds.size() <= 1) { return; }

		int mainZone = -1;
		for (auto s : entrySeeds)
		{
			int idx = indexOf(s.x, s.y);
			if (idx >= 0 && idx < (int)zoneId.size() && zoneId[idx] != -1)
			{
				mainZone = zoneId[idx];
				break;
			}
		}
		if (mainZone == -1)
		{
			mainZone = 0;
			for (int i = 1; i < (int)zoneSeeds.size(); i++)
			{
				if (zoneSizes[i] > zoneSizes[mainZone])
				{
					mainZone = i;
				}
			}
		}

		for (int i = 0; i < (int)zoneSeeds.size(); i++)
		{
			if (i == mainZone) { continue; }
			glm::ivec2 start = zoneSeeds[i];
			glm::ivec2 end = zoneSeeds[mainZone];
			bool horizontalFirst = getRandomChance(rng, 0.5f);
			if (horizontalFirst)
			{
				int stepX = (end.x >= start.x) ? 1 : -1;
				for (int x = start.x; x != end.x; x += stepX)
				{
					carveCaveTile(x, start.y);
				}
				int stepY = (end.y >= start.y) ? 1 : -1;
				for (int y = start.y; y != end.y; y += stepY)
				{
					carveCaveTile(end.x, y);
				}
			}
			else
			{
				int stepY = (end.y >= start.y) ? 1 : -1;
				for (int y = start.y; y != end.y; y += stepY)
				{
					carveCaveTile(start.x, y);
				}
				int stepX = (end.x >= start.x) ? 1 : -1;
				for (int x = start.x; x != end.x; x += stepX)
				{
					carveCaveTile(x, end.y);
				}
			}
			carveCaveTile(end.x, end.y);
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

	for (auto &room : outInfo.rooms)
	{
		room.doorPositions.clear();
	}
	for (auto &entry : doorConnections)
	{
		if (entry.roomIndex < 0 || entry.roomIndex >= (int)outInfo.rooms.size()) { continue; }
		bool exists = false;
		for (auto d : outInfo.rooms[entry.roomIndex].doorPositions)
		{
			if (d == entry.doorPos) { exists = true; break; }
		}
		if (!exists)
		{
			outInfo.rooms[entry.roomIndex].doorPositions.push_back(entry.doorPos);
		}
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

	auto findCorridorStyleForDoor = [&](glm::ivec2 doorPos, glm::ivec2 doorOut) -> int
	{
		for (auto &link : corridorLinks)
		{
			if (link.from == doorPos || link.to == doorPos
				|| link.from == doorOut || link.to == doorOut)
			{
				return link.style;
			}
		}
		return (int)Corridor_Dungeon;
	};

	auto forceDoorConnection = [&](int roomIndex, glm::ivec2 d, int style)
	{
		const Rect &room = rooms[roomIndex];
		auto carveOutsideTile = [&](int x, int y)
		{
			if (x < 0 || y < 0 || x >= sizeX || y >= sizeY) { return; }
			paintCorridorTile(x, y, style);
			auto &over = map.secondLayer.getBlockUnsafe(x, y);
			if (over.type != Blocks::none && isBlockColidable(over.type))
			{
				over.type = Blocks::none;
			}
		};

		bool onNorth = d.y == room.y;
		bool onSouth = d.y == room.y2() - 2;
		bool onWest = d.x == room.x;
		bool onEast = d.x == room.x2() - 2;
		if (onNorth)
		{
			int y = room.y - 1;
			for (int x = d.x; x <= d.x + 1; x++)
			{
				carveOutsideTile(x, y);
			}
		}
		if (onSouth)
		{
			int y = room.y2();
			for (int x = d.x; x <= d.x + 1; x++)
			{
				carveOutsideTile(x, y);
			}
		}
		if (onWest)
		{
			int x = room.x - 1;
			for (int y = d.y; y <= d.y + 1; y++)
			{
				carveOutsideTile(x, y);
			}
		}
		if (onEast)
		{
			int x = room.x2();
			for (int y = d.y; y <= d.y + 1; y++)
			{
				carveOutsideTile(x, y);
			}
		}
	};

	// Re-open door tiles and ensure corridor stubs meet the room openings.
	for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
	{
		clearDoorTilesForRoom(roomIndex);
	}
	for (auto &entry : doorConnections)
	{
		if (entry.roomIndex < 0 || entry.roomIndex >= (int)rooms.size()) { continue; }
		int style = entry.style;
		if (style == Corridor_Dungeon)
		{
			style = findCorridorStyleForDoor(entry.doorPos, entry.doorOut);
		}
		forceDoorConnection(entry.roomIndex, entry.doorPos, style);
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
					if (over.type != Blocks::none && (isBlockColidable(over.type)
						|| over.type == Blocks::woodenDecorations
						|| over.type == Blocks::wallDecorations
						|| over.type == Blocks::exit))
					{
						continue;
					}
					fallback = candidate;
					break;
				}
				glm::vec2 spawnPos = {fallback.x + 0.5f, fallback.y + 0.5f};
				outRoom.enemySpawnPositions.push_back(spawnPos);
				outInfo.enemySpawnPositions.push_back(spawnPos);
			}
		}
	}

	// Ensure each room has enough spawn slots for items/enemies.
	{
		const int minSpawnSlots = 7;
		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			const Rect &room = rooms[roomIndex];
			auto &outRoom = outInfo.rooms[roomIndex];
			if (outRoom.isEmptyRoom) { continue; }
			int desired = minSpawnSlots;
			if (room.isBigRoom || room.isCaveMaze) { desired += 3; }
			if ((int)outRoom.enemySpawnPositions.size() >= desired) { continue; }

			std::vector<glm::ivec2> used;
			used.reserve(outRoom.enemySpawnPositions.size());
			for (auto &pos : outRoom.enemySpawnPositions)
			{
				used.push_back({(int)pos.x, (int)pos.y});
			}

			auto isUsed = [&](glm::ivec2 pos)
			{
				for (auto &u : used)
				{
					if (u.x == pos.x && u.y == pos.y) { return true; }
				}
				return false;
			};

			std::vector<glm::ivec2> candidates;
			int minX = room.x + 1;
			int maxX = room.x2() - 2;
			int minY = room.y + 1;
			int maxY = room.y2() - 2;
			if (minX > maxX || minY > maxY) { continue; }
			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					glm::ivec2 pos = {x, y};
					if (isUsed(pos)) { continue; }
					if (isDoorTooClose(roomIndex, pos, cosmetics.doorSpawnMinDist)) { continue; }
					auto &tile = map.firstLayer.getBlockUnsafe(x, y);
					if (!canSpawnOnTile(room, tile.type)) { continue; }
					if (isWall(tile.type) || tile.type == Blocks::none) { continue; }
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					if (over.type != Blocks::none && (isBlockColidable(over.type)
						|| over.type == Blocks::woodenDecorations
						|| over.type == Blocks::wallDecorations
						|| over.type == Blocks::exit
						|| over.type == Blocks::spikeTrap))
					{
						continue;
					}
					candidates.push_back(pos);
				}
			}

			while ((int)outRoom.enemySpawnPositions.size() < desired && !candidates.empty())
			{
				int index = getRandomInt(rng, 0, (int)candidates.size() - 1);
				glm::ivec2 pos = candidates[index];
				candidates[index] = candidates.back();
				candidates.pop_back();
				outRoom.enemySpawnPositions.push_back({pos.x + 0.5f, pos.y + 0.5f});
				outInfo.enemySpawnPositions.push_back({pos.x + 0.5f, pos.y + 0.5f});
				used.push_back(pos);
			}
		}
	}

	// Final room flags for size and cover after all generation passes.
	{
		const int smallRoomAreaThreshold = 240;
		const float lowCoverThreshold = 0.05f;
		for (int roomIndex = 0; roomIndex < (int)rooms.size(); roomIndex++)
		{
			const Rect &room = rooms[roomIndex];
			auto &outRoom = outInfo.rooms[roomIndex];
			int area = room.w * room.h;
			outRoom.isSmallRoom = !room.isBigRoom && area <= smallRoomAreaThreshold;

			int coverCount = 0;
			int total = 0;
			for (int y = room.y + 1; y < room.y2() - 1; y++)
			{
				for (int x = room.x + 1; x < room.x2() - 1; x++)
				{
					total++;
					auto &base = map.firstLayer.getBlockUnsafe(x, y);
					auto &over = map.secondLayer.getBlockUnsafe(x, y);
					bool hasCover = isWall(base.type) || base.type == Blocks::none;
					if (over.type != Blocks::none && (isBlockColidable(over.type)
						|| over.type == Blocks::woodenDecorations
						|| over.type == Blocks::wallDecorations))
					{
						hasCover = true;
					}
					if (hasCover) { coverCount++; }
				}
			}
			float ratio = total > 0 ? (float)coverCount / (float)total : 1.0f;
			outRoom.isLowCoverRoom = ratio < lowCoverThreshold;
		}
	}

	// Final pass to restore room perimeter walls and doors.
	enforceRoomPerimeters();
}
