#include "aStar.h"

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
