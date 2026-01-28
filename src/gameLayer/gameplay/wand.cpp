#include "wand.h"

Wand makeDefaultWand()
{
	Wand wand;

	wand.up = {WandSlotType::Element, Elements::Fire};
	wand.down = {WandSlotType::Element, Elements::Earth};
	wand.left = {WandSlotType::Element, Elements::Ice};
	wand.right = {WandSlotType::Element, Elements::Water};
	wand.maxElementsPerCast = 4;
	wand.sanitize();

	return wand;
}


Wand makeStarterWand()
{
	Wand wand;

	wand.up = {WandSlotType::Element, Elements::Ice};
	wand.maxElementsPerCast = 2;
	wand.sanitize();

	return wand;
}
