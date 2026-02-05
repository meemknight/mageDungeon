#include <gameplay/droppedItems.h>
#include <gameplay/assetsManager.h>
#include <gameplay/inputPrompts.h>
#include <gameplay/particleSystem.h>
#include <gameplay/elements.h>
#include <particles/particleCreator.h>
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
	constexpr int coinFrameCount = 6;
	constexpr float coinFrameTime = 0.08f;
	constexpr float itemParticleInterval = 0.6f;
	constexpr int magicStoneDropOdds = 8; // 1 in N chance for chests to drop a magic stone.

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

	int getCoinFrame(float animTimer)
	{
		int frame = (int)(animTimer / coinFrameTime) % coinFrameCount;
		if (frame < 0) { frame = 0; }
		return frame;
	}

	ParticleSettings buildDropParticle(glm::vec4 startColor, glm::vec4 endColor,
		int count, float sizeScale, float speedScale, float lifeScale)
	{
		ParticleSettings p = getSmallSquareParticle(startColor, endColor);
		p.onCreateCount = (short)count;
		p.particleLifeTime *= lifeScale;
		p.velocityX *= speedScale;
		p.velocityY *= speedScale;
		p.dragX *= speedScale;
		p.dragY *= speedScale;
		p.createApearence.size *= sizeScale;
		p.endApearence.size *= sizeScale;
		p.folowParent = false;
		return p;
	}

	void emitDropParticles(ParticleSystem &particleSystem, std::ranlux24_base &rng,
		glm::vec2 pos, glm::vec4 startColor, glm::vec4 endColor,
		int count, float sizeScale, float speedScale, float lifeScale)
	{
		ParticleSettings particle = buildDropParticle(startColor, endColor,
			count, sizeScale, speedScale, lifeScale);
		particleSystem.emitParticles(particle, pos, rng, pos);
	}

	ParticleSettings buildPickupParticle(glm::vec4 startColor, glm::vec4 endColor, int count)
	{
		ParticleSettings p = getSmallSquareParticle(startColor, endColor);
		p.onCreateCount = (short)count;
		p.particleLifeTime = {0.25f, 0.5f};
		p.velocityX = glm::vec2{-20, 20} * PIXEL_SIZE;
		p.velocityY = glm::vec2{-20, 20} * PIXEL_SIZE;
		p.dragX = glm::vec2{-20, -30} * PIXEL_SIZE;
		p.dragY = glm::vec2{-20, -30} * PIXEL_SIZE;
		p.createApearence.size = glm::vec2{2.0f, 3.0f} * PIXEL_SIZE;
		p.endApearence.size = glm::vec2{1.0f, 1.5f} * PIXEL_SIZE;
		p.tranzitionType = ParticleSettings::TRANZITION_TYPES::abruptCurbe;
		p.animationType = ParticleSettings::ANIMATION_TYPES::animationSpiral;
		p.animationSpeed = {8.0f, 12.0f};
		p.animationScaleX = glm::vec2{PIXEL_SIZE * 3.0f, PIXEL_SIZE * 6.0f};
		p.animationScaleY = glm::vec2{PIXEL_SIZE * 3.0f, PIXEL_SIZE * 6.0f};
		p.animationRotation = {0.0f, 360.0f};
		p.animationPhase = {0.0f, 6.2831f};
		p.folowParent = false;
		return p;
	}

	void emitPickupParticles(ParticleSystem &particleSystem, std::ranlux24_base &rng,
		glm::vec2 pos, glm::vec4 startColor, glm::vec4 endColor, int count)
	{
		ParticleSettings particle = buildPickupParticle(startColor, endColor, count);
		particleSystem.emitParticles(particle, pos, rng, pos);
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
	item.particleTimer = getRandomFloat(rng, 0.0f, itemParticleInterval);
	items.push_back(item);
}

void DroppedItemSystem::spawnChest(glm::vec2 pos, std::ranlux24_base &rng, const Wand *wand, bool forceHearth)
{
	DroppedItem item;
	item.type = DroppedItemType::Chest;
	item.pos = pos;
	item.hoverTimer = getRandomFloat(rng, 0.0f, 6.2831f);
	item.particleTimer = getRandomFloat(rng, 0.0f, itemParticleInterval);
	item.chestOpenTimer = 0.0f;
	item.chestOpening = false;
	item.chestHasWand = wand != nullptr;
	item.chestHasHearth = forceHearth && !item.chestHasWand;
	if (wand)
	{
		item.chestWand = *wand;
	}
	items.push_back(item);
}

