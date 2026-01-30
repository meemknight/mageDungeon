#pragma once
#include <gameplay/elements.h>
#include <random>
#include <algorithm>

// Wand defines which elements can be selected in the spell UI.
// Empty slots can be filled later by other systems, disabled slots are inactive.
enum class WandSlotType
{
	Disabled,
	Empty,
	Element
};

struct WandSlot
{
	WandSlotType type = WandSlotType::Empty;
	int element = Elements::NoneElement;
	int castCount = 1;
};

// Quick action stores an element recipe bound to a wand.
struct QuickAction
{
	static constexpr int MAX_ELEMENTS = 7;
	unsigned char elements[MAX_ELEMENTS] = {};
	char count = 0;

	bool add(int element, int maxElements = MAX_ELEMENTS)
	{
		maxElements = std::min(MAX_ELEMENTS, maxElements);
		if (count < maxElements)
		{
			elements[count] = (unsigned char)element;
			count++;
			return true;
		}
		return false;
	}

	void clear() { count = 0; }
};

struct Wand
{
	WandSlot up;
	WandSlot down;
	WandSlot left;
	WandSlot right;

	WandSlot alwaysCast = WandSlot{WandSlotType::Disabled};

	int maxMana = 4;
	float manaChargeSpeed = 0.5f;

	// Max elements allowed per cast (1..7).
	int maxElementsPerCast = 4;
	int wandSprite = 0;
	QuickAction quickActions[4] = {};

	void sanitize()
	{
		if (maxElementsPerCast < 1) { maxElementsPerCast = 1; }
		if (maxElementsPerCast > 7) { maxElementsPerCast = 7; }
	}

	enum WandSprite
	{
		//tier 0 wands
		starterWand = 0,

		//tier 1 wands
		oakWand,
		birchWand,
		cherryWand,

		//tier 2 wands
		longOakWand,
		ashWand,
		opalWand,

		//tier 3 wands
		boneWand,

		//this wand will always spawn with minimum 3 (sometimes even 4) DIFFERENT elements on
		unicornWand,

		cobaltWand,
		obsidianWand,

		//this wand has earth elemental affinity
		earthWand,

		//this wand has water elemental affinity
		waterWand,

		//this wand has ice elemental affinity
		iceWand,

		//this wand has fire elemental affinity
		fireWand,

		//this wand is slightly more powerfull
		elderWand,

		//tier 4 wands

		//this wand has earth elemental affinity
		earthStaff,

		//this wand has water elemental affinity
		waterStaff,

		//this wand has ice elemental affinity
		iceStaff,

		//this wand has fire elemental affinity
		fireStaff,
		
		diamondStaff,

		//tier 5 wands

		//this wand has earth elemental affinity
		elderEarthStaff,

		//this wand has water elemental affinity
		elderWaterStaff,

		//this wand has ice elemental affinity
		elderIceStaff,

		//this wand has fire elemental affinity
		elderFireStaff,

		elderStaff,

	};

};

Wand makeTestWand();
Wand makeStarterWand(std::ranlux24_base &rng);
Wand getRandomWand(int tier, std::ranlux24_base &rng);

// Returns a display name for a wand sprite.
const char *getWandSpriteName(int sprite);

// When true, quick casts fire immediately instead of just loading.
constexpr bool kQuickCastInstant = true;
