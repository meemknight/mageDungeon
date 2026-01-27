#include "spellTypes.h"



namespace SpellTypes
{

	std::unique_ptr<Spell> getSpellFromRecepie(SpellRecepie recepie)
	{

		SpellRecepie allRecepies[] =
		{
			{},
			{},
			{Elements::Fire},	//fireBolt,
			{Elements::Water},	//waterBolt,
			{Elements::Water,Elements::Water},	//waterHomingMissle,
			{Elements::Earth},	//earthBolt,
			{Elements::Ice},	//iceBolt,
			{Elements::Fire,Elements::Fire,Elements::Fire},	//dragonsBreath,
			{Elements::Ice,Elements::Ice},	//iceTrap,
			{Elements::Ice,Elements::Fire},	//fireTrap,
			{Elements::Ice,Elements::Water},//waterTrap,
			{Elements::Ice,Elements::Earth},	//earthTrap,
			{Elements::Water,Elements::Fire},	//fireHomingMissle,
			{Elements::Water,Elements::Earth},	//earthHomingMissle,
			{Elements::Water,Elements::Ice},	//iceHomingMissle,
			{Elements::Fire,Elements::Fire},	//flameWall,
			{Elements::Fire,Elements::Ice},	//iceWall,
			{Elements::Fire,Elements::Water},	//boulder,
			{Elements::Water,Elements::Water,Elements::Water},	//waterSiphon,



		};

		static_assert(sizeof(allRecepies) / sizeof(allRecepies[0]) == SPELLS_COUNT);

		for (int i = 0; i < SPELLS_COUNT; i++)
		{
			if (allRecepies[i] == recepie)
			{
				return getSpell((Spells)i);
			}
		}

		return getSpell((Spells)0);

	}

	std::unique_ptr<Spell> getSpell(int spellType)
	{
		switch (spellType)
		{

		case fireBolt:
		return std::make_unique<BasicMagicMissleSpell>(getBasicMagicMissleSpell(Elements::Fire));
		break;

		case waterBolt:
		return std::make_unique<BasicMagicMissleSpell>(getBasicMagicMissleSpell(Elements::Water));
		break;

		case waterHomingMissle:
		return std::make_unique<BasicMagicMissleSpell>(getHomingMissleSpell(Elements::Water));
		break;

		case fireHomingMissle:
		return std::make_unique<BasicMagicMissleSpell>(getHomingMissleSpell(Elements::Fire));
		break;

		case earthHomingMissle:
		return std::make_unique<BasicMagicMissleSpell>(getHomingMissleSpell(Elements::Earth));
		break;

		case iceHomingMissle:
		return std::make_unique<BasicMagicMissleSpell>(getHomingMissleSpell(Elements::Ice));
		break;

		case earthBolt:
		return std::make_unique<BasicMagicMissleSpell>(getBasicMagicMissleSpell(Elements::Earth));
		break;

		case iceBolt:
		return std::make_unique<BasicMagicMissleSpell>(getBasicMagicMissleSpell(Elements::Ice));
		break;

		case dragonsBreath:
		return std::make_unique<BasicMagicMissleSpell>(getBasicBurstSpell(Elements::Fire));
		break;

		case iceTrap:
		return std::make_unique<BasicMagicMissleSpell>(getTrapSpell(Elements::Ice));
		break;

		case fireTrap:
		return std::make_unique<BasicMagicMissleSpell>(getTrapSpell(Elements::Fire));
		break;

		case waterTrap:
		return std::make_unique<BasicMagicMissleSpell>(getTrapSpell(Elements::Water));
		break;

		case earthTrap:
		return std::make_unique<BasicMagicMissleSpell>(getTrapSpell(Elements::Earth));
		break;

		case flameWall:
		return std::make_unique<FlameWallSpell>(getFlameWallSpell(Elements::Fire));
		break;

		case iceWall:
		return std::make_unique<FlameWallSpell>(getFlameWallSpell(Elements::Ice));
		break;

		case boulder:
		return std::make_unique<BasicMagicMissleSpell>(getBoulderSpell());
		break;

		case waterSiphon:
		return std::make_unique<WaterSiphonSpell>(getWaterSiphonSpell());
		break;

		case none:
		case sparkBolt:
		default:

		return std::make_unique<BasicMagicMissleSpell>(getBasicMagicMissleSpell(0));

		}

	}

	//next enemies pushing in one another
	//cast spells with the initial speed the player casts it


};
