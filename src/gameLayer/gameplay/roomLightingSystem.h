#pragma once
#include <vector>
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>

struct Map;
struct FloorInfo;
struct FloorRoom;

namespace gl2d
{
	struct Renderer2D;
}

// Uses the same "inside room" rule as trap-room triggering.
bool isInsideRoomTriggerBounds(const FloorRoom &room, const glm::vec4 &aabb, float inset);

// Handles room/corridor discovery and the reveal fade overlay.
struct RoomLightingSystem
{
	glm::ivec2 size = {};
	std::vector<unsigned char> revealedTiles;
	std::vector<float> revealFade;

	std::vector<int> roomByTile;
	std::vector<unsigned char> roomLit;

	std::vector<int> corridorFloorComponentByTile;
	std::vector<std::vector<int>> corridorRevealTiles;
	std::vector<unsigned char> corridorLit;

	float fadeSpeed = 7.0f; // how fast newly-lit tiles fade from black
	bool enableLowLightLayer = false; // 0.8 alpha dark layer around revealed zones

	void resetForFloor(Map &map, const FloorInfo &floorInfo);
	void update(float deltaTime, Map &map, const FloorInfo &floorInfo,
		const glm::vec4 &playerAabb, float roomTriggerInset);
	void renderOverlay(gl2d::Renderer2D &renderer, Map &map);
	bool isTileVisible(Map &map, int x, int y) const;

	void revealRoom(int roomIndex, const FloorInfo &floorInfo);
	void revealCorridorAtTile(Map &map, glm::ivec2 tile);

private:
	void revealTile(int x, int y);
	int toIndex(int x, int y) const;
};
