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