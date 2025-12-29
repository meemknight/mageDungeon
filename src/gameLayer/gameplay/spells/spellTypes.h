#include "spells.h"

namespace SpellTypes
{


	inline BasicMagicMissleSpell getBasicMagicMissleSpell(int element)
	{

		BasicMagicMissleSpell ret;

		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1;
		ret.projectile = std::make_unique<BasicMagicMissle>();
		//todo other stats

		return ret;
	}

	inline BasicMagicMissleSpell getBasicBurstSpell(int element)
	{
		
		BasicMagicMissleSpell ret;

		ret.element = element;
		ret.maxFireCount = 60;
		ret.elementsPerCast = 3;
		ret.triggerDelay = 0.03;
		ret.projectile = std::make_unique<BasicMagicMissle>();
		ret.projectile->timeAlieve = 0.25;
		ret.driftAngleDegrees = 35.f;
		//todo other stats

		return ret;
	}

};

