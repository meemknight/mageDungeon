#include <gameplay/roomLightingSystem.h>
#include <gameplay/map.h>
#include <gameplay/blocks.h>
#include <gameplay/Physics.h>
#include <gameplay/doors.h>
#include <worldGen/floorGen.h>
#include <gl2d/gl2d.h>
#include <queue>
#include <algorithm>
#include <cmath>
#include <vector>

bool isInsideRoomTriggerBounds(const FloorRoom &room, const glm::vec4 &aabb, float inset)
{
	float clampedInset = std::max(0.0f, inset);
	float minX = room.pos.x + clampedInset;
	float minY = room.pos.y + clampedInset;
	float maxX = room.pos.x + room.size.x - clampedInset;
	float maxY = room.pos.y + room.size.y - clampedInset;

	if (maxX > minX && maxY > minY)
	{
		if (aabb.x >= minX && aabb.y >= minY
			&& (aabb.x + aabb.z) <= maxX && (aabb.y + aabb.w) <= maxY)
		{
			return true;
		}
	}

	// Fallback for doorway-edge entries: center must be inside the room interior
	// (past the wall/door rim), so trigger cannot happen before passing the door.
	float centerMinX = room.pos.x + 1.0f;
	float centerMinY = room.pos.y + 1.0f;
	float centerMaxX = room.pos.x + room.size.x - 1.0f;
	float centerMaxY = room.pos.y + room.size.y - 1.0f;
	if (centerMaxX <= centerMinX || centerMaxY <= centerMinY) { return false; }

	glm::vec2 center = {aabb.x + aabb.z * 0.5f, aabb.y + aabb.w * 0.5f};
	return center.x >= centerMinX && center.x <= centerMaxX
		&& center.y >= centerMinY && center.y <= centerMaxY;
}

namespace
{
	bool isDilationWallTileLit(const RoomLightingSystem &lighting, Map &map, int x, int y);
	bool isTileVisibleWithoutDoorBoost(const RoomLightingSystem &lighting, Map &map, int x, int y);

	bool inBounds(glm::ivec2 size, int x, int y)
	{
		return x >= 0 && y >= 0 && x < size.x && y < size.y;
	}

	bool hasTile(Map &map, int x, int y)
	{
		if (!inBounds(map.size, x, y)) { return false; }
		auto *base = map.firstLayer.getBlockSafe(x, y);
		auto *over = map.secondLayer.getBlockSafe(x, y);
		if (!base && !over) { return false; }
		auto baseType = base ? base->type : Blocks::none;
		auto overType = over ? over->type : Blocks::none;
		return baseType != Blocks::none || overType != Blocks::none;
	}

	bool isWalkable(Map &map, int x, int y)
	{
		if (!inBounds(map.size, x, y)) { return false; }
		if (!hasTile(map, x, y)) { return false; }
		return !map.isCollidableAtPosSafe(x, y);
	}

