#include "wand.h"
#include <gameplay/wand.h>
#include <randomStuff.h>
#include <algorithm>
#include <array>
#include <vector>

Wand makeTestWand()
{
	Wand wand;

	wand.up = {WandSlotType::Element, Elements::Fire, 2};
	wand.down = {WandSlotType::Element, Elements::Earth, 2};
	wand.left = {WandSlotType::Element, Elements::Ice, 2};
	wand.right = {WandSlotType::Element, Elements::Water, 2};
	wand.alwaysCast = {WandSlotType::Disabled};
	wand.maxMana = 8;
	wand.manaChargeSpeed = 1.0f;
	wand.maxElementsPerCast = 4;
	wand.wandSprite = Wand::oakWand;
	wand.sanitize();

	return wand;
}


Wand makeStarterWand(std::ranlux24_base &rng)
{
	Wand wand;

	int element = getRandomInt(rng, Elements::Fire, Elements::Ice);
	wand.up = {WandSlotType::Element, element, 1};
	wand.down = {WandSlotType::Disabled, Elements::NoneElement, 1};
	wand.left = {WandSlotType::Disabled, Elements::NoneElement, 1};
	wand.right = {WandSlotType::Empty, Elements::NoneElement, 1};
	wand.alwaysCast = {WandSlotType::Disabled};
	wand.maxMana = 3;
	wand.manaChargeSpeed = 0.4f;
	wand.maxElementsPerCast = 2;
	wand.wandSprite = Wand::starterWand;
	wand.sanitize();

	return wand;
}

const char *getWandSpriteName(int sprite)
{
	switch ((Wand::WandSprite)sprite)
	{
		case Wand::starterWand: return "Starter Wand";
		case Wand::oakWand: return "Oak Wand";
		case Wand::birchWand: return "Birch Wand";
		case Wand::cherryWand: return "Cherry Wand";
		case Wand::longOakWand: return "Long Oak Wand";
		case Wand::ashWand: return "Ash Wand";
		case Wand::opalWand: return "Opal Wand";
		case Wand::boneWand: return "Bone Wand";
		case Wand::unicornWand: return "Unicorn Wand";
		case Wand::cobaltWand: return "Cobalt Wand";
		case Wand::obsidianWand: return "Obsidian Wand";
		case Wand::earthWand: return "Earth Wand";
		case Wand::waterWand: return "Water Wand";
		case Wand::iceWand: return "Ice Wand";
		case Wand::fireWand: return "Fire Wand";
		case Wand::elderWand: return "Elder Wand";
		case Wand::earthStaff: return "Earth Staff";
		case Wand::waterStaff: return "Water Staff";
		case Wand::iceStaff: return "Ice Staff";
		case Wand::fireStaff: return "Fire Staff";
		case Wand::diamondStaff: return "Diamond Staff";
		case Wand::elderEarthStaff: return "Elder Earth Staff";
		case Wand::elderWaterStaff: return "Elder Water Staff";
		case Wand::elderIceStaff: return "Elder Ice Staff";
		case Wand::elderFireStaff: return "Elder Fire Staff";
		case Wand::elderStaff: return "Elder Staff";
		default: return "Wand";
	}
}

namespace
{
	struct TierRules
	{
		int minPoints = 0;
		int maxPoints = 0;
		int minMana = 0;
		int maxMana = 0;
		float minCharge = 0.0f;
		float maxCharge = 0.0f;
		int minElementsPerCast = 0;
		int maxElementsPerCast = 0;
		int minActiveSlots = 0;
		int maxActiveSlots = 0;
		int maxCastCount = 0;
		float alwaysCastChance = 0.0f;
	};

	const TierRules tierRules[6] =
	{
		{},
		{3, 6, 3, 5, 0.45f, 0.75f, 2, 3, 1, 3, 2, 0.05f},
		{5, 8, 4, 7, 0.5f, 0.8f, 2, 4, 2, 4, 4, 0.08f},
		{7, 11, 5, 9, 0.6f, 0.95f, 3, 5, 2, 4, 5, 0.1f},
		{9, 14, 7, 12, 0.75f, 1.1f, 4, 6, 3, 4, 6, 0.15f},
		{12, 17, 9, 15, 0.9f, 1.3f, 5, 7, 3, 4, 7, 0.2f},
	};

