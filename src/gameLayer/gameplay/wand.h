#pragma once
#include <gameplay/elements.h>

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
};

struct Wand
{
	WandSlot up;
	WandSlot down;
	WandSlot left;
	WandSlot right;

	// Max elements allowed per cast (1..7).
	int maxElementsPerCast = 4;

	void sanitize()
	{
		if (maxElementsPerCast < 1) { maxElementsPerCast = 1; }
		if (maxElementsPerCast > 7) { maxElementsPerCast = 7; }
	}
};

Wand makeDefaultWand();
Wand makeStarterWand();
