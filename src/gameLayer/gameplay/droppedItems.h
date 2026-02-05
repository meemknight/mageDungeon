#include <glm/vec2.hpp>
#include <vector>
#include <random>
#include <gameplay/Physics.h>
#include <gameplay/elements.h>
#include <gameplay/wand.h>

struct AssetsManager;
struct ParticleSystem;

namespace gl2d
{
	struct Renderer2D;
}

// Simple dropped item container. Uses type to decide which field is active.
enum class DroppedItemType
{
	None,
	Wand,
	Chest,
	Hearth,
	Coin,
	MagicStone
};

struct DroppedItem
{
	DroppedItemType type = DroppedItemType::None;
	glm::vec2 pos = {};
	Wand wand = {};
	int stoneElement = Elements::NoneElement; // magic stone element
	int stoneUses = 1; // magic stone uses
	float hoverTimer = 0.0f;
	float particleTimer = 0.0f; // idle particle pacing
	float chestOpenTimer = 0.0f; // chest open animation + fade timer
	bool chestOpening = false;
	bool chestHasWand = false;
	Wand chestWand = {};
	bool chestHasHearth = false;
};

// Handles dropped items in the world (spawn, hover, render, pickup).
// Chests play an open animation, drop a coin, hearth, or rare magic stone, then fade out before removal.
struct DroppedItemSystem
{
	std::vector<DroppedItem> items;

	void clear();
	void update(float deltaTime, ParticleSystem &particleSystem, std::ranlux24_base &rng);
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetsManager,
		glm::vec2 playerPos, bool usesController);

	void spawnWand(glm::vec2 pos, const Wand &wand, std::ranlux24_base &rng);
	void spawnChest(glm::vec2 pos, std::ranlux24_base &rng, const Wand *wand = nullptr, bool forceHearth = false);
	void spawnHearth(glm::vec2 pos, std::ranlux24_base &rng);
	void spawnCoin(glm::vec2 pos, std::ranlux24_base &rng);
	void spawnMagicStone(glm::vec2 pos, int element, int uses, std::ranlux24_base &rng);
	int findClosestInteractableIndex(glm::vec2 playerPos, float maxDist2) const;
	bool openChest(int itemIndex, std::ranlux24_base &rng, ParticleSystem &particleSystem);
	bool takeHearth(int itemIndex, ParticleSystem &particleSystem, std::ranlux24_base &rng);
	bool takeCoin(int itemIndex, ParticleSystem &particleSystem, std::ranlux24_base &rng);
	bool takeMagicStone(int itemIndex, ParticleSystem &particleSystem, std::ranlux24_base &rng,
		int &outElement, int &outUses);
	void emitWandPickupParticles(glm::vec2 pos, ParticleSystem &particleSystem, std::ranlux24_base &rng);
	bool trySwapWithPlayerIndex(int itemIndex, Wand *playerWands, bool *hasWands,
		int activeIndex, int &outSlot, bool &outSwapped, int &outItemIndex);
	// tries to pick up or swap a wand; outputs slot, swap info, and item index
	bool trySwapWithPlayer(glm::vec2 playerPos, Wand *playerWands, bool *hasWands,
		int activeIndex, int &outSlot, bool &outSwapped, int &outItemIndex, bool trigger);
};