	bool hasLitWalkableNeighbor(const RoomLightingSystem &lighting, Map &map, int x, int y)
	{
		const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		for (auto d : dirs)
		{
			int nx = x + d.x;
			int ny = y + d.y;
			if (!inBounds(lighting.size, nx, ny)) { continue; }
			if (!isWalkable(map, nx, ny)) { continue; }
			// Walls light up only when directly touching a lit non-wall tile.
			// Use the no-door-boost query here to avoid recursive wall <-> door visibility loops.
			if (isTileVisibleWithoutDoorBoost(lighting, map, nx, ny))
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

		return hasLitWalkableNeighbor(lighting, map, x, y);
	}

	// Same visibility rule as RoomLightingSystem::isTileVisible, but it skips door boost checks.
	// This keeps wall dilation evaluation finite and prevents recursion between door boosts and walls.
	bool isTileVisibleWithoutDoorBoost(const RoomLightingSystem &lighting, Map &map, int tx, int ty)
	{
		if (!inBounds(lighting.size, tx, ty)) { return true; }
		if (!hasTile(map, tx, ty)) { return true; }

		int tidx = tx + ty * lighting.size.x;
		if (lighting.revealedTiles[tidx]) { return true; }

		auto isRevealedLightSource = [&](int sx, int sy)
		{
			if (!inBounds(lighting.size, sx, sy)) { return false; }
			int sIdx = sx + sy * lighting.size.x;
			if (!lighting.revealedTiles[sIdx]) { return false; }
			return isWalkable(map, sx, sy);
		};

		int tRoomIndex = lighting.roomByTile[tidx];
		bool tInUnexploredRoom = tRoomIndex >= 0 && tRoomIndex < (int)lighting.roomLit.size() && !lighting.roomLit[tRoomIndex];
		int tCorridorIndex = lighting.corridorComponentByTile[tidx];
		bool tInUnexploredCorridor = tCorridorIndex >= 0 && tCorridorIndex < (int)lighting.corridorLit.size() && !lighting.corridorLit[tCorridorIndex];

		if (isDilationWallTileLit(lighting, map, tx, ty))
		{
			return true;
		}

		if (!tInUnexploredRoom && !tInUnexploredCorridor)
		{
			for (int oy = -1; oy <= 1; oy++)
			{
				for (int ox = -1; ox <= 1; ox++)
				{
					if (ox == 0 && oy == 0) { continue; }
					int nx = tx + ox;
					int ny = ty + oy;
					if (isRevealedLightSource(nx, ny)) { return true; }
				}
			}
		}

		if (isRevealedLightSource(tx, ty + 1)) { return true; }
		if (isRevealedLightSource(tx, ty + 2)) { return true; }

		if (tInUnexploredRoom || tInUnexploredCorridor)
		{
			return false;
		}

		return false;
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

void RoomLightingSystem::resetForFloor(Map &map, const FloorInfo &floorInfo, const DoorHolder &doorHolder)
{
	size = map.size;
	int count = std::max(0, size.x * size.y);
	revealedTiles.assign(count, 0);
	revealFade.assign(count, 0.0f);
	roomByTile.assign(count, -1);
	doorBoostSourcesByTile.assign(count, {});
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

	// Door lighting boost patterns using real door orientation.
	for (const auto &doorPair : doorHolder.doors)
	{
		const auto &doorPos = doorPair.first;
		const auto &door = doorPair.second;
		std::vector<int> sources;
		auto addSource = [&](int x, int y)
		{
			if (!inBounds(size, x, y)) { return; }
			sources.push_back(toIndex(x, y));
		};

		if (door.orientation == Door::Orientation::Horizontal)
		{
			// Horizontal doors can be lit from top and bottom door tiles.
			addSource(doorPos.x, doorPos.y);
			addSource(doorPos.x + 1, doorPos.y);
			addSource(doorPos.x, doorPos.y + 1);
			addSource(doorPos.x + 1, doorPos.y + 1);
		}
		else
		{
			addSource(doorPos.x, doorPos.y);
			addSource(doorPos.x + 1, doorPos.y);
		}

		if (sources.empty()) { continue; }

		auto linkBoost = [&](int x, int y)
		{
			if (!inBounds(size, x, y)) { return; }
			if (!hasTile(map, x, y)) { return; }
			auto &dst = doorBoostSourcesByTile[toIndex(x, y)];
			for (int source : sources)
			{
				dst.push_back(source);
			}
		};

		if (door.orientation == Door::Orientation::Horizontal)
		{
			// Horizontal door at X,Y (only when X,Y is lit):
			// X Y
			// X+1 Y
			// X Y+1
			// X+1 Y+1
			// X Y-1
			// X Y-2
			// X+1 Y-1
			// X+1 Y-2
			// X-1 Y
			// X+3 Y
			linkBoost(doorPos.x,     doorPos.y);
			linkBoost(doorPos.x + 1, doorPos.y);
			linkBoost(doorPos.x,     doorPos.y + 1);
			linkBoost(doorPos.x + 1, doorPos.y + 1);
			linkBoost(doorPos.x,     doorPos.y - 1);
			linkBoost(doorPos.x,     doorPos.y - 2);
			linkBoost(doorPos.x + 1, doorPos.y - 1);
			linkBoost(doorPos.x + 1, doorPos.y - 2);
			linkBoost(doorPos.x - 1, doorPos.y);
			linkBoost(doorPos.x + 3, doorPos.y);
		}
		else if (door.orientation == Door::Orientation::Vertical)
		{
			// Vertical door: light just below when door tile is lit.
			linkBoost(doorPos.x, doorPos.y + 1);
			linkBoost(doorPos.x + 1, doorPos.y + 1);
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

	// Horizontal-door boost: extra tiles become visible when the door source is lit.
	if (idx >= 0 && idx < (int)doorBoostSourcesByTile.size())
	{
		for (int sourceIdx : doorBoostSourcesByTile[idx])
		{
			if (sourceIdx >= 0 && sourceIdx < (int)revealedTiles.size())
			{
				int sx = sourceIdx % size.x;
				int sy = sourceIdx / size.x;
				if (isTileVisibleWithoutDoorBoost(*this, map, sx, sy))
				{
					return true;
				}
			}
		}
	}

	return isTileVisibleWithoutDoorBoost(*this, map, x, y);
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