void DroppedItemSystem::spawnHearth(glm::vec2 pos, std::ranlux24_base &rng)
{
	DroppedItem item;
	item.type = DroppedItemType::Hearth;
	item.pos = pos;
	item.hoverTimer = getRandomFloat(rng, 0.0f, 6.2831f);
	item.particleTimer = getRandomFloat(rng, 0.0f, itemParticleInterval);
	items.push_back(item);
}

void DroppedItemSystem::spawnCoin(glm::vec2 pos, std::ranlux24_base &rng)
{
	DroppedItem item;
	item.type = DroppedItemType::Coin;
	item.pos = pos;
	item.hoverTimer = getRandomFloat(rng, 0.0f, 6.2831f);
	item.particleTimer = getRandomFloat(rng, 0.0f, itemParticleInterval);
	items.push_back(item);
}

void DroppedItemSystem::spawnMagicStone(glm::vec2 pos, int element, int uses, std::ranlux24_base &rng)
{
	DroppedItem item;
	item.type = DroppedItemType::MagicStone;
	item.pos = pos;
	item.stoneElement = element;
	item.stoneUses = uses;
	item.hoverTimer = getRandomFloat(rng, 0.0f, 6.2831f);
	item.particleTimer = getRandomFloat(rng, 0.0f, itemParticleInterval);
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
		else if (item.type == DroppedItemType::Hearth)
		{
			canInteract = true;
		}
		else if (item.type == DroppedItemType::Coin)
		{
			canInteract = true;
		}
		else if (item.type == DroppedItemType::MagicStone)
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

void DroppedItemSystem::update(float deltaTime, ParticleSystem &particleSystem, std::ranlux24_base &rng)
{
	const glm::vec4 hearthStart = {1.0f, 0.2f, 0.2f, 0.65f};
	const glm::vec4 hearthEnd = {0.6f, 0.05f, 0.05f, 0.0f};
	const glm::vec4 coinStart = {1.0f, 0.9f, 0.25f, 0.65f};
	const glm::vec4 coinEnd = {0.8f, 0.6f, 0.1f, 0.0f};

	for (int i = 0; i < (int)items.size();)
	{
		auto &item = items[i];
		item.hoverTimer += deltaTime;
		item.particleTimer += deltaTime;
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

		if (item.particleTimer >= itemParticleInterval)
		{
			item.particleTimer -= itemParticleInterval;
			if (item.type == DroppedItemType::Hearth)
			{
				emitDropParticles(particleSystem, rng, item.pos,
					hearthStart, hearthEnd, 2, 0.7f, 0.6f, 0.7f);
			}
			else if (item.type == DroppedItemType::Coin)
			{
				emitDropParticles(particleSystem, rng, item.pos,
					coinStart, coinEnd, 2, 0.7f, 0.6f, 0.7f);
			}
			else if (item.type == DroppedItemType::MagicStone)
			{
				auto stoneStart = elementToSecondaryColor(item.stoneElement); stoneStart.a = 0.65f;
				auto stoneEnd = elementToColor(item.stoneElement); stoneEnd.a = 0.0f;
				emitDropParticles(particleSystem, rng, item.pos,
					stoneStart, stoneEnd, 2, 0.7f, 0.6f, 0.7f);
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
	const float hearthSize = PIXEL_SIZE * 16.0f;
	const float hearthShadowSize = PIXEL_SIZE * 8.0f;
	const float coinSize = PIXEL_SIZE * 16.0f;
	const float coinShadowSize = PIXEL_SIZE * 8.0f;
	const float stoneSize = PIXEL_SIZE * 16.0f;
	const float stoneShadowSize = PIXEL_SIZE * 8.0f;
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
		else if (item.type == DroppedItemType::Hearth)
		{
			float hover = std::sin(item.hoverTimer * hoverSpeed) * hoverHeight;
			glm::vec2 basePos = item.pos;
			glm::vec2 drawPos = {basePos.x, basePos.y - hover};

			glm::vec4 shadowRect = {basePos.x - hearthShadowSize * 0.5f, basePos.y - hearthShadowSize * 0.5f,
				hearthShadowSize, hearthShadowSize};
			renderer.renderRectangle(shadowRect, assetsManager.particleCircle,
				{0.0f, 0.0f, 0.0f, 0.45f});

			glm::vec4 hearthRect = {drawPos.x - hearthSize * 0.5f, drawPos.y - hearthSize * 0.5f,
				hearthSize, hearthSize};
			renderer.renderRectangle(hearthRect, assetsManager.hearth.texture, {1, 1, 1, 1},
				{hearthSize * 0.5f, hearthSize * 0.5f}, 0.0f,
				assetsManager.hearth.atlas.get(0, 0));

			if (i == promptIndex)
			{
				float promptOffset = hearthSize * 0.65f + promptSize * 0.5f;
				glm::vec2 promptPos = {drawPos.x, drawPos.y - promptOffset};
				renderPrompt(renderer, assetsManager, usesController,
					pickupPrompt, promptPos, promptSize, promptAlpha);
			}
		}
		else if (item.type == DroppedItemType::Coin)
		{
			float hover = std::sin(item.hoverTimer * hoverSpeed) * hoverHeight;
			glm::vec2 basePos = item.pos;
			glm::vec2 drawPos = {basePos.x, basePos.y - hover};

			glm::vec4 shadowRect = {basePos.x - coinShadowSize * 0.5f, basePos.y - coinShadowSize * 0.5f,
				coinShadowSize, coinShadowSize};
			renderer.renderRectangle(shadowRect, assetsManager.particleCircle,
				{0.0f, 0.0f, 0.0f, 0.45f});

			int frame = getCoinFrame(item.hoverTimer);
			glm::vec4 coinRect = {drawPos.x - coinSize * 0.5f, drawPos.y - coinSize * 0.5f,
				coinSize, coinSize};
			renderer.renderRectangle(coinRect, assetsManager.coin.texture, {1, 1, 1, 1},
				{coinSize * 0.5f, coinSize * 0.5f}, 0.0f,
				assetsManager.coin.atlas.get(frame, 0));

			if (i == promptIndex)
			{
				float promptOffset = coinSize * 0.65f + promptSize * 0.5f;
				glm::vec2 promptPos = {drawPos.x, drawPos.y - promptOffset};
				renderPrompt(renderer, assetsManager, usesController,
					pickupPrompt, promptPos, promptSize, promptAlpha);
			}
		}
		else if (item.type == DroppedItemType::MagicStone)
		{
			float hover = std::sin(item.hoverTimer * hoverSpeed) * hoverHeight;
			glm::vec2 basePos = item.pos;
			glm::vec2 drawPos = {basePos.x, basePos.y - hover};

			glm::vec4 shadowRect = {basePos.x - stoneShadowSize * 0.5f, basePos.y - stoneShadowSize * 0.5f,
				stoneShadowSize, stoneShadowSize};
			renderer.renderRectangle(shadowRect, assetsManager.particleCircle,
				{0.0f, 0.0f, 0.0f, 0.45f});

			glm::vec4 stoneRect = {drawPos.x - stoneSize * 0.5f, drawPos.y - stoneSize * 0.5f,
				stoneSize, stoneSize};
			renderer.renderRectangle(stoneRect, assetsManager.elements.texture, {1, 1, 1, 1},
				{stoneSize * 0.5f, stoneSize * 0.5f}, 0.0f,
				assetsManager.elements.atlas.get(item.stoneElement, 0));

			if (i == promptIndex)
			{
				float promptOffset = stoneSize * 0.65f + promptSize * 0.5f;
				glm::vec2 promptPos = {drawPos.x, drawPos.y - promptOffset};
				renderPrompt(renderer, assetsManager, usesController,
					pickupPrompt, promptPos, promptSize, promptAlpha);
			}
		}
	}
}

bool DroppedItemSystem::openChest(int itemIndex, std::ranlux24_base &rng, ParticleSystem &particleSystem)
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

	const glm::vec4 hearthStart = {1.0f, 0.2f, 0.2f, 1.0f};
	const glm::vec4 hearthEnd = {0.6f, 0.05f, 0.05f, 0.0f};
	const glm::vec4 coinStart = {1.0f, 0.9f, 0.25f, 1.0f};
	const glm::vec4 coinEnd = {0.8f, 0.6f, 0.1f, 0.0f};
	const glm::vec4 chestStart = {1.0f, 1.0f, 1.0f, 0.9f};
	const glm::vec4 chestEnd = {1.0f, 1.0f, 1.0f, 0.0f};

	emitDropParticles(particleSystem, rng, item.pos,
		chestStart, chestEnd, 18, 1.1f, 1.3f, 0.8f);

	if (item.chestHasWand)
	{
		spawnWand(item.pos, item.chestWand, rng);
		emitWandPickupParticles(item.pos, particleSystem, rng);
		return true;
	}
	if (item.chestHasHearth)
	{
		spawnHearth(item.pos, rng);
		emitDropParticles(particleSystem, rng, item.pos,
			hearthStart, hearthEnd, 10, 1.05f, 1.2f, 0.85f);
		return true;
	}

	bool dropStone = getRandomInt(rng, 0, magicStoneDropOdds - 1) == 0;
	if (dropStone)
	{
		int stoneElement = getRandomInt(rng, Elements::Fire, Elements::Ice);
		int stoneUses = 2;
		spawnMagicStone(item.pos, stoneElement, stoneUses, rng);
		auto stoneStart = elementToSecondaryColor(stoneElement); stoneStart.a = 1.0f;
		auto stoneEnd = elementToColor(stoneElement); stoneEnd.a = 0.0f;
		emitDropParticles(particleSystem, rng, item.pos,
			stoneStart, stoneEnd, 8, 1.0f, 1.0f, 0.9f);
	}
	else if (getRandomInt(rng, 0, 1) == 0)
	{
		spawnHearth(item.pos, rng);
		emitDropParticles(particleSystem, rng, item.pos,
			hearthStart, hearthEnd, 8, 1.0f, 1.0f, 0.9f);
	}
	else
	{
		spawnCoin(item.pos, rng);
		emitDropParticles(particleSystem, rng, item.pos,
			coinStart, coinEnd, 8, 1.0f, 1.0f, 0.9f);
	}
	return true;
}

bool DroppedItemSystem::takeHearth(int itemIndex, ParticleSystem &particleSystem, std::ranlux24_base &rng)
{
	if (itemIndex < 0 || itemIndex >= (int)items.size())
	{
		return false;
	}
	if (items[itemIndex].type != DroppedItemType::Hearth)
	{
		return false;
	}
	const glm::vec4 hearthStart = {1.0f, 0.2f, 0.2f, 1.0f};
	const glm::vec4 hearthEnd = {0.6f, 0.05f, 0.05f, 0.0f};
	emitPickupParticles(particleSystem, rng, items[itemIndex].pos,
		hearthStart, hearthEnd, 20);
	items[itemIndex] = items.back();
	items.pop_back();
	return true;
}

bool DroppedItemSystem::takeCoin(int itemIndex, ParticleSystem &particleSystem, std::ranlux24_base &rng)
{
	if (itemIndex < 0 || itemIndex >= (int)items.size())
	{
		return false;
	}
	if (items[itemIndex].type != DroppedItemType::Coin)
	{
		return false;
	}
	const glm::vec4 coinStart = {1.0f, 0.9f, 0.25f, 1.0f};
	const glm::vec4 coinEnd = {0.8f, 0.6f, 0.1f, 0.0f};
	emitPickupParticles(particleSystem, rng, items[itemIndex].pos,
		coinStart, coinEnd, 20);
	items[itemIndex] = items.back();
	items.pop_back();
	return true;
}

bool DroppedItemSystem::takeMagicStone(int itemIndex, ParticleSystem &particleSystem, std::ranlux24_base &rng,
	int &outElement, int &outUses)
{
	if (itemIndex < 0 || itemIndex >= (int)items.size())
	{
		return false;
	}
	if (items[itemIndex].type != DroppedItemType::MagicStone)
	{
		return false;
	}
	outElement = items[itemIndex].stoneElement;
	outUses = items[itemIndex].stoneUses;
	auto stoneStart = elementToSecondaryColor(outElement); stoneStart.a = 1.0f;
	auto stoneEnd = elementToColor(outElement); stoneEnd.a = 0.0f;
	emitPickupParticles(particleSystem, rng, items[itemIndex].pos,
		stoneStart, stoneEnd, 20);
	items[itemIndex] = items.back();
	items.pop_back();
	return true;
}

void DroppedItemSystem::emitWandPickupParticles(glm::vec2 pos, ParticleSystem &particleSystem, std::ranlux24_base &rng)
{
	const glm::vec4 wandStart = {1.0f, 1.0f, 1.0f, 1.0f};
	const glm::vec4 wandEnd = {0.8f, 0.9f, 1.0f, 0.0f};
	emitPickupParticles(particleSystem, rng, pos,
		wandStart, wandEnd, 20);
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
