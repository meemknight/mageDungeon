#include <gameplay/droppedItems.h>
#include <gameplay/assetsManager.h>
#include <gameplay/inputPrompts.h>
#include <randomStuff.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <cmath>

namespace
{
	constexpr int chestFrameCount = 4;
	constexpr float chestFrameTime = 0.12f;
	constexpr float chestHoldTime = 0.35f;
	constexpr float chestFadeTime = 0.35f;
	constexpr float chestTotalTime = chestFrameCount * chestFrameTime + chestHoldTime + chestFadeTime;

	int findClosestWandIndex(const std::vector<DroppedItem> &items,
		glm::vec2 playerPos, float maxDist2)
	{
		int bestIndex = -1;
		float bestDist2 = maxDist2;

		for (int i = 0; i < (int)items.size(); i++)
		{
			const auto &item = items[i];
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

		return bestIndex;
	}

	int getChestFrame(float openTimer)
	{
		int frame = (int)(openTimer / chestFrameTime);
		if (frame < 0) { frame = 0; }
		if (frame >= chestFrameCount) { frame = chestFrameCount - 1; }
		return frame;
	}

	float getChestAlpha(float openTimer)
	{
		float fadeStart = chestFrameCount * chestFrameTime + chestHoldTime;
		if (openTimer <= fadeStart)
		{
			return 1.0f;
		}
		float fadeT = (openTimer - fadeStart) / chestFadeTime;
		return 1.0f - glm::clamp(fadeT, 0.0f, 1.0f);
	}
}

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

void DroppedItemSystem::spawnChest(glm::vec2 pos, std::ranlux24_base &rng)
{
	DroppedItem item;
	item.type = DroppedItemType::Chest;
	item.pos = pos;
	item.hoverTimer = getRandomFloat(rng, 0.0f, 6.2831f);
	item.chestOpenTimer = 0.0f;
	item.chestOpening = false;
	items.push_back(item);
}

int DroppedItemSystem::findClosestInteractableIndex(glm::vec2 playerPos, float maxDist2) const
{
	int bestIndex = -1;
	float bestDist2 = maxDist2;

	for (int i = 0; i < (int)items.size(); i++)
	{
		const auto &item = items[i];
		bool canInteract = false;
		if (item.type == DroppedItemType::Wand)
		{
			canInteract = true;
		}
		else if (item.type == DroppedItemType::Chest && !item.chestOpening)
		{
			canInteract = true;
		}

		if (!canInteract)
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

	return bestIndex;
}

void DroppedItemSystem::update(float deltaTime)
{
	for (int i = 0; i < (int)items.size();)
	{
		auto &item = items[i];
		item.hoverTimer += deltaTime;
		if (item.type == DroppedItemType::Chest && item.chestOpening)
		{
			item.chestOpenTimer += deltaTime;
			if (item.chestOpenTimer >= chestTotalTime)
			{
				items[i] = items.back();
				items.pop_back();
				continue;
			}
		}
		i++;
	}
}

void DroppedItemSystem::render(gl2d::Renderer2D &renderer, AssetsManager &assetsManager,
	glm::vec2 playerPos, bool usesController)
{
	constexpr float hoverSpeed = 2.6f;
	constexpr float rotateSpeed = 2.0f;
	const float hoverHeight = PIXEL_SIZE * 1.5f;
	const float shadowSize = PIXEL_SIZE * 10.0f;
	const float wandSize = PIXEL_SIZE * 16.0f;
	const float chestSize = PIXEL_SIZE * 16.0f;
	const float chestShadowSize = PIXEL_SIZE * 12.0f;
	const float pickupRadius = PIXEL_SIZE * 16.0f;
	const float promptSize = PIXEL_SIZE * 12.0f;
	const float promptAlpha = 0.72f;
	const ButtonPrompt pickupPrompt = {"E", "A"};

	// show pickup prompt on the closest interactable item in range
	int promptIndex = findClosestInteractableIndex(playerPos, pickupRadius * pickupRadius);

	for (int i = 0; i < (int)items.size(); i++)
	{
		const auto &item = items[i];
		if (item.type == DroppedItemType::Wand)
		{
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

			if (i == promptIndex)
			{
				float promptOffset = wandSize * 0.65f + promptSize * 0.5f;
				glm::vec2 promptPos = {drawPos.x, drawPos.y - promptOffset};
				renderPrompt(renderer, assetsManager, usesController,
					pickupPrompt, promptPos, promptSize, promptAlpha);
			}
		}
		else if (item.type == DroppedItemType::Chest)
		{
			float alpha = item.chestOpening ? getChestAlpha(item.chestOpenTimer) : 1.0f;
			int frame = item.chestOpening ? getChestFrame(item.chestOpenTimer) : 0;
			glm::vec2 basePos = item.pos;
			glm::vec2 drawPos = basePos;

			glm::vec4 shadowRect = {basePos.x - chestShadowSize * 0.5f, basePos.y - chestShadowSize * 0.5f,
				chestShadowSize, chestShadowSize};
			renderer.renderRectangle(shadowRect, assetsManager.particleCircle,
				{0.0f, 0.0f, 0.0f, 0.45f * alpha});

			glm::vec4 chestRect = {drawPos.x - chestSize * 0.5f, drawPos.y - chestSize * 0.5f,
				chestSize, chestSize};
			renderer.renderRectangle(chestRect, assetsManager.woodenChest.texture, {1, 1, 1, alpha},
				{chestSize * 0.5f, chestSize * 0.5f}, 0.0f,
				assetsManager.woodenChest.atlas.get(frame, 0));

			if (i == promptIndex)
			{
				float promptOffset = chestSize * 0.65f + promptSize * 0.5f;
				glm::vec2 promptPos = {drawPos.x, drawPos.y - promptOffset};
				renderPrompt(renderer, assetsManager, usesController,
					pickupPrompt, promptPos, promptSize, promptAlpha);
			}
		}
	}
}

bool DroppedItemSystem::openChest(int itemIndex)
{
	if (itemIndex < 0 || itemIndex >= (int)items.size())
	{
		return false;
	}

	auto &item = items[itemIndex];
	if (item.type != DroppedItemType::Chest || item.chestOpening)
	{
		return false;
	}

	item.chestOpening = true;
	item.chestOpenTimer = 0.0f;
	return true;
}

bool DroppedItemSystem::trySwapWithPlayerIndex(int itemIndex, Wand *playerWands, bool *hasWands,
	int activeIndex, int &outSlot, bool &outSwapped, int &outItemIndex)
{
	outSlot = -1;
	outSwapped = false;
	outItemIndex = -1;

	if (itemIndex < 0 || itemIndex >= (int)items.size())
	{
		return false;
	}
	if (activeIndex < 0 || activeIndex > 1)
	{
		activeIndex = 0;
	}
	if (items[itemIndex].type != DroppedItemType::Wand)
	{
		return false;
	}

	int targetSlot = -1;
	if (!hasWands[0]) { targetSlot = 0; }
	else if (!hasWands[1]) { targetSlot = 1; }
	else { targetSlot = activeIndex; }

	outSlot = targetSlot;
	outItemIndex = itemIndex;
	if (!hasWands[targetSlot])
	{
		playerWands[targetSlot] = items[itemIndex].wand;
		hasWands[targetSlot] = true;
		items[itemIndex] = items.back();
		items.pop_back();
		return true;
	}

	std::swap(playerWands[targetSlot], items[itemIndex].wand);
	outSwapped = true;
	return true;
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

	const float pickupRadius = PIXEL_SIZE * 16.0f;
	int bestIndex = findClosestWandIndex(items, playerPos, pickupRadius * pickupRadius);
	return trySwapWithPlayerIndex(bestIndex, playerWands, hasWands, activeIndex,
		outSlot, outSwapped, outItemIndex);
}
