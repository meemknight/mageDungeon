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
			{Elements::Earth,Elements::Earth},	//earthThorn,z
			{Elements::Earth,Elements::Fire},	//fireStandby,
			{Elements::Earth,Elements::Water},	//waterStandby,
			{Elements::Earth,Elements::Ice},	//iceStandbySimple,
			{Elements::Earth,Elements::Water,Elements::Water},	//waterHomingStandby,
			{Elements::Earth,Elements::Ice,Elements::Ice},	//iceStandby,
			{Elements::Earth,Elements::Water,Elements::Water,Elements::Water},	//fastWaterStandby,
			{Elements::Earth,Elements::Water,Elements::Ice,Elements::Ice},	//fastIceStandby,
			{Elements::Earth,Elements::Fire,Elements::Water,Elements::Fire},	//meteoritesStandby,
			{Elements::Earth,Elements::Earth,Elements::Water},	//summonWaterSlime,
			{Elements::Earth,Elements::Earth,Elements::Fire},	//summonFireSlime,
			{Elements::Earth,Elements::Earth,Elements::Ice},	//summonIceSlime,
			{Elements::Earth,Elements::Water,Elements::Fire},	//cinderCompass,
			{Elements::Fire,Elements::Earth},	//thornWall,
			{Elements::Earth,Elements::Earth,Elements::Earth,Elements::Earth},	//wildGrowth,
			{Elements::Earth,Elements::Water,Elements::Earth},	//earthWaterThorn,
			{Elements::Earth,Elements::Water,Elements::Earth,Elements::Water},	//earthWaterHealing,
			{Elements::Earth,Elements::Water,Elements::Ice},	//earthWaterShield,
			{Elements::Ice},	//iceBolt,
			{Elements::Fire,Elements::Fire,Elements::Fire},	//dragonsBreath,
			{Elements::Fire,Elements::Fire,Elements::Water},	//waterDragonsBreath,
			{Elements::Fire,Elements::Fire,Elements::Earth},	//earthDragonsBreath,
			{Elements::Ice,Elements::Ice},	//iceTrap,
			{Elements::Ice,Elements::Fire},	//fireTrap,
			{Elements::Ice,Elements::Fire,Elements::Fire},	//bigFireTrap,
			{Elements::Ice,Elements::Fire,Elements::Ice},	//bigIceTrap,
			{Elements::Ice,Elements::Fire,Elements::Water},	//bigWaterTrap,
			{Elements::Ice,Elements::Fire,Elements::Earth},	//bigEarthTrap,
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
			{Elements::Ice,Elements::Ice,Elements::Water},	//chainBolt,
			{Elements::Ice,Elements::Ice,Elements::Fire},	//chainBoltFire,
			{Elements::Ice,Elements::Ice,Elements::Water,Elements::Water},	//bigChainBolt,
			{Elements::Ice,Elements::Ice,Elements::Fire,Elements::Fire},	//bigChainBoltFire,
			{Elements::Water,Elements::Water,Elements::Water,Elements::Water},	//waterTsunami,
			{Elements::Water,Elements::Water,Elements::Water,Elements::Fire},	//fireTsunami,
			{Elements::Water,Elements::Fire,Elements::Water},	//homingBoulders,
			{Elements::Ice,Elements::Earth,Elements::Ice},	//iceSword,
			{Elements::Ice,Elements::Earth,Elements::Fire},	//fireSword,
			{Elements::Earth,Elements::Earth,Elements::Earth},	//earthRicochet,
			{Elements::Earth,Elements::Earth,Elements::Earth,Elements::Ice},	//earthRicochetIce,
			{Elements::Earth,Elements::Earth,Elements::Earth,Elements::Water},	//earthRicochetWater,
			{Elements::Ice,Elements::Ice,Elements::Ice},	//bigIceBlock,
			{Elements::Fire,Elements::Fire,Elements::Ice},	//iceDragonsBreath,
			{Elements::Fire,Elements::Fire,Elements::Ice,Elements::Ice},	//bigIceDragonsBreath,
			{Elements::Fire,Elements::Fire,Elements::Water,Elements::Water},	//bigWaterDragonsBreath,
			{Elements::Fire,Elements::Water,Elements::Fire},	//meteorite,
			{Elements::Fire,Elements::Water,Elements::Ice},	//iceMeteorite,
			{Elements::Fire,Elements::Water,Elements::Fire,Elements::Water},	//meteoriteShower,
			{Elements::Fire,Elements::Water,Elements::Ice,Elements::Ice},	//iceMeteoriteShower,
			{Elements::Fire,Elements::Water,Elements::Fire,Elements::Water,Elements::Fire},	//meteoriteApocalipse,
			{Elements::Fire,Elements::Fire,Elements::Fire,Elements::Fire},	//inferno,
			{Elements::Water,Elements::Fire,Elements::Water,Elements::Fire},	//homingMeteorites,
			{Elements::Ice,Elements::Water,Elements::Ice},	//piercingIceBolt,
			{Elements::Fire,Elements::Ice,Elements::Fire},	//piercingFireBolt,
			{Elements::Ice,Elements::Water,Elements::Water},	//piercingWaterBolt,
			{Elements::Earth,Elements::Fire,Elements::Earth},	//piercingEarthBolt,
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
			"Fast Water Standby",
			"Fast Ice Standby",
			"Meteorites Standby",
			"Summon Water Slime",
			"Summon Fire Slime",
			"Summon Ice Slime",
			"Cinder Compass",
			"Thorn Wall",
			"Wild Growth",
			"Earth Water Thorn",
			"Healing",
			"Shielding",
			"Ice Bolt",
			"Dragon Breath",
			"Water Dragon Breath",
			"Earth Dragon Breath",
			"Ice Trap",
			"Fire Trap",
			"Big Fire Trap",
			"Big Ice Trap",
			"Big Water Trap",
			"Big Earth Trap",
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
			"Water Chain Bolt",
			"Fire Chain Bolt",
			"Big Ice Chain Bolt",
			"Big Fire Chain Bolt",
			"Tsunami",
			"Fire Tsunami",
			"Homing Boulders",
			"Ice Sword",
			"Fire Sword",
			"Earth Ricochet",
			"Earth Ricochet Ice",
			"Earth Ricochet Water",
			"Big Ice Block",
			"Ice Dragon Breath",
			"Big Ice Dragon Breath",
			"Big Water Dragon Breath",
			"Meteorite",
			"Ice Meteorite",
			"Meteorite Shower",
			"Ice Meteorite Shower",
			"Meteorite Apocalipse",
			"Inferno",
			"Homing Meteorites",
			"Piercing Ice Bolt",
			"Piercing Fire Bolt",
			"Piercing Water Bolt",
			"Piercing Earth Bolt",
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

		case fastWaterStandby:
		return std::make_unique<StandbyProjectilesSpell>(getFastBoltStandbySpell(Elements::Water));
		break;

		case fastIceStandby:
		return std::make_unique<StandbyProjectilesSpell>(getFastBoltStandbySpell(Elements::Ice));
		break;

		case meteoritesStandby:
		return std::make_unique<StandbyProjectilesSpell>(getMeteoritesStandbySpell());
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

		case earthWaterHealing:
		return std::make_unique<HealingSpell>(getHealingSpell());
		break;

		case earthWaterShield:
		return std::make_unique<ShieldSpell>(getShieldSpell());
		break;

		case iceBolt:
		return std::make_unique<BasicMagicMissleSpell>(getBasicMagicMissleSpell(Elements::Ice));
		break;

		case dragonsBreath:
		return std::make_unique<BasicMagicMissleSpell>(getBasicBurstSpell(Elements::Fire));
		break;

		case waterDragonsBreath:
		return std::make_unique<BasicMagicMissleSpell>(getBasicBurstSpell(Elements::Water));
		break;

		case earthDragonsBreath:
		return std::make_unique<BasicMagicMissleSpell>(getBasicBurstSpell(Elements::Earth));
		break;

		case iceTrap:
		return std::make_unique<BasicMagicMissleSpell>(getTrapSpell(Elements::Ice));
		break;

		case fireTrap:
		return std::make_unique<BasicMagicMissleSpell>(getTrapSpell(Elements::Fire));
		break;

		case bigFireTrap:
		return std::make_unique<BasicMagicMissleSpell>(getBigFireTrapSpell());
		break;

		case bigIceTrap:
		return std::make_unique<BasicMagicMissleSpell>(getBigIceTrapSpell());
		break;

		case bigWaterTrap:
		return std::make_unique<BasicMagicMissleSpell>(getBigWaterTrapSpell());
		break;

		case bigEarthTrap:
		return std::make_unique<EarthTrapSpell>(getBigEarthTrapSpell());
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

		case chainBolt:
		return std::make_unique<ChainBoltSpell>(getChainBoltSpell());
		break;

		case chainBoltFire:
		return std::make_unique<ChainBoltSpell>(getFireChainBoltSpell());
		break;

		case bigChainBolt:
		return std::make_unique<ChainBoltSpell>(getBigChainBoltSpell());
		break;

		case bigChainBoltFire:
		return std::make_unique<ChainBoltSpell>(getBigFireChainBoltSpell());
		break;

		case waterTsunami:
		return std::make_unique<TsunamiSpell>(getTsunamiSpell(Elements::Water));
		break;

		case fireTsunami:
		return std::make_unique<TsunamiSpell>(getTsunamiSpell(Elements::Fire));
		break;

		case homingBoulders:
		return std::make_unique<HomingBouldersSpell>(getHomingBouldersSpell());
		break;

		case iceSword:
		return std::make_unique<WaterSiphonSpell>(getIceSwordSpell());
		break;

		case fireSword:
		return std::make_unique<WaterSiphonSpell>(getFireSwordSpell());
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

		case bigIceDragonsBreath:
		return std::make_unique<BasicMagicMissleSpell>(getBigIceDragonsBreathSpell());
		break;

		case bigWaterDragonsBreath:
		return std::make_unique<BasicMagicMissleSpell>(getBigWaterDragonsBreathSpell());
		break;

		case meteorite:
		return std::make_unique<BasicMagicMissleSpell>(getMeteoriteSpell(Elements::Fire));
		break;

		case iceMeteorite:
		return std::make_unique<BasicMagicMissleSpell>(getMeteoriteSpell(Elements::Ice));
		break;

		case meteoriteShower:
		return std::make_unique<MeteoriteShowerSpell>(getMeteoriteShowerSpell(Elements::Fire));
		break;

		case iceMeteoriteShower:
		return std::make_unique<MeteoriteShowerSpell>(getMeteoriteShowerSpell(Elements::Ice));
		break;

		case meteoriteApocalipse:
		return std::make_unique<MeteoriteShowerSpell>(getMeteoriteApocalipseSpell());
		break;

		case inferno:
		return std::make_unique<InfernoSpell>(getInfernoSpell());
		break;

		case homingMeteorites:
		return std::make_unique<HomingMeteoriteVolleySpell>(getHomingMeteoritesSpell());
		break;

		case piercingIceBolt:
		return std::make_unique<BasicMagicMissleSpell>(getPiercingBoltSpell(Elements::Ice));
		break;

		case piercingFireBolt:
		return std::make_unique<BasicMagicMissleSpell>(getPiercingBoltSpell(Elements::Fire));
		break;

		case piercingWaterBolt:
		return std::make_unique<BasicMagicMissleSpell>(getPiercingBoltSpell(Elements::Water));
		break;

		case piercingEarthBolt:
		return std::make_unique<BasicMagicMissleSpell>(getPiercingBoltSpell(Elements::Earth));
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
