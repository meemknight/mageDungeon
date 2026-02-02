#include <glm/vec2.hpp>
#include <vector>
#include <random>
#include <gameplay/Physics.h>
#include <gameplay/wand.h>

struct AssetsManager;

namespace gl2d
{
	struct Renderer2D;
}

// Simple dropped item container. Uses type to decide which field is active.
enum class DroppedItemType
{
	None,
	Wand,
	Chest
};

struct DroppedItem
{
	DroppedItemType type = DroppedItemType::None;
	glm::vec2 pos = {};
	Wand wand = {};
	float hoverTimer = 0.0f;
	float chestOpenTimer = 0.0f; // chest open animation + fade timer
	bool chestOpening = false;
};

// Handles dropped items in the world (spawn, hover, render, pickup).
// Chests play an open animation then fade out before removal.
struct DroppedItemSystem
{
	std::vector<DroppedItem> items;

	void clear();
	void update(float deltaTime);
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetsManager,
		glm::vec2 playerPos, bool usesController);

	void spawnWand(glm::vec2 pos, const Wand &wand, std::ranlux24_base &rng);
	void spawnChest(glm::vec2 pos, std::ranlux24_base &rng);
	int findClosestInteractableIndex(glm::vec2 playerPos, float maxDist2) const;
	bool openChest(int itemIndex);
	bool trySwapWithPlayerIndex(int itemIndex, Wand *playerWands, bool *hasWands,
		int activeIndex, int &outSlot, bool &outSwapped, int &outItemIndex);
	// tries to pick up or swap a wand; outputs slot, swap info, and item index
	bool trySwapWithPlayer(glm::vec2 playerPos, Wand *playerWands, bool *hasWands,
		int activeIndex, int &outSlot, bool &outSwapped, int &outItemIndex, bool trigger);
};
