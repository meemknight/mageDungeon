#include <gameplay/roomLightingSystem.h>
#include <gameplay/map.h>
#include <gameplay/blocks.h>
#include <gameplay/Physics.h>
#include <worldGen/floorGen.h>
#include <gl2d/gl2d.h>
#include <queue>
#include <algorithm>
#include <cmath>
#include <vector>

bool isInsideRoomTriggerBounds(const FloorRoom &room, const glm::vec4 &aabb, float inset)
{
	float minX = room.pos.x + inset;
	float minY = room.pos.y + inset;
	float maxX = room.pos.x + room.size.x - inset;
	float maxY = room.pos.y + room.size.y - inset;
	if (maxX <= minX || maxY <= minY) { return false; }
	return aabb.x >= minX && aabb.y >= minY
		&& (aabb.x + aabb.z) <= maxX && (aabb.y + aabb.w) <= maxY;
}

namespace
{
	bool inBounds(glm::ivec2 size, int x, int y)
	{
		return x >= 0 && y >= 0 && x < size.x && y < size.y;
	}

	bool hasTile(Map &map, int x, int y)
	{
		auto base = map.firstLayer.getBlockUnsafe(x, y).type;
		auto over = map.secondLayer.getBlockUnsafe(x, y).type;
		return base != Blocks::none || over != Blocks::none;
	}

	bool isWalkable(Map &map, int x, int y)
	{
		if (!inBounds(map.size, x, y)) { return false; }
		if (!hasTile(map, x, y)) { return false; }
		return !map.isCollidableAtPosSafe(x, y);
	}

	bool hasLitNeighbor(const RoomLightingSystem &lighting, int x, int y)
	{
		auto idxOf = [&](int tx, int ty)
		{
			return tx + ty * lighting.size.x;
		};
		const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		for (auto d : dirs)
		{
			int nx = x + d.x;
			int ny = y + d.y;
			if (!inBounds(lighting.size, nx, ny)) { continue; }
			int nIdx = idxOf(nx, ny);
			if (lighting.revealedTiles[nIdx])
			{
				return true;
			}
		}
		return false;
	}

	// One-tile light dilation: reveal nearby wall tiles around lit areas.
	bool isDilationWallTileLit(const RoomLightingSystem &lighting, Map &map, int x, int y)
	{
		if (!inBounds(lighting.size, x, y)) { return false; }
		if (!hasTile(map, x, y)) { return false; }
		if (isWalkable(map, x, y)) { return false; }

		return hasLitNeighbor(lighting, x, y);
	}

	// Second light ring: keep tiles mostly dark, but slightly visible.
	bool isLowLightTile(const RoomLightingSystem &lighting, Map &map, int x, int y)
	{
		if (!inBounds(lighting.size, x, y)) { return false; }
		if (!hasTile(map, x, y)) { return false; }
		if (!isWalkable(map, x, y)) { return false; } // keep wall lighting crisp (no 0.8 wall band)

		int idx = x + y * lighting.size.x;
		if (lighting.revealedTiles[idx]) { return false; }
		if (isDilationWallTileLit(lighting, map, x, y)) { return false; }

		const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		for (auto d : dirs)
		{
			int nx = x + d.x;
			int ny = y + d.y;
			if (!inBounds(lighting.size, nx, ny)) { continue; }
			if (!hasTile(map, nx, ny)) { continue; }

			int nIdx = nx + ny * lighting.size.x;
			if (lighting.revealedTiles[nIdx] || isDilationWallTileLit(lighting, map, nx, ny))
			{
				return true;
			}
		}
		return false;
	}

