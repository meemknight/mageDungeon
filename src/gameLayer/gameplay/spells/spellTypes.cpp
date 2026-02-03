#include "spellTypes.h"



namespace SpellTypes
{

	static const SpellRecepie *getAllRecepies()
	{
		static SpellRecepie allRecepies[] =
		{
			{},
			{},
			{Elements::Fire},	//fireBolt,
			{Elements::Water},	//waterBolt,
			{Elements::Water,Elements::Water},	//waterHomingMissle,
			{Elements::Earth},	//earthBolt,
			{Elements::Earth,Elements::Earth},	//earthThorn,
			{Elements::Earth,Elements::Fire},	//fireStandby,
			{Elements::Earth,Elements::Water},	//waterStandby,
			{Elements::Earth,Elements::Ice},	//iceStandbySimple,
			{Elements::Earth,Elements::Water,Elements::Water},	//waterHomingStandby,
			{Elements::Earth,Elements::Ice,Elements::Ice},	//iceStandby,
			{Elements::Earth,Elements::Earth,Elements::Water},	//summonWaterSlime,
			{Elements::Earth,Elements::Earth,Elements::Fire},	//summonFireSlime,
			{Elements::Earth,Elements::Earth,Elements::Ice},	//summonIceSlime,
			{Elements::Earth,Elements::Water,Elements::Fire},	//cinderCompass,
			{Elements::Fire,Elements::Earth},	//thornWall,
			{Elements::Earth,Elements::Earth,Elements::Earth,Elements::Earth},	//wildGrowth,
			{Elements::Earth,Elements::Water,Elements::Earth,Elements::Water},	//earthWaterThorn,
			{Elements::Ice},	//iceBolt,
			{Elements::Fire,Elements::Fire,Elements::Fire},	//dragonsBreath,
			{Elements::Ice,Elements::Ice},	//iceTrap,
			{Elements::Ice,Elements::Fire},	//fireTrap,
			{Elements::Ice,Elements::Water},//waterTrap,
			{Elements::Ice,Elements::Earth},	//earthTrap,
			{Elements::Water,Elements::Fire},	//fireHomingMissle,
			{Elements::Water,Elements::Earth},	//earthHomingMissle,
			{Elements::Water,Elements::Ice},	//iceHomingMissle,
			{Elements::Water,Elements::Fire,Elements::Fire},	//fastFireBolt,
			{Elements::Water,Elements::Ice,Elements::Ice},	//fastIceBolt,
			{Elements::Water,Elements::Water,Elements::Fire},	//aimableFireBolt,
			{Elements::Water,Elements::Water,Elements::Ice},	//aimableIceBolt,
			{Elements::Water,Elements::Water,Elements::Earth},	//aimableEarthBolt,
			{Elements::Fire,Elements::Fire},	//flameWall,
			{Elements::Fire,Elements::Ice},	//iceWall,
			{Elements::Fire,Elements::Water},	//boulder,
			{Elements::Water,Elements::Water,Elements::Water},	//waterSiphon,
			{Elements::Earth,Elements::Earth,Elements::Earth},	//earthRicochet,
			{Elements::Earth,Elements::Earth,Elements::Earth,Elements::Ice},	//earthRicochetIce,
			{Elements::Earth,Elements::Earth,Elements::Earth,Elements::Water},	//earthRicochetWater,
			{Elements::Ice,Elements::Ice,Elements::Ice},	//bigIceBlock,
			{Elements::Fire,Elements::Fire,Elements::Ice},	//iceDragonsBreath,
		};

		static_assert(sizeof(allRecepies) / sizeof(allRecepies[0]) == SPELLS_COUNT);
		return allRecepies;
	}

	std::unique_ptr<Spell> getSpellFromRecepie(SpellRecepie recepie)
	{
		const SpellRecepie *allRecepies = getAllRecepies();

		for (int i = 0; i < SPELLS_COUNT; i++)
		{
			if (allRecepies[i] == recepie)
			{
				return getSpell((Spells)i);
			}
		}

		if (recepie.count == 4)
		{
			bool used[5] = {};
			bool allDifferent = true;
			for (int i = 0; i < 4; i++)
			{
				int element = recepie.elements[i];
				if (element == Elements::NoneElement)
				{
					allDifferent = false;
					break;
				}
				if (used[element])
				{
					allDifferent = false;
					break;
				}
				used[element] = true;
			}
			if (allDifferent)
			{
				return std::make_unique<BasicMagicMissleSpell>(getWildMagicSpell());
			}
		}

		return getSpell((Spells)0);

	}

	SpellRecepie getSpellRecepie(int spellType)
	{
		const SpellRecepie *allRecepies = getAllRecepies();
		if (spellType < 0 || spellType >= SPELLS_COUNT)
		{
			return {};
		}
		return allRecepies[spellType];
	}

