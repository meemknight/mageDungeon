#include <gameplay/droppedItems.h>
#include <gameplay/assetsManager.h>
#include <randomStuff.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <cmath>

void DroppedItemSystem::clear()
{
	items.clear();
}

void DroppedItemSystem::spawnWand(glm::vec2 pos, const Wand &wand, std::ranlux24_base &rng)
{
	DroppedItem item;
	item.type = DroppedItemType::Wand;
	item.pos = pos;
	item.wand = wand;
	item.hoverTimer = getRandomFloat(rng, 0.0f, 6.2831f);
	items.push_back(item);
}

void DroppedItemSystem::update(float deltaTime)
{
	for (auto &item : items)
	{
		item.hoverTimer += deltaTime;
	}
}

void DroppedItemSystem::render(gl2d::Renderer2D &renderer, AssetsManager &assetsManager)
{
	constexpr float hoverSpeed = 2.6f;
	constexpr float rotateSpeed = 2.0f;
	const float hoverHeight = PIXEL_SIZE * 1.5f;
	const float shadowSize = PIXEL_SIZE * 10.0f;
	const float wandSize = PIXEL_SIZE * 16.0f;

	for (const auto &item : items)
	{
		if (item.type != DroppedItemType::Wand)
		{
			continue;
		}

		float hover = std::sin(item.hoverTimer * hoverSpeed) * hoverHeight;
		float rotation = std::sin(item.hoverTimer * rotateSpeed) * 8.0f;
		glm::vec2 basePos = item.pos;
		glm::vec2 drawPos = {basePos.x, basePos.y - hover};

		glm::vec4 shadowRect = {basePos.x - shadowSize * 0.5f, basePos.y - shadowSize * 0.5f,
			shadowSize, shadowSize};
		renderer.renderRectangle(shadowRect, assetsManager.particleCircle,
			{0.0f, 0.0f, 0.0f, 0.5f});

		glm::vec4 wandRect = {drawPos.x - wandSize * 0.5f, drawPos.y - wandSize * 0.5f,
			wandSize, wandSize};
		renderer.renderRectangle(wandRect, assetsManager.wands.texture, {1, 1, 1, 1},
			{wandSize * 0.5f, wandSize * 0.5f}, rotation,
			assetsManager.wands.atlas.get(item.wand.wandSprite, 0));
	}
}

bool DroppedItemSystem::trySwapWithPlayer(glm::vec2 playerPos, Wand *playerWands, bool *hasWands,
	int activeIndex, int &outSlot, bool &outSwapped, int &outItemIndex, bool trigger)
{
	outSlot = -1;
	outSwapped = false;
	outItemIndex = -1;
	if (!trigger)
	{
		return false;
	}
	if (activeIndex < 0 || activeIndex > 1)
	{
		activeIndex = 0;
	}

	const float pickupRadius = PIXEL_SIZE * 16.0f;
	float bestDist2 = pickupRadius * pickupRadius;
	int bestIndex = -1;

	for (int i = 0; i < (int)items.size(); i++)
	{
		auto &item = items[i];
		if (item.type != DroppedItemType::Wand)
		{
			continue;
		}

		glm::vec2 diff = item.pos - playerPos;
		float dist2 = glm::dot(diff, diff);
		if (dist2 <= bestDist2)
		{
			bestDist2 = dist2;
			bestIndex = i;
		}
	}

	if (bestIndex >= 0)
	{
		int targetSlot = -1;
		if (!hasWands[0]) { targetSlot = 0; }
		else if (!hasWands[1]) { targetSlot = 1; }
		else { targetSlot = activeIndex; }

		outSlot = targetSlot;
		outItemIndex = bestIndex;
		if (!hasWands[targetSlot])
		{
			playerWands[targetSlot] = items[bestIndex].wand;
			hasWands[targetSlot] = true;
			items[bestIndex] = items.back();
			items.pop_back();
			return true;
		}

		std::swap(playerWands[targetSlot], items[bestIndex].wand);
		outSwapped = true;
		return true;
	}

	return false;
}
