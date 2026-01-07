#include "aStar.h"



// Bresenham LOS over tiles (8-dir “ray” through grid)
bool HasLineOfSightTiles(Map &map, glm::ivec2 a, glm::ivec2 b)
{
	int x0 = a.x, y0 = a.y;
	int x1 = b.x, y1 = b.y;

	int dx = std::abs(x1 - x0);
	int dy = std::abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;

	int err = dx - dy;

	// If you want walls to block even when standing "inside" them, check start too.
	// Usually you skip start tile and check everything after it.
	while (!(x0 == x1 && y0 == y1))
	{
		int e2 = err * 2;

		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }

		// Don’t block on the destination tile if you want “seeing the player”
		if (x0 == x1 && y0 == y1) break;

		if (IsBlockedTileLineOfSight(map, x0, y0))
			return false;
	}
	return true;
}

// “Direct chase” just means LOS is clear between current tile and player tile.
// If you want stricter collision safety, you can also do a couple map.isCollidableAtPosSafe
// probes along the segment, but usually Bresenham LOS is enough for grid worlds.
bool CanChaseDirectSight(Map &map, const glm::vec2 &enemyPos, const glm::vec2 &playerPos)
{
	glm::ivec2 eT = WorldToTile(enemyPos);
	glm::ivec2 pT = WorldToTile(playerPos);
	return HasLineOfSightTiles(map, eT, pT);
}



// Supercover Bresenham: marks all cells touched by the segment from a->b.
bool HasLineOfSightGrid(Map &map, glm::ivec2 a, glm::ivec2 b)
{
	int x0 = a.x, y0 = a.y;
	int x1 = b.x, y1 = b.y;

	int dx = std::abs(x1 - x0);
	int dy = std::abs(y1 - y0);

	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;

	int err = dx - dy;

	// Check start
	if (IsBlockedTile(map, x0, y0)) return false;

	while (!(x0 == x1 && y0 == y1))
	{
		int e2 = err * 2;

		int xPrev = x0;
		int yPrev = y0;

		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }

		// Supercover: if we moved diagonally, also cover the “side” tiles
		// to avoid cutting corners through blocked cells.
		if (x0 != xPrev && y0 != yPrev)
		{
			// Only blocked if the diagonal passes between TWO blockers (tight corner).
			if (IsBlockedTile(map, xPrev, y0) && IsBlockedTile(map, x0, yPrev))
				return false;
		}

		if (IsBlockedTile(map, x0, y0)) return false;
	}

	return true;
}

std::vector<glm::ivec2> SmoothPathLOS(Map &map, const std::vector<glm::ivec2> &path)
{
	if (path.size() <= 2) return path;

	std::vector<glm::ivec2> out;
	out.reserve(path.size());

	std::size_t i = 0;
	out.push_back(path[i]);

	while (i + 1 < path.size())
	{
		// take the farthest j we can see from i
		std::size_t bestJ = i + 1;

		// Greedy farthest-visible search
		for (std::size_t j = i + 2; j < path.size(); ++j)
		{
			if (HasLineOfSightGrid(map, path[i], path[j]))
				bestJ = j;
			else
				break; // path is contiguous; once blocked, farther j is very likely blocked too
		}

		out.push_back(path[bestJ]);
		i = bestJ;
	}

	return out;
}

glm::ivec2 WorldToCell(glm::vec2 p)
{
	return glm::ivec2((int)std::floor(p.x), (int)std::floor(p.y));
}

bool CanChaseDirect(Map &map, glm::vec2 enemyPos, glm::vec2 playerPos)
{
	glm::ivec2 e = WorldToCell(enemyPos);
	glm::ivec2 p = WorldToCell(playerPos);
	return HasLineOfSightGrid(map, e, p);
}

std::size_t PickLookaheadIndex(Map &map,
	glm::vec2 enemyPos,
	const std::vector<glm::ivec2> &path,
	std::size_t currentIndex)
{
	glm::ivec2 eCell = WorldToCell(enemyPos);

	std::size_t best = currentIndex;
	for (std::size_t i = currentIndex; i < path.size(); ++i)
	{
		if (HasLineOfSightGrid(map, eCell, path[i]))
			best = i;
		else
			break;
	}
	return best;
}