	const int elementSlotCost = 2;
	const int emptySlotCost = 2;
	const int alwaysCastCost = 1;
	const int castCountCost = 1;
	const int maxElementsCost = 2;
	const int maxManaCost = 1;
	const int chargeSpeedCost = 1;
	const float chargeSpeedStep = 0.1f;
	const float affinityChance = 0.65f;
	const float affinityAlwaysCastChance = 0.45f;
	const float wowChance = 0.02f;
	const float emptySlotChance = 0.5f;
	const float affinityThirdChance = 0.01f;
	const int maxElementCopies = 2;
	const float tier1EmptySlotChance = 0.9f;
	const float combineDuplicateChance = 0.5f;
	const float badWandChance = 0.02f;
	const float tier2MaxElementsBonusChance = 0.25f;

	Wand::WandSprite getRandomSpriteForTier(int tier, std::ranlux24_base &rng)
	{
		switch (tier)
		{
			case 1:
			{
				const Wand::WandSprite sprites[] = {Wand::oakWand, Wand::birchWand, Wand::cherryWand};
				return sprites[getRandomInt(rng, 0, 2)];
			}
			case 2:
			{
				const Wand::WandSprite sprites[] = {Wand::longOakWand, Wand::ashWand, Wand::opalWand};
				return sprites[getRandomInt(rng, 0, 2)];
			}
			case 3:
			{
				const Wand::WandSprite sprites[] = {
					Wand::boneWand,
					Wand::unicornWand,
					Wand::cobaltWand,
					Wand::obsidianWand,
					Wand::earthWand,
					Wand::waterWand,
					Wand::iceWand,
					Wand::fireWand,
					Wand::elderWand,
				};
				return sprites[getRandomInt(rng, 0, 8)];
			}
			case 4:
			{
				const Wand::WandSprite sprites[] = {
					Wand::earthStaff,
					Wand::waterStaff,
					Wand::iceStaff,
					Wand::fireStaff,
					Wand::diamondStaff,
				};
				return sprites[getRandomInt(rng, 0, 4)];
			}
			case 5:
			{
				const Wand::WandSprite sprites[] = {
					Wand::elderEarthStaff,
					Wand::elderWaterStaff,
					Wand::elderIceStaff,
					Wand::elderFireStaff,
					Wand::elderStaff,
				};
				return sprites[getRandomInt(rng, 0, 4)];
			}
			default:
				break;
		}

		return Wand::starterWand;
	}

	int getAffinityElement(Wand::WandSprite sprite)
	{
		switch (sprite)
		{
			case Wand::earthWand:
			case Wand::earthStaff:
			case Wand::elderEarthStaff:
				return Elements::Earth;
			case Wand::waterWand:
			case Wand::waterStaff:
			case Wand::elderWaterStaff:
				return Elements::Water;
			case Wand::iceWand:
			case Wand::iceStaff:
			case Wand::elderIceStaff:
				return Elements::Ice;
			case Wand::fireWand:
			case Wand::fireStaff:
			case Wand::elderFireStaff:
				return Elements::Fire;
			default:
				break;
		}

		return Elements::NoneElement;
	}

	bool isElderWandSprite(Wand::WandSprite sprite)
	{
		switch (sprite)
		{
			case Wand::elderWand:
			case Wand::elderStaff:
			case Wand::elderEarthStaff:
			case Wand::elderWaterStaff:
			case Wand::elderIceStaff:
			case Wand::elderFireStaff:
				return true;
			default:
				break;
		}

		return false;
	}

	int getRandomElement(std::ranlux24_base &rng)
	{
		return getRandomInt(rng, Elements::Fire, Elements::Ice);
	}

	int getRandomElementWithAffinity(std::ranlux24_base &rng, int affinity,
		const std::array<int, 5> &elementCounts, int maxCopies, int maxAffinityCopies)
	{
		std::array<int, 4> available = {Elements::Fire, Elements::Water, Elements::Earth, Elements::Ice};
		int availableCount = 0;
		for (int i = 0; i < 4; i++)
		{
			int element = available[i];
			int limit = element == affinity ? maxAffinityCopies : maxCopies;
			if (elementCounts[element] < limit)
			{
				available[availableCount++] = element;
			}
		}

		if (affinity != Elements::NoneElement)
		{
			int limit = maxAffinityCopies;
			if (elementCounts[affinity] < limit && getRandomChance(rng, affinityChance))
			{
				return affinity;
			}
		}

		if (availableCount > 0)
		{
			return available[getRandomInt(rng, 0, availableCount - 1)];
		}

		return getRandomElement(rng);
	}