	const char *getSpellName(int spellType)
	{
		static const char *names[] =
		{
			"None",
			"Spark Bolt",
			"Fire Bolt",
			"Water Bolt",
			"Water Homing",
			"Earth Bolt",
			"Earth Thorn",
			"Fire Standby",
			"Water Standby",
			"Ice Standby",
			"Water Homing Standby",
			"Ice Standby Volley",
			"Summon Water Slime",
			"Summon Fire Slime",
			"Summon Ice Slime",
			"Cinder Compass",
			"Thorn Wall",
			"Wild Growth",
			"Earth Water Thorn",
			"Ice Bolt",
			"Dragon Breath",
			"Ice Trap",
			"Fire Trap",
			"Water Trap",
			"Earth Trap",
			"Fire Homing",
			"Earth Homing",
			"Ice Homing",
			"Fast Fire Bolt",
			"Fast Ice Bolt",
			"Aimable Fire Bolt",
			"Aimable Ice Bolt",
			"Aimable Earth Bolt",
			"Flame Wall",
			"Ice Wall",
			"Boulder",
			"Water Siphon",
			"Earth Ricochet",
			"Earth Ricochet Ice",
			"Earth Ricochet Water",
			"Big Ice Block",
			"Ice Dragon Breath",
		};

		static_assert(sizeof(names) / sizeof(names[0]) == SPELLS_COUNT);
		if (spellType < 0 || spellType >= SPELLS_COUNT)
		{
			return "Spell";
		}
		return names[spellType];
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

		case fastFireBolt:
		return std::make_unique<BasicMagicMissleSpell>(getFastMagicBoltSpell(Elements::Fire));
		break;

		case fastIceBolt:
		return std::make_unique<BasicMagicMissleSpell>(getFastMagicBoltSpell(Elements::Ice));
		break;

		case aimableFireBolt:
		return std::make_unique<BasicMagicMissleSpell>(getAimableBoltSpell(Elements::Fire));
		break;

		case aimableIceBolt:
		return std::make_unique<BasicMagicMissleSpell>(getAimableBoltSpell(Elements::Ice));
		break;

		case aimableEarthBolt:
		return std::make_unique<BasicMagicMissleSpell>(getAimableEarthBoltSpell());
		break;

		case earthBolt:
		return std::make_unique<BasicMagicMissleSpell>(getBasicMagicMissleSpell(Elements::Earth));
		break;

		case earthThorn:
		return std::make_unique<BasicMagicMissleSpell>(getEarthThornSpell());
		break;

		case fireStandby:
		return std::make_unique<StandbyProjectilesSpell>(getFireStandbySpell());
		break;

		case waterStandby:
		return std::make_unique<StandbyProjectilesSpell>(getWaterStandbySpell());
		break;

		case iceStandbySimple:
		return std::make_unique<StandbyProjectilesSpell>(getIceStandbySimpleSpell());
		break;

		case waterHomingStandby:
		return std::make_unique<StandbyProjectilesSpell>(getWaterHomingStandbySpell());
		break;

		case iceStandby:
		return std::make_unique<DualStandbyProjectilesSpell>(getIceStandbySpell());
		break;

		case summonWaterSlime:
		return std::make_unique<SummonSpell>(getSummonWaterSlimeSpell());
		break;

		case summonFireSlime:
		return std::make_unique<SummonSpell>(getSummonFireSlimeSpell());
		break;

		case summonIceSlime:
		return std::make_unique<SummonSpell>(getSummonIceSlimeSpell());
		break;

		case cinderCompass:
		return std::make_unique<HomingVolleySpell>(getCinderCompassSpell());
		break;

		case thornWall:
		return std::make_unique<ThornWallSpell>(getThornWallSpell());
		break;

		case wildGrowth:
		return std::make_unique<WildGrowthSpell>(getWildGrowthSpell());
		break;

		case earthWaterThorn:
		return std::make_unique<BasicMagicMissleSpell>(getEarthWaterThornSpell());
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
		return std::make_unique<EarthTrapSpell>(getEarthTrapSpell());
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

		case earthRicochet:
		return std::make_unique<TripleEarthRicochetSpell>(getEarthRicochetSpell());
		break;

		case earthRicochetIce:
		return std::make_unique<BasicMagicMissleSpell>(getEarthRicochetIceSpell());
		break;

		case earthRicochetWater:
		return std::make_unique<BasicMagicMissleSpell>(getEarthRicochetWaterSpell());
		break;

		case bigIceBlock:
		return std::make_unique<BasicMagicMissleSpell>(getBigIceBlockSpell());
		break;

		case iceDragonsBreath:
		return std::make_unique<BasicMagicMissleSpell>(getBasicBurstSpell(Elements::Ice));
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