	// Builds a small radial alpha texture used to soften light edges without shaders.
	gl2d::Texture &getSoftEdgeGradientTexture()
	{
		static gl2d::Texture gradient;
		if (gradient.isValid()) { return gradient; }

		constexpr int size = 24;
		std::vector<unsigned char> data(size * size * 4, 0);
		float center = (size - 1) * 0.5f;
		float maxDist = center * 1.12f;

		for (int y = 0; y < size; y++)
		{
			for (int x = 0; x < size; x++)
			{
				float dx = (float)x - center;
				float dy = (float)y - center;
				float dist = std::sqrt(dx * dx + dy * dy);
				float t = 1.0f - glm::clamp(dist / maxDist, 0.0f, 1.0f);
				float a = t * t;

				int idx = (x + y * size) * 4;
				data[idx + 0] = 0;
				data[idx + 1] = 0;
				data[idx + 2] = 0;
				data[idx + 3] = (unsigned char)glm::clamp(a * 255.0f, 0.0f, 255.0f);
			}
		}

		gradient.createFromBuffer((const char *)data.data(), size, size, false, false);
		return gradient;
	}
}

int RoomLightingSystem::toIndex(int x, int y) const
{
	return x + y * size.x;
}

void RoomLightingSystem::revealTile(int x, int y)
{
	if (!inBounds(size, x, y)) { return; }
	int idx = toIndex(x, y);
	if (!revealedTiles[idx])
	{
		revealedTiles[idx] = 1;
		revealFade[idx] = 1.0f;
	}
}

void RoomLightingSystem::resetForFloor(Map &map, const FloorInfo &floorInfo)
{
	size = map.size;
	int count = std::max(0, size.x * size.y);
	revealedTiles.assign(count, 0);
	revealFade.assign(count, 0.0f);
	roomByTile.assign(count, -1);
	roomLit.assign(floorInfo.rooms.size(), 0);

	for (int roomIndex = 0; roomIndex < (int)floorInfo.rooms.size(); roomIndex++)
	{
		const auto &room = floorInfo.rooms[roomIndex];
		int minX = std::max(0, room.pos.x);
		int minY = std::max(0, room.pos.y);
		int maxX = std::min(size.x, room.pos.x + room.size.x);
		int maxY = std::min(size.y, room.pos.y + room.size.y);
		for (int y = minY; y < maxY; y++)
		{
			for (int x = minX; x < maxX; x++)
			{
				roomByTile[toIndex(x, y)] = roomIndex;
			}
		}
	}

	// Spawn room starts visible so the player is never in darkness on floor load.
	if (floorInfo.spawnRoomIndex && *floorInfo.spawnRoomIndex >= 0
		&& *floorInfo.spawnRoomIndex < (int)floorInfo.rooms.size())
	{
		int roomIndex = *floorInfo.spawnRoomIndex;
		roomLit[roomIndex] = 1;
		const auto &room = floorInfo.rooms[roomIndex];
		int minX = std::max(0, room.pos.x);
		int minY = std::max(0, room.pos.y);
		int maxX = std::min(size.x, room.pos.x + room.size.x);
		int maxY = std::min(size.y, room.pos.y + room.size.y);
		for (int y = minY; y < maxY; y++)
		{
			for (int x = minX; x < maxX; x++)
			{
				int idx = toIndex(x, y);
				revealedTiles[idx] = 1;
				revealFade[idx] = 0.0f;
			}
		}
	}

	corridorFloorComponentByTile.assign(count, -1);
	corridorComponentByTile.assign(count, -1);
	corridorRevealTiles.clear();
	corridorLit.clear();

	std::vector<unsigned char> visited(count, 0);
	std::vector<unsigned char> wallAdded(count, 0);
	std::queue<glm::ivec2> q;
	const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

	for (int y = 0; y < size.y; y++)
	{
		for (int x = 0; x < size.x; x++)
		{
			int idx = toIndex(x, y);
			if (visited[idx]) { continue; }
			if (roomByTile[idx] != -1) { continue; }
			if (!isWalkable(map, x, y)) { continue; }

			int componentId = (int)corridorRevealTiles.size();
			corridorRevealTiles.push_back({});
			corridorLit.push_back(0);

			q.push({x, y});
			visited[idx] = 1;
			while (!q.empty())
			{
				auto p = q.front();
				q.pop();
				int pIdx = toIndex(p.x, p.y);
				corridorFloorComponentByTile[pIdx] = componentId;
				corridorRevealTiles[componentId].push_back(pIdx);

				for (auto d : dirs)
				{
					int nx = p.x + d.x;
					int ny = p.y + d.y;
					if (!inBounds(size, nx, ny)) { continue; }
					int nIdx = toIndex(nx, ny);
					if (roomByTile[nIdx] != -1) { continue; }
					if (!isWalkable(map, nx, ny)) { continue; }
					if (visited[nIdx]) { continue; }
					visited[nIdx] = 1;
					q.push({nx, ny});
				}
			}

			for (int tileIdx : corridorRevealTiles[componentId])
			{
				int tx = tileIdx % size.x;
				int ty = tileIdx / size.x;
				for (auto d : dirs)
				{
					int nx = tx + d.x;
					int ny = ty + d.y;
					if (!inBounds(size, nx, ny)) { continue; }
					int nIdx = toIndex(nx, ny);
					if (roomByTile[nIdx] != -1) { continue; }
					if (isWalkable(map, nx, ny)) { continue; }
					if (wallAdded[nIdx]) { continue; }
					wallAdded[nIdx] = 1;
					corridorRevealTiles[componentId].push_back(nIdx);
				}
			}

			for (int tileIdx : corridorRevealTiles[componentId])
			{
				corridorComponentByTile[tileIdx] = componentId;
			}
		}
	}
}

