#pragma once
#include "spells.h"


struct SpellRecepie
{

	constexpr static int MAX_ELEMENTS = 7;
	unsigned char elements[MAX_ELEMENTS] = {};
	char count = 0;

	SpellRecepie() = default;

	SpellRecepie(std::initializer_list<unsigned char> init)
	{
		count = static_cast<char>(
			std::min(init.size(), static_cast<size_t>(MAX_ELEMENTS))
			);

		std::copy_n(init.begin(), count, elements);
	}

	bool add(int element, int maxElements = MAX_ELEMENTS)
	{
		maxElements = std::min(MAX_ELEMENTS, maxElements);

		if (count < maxElements)
		{
			elements[count] = element;
			count++;
			return true;
		}

		return false;
	}

	void clear() { *this = {}; }

	bool operator==(const SpellRecepie &other) const
	{
		if (count != other.count)
			return false;

		return std::equal(
			elements,
			elements + count,
			other.elements
		);
	}

};



namespace SpellTypes
{

	inline BasicMagicMissleSpell getBasicMagicMissleSpell(int element)
	{

		BasicMagicMissleSpell ret;

		HitStats hitStats;

		if (element == 0)
		{
			hitStats.damage = 1;
			hitStats.pushBack = 2;


		}
		else
		{
			hitStats.damage = 3;
			hitStats.pushBack = 5.2;
		}

		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1;
		ret.projectile = std::make_unique<BasicMagicMissle>(hitStats);

		if (element == 0)
		{
			ret.projectile->physics.transform.size *= 0.8f;
		}

		return ret;

	}

	inline BasicMagicMissleSpell getBasicBurstSpell(int element)
	{
		
		BasicMagicMissleSpell ret;


		HitStats hitStats;
		hitStats.pushBack = 0.3;
		hitStats.damage = 0.5;

		ret.element = element;
		ret.maxFireCount = 100;
		ret.elementsPerCast = 3;
		ret.triggerDelay = 0.03;
		ret.projectile = std::make_unique<BasicMagicMissle>(hitStats, 2);
		ret.projectile->timeAlieve = 0.25;
		ret.driftAngleDegrees = 35.f;

		return ret;
	}

	inline BasicMagicMissleSpell getTrapSpell(int element)
	{

		BasicMagicMissleSpell ret;

		HitStats hitStats;
		//hitStats.pushBack = 0.3;
		hitStats.damage = 15;

		ret.element = element;
		ret.throwVelocity = 0;
		ret.projectile = std::make_unique<TrapProjectile>(hitStats);
		ret.projectile->element = element;
		
		return ret;
	}

	inline BasicMagicMissleSpell getHomingMissleSpell(int element)
	{
		BasicMagicMissleSpell ret;

		HitStats hitStats;
		hitStats.damage = 7;
		hitStats.pushBack = 5.2f;

		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<HomingMagicMissle>(hitStats);
		ret.projectile->element = element;

		return ret;
	}

	inline FlameWallSpell getFlameWallSpell(int element)
	{
		FlameWallSpell ret;
		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<ElementWallProjectile>(element);
		ret.projectile->element = element;
		ret.wallOffset = 1.2f;
		return ret;
	}

	inline BasicMagicMissleSpell getBoulderSpell()
	{
		BasicMagicMissleSpell ret;
		ret.element = Elements::NoneElement;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<BoulderProjectile>();
		ret.projectile->element = Elements::NoneElement;
		ret.throwVelocity = 9.0f;
		return ret;
	}

	inline WaterSiphonSpell getWaterSiphonSpell()
	{
		WaterSiphonSpell ret;
		ret.element = Elements::Water;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		return ret;
	}

	enum Spells
	{
		none,
		sparkBolt, //empty spell with no element
		fireBolt,
		waterBolt,
		waterHomingMissle,
		earthBolt,
		iceBolt,
		dragonsBreath,
		iceTrap,
		fireTrap,
		waterTrap,
		earthTrap,
		fireHomingMissle,
		earthHomingMissle,
		iceHomingMissle,
		flameWall,
		iceWall,
		boulder,
		waterSiphon,

		SPELLS_COUNT
	};

	std::unique_ptr<Spell> getSpellFromRecepie(SpellRecepie recepie);

	std::unique_ptr<Spell> getSpell(int spellType);


};

