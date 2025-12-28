#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <queue>
#include <limits>
#include <cmath>
#include <algorithm>
#include <gameplay/map.h>


// Returns path INCLUDING start? Here: returns a list of tiles to visit (excluding start, including goal if reachable).
std::vector<glm::ivec2> findPathAStar8(Map &map,
	const glm::ivec2 &start,
	const glm::ivec2 &goal);

bool CanChaseDirect(Map &map, glm::vec2 enemyPos, glm::vec2 playerPos);


std::size_t PickLookaheadIndex(Map &map,
	glm::vec2 enemyPos,
	const std::vector<glm::ivec2> &path,
	std::size_t currentIndex);


static inline bool IsBlockedTile(Map &map, int x, int y)
{
	return map.isCollidableAtPosSafe(x, y);
}


static bool IsBlockedTileLineOfSight(Map &map, int tx, int ty)
{
	// Treat outside as blocked
	if (tx < 0 || ty < 0 || tx >= map.size.x || ty >= map.size.y) return true;
	// If your collision check expects world coords, use tile center:
	return map.isCollidableAtPosSafe((float)tx + 0.5f, (float)ty + 0.5f);
}


// Bresenham LOS over tiles (8-dir “ray” through grid)
bool HasLineOfSightTiles(Map &map, glm::ivec2 a, glm::ivec2 b);


// “Direct chase” just means LOS is clear between current tile and player tile.
// If you want stricter collision safety, you can also do a couple map.isCollidableAtPosSafe
// probes along the segment, but usually Bresenham LOS is enough for grid worlds.
bool CanChaseDirectSight(Map &map, const glm::vec2 &enemyPos, const glm::vec2 &playerPos);

inline static glm::ivec2 WorldToTile(const glm::vec2 &p)
{
	// Assuming 1 world unit per tile
	return glm::ivec2((int)std::floor(p.x), (int)std::floor(p.y));
}