void RoomLightingSystem::revealRoom(int roomIndex, const FloorInfo &floorInfo)
{
	if (roomIndex < 0 || roomIndex >= (int)floorInfo.rooms.size()) { return; }
	if (roomLit[roomIndex]) { return; }
	roomLit[roomIndex] = 1;

	const auto &room = floorInfo.rooms[roomIndex];
	int minX = std::max(0, room.pos.x);
	int minY = std::max(0, room.pos.y);
	int maxX = std::min(size.x, room.pos.x + room.size.x);
	int maxY = std::min(size.y, room.pos.y + room.size.y);
	for (int y = minY; y < maxY; y++)
	{
		for (int x = minX; x < maxX; x++)
		{
			revealTile(x, y);
		}
	}

}

void RoomLightingSystem::revealCorridorAtTile(Map &map, glm::ivec2 tile)
{
	if (!inBounds(size, tile.x, tile.y)) { return; }
	int idx = toIndex(tile.x, tile.y);
	if (roomByTile[idx] != -1) { return; }
	if (!isWalkable(map, tile.x, tile.y)) { return; }
	int componentId = corridorFloorComponentByTile[idx];
	if (componentId < 0 || componentId >= (int)corridorRevealTiles.size()) { return; }
	if (corridorLit[componentId]) { return; }
	corridorLit[componentId] = 1;

	for (int tileIdx : corridorRevealTiles[componentId])
	{
		int x = tileIdx % size.x;
		int y = tileIdx / size.x;
		revealTile(x, y);
	}
}

void RoomLightingSystem::update(float deltaTime, Map &map, const FloorInfo &floorInfo,
	const glm::vec4 &playerAabb, float roomTriggerInset)
{
	if (size.x <= 0 || size.y <= 0) { return; }

	for (float &fade : revealFade)
	{
		if (fade <= 0.0f) { continue; }
		fade -= deltaTime * fadeSpeed;
		if (fade < 0.0f) { fade = 0.0f; }
	}

	for (int roomIndex = 0; roomIndex < (int)floorInfo.rooms.size(); roomIndex++)
	{
		if (roomLit[roomIndex]) { continue; }
		if (isInsideRoomTriggerBounds(floorInfo.rooms[roomIndex], playerAabb, roomTriggerInset))
		{
			revealRoom(roomIndex, floorInfo);
		}
	}

	glm::vec2 center = {playerAabb.x + playerAabb.z * 0.5f, playerAabb.y + playerAabb.w * 0.5f};
	glm::ivec2 playerTile = {(int)std::floor(center.x), (int)std::floor(center.y)};
	revealCorridorAtTile(map, playerTile);
}

