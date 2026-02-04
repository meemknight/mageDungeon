#pragma once
#include <glm/vec2.hpp>
#include <unordered_map>

// Door data stored separately from the tile map.
struct Door
{
	enum class Orientation
	{
		Horizontal,
		Vertical
	};

	Orientation orientation = Orientation::Horizontal;
	bool open = false;
};

struct DoorPosHash
{
	size_t operator()(const glm::ivec2 &pos) const noexcept
	{
		return (size_t)(pos.x * 73856093) ^ (size_t)(pos.y * 19349663);
	}
};

struct DoorPosEqual
{
	bool operator()(const glm::ivec2 &a, const glm::ivec2 &b) const noexcept
	{
		return a.x == b.x && a.y == b.y;
	}
};

// Holds doors keyed by their bottom-left tile position.
struct DoorHolder
{
	std::unordered_map<glm::ivec2, Door, DoorPosHash, DoorPosEqual> doors;

	void clear()
	{
		doors.clear();
	}

	void addDoor(glm::ivec2 pos, Door::Orientation orientation)
	{
		Door door;
		door.orientation = orientation;
		doors[pos] = door;
	}
};
