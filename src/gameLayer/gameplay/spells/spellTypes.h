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

	inline TripleEarthRicochetSpell getEarthRicochetSpell()
	{
		TripleEarthRicochetSpell ret;
		ret.element = Elements::Earth;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<RicochetProjectile>();
		ret.projectile->element = Elements::Earth;
		ret.throwVelocity = 14.5f;
		ret.angleA = -30.0f;
		ret.angleB = 30.0f;
		return ret;
	}

	inline BasicMagicMissleSpell getEarthRicochetVolleySpell(int element, int shots, float damage,
		float delay, float driftDegrees)
	{
		BasicMagicMissleSpell ret;
		ret.element = element;
		ret.maxFireCount = shots;
		ret.triggerDelay = delay;
		ret.driftAngleDegrees = driftDegrees;
		ret.projectile = std::make_unique<RicochetProjectile>();
		ret.projectile->element = element;
		ret.throwVelocity = 14.5f;

		if (auto ricochet = dynamic_cast<RicochetProjectile *>(ret.projectile.get()))
		{
			ricochet->setDamage(damage);
		}

		return ret;
	}

	inline BasicMagicMissleSpell getEarthRicochetIceSpell()
	{
		return getEarthRicochetVolleySpell(Elements::Ice, 6, 4.0f, 0.2f, 6.0f);
	}

	inline BasicMagicMissleSpell getEarthRicochetWaterSpell()
	{
		return getEarthRicochetVolleySpell(Elements::Water, 24, 1.0f, 0.2f, 6.0f);
	}

	inline BasicMagicMissleSpell getEarthThornSpell()
	{
		BasicMagicMissleSpell ret;
		ret.element = Elements::Earth;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<EarthThornBoltProjectile>();
		ret.projectile->element = Elements::Earth;
		ret.throwVelocity = 7.5f;
		return ret;
	}

	inline StandbyProjectilesSpell getFireStandbySpell()
	{
		StandbyProjectilesSpell ret;
		HitStats hitStats;
		hitStats.damage = 2.5f;
		hitStats.pushBack = 4.6f;
		ret.element = Elements::Fire;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<BasicMagicMissle>(hitStats);
		ret.projectile->element = Elements::Fire;
		ret.standbyCount = 3;
		ret.throwVelocity = 10.0f;
		ret.standbyLifetime = 14.0f;
		return ret;
	}

	inline StandbyProjectilesSpell getWaterStandbySpell()
	{
		StandbyProjectilesSpell ret;
		HitStats hitStats;
		hitStats.damage = 2.5f;
		hitStats.pushBack = 4.6f;
		ret.element = Elements::Water;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<BasicMagicMissle>(hitStats);
		ret.projectile->element = Elements::Water;
		ret.standbyCount = 3;
		ret.throwVelocity = 10.0f;
		ret.standbyLifetime = 14.0f;
		return ret;
	}

	inline StandbyProjectilesSpell getWaterHomingStandbySpell()
	{
		StandbyProjectilesSpell ret;
		HitStats hitStats;
		hitStats.damage = 7.0f;
		hitStats.pushBack = 5.2f;
		ret.element = Elements::Water;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<HomingMagicMissle>(hitStats);
		ret.projectile->element = Elements::Water;
		ret.projectile->timeAlieve = 18.0f;
		ret.standbyCount = 4;
		ret.throwVelocity = 10.0f;
		ret.standbyLifetime = 18.0f;

		glm::vec4 startColor = elementToColor(Elements::Water); startColor.a = 0.3f;
		glm::vec4 endColor = {0.7f, 0.3f, 0.95f, 0.3f};
		ret.hasStandbyEmission = true;
		ret.standbyEmission.sustain = getBasicMagicMissleParticle(startColor, endColor);
		ret.standbyEmission.release = getBasicMagicMissleParticle(startColor, endColor);
		ret.standbyEmission.release.particleLifeTime *= 2.0f;
		ret.standbyEmission.emitTimer = 0.01f;
		ret.standbyEmission.sustain.folowParent = true;
		ret.standbyEmission.release.folowParent = true;
		ret.standbyEmission.create = ret.standbyEmission.sustain;
		ret.standbyEmission.create.folowParent = true;
		return ret;
	}

	inline DualStandbyProjectilesSpell getIceStandbySpell()
	{
		DualStandbyProjectilesSpell ret;
		ret.element = Elements::Ice;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;

		HitStats frostStats;
		frostStats.damage = 6.0f;
		frostStats.pushBack = 5.2f;
		auto frostProjectile = std::make_unique<BasicMagicMissle>(frostStats, 1.25f);
		frostProjectile->element = Elements::Ice;
		frostProjectile->physics.transform.size *= 1.2f;

		HitStats snowStats;
		snowStats.damage = 3.0f;
		snowStats.pushBack = 4.6f;
		auto snowProjectile = std::make_unique<BasicMagicMissle>(snowStats, 0.7f);
		snowProjectile->element = Elements::Ice;
		snowProjectile->physics.transform.size *= 0.7f;

		glm::vec4 frostStart = elementToSecondaryColor(Elements::Ice); frostStart.a = 0.35f;
		glm::vec4 frostEnd = elementToColor(Elements::Ice); frostEnd.a = 0.25f;
		ParticleEmissionSettings frostEmission;
		frostEmission.sustain = getBasicMagicMissleParticle(frostStart, frostEnd);
		frostEmission.release = getBasicMagicMissleParticle(frostStart, frostEnd);
		frostEmission.release.particleLifeTime *= 2.0f;
		frostEmission.emitTimer = 0.01f;
		frostEmission.sustain.folowParent = true;
		frostEmission.release.folowParent = true;
		frostEmission.create = frostEmission.sustain;
		frostEmission.create.folowParent = true;

		glm::vec4 snowStart = {0.95f, 0.97f, 1.0f, 0.35f};
		glm::vec4 snowEnd = {0.7f, 0.82f, 0.92f, 0.2f};
		ParticleEmissionSettings snowEmission;
		snowEmission.sustain = getBasicMagicMissleParticle(snowStart, snowEnd);
		snowEmission.release = getBasicMagicMissleParticle(snowStart, snowEnd);
		snowEmission.release.particleLifeTime *= 2.0f;
		snowEmission.emitTimer = 0.01f;
		snowEmission.sustain.folowParent = true;
		snowEmission.release.folowParent = true;
		snowEmission.create = snowEmission.sustain;
		snowEmission.create.folowParent = true;

		frostProjectile->hasCustomEmission = true;
		frostProjectile->customEmission = frostEmission;
		snowProjectile->hasCustomEmission = true;
		snowProjectile->customEmission = snowEmission;

		ret.primaryProjectile = std::move(frostProjectile);
		ret.primaryCount = 3;
		ret.primaryStandbyLifetime = 14.0f;
		ret.primaryThrowVelocity = 10.0f;
		ret.hasPrimaryEmission = true;
		ret.primaryEmission = frostEmission;

		ret.secondaryProjectile = std::move(snowProjectile);
		ret.secondaryCount = 3;
		ret.secondaryStandbyLifetime = 14.0f;
		ret.secondaryThrowVelocity = 10.0f;
		ret.hasSecondaryEmission = true;
		ret.secondaryEmission = snowEmission;

		return ret;
	}

	inline BasicMagicMissleSpell getEarthWaterThornSpell()
	{
		BasicMagicMissleSpell ret;
		ret.element = Elements::Earth;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<EarthWaterThornBoltProjectile>();
		ret.projectile->element = Elements::Earth;
		ret.throwVelocity = 7.5f;
		return ret;
	}

	inline BasicMagicMissleSpell getBigIceBlockSpell()
	{
		BasicMagicMissleSpell ret;
		ret.element = Elements::Ice;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<BigIceBlockProjectile>();
		ret.projectile->element = Elements::Ice;
		ret.throwVelocity = 7.0f;
		return ret;
	}

	inline BasicMagicMissleSpell getFastMagicBoltSpell(int element)
	{
		BasicMagicMissleSpell ret;
		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<FastMagicBoltProjectile>();
		ret.projectile->element = element;
		ret.throwVelocity = 6.5f;
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
		earthThorn,
		fireStandby,
		waterStandby,
		waterHomingStandby,
		iceStandby,
		earthWaterThorn,
		iceBolt,
		dragonsBreath,
		iceTrap,
		fireTrap,
		waterTrap,
		earthTrap,
		fireHomingMissle,
		earthHomingMissle,
		iceHomingMissle,
		fastFireBolt,
		fastIceBolt,
		flameWall,
		iceWall,
		boulder,
		waterSiphon,
		earthRicochet,
		earthRicochetIce,
		earthRicochetWater,
		bigIceBlock,

		SPELLS_COUNT
	};

	std::unique_ptr<Spell> getSpellFromRecepie(SpellRecepie recepie);

	std::unique_ptr<Spell> getSpell(int spellType);


};