glm::vec2 CellCenter(glm::ivec2 c)
{
	return glm::vec2(c) + glm::vec2(0.5f, 0.5f);
}



float octileHeuristic(const glm::ivec2 &a, const glm::ivec2 &b)
{
	// For 8-way movement with costs: D=1, D2=sqrt(2)
	const int dx = std::abs(a.x - b.x);
	const int dy = std::abs(a.y - b.y);
	const int mn = std::min(dx, dy);
	const int mx = std::max(dx, dy);
	return float(mx - mn) * 1.0f + float(mn) * 1.41421356237f;
}

bool inBounds(const glm::ivec2 &p, const glm::ivec2 &size)
{
	return (p.x >= 0 && p.y >= 0 && p.x < size.x && p.y < size.y);
}

int idx(const glm::ivec2 &p, const glm::ivec2 &size)
{
	return p.y * size.x + p.x;
}

// Prevent "corner cutting" on diagonals:
// If moving diagonally, require BOTH side-adjacent tiles to be free.
bool canStep8(Map &map, const glm::ivec2 &from, const glm::ivec2 &to)
{
	if (map.isCollidableAtPosSafe(to.x, to.y)) return false;

	const glm::ivec2 d = to - from;
	const bool diag = (d.x != 0 && d.y != 0);
	if (!diag) return true;

	// side tiles
	if (map.isCollidableAtPosSafe(from.x + d.x, from.y)) return false;
	if (map.isCollidableAtPosSafe(from.x, from.y + d.y)) return false;

	return true;
}

// Returns path INCLUDING start? Here: returns a list of tiles to visit (excluding start, including goal if reachable).
std::vector<glm::ivec2> findPathAStar8(Map &map, const glm::ivec2 &start, const glm::ivec2 &goal)
{
	std::vector<glm::ivec2> empty;
	if (!inBounds(start, map.size) || !inBounds(goal, map.size)) return empty;
	if (map.isCollidableAtPosSafe(goal.x, goal.y)) return empty;
	if (start == goal) return empty;

	const int N = map.size.x * map.size.y;

	std::vector<float> gScore(N, std::numeric_limits<float>::infinity());
	std::vector<glm::ivec2> parent(N, glm::ivec2(-1));
	std::vector<uint8_t> closed(N, 0);

	struct Node
	{
		glm::ivec2 p;
		float f;
		float g;
	};
	struct Cmp
	{
		bool operator()(const Node &a, const Node &b) const { return a.f > b.f; }
	};
	std::priority_queue<Node, std::vector<Node>, Cmp> open;

	gScore[idx(start, map.size)] = 0.0f;
	open.push({start, octileHeuristic(start, goal), 0.0f});

	static const glm::ivec2 dirs[8] = {
		{1, 0},{-1, 0},{0, 1},{0, -1},
		{1, 1},{1, -1},{-1, 1},{-1, -1}
	};

	while (!open.empty())
	{
		Node cur = open.top();
		open.pop();

		const int curI = idx(cur.p, map.size);
		if (closed[curI]) continue;
		closed[curI] = 1;

		if (cur.p == goal)
		{
			// Reconstruct (exclude start)
			std::vector<glm::ivec2> path;
			glm::ivec2 t = goal;
			while (t != start)
			{
				path.push_back(t);
				t = parent[idx(t, map.size)];
				if (t.x < 0) break; // safety
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		for (glm::ivec2 d : dirs)
		{
			glm::ivec2 nxt = cur.p + d;
			if (!inBounds(nxt, map.size)) continue;
			if (!canStep8(map, cur.p, nxt)) continue;

			const float stepCost = (d.x != 0 && d.y != 0) ? 1.41421356237f : 1.0f;
			const float tentativeG = gScore[curI] + stepCost;

			const int ni = idx(nxt, map.size);
			if (closed[ni]) continue;

			if (tentativeG < gScore[ni])
			{
				gScore[ni] = tentativeG;
				parent[ni] = cur.p;
				const float f = tentativeG + octileHeuristic(nxt, goal);
				open.push({nxt, f, tentativeG});
			}
		}
	}

	return empty; // unreachable
}