	WandSlot &getSlotByIndex(Wand &wand, int index)
	{
		switch (index)
		{
			case 0: return wand.up;
			case 1: return wand.down;
			case 2: return wand.left;
			default: return wand.right;
		}
	}
}

// Generates a balanced random wand using a tier point budget.
Wand getRandomWand(int tier, std::ranlux24_base &rng)
{
	if (tier <= 0)
	{
		return makeStarterWand(rng);
	}
	if (tier > 5)
	{
		tier = 5;
	}

	const auto &rules = tierRules[tier];

	Wand wand;
	auto disableSlot = [&](WandSlot &slot)
	{
		slot.type = WandSlotType::Disabled;
		slot.element = Elements::NoneElement;
		slot.castCount = 1;
	};

	disableSlot(wand.up);
	disableSlot(wand.down);
	disableSlot(wand.left);
	disableSlot(wand.right);
	disableSlot(wand.alwaysCast);

	wand.wandSprite = getRandomSpriteForTier(tier, rng);
	const auto sprite = static_cast<Wand::WandSprite>(wand.wandSprite);
	const int affinityElement = getAffinityElement(sprite);
	const bool isUnicorn = sprite == Wand::unicornWand;
	const bool isElder = isElderWandSprite(sprite);
	int maxAffinityCopies = maxElementCopies;
	// affinity wands can very rarely roll a third matching element
	if (affinityElement != Elements::NoneElement && getRandomChance(rng, affinityThirdChance))
	{
		maxAffinityCopies = maxElementCopies + 1;
	}

	int points = getRandomInt(rng, rules.minPoints, rules.maxPoints);
	if (isElder)
	{
		points += 2;
	}

	bool wowRoll = getRandomChance(rng, wowChance);
	int maxCastCountAllowed = rules.maxCastCount;
	int maxElementsAllowed = rules.maxElementsPerCast;
	bool usedOvercap = false;
	if (wowRoll)
	{
		points += getRandomInt(rng, 1, 2);
		maxCastCountAllowed = rules.maxCastCount + 1;
		maxElementsAllowed = std::min(rules.maxElementsPerCast + 1, 7);
	}

	wand.maxMana = rules.minMana;
	wand.manaChargeSpeed = rules.minCharge;
	wand.maxElementsPerCast = rules.minElementsPerCast;
	if (tier >= 2 && points >= maxElementsCost && wand.maxElementsPerCast < maxElementsAllowed &&
		getRandomChance(rng, tier2MaxElementsBonusChance))
	{
		wand.maxElementsPerCast++;
		points -= maxElementsCost;
	}

	std::array<int, 4> slotOrder = {0, 2, 1, 3};
	if (getRandomChance(rng, 0.5f))
	{
		slotOrder = {0, 3, 1, 2};
	}
	int slotCursor = 0;
	int activeSlots = 0;

	auto hasAvailableSlot = [&]()
	{
		return wand.up.type == WandSlotType::Disabled ||
			wand.left.type == WandSlotType::Disabled ||
			wand.right.type == WandSlotType::Disabled ||
			wand.down.type == WandSlotType::Disabled;
	};

	auto getNextSlotIndex = [&]()
	{
		while (slotCursor < (int)slotOrder.size())
		{
			int slotIndex = slotOrder[slotCursor++];
			if (getSlotByIndex(wand, slotIndex).type == WandSlotType::Disabled)
			{
				return slotIndex;
			}
		}
		return -1;
	};

	std::array<int, 5> elementCounts = {};
	auto adjustElementCount = [&](int element, int delta)
	{
		if (element >= Elements::Fire && element <= Elements::Ice)
		{
			elementCounts[element] += delta;
		}
	};

	auto setSlotElement = [&](WandSlot &slot, int element)
	{
		if (slot.type == WandSlotType::Element)
		{
			adjustElementCount(slot.element, -1);
		}
		slot.element = element;
		adjustElementCount(element, 1);
	};

	auto setElementSlot = [&](int slotIndex, int element, int castCount)
	{
		auto &slot = getSlotByIndex(wand, slotIndex);
		if (slot.type == WandSlotType::Disabled)
		{
			activeSlots++;
		}
		slot.type = WandSlotType::Element;
		setSlotElement(slot, element);
		slot.castCount = castCount;
	};

	auto setEmptySlot = [&](int slotIndex)
	{
		auto &slot = getSlotByIndex(wand, slotIndex);
		if (slot.type == WandSlotType::Disabled)
		{
			activeSlots++;
		}
		if (slot.type == WandSlotType::Element)
		{
			adjustElementCount(slot.element, -1);
		}
		slot.type = WandSlotType::Empty;
		slot.element = Elements::NoneElement;
		slot.castCount = 1;
	};

	auto setAlwaysCastSlot = [&](int element)
	{
		if (wand.alwaysCast.type == WandSlotType::Element)
		{
			adjustElementCount(wand.alwaysCast.element, -1);
		}
		wand.alwaysCast.type = WandSlotType::Element;
		wand.alwaysCast.element = element;
		wand.alwaysCast.castCount = 1;
		adjustElementCount(element, 1);
	};

	if (affinityElement != Elements::NoneElement)
	{
		setElementSlot(0, affinityElement, std::max(1, wand.up.castCount));
		points -= elementSlotCost;
		if (points < 0) { points = 0; }
	}

	float alwaysCastChance = rules.alwaysCastChance;
	if (affinityElement != Elements::NoneElement)
	{
		alwaysCastChance = std::max(alwaysCastChance, affinityAlwaysCastChance);
	}

	if (affinityElement != Elements::NoneElement && points >= alwaysCastCost &&
		wand.alwaysCast.type != WandSlotType::Element && getRandomChance(rng, alwaysCastChance))
	{
		setAlwaysCastSlot(affinityElement);
		points -= alwaysCastCost;
		if (points < 0) { points = 0; }
	}

	if (isUnicorn)
	{
		int requiredSlots = 3;
		if (rules.maxActiveSlots >= 4 && points >= elementSlotCost * 4 && getRandomChance(rng, 0.25f))
		{
			requiredSlots = 4;
		}

		std::array<int, 4> elements = {Elements::Fire, Elements::Water, Elements::Earth, Elements::Ice};
		std::shuffle(elements.begin(), elements.end(), rng);

		for (int i = 0; i < requiredSlots; i++)
		{
			int slotIndex = getNextSlotIndex();
			if (slotIndex < 0) { break; }
			setElementSlot(slotIndex, elements[i], 1);
			points -= elementSlotCost;
			if (points < 0) { points = 0; }
		}
	}

	while (activeSlots < rules.minActiveSlots && hasAvailableSlot() && points >= elementSlotCost)
	{
		int slotIndex = getNextSlotIndex();
		if (slotIndex < 0) { break; }
		setElementSlot(slotIndex, getRandomElementWithAffinity(rng, affinityElement, elementCounts,
			maxElementCopies, maxAffinityCopies), 1);
		points -= elementSlotCost;
	}

	// tier 1 wands should almost always offer an empty slot for customization
	if (tier == 1 && hasAvailableSlot() && activeSlots < rules.maxActiveSlots &&
		getRandomChance(rng, tier1EmptySlotChance))
	{
		int slotIndex = getNextSlotIndex();
		if (slotIndex >= 0)
		{
			setEmptySlot(slotIndex);
			points -= emptySlotCost;
			if (points < 0) { points = 0; }
		}
	}

	int guard = 0;
	while (points > 0 && guard < 200)
	{
		guard++;

		enum ActionType
		{
			AddElementSlot,
			AddEmptySlot,
			IncreaseCastCount,
			AddAlwaysCast,
			IncreaseMaxElements,
			IncreaseMaxMana,
			IncreaseChargeSpeed,
		};

		struct Action
		{
			ActionType type = AddElementSlot;
			int cost = 0;
			int slotIndex = -1;
		};

		std::vector<Action> actions;
		actions.reserve(16);

		if (hasAvailableSlot() && activeSlots < rules.maxActiveSlots && points >= elementSlotCost)
		{
			actions.push_back({AddElementSlot, elementSlotCost});
		}

		if (hasAvailableSlot() && activeSlots < rules.maxActiveSlots && points >= emptySlotCost &&
			getRandomChance(rng, emptySlotChance))
		{
			actions.push_back({AddEmptySlot, emptySlotCost});
			actions.push_back({AddEmptySlot, emptySlotCost});
		}

		if (points >= castCountCost)
		{
			auto canIncreaseCast = [&](const WandSlot &slot)
			{
				if (slot.type != WandSlotType::Element) { return false; }
				if (slot.castCount < rules.maxCastCount) { return true; }
				return wowRoll && !usedOvercap && slot.castCount < maxCastCountAllowed;
			};

			std::vector<int> elementSlots;
			elementSlots.reserve(4);
			if (canIncreaseCast(wand.up)) { elementSlots.push_back(0); }
			if (canIncreaseCast(wand.down)) { elementSlots.push_back(1); }
			if (canIncreaseCast(wand.left)) { elementSlots.push_back(2); }
			if (canIncreaseCast(wand.right)) { elementSlots.push_back(3); }

			if (!elementSlots.empty())
			{
				int slotIndex = elementSlots[getRandomInt(rng, 0, (int)elementSlots.size() - 1)];
				actions.push_back({IncreaseCastCount, castCountCost, slotIndex});
				actions.push_back({IncreaseCastCount, castCountCost, slotIndex});
				actions.push_back({IncreaseCastCount, castCountCost, slotIndex});
			}
		}

		if (points >= alwaysCastCost && wand.alwaysCast.type != WandSlotType::Element)
		{
			bool canAddAlwaysCast = true;
			if (affinityElement != Elements::NoneElement &&
				elementCounts[affinityElement] >= maxAffinityCopies)
			{
				canAddAlwaysCast = false;
			}

			if (canAddAlwaysCast && getRandomChance(rng, alwaysCastChance))
			{
				actions.push_back({AddAlwaysCast, alwaysCastCost});
			}
		}

		if (points >= maxElementsCost && wand.maxElementsPerCast < maxElementsAllowed)
		{
			actions.push_back({IncreaseMaxElements, maxElementsCost});
			if (tier >= 2 && getRandomChance(rng, 0.35f))
			{
				actions.push_back({IncreaseMaxElements, maxElementsCost});
			}
		}

		if (points >= maxManaCost && wand.maxMana < rules.maxMana)
		{
			actions.push_back({IncreaseMaxMana, maxManaCost});
		}

		if (points >= chargeSpeedCost && wand.manaChargeSpeed + chargeSpeedStep <= rules.maxCharge + 0.0001f)
		{
			actions.push_back({IncreaseChargeSpeed, chargeSpeedCost});
		}

		if (actions.empty())
		{
			break;
		}

		Action action = actions[getRandomInt(rng, 0, (int)actions.size() - 1)];
		switch (action.type)
		{
		case AddElementSlot:
		{
			int slotIndex = getNextSlotIndex();
			if (slotIndex < 0) { break; }
			setElementSlot(slotIndex, getRandomElementWithAffinity(rng, affinityElement, elementCounts,
				maxElementCopies, maxAffinityCopies), 1);
			points -= elementSlotCost;
		} break;
		case AddEmptySlot:
		{
			int slotIndex = getNextSlotIndex();
			if (slotIndex < 0) { break; }
			setEmptySlot(slotIndex);
			points -= emptySlotCost;
		} break;
		case IncreaseCastCount:
		{
			auto &slot = getSlotByIndex(wand, action.slotIndex);
			if (slot.castCount < rules.maxCastCount ||
				(wowRoll && !usedOvercap && slot.castCount < maxCastCountAllowed))
			{
				slot.castCount++;
				if (slot.castCount > rules.maxCastCount)
				{
					usedOvercap = true;
				}
				points -= castCountCost;
			}
		} break;
		case AddAlwaysCast:
		{
			int alwaysCastElement = affinityElement != Elements::NoneElement
				? affinityElement
				: getRandomElementWithAffinity(rng, affinityElement, elementCounts, maxElementCopies, maxAffinityCopies);
			setAlwaysCastSlot(alwaysCastElement);
			points -= alwaysCastCost;
		} break;
			case IncreaseMaxElements:
			{
				wand.maxElementsPerCast++;
				points -= maxElementsCost;
			} break;
			case IncreaseMaxMana:
			{
				wand.maxMana++;
				points -= maxManaCost;
			} break;
			case IncreaseChargeSpeed:
			{
				wand.manaChargeSpeed += chargeSpeedStep;
				points -= chargeSpeedCost;
			} break;
		}
	}

	// Prefer combining duplicate elements into fewer slots.
	if (getRandomChance(rng, combineDuplicateChance))
	{
		auto combineElement = [&](int element)
		{
			int slotIndices[4] = {-1, -1, -1, -1};
			int slotCount = 0;
			if (wand.up.type == WandSlotType::Element && wand.up.element == element) { slotIndices[slotCount++] = 0; }
			if (wand.left.type == WandSlotType::Element && wand.left.element == element) { slotIndices[slotCount++] = 2; }
			if (wand.right.type == WandSlotType::Element && wand.right.element == element) { slotIndices[slotCount++] = 3; }
			if (wand.down.type == WandSlotType::Element && wand.down.element == element) { slotIndices[slotCount++] = 1; }

			if (slotCount <= 1) { return; }

			int primaryIndex = slotIndices[0];
			auto &primarySlot = getSlotByIndex(wand, primaryIndex);

			for (int i = 1; i < slotCount; i++)
			{
				int otherIndex = slotIndices[i];
				auto &otherSlot = getSlotByIndex(wand, otherIndex);
				if (otherSlot.castCount <= 0) { continue; }

				int transferable = std::min(otherSlot.castCount, maxCastCountAllowed - primarySlot.castCount);
				if (transferable > 0)
				{
					primarySlot.castCount += transferable;
					otherSlot.castCount -= transferable;
				}

				if (otherSlot.castCount <= 0)
				{
					setEmptySlot(otherIndex);
				}
			}
		};

		combineElement(Elements::Fire);
		combineElement(Elements::Water);
		combineElement(Elements::Earth);
		combineElement(Elements::Ice);
	}

	int totalElementCount = 0;
	if (wand.up.type == WandSlotType::Element) { totalElementCount++; }
	if (wand.down.type == WandSlotType::Element) { totalElementCount++; }
	if (wand.left.type == WandSlotType::Element) { totalElementCount++; }
	if (wand.right.type == WandSlotType::Element) { totalElementCount++; }
	if (totalElementCount <= 0)
	{
		int slotIndex = 0;
		int element = getRandomElementWithAffinity(rng, affinityElement, elementCounts,
			maxElementCopies, maxAffinityCopies);
		setElementSlot(slotIndex, element, 1);
	}

	// Bias higher tiers (and tier 1) to have at least one slot with 2 uses.
	if ((tier >= 1) && maxCastCountAllowed >= 2)
	{
		int maxCount = 0;
		int elementSlots[4] = {-1, -1, -1, -1};
		int elementSlotCount = 0;
		if (wand.up.type == WandSlotType::Element)
		{
			maxCount = std::max(maxCount, wand.up.castCount);
			elementSlots[elementSlotCount++] = 0;
		}
		if (wand.down.type == WandSlotType::Element)
		{
			maxCount = std::max(maxCount, wand.down.castCount);
			elementSlots[elementSlotCount++] = 1;
		}
		if (wand.left.type == WandSlotType::Element)
		{
			maxCount = std::max(maxCount, wand.left.castCount);
			elementSlots[elementSlotCount++] = 2;
		}
		if (wand.right.type == WandSlotType::Element)
		{
			maxCount = std::max(maxCount, wand.right.castCount);
			elementSlots[elementSlotCount++] = 3;
		}

		if (elementSlotCount > 0 && maxCount < 2)
		{
			int pick = elementSlots[getRandomInt(rng, 0, elementSlotCount - 1)];
			auto &slot = getSlotByIndex(wand, pick);
			slot.castCount = std::min(maxCastCountAllowed, 2);
		}
	}

	bool allowBadWand = getRandomChance(rng, badWandChance);
	if (!allowBadWand)
	{
		int highestCastCount = 0;
		if (wand.up.type == WandSlotType::Element) { highestCastCount = std::max(highestCastCount, wand.up.castCount); }
		if (wand.down.type == WandSlotType::Element) { highestCastCount = std::max(highestCastCount, wand.down.castCount); }
		if (wand.left.type == WandSlotType::Element) { highestCastCount = std::max(highestCastCount, wand.left.castCount); }
		if (wand.right.type == WandSlotType::Element) { highestCastCount = std::max(highestCastCount, wand.right.castCount); }

		if (highestCastCount > wand.maxElementsPerCast)
		{
			if (wand.maxElementsPerCast < maxElementsAllowed)
			{
				wand.maxElementsPerCast = std::min(maxElementsAllowed, highestCastCount);
			}
			else
			{
				int clampValue = wand.maxElementsPerCast;
				if (wand.up.type == WandSlotType::Element) { wand.up.castCount = std::min(wand.up.castCount, clampValue); }
				if (wand.down.type == WandSlotType::Element) { wand.down.castCount = std::min(wand.down.castCount, clampValue); }
				if (wand.left.type == WandSlotType::Element) { wand.left.castCount = std::min(wand.left.castCount, clampValue); }
				if (wand.right.type == WandSlotType::Element) { wand.right.castCount = std::min(wand.right.castCount, clampValue); }
			}
		}
	}

	if (wand.maxMana > rules.maxMana) { wand.maxMana = rules.maxMana; }
	if (wand.manaChargeSpeed > rules.maxCharge) { wand.manaChargeSpeed = rules.maxCharge; }

	wand.sanitize();
	return wand;
}
