#include "wand.h"
#include <randomStuff.h>

Wand makeTestWand()
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


Wand makeStarterWand(std::ranlux24_base &rng)
{
	Wand wand;

	int element = getRandomInt(rng, Elements::Fire, Elements::Ice);
	wand.up = {WandSlotType::Element, element};
	wand.maxElementsPerCast = 2;
	wand.sanitize();

	return wand;
}