bool RoomLightingSystem::isTileVisible(Map &map, int x, int y) const
{
	if (!inBounds(size, x, y)) { return true; }
	if (!hasTile(map, x, y)) { return true; }
	int idx = x + y * size.x;
	if (revealedTiles[idx])
	{
		return true;
	}

	int roomIndex = roomByTile[idx];
	bool isInUnexploredRoom = roomIndex >= 0 && roomIndex < (int)roomLit.size() && !roomLit[roomIndex];
	int corridorIndex = corridorComponentByTile[idx];
	bool isInUnexploredCorridor = corridorIndex >= 0 && corridorIndex < (int)corridorLit.size() && !corridorLit[corridorIndex];

	// One-ring expansion: neighbors of revealed tiles are also lit.
	if (!isInUnexploredRoom && !isInUnexploredCorridor)
	{
		for (int oy = -1; oy <= 1; oy++)
		{
			for (int ox = -1; ox <= 1; ox++)
			{
				if (ox == 0 && oy == 0) { continue; }
				int nx = x + ox;
				int ny = y + oy;
				if (!inBounds(size, nx, ny)) { continue; }
				int nIdx = nx + ny * size.x;
				if (revealedTiles[nIdx])
				{
					return true;
				}
			}
		}
	}

	// Extra up extension: 1 tile above and 2 tiles above revealed tiles.
	int belowY = y + 1;
	if (inBounds(size, x, belowY))
	{
		int belowIdx = x + belowY * size.x;
		if (revealedTiles[belowIdx])
		{
			return true;
		}
	}
	int belowY2 = y + 2;
	if (inBounds(size, x, belowY2))
	{
		int belowIdx2 = x + belowY2 * size.x;
		if (revealedTiles[belowIdx2])
		{
			return true;
		}
	}

	if (isInUnexploredRoom || isInUnexploredCorridor)
	{
		// Keep unexplored rooms/corridors in shadow (except the explicit up-extension above).
		return false;
	}

	return isDilationWallTileLit(*this, map, x, y);
}

void RoomLightingSystem::renderOverlay(gl2d::Renderer2D &renderer, Map &map)
{
	if (size.x <= 0 || size.y <= 0) { return; }

	auto viewRect = renderer.getViewRect();
	glm::ivec4 view = {};
	view.x = std::max(0, (int)std::floor(viewRect.x) - 2);
	view.y = std::max(0, (int)std::floor(viewRect.y) - 2);
	view.z = std::min(size.x - 1, (int)std::ceil(viewRect.x + viewRect.z) + 2);
	view.w = std::min(size.y - 1, (int)std::ceil(viewRect.y + viewRect.w) + 2);

	auto isDarkTile = [&](int x, int y)
	{
		if (!inBounds(size, x, y)) { return false; }
		if (!hasTile(map, x, y)) { return false; }
		return !isTileVisible(map, x, y);
	};

	auto hasDarkInRadius = [&](int x, int y, int radius)
	{
		for (int oy = -radius; oy <= radius; oy++)
		{
			for (int ox = -radius; ox <= radius; ox++)
			{
				if (ox == 0 && oy == 0) { continue; }
				if (isDarkTile(x + ox, y + oy)) { return true; }
			}
		}
		return false;
	};

	for (int y = view.y; y <= view.w; y++)
	{
		for (int x = view.x; x <= view.z; x++)
		{
			if (!inBounds(size, x, y)) { continue; }
			if (!hasTile(map, x, y)) { continue; }

			int idx = toIndex(x, y);
			float alpha = 0.0f;
			if (!isTileVisible(map, x, y))
			{
				alpha = (enableLowLightLayer && isLowLightTile(*this, map, x, y)) ? 0.8f : 1.0f;
			}
			else if (revealFade[idx] > 0.0f)
			{
				alpha = revealFade[idx];
			}
			else if (enableSoftEdge)
			{
				// Multi-segment penumbra using layered dark bands.
				if (hasDarkInRadius(x, y, 1)) { alpha += softEdgeNearAlpha * 0.85f; }
				if (hasDarkInRadius(x, y, 2)) { alpha += softEdgeFarAlpha * 0.90f; }
				if (hasDarkInRadius(x, y, 3)) { alpha += softEdgeFarAlpha * 0.50f; }
				alpha = glm::clamp(alpha, 0.0f, 0.50f);
			}
			if (alpha <= 0.0f) { continue; }

			renderer.renderRectangle({(float)x, (float)y, 1.0f, 1.0f}, {0, 0, 0, alpha});
		}
	}

}
