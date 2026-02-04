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
		hitStats.damage = 0.2;

		ret.element = element;
		ret.maxFireCount = 171;
		ret.elementsPerCast = 3;
		ret.triggerDelay = 0.03;
		// Small animated particles for dragon breath bursts.
		auto buildDragonBreathEmission = [&](int burstElement)
		{
			ParticleEmissionSettings emission;
			glm::vec4 startColor = elementToSecondaryColor(burstElement); startColor.a = 0.65f;
			glm::vec4 endColor = elementToColor(burstElement); endColor.a = 0.35f;
			if (burstElement == Elements::Ice)
			{
				startColor.a = 0.8f;
				endColor.a = 0.55f;
			}

			ParticleSettings spark;
			if (burstElement == Elements::Ice)
			{
				spark = getFrostShardParticle(startColor, endColor);
				spark.animationType = ParticleSettings::ANIMATION_TYPES::animationFigure8;
				spark.onCreateCount = 2;
				spark.particleLifeTime = {0.12f, 0.25f};
				spark.velocityX *= 0.35f;
				spark.velocityY *= 0.35f;
				spark.dragX *= 0.6f;
				spark.dragY *= 0.6f;
				spark.createApearence.size *= 0.75f;
				spark.endApearence.size *= 0.75f;
				spark.animationSpeed = {-10.0f, 10.0f};
				spark.animationAcceleration = {-2.0f, 2.0f};
				spark.animationScaleX = {PIXEL_SIZE * 2.0f, PIXEL_SIZE * 4.0f};
				spark.animationScaleY = {PIXEL_SIZE * 2.0f, PIXEL_SIZE * 4.0f};
				spark.animationRotation = {-20.0f, 20.0f};
				spark.animationPhase = {0.0f, 6.2831853f};
			}
			else
			{
				spark = getBasicMagicMissleParticle(startColor, endColor);
				spark.animationType = ParticleSettings::ANIMATION_TYPES::animationNone;
				spark.onCreateCount = 2;
				spark.createApearence.size *= 1.2f;
				spark.endApearence.size *= 1.2f;
			}
			spark.folowParent = true;

			emission.sustain = spark;
			emission.create = spark;
			emission.release = spark;
			emission.release.particleLifeTime *= 1.6f;
			emission.release.folowParent = false;
		emission.emitTimer = 0.02f;

			return emission;
		};

		ret.projectile = std::make_unique<BasicMagicMissle>(hitStats, 1.2f);
		ret.projectile->timeAlieve = 0.25f;
		ret.driftAngleDegrees = 35.f;
		if (auto missle = dynamic_cast<BasicMagicMissle *>(ret.projectile.get()))
		{
			if (element == Elements::Water || element == Elements::Earth)
			{
				missle->statusAmount = 0.0f;
			}
			if (element == Elements::Ice || element == Elements::Fire)
			{
				missle->hasCustomEmission = true;
				missle->customEmission = buildDragonBreathEmission(element);
			}
		}

		return ret;
	}

	inline BasicMagicMissleSpell getBigDragonBreathSpell(int element)
	{
		auto ret = getBasicBurstSpell(element);
		ret.maxFireCount = 270;
		if (ret.projectile)
		{
			ret.projectile->timeAlieve = 2.0f;
		}
		return ret;
	}

	inline BasicMagicMissleSpell getBigIceDragonsBreathSpell()
	{
		return getBigDragonBreathSpell(Elements::Ice);
	}

	inline BasicMagicMissleSpell getBigWaterDragonsBreathSpell()
	{
		return getBigDragonBreathSpell(Elements::Water);
	}

	inline BasicMagicMissleSpell getTrapSpell(int element)
	{

		BasicMagicMissleSpell ret;

		HitStats hitStats;
		//hitStats.pushBack = 0.3;
		if (element == Elements::Water)
		{
			hitStats.damage = 10.0f;
		}
		else if (element == Elements::Earth)
		{
			hitStats.damage = 0.0f;
		}
		else
		{
			hitStats.damage = 5.0f;
		}

		ret.element = element;
		ret.throwVelocity = 0;
		ret.projectile = std::make_unique<TrapProjectile>(hitStats);
		ret.projectile->element = element;
		
		return ret;
	}

	// Big trap: larger radius with heavier particle ring.
	inline BasicMagicMissleSpell getBigTrapSpell(int element, float damage)
	{
		BasicMagicMissleSpell ret;

		HitStats hitStats;
		hitStats.damage = damage;

		ret.element = element;
		ret.throwVelocity = 0;
		auto trap = std::make_unique<TrapProjectile>(hitStats);
		trap->element = element;
		trap->trapRadious = 1.8f;
		trap->particleSizeScale = 1.35f;
		trap->particleCountScale = 2.4f;
		trap->ringStepScale = 0.7f;
		trap->ringEmitIntervalScale = 0.7f;
		trap->orbitEmitIntervalScale = 0.75f;
		trap->particleSystem.maxCount = 240;
		ret.projectile = std::move(trap);

		return ret;
	}

	inline BasicMagicMissleSpell getBigFireTrapSpell()
	{
		return getBigTrapSpell(Elements::Fire, 14.0f);
	}

	inline BasicMagicMissleSpell getBigIceTrapSpell()
	{
		return getBigTrapSpell(Elements::Ice, 14.0f);
	}

	inline BasicMagicMissleSpell getBigWaterTrapSpell()
	{
		return getBigTrapSpell(Elements::Water, 28.0f);
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

	inline BasicMagicMissleSpell getMeteoriteSpell(int element)
	{
		BasicMagicMissleSpell ret;
		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<MeteoriteProjectile>();
		ret.projectile->element = element;
		ret.throwVelocity = 9.0f;
		return ret;
	}

	inline MeteoriteShowerSpell getMeteoriteShowerSpell(int element)
	{
		MeteoriteShowerSpell ret;
		ret.element = element;
		ret.maxFireCount = 10;
		ret.triggerDelay = 0.22f;
		ret.impactDelay = 0.35f;
		ret.explosionRadius = 2.0f;
		ret.explosionDamage = 7.0f;
		ret.explosionBurn = 2.0f;
		ret.spawnAttempts = 16;
		return ret;
	}

	inline MeteoriteShowerSpell getMeteoriteApocalipseSpell()
	{
		auto ret = getMeteoriteShowerSpell(Elements::Fire);
		ret.maxFireCount = 20;
		return ret;
	}

	inline InfernoSpell getInfernoSpell()
	{
		InfernoSpell ret;
		ret.element = Elements::Fire;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.maxDuration = 1.8f;
		ret.spawnInterval = 0.004f;
		ret.fireDebuff = 10.0f;
		return ret;
	}

	inline HomingMeteoriteVolleySpell getHomingMeteoritesSpell()
	{
		HomingMeteoriteVolleySpell ret;
		ret.element = Elements::Fire;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<HomingMeteoriteProjectile>();
		ret.projectile->element = Elements::Fire;
		ret.shotCount = 5;
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

	inline StandbyProjectilesSpell getIceStandbySimpleSpell()
	{
		StandbyProjectilesSpell ret;
		HitStats hitStats;
		hitStats.damage = 2.5f;
		hitStats.pushBack = 4.6f;
		ret.element = Elements::Ice;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<BasicMagicMissle>(hitStats);
		ret.projectile->element = Elements::Ice;
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

	inline StandbyProjectilesSpell getFastBoltStandbySpell(int element)
	{
		StandbyProjectilesSpell ret;
		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto projectile = std::make_unique<FastMagicBoltProjectile>();
		projectile->element = element;
		ret.projectile = std::move(projectile);
		ret.standbyCount = 3;
		ret.throwVelocity = 6.5f;
		ret.standbyLifetime = 14.0f;

		glm::vec4 startColor = elementToSecondaryColor(element); startColor.a = 0.35f;
		glm::vec4 endColor = elementToColor(element); endColor.a = 0.2f;
		ret.hasStandbyEmission = true;
		ret.standbyEmission.sustain = getBasicMagicMissleParticle(startColor, endColor);
		ret.standbyEmission.release = getBasicMagicMissleParticle(startColor, endColor);
		ret.standbyEmission.release.particleLifeTime *= 1.6f;
		ret.standbyEmission.emitTimer = 0.02f;
		ret.standbyEmission.sustain.onCreateCount = 1;
		ret.standbyEmission.sustain.createApearence.size *= 0.55f;
		ret.standbyEmission.sustain.endApearence.size *= 0.55f;
		ret.standbyEmission.sustain.folowParent = true;
		ret.standbyEmission.release.folowParent = true;
		ret.standbyEmission.create = ret.standbyEmission.sustain;
		ret.standbyEmission.create.folowParent = true;

		ret.hasSecondaryEmission = true;
		ret.secondaryEmission.sustain = getBasicMagicMissleParticle(startColor, endColor);
		ret.secondaryEmission.release = getBasicMagicMissleParticle(startColor, endColor);
		ret.secondaryEmission.release.particleLifeTime *= 1.6f;
		ret.secondaryEmission.emitTimer = 0.02f;
		ret.secondaryEmission.sustain.onCreateCount = 1;
		ret.secondaryEmission.sustain.createApearence.size *= 0.5f;
		ret.secondaryEmission.sustain.endApearence.size *= 0.5f;
		ret.secondaryEmission.sustain.animationType = ParticleSettings::ANIMATION_TYPES::animationAtom;
		ret.secondaryEmission.sustain.animationSpeed = {9.0f, 14.0f};
		ret.secondaryEmission.sustain.animationScaleX = {PIXEL_SIZE * 2.6f, PIXEL_SIZE * 4.2f};
		ret.secondaryEmission.sustain.animationScaleY = {PIXEL_SIZE * 2.6f, PIXEL_SIZE * 4.2f};
		ret.secondaryEmission.sustain.animationPhase = {0.0f, 6.2831853f};
		ret.secondaryEmission.sustain.folowParent = true;
		ret.secondaryEmission.release.folowParent = true;
		ret.secondaryEmission.create = ret.secondaryEmission.sustain;
		ret.secondaryEmission.create.folowParent = true;
		return ret;
	}

	inline StandbyProjectilesSpell getMeteoritesStandbySpell()
	{
		StandbyProjectilesSpell ret;
		ret.element = Elements::Fire;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<MeteoriteProjectile>();
		ret.projectile->element = Elements::Fire;
		ret.standbyCount = 3;
		ret.throwVelocity = 9.0f;
		ret.standbyLifetime = 14.0f;

		glm::vec4 startColor = elementToSecondaryColor(Elements::Fire); startColor.a = 0.75f;
		glm::vec4 endColor = elementToColor(Elements::Fire); endColor.a = 0.55f;
		ret.hasStandbyEmission = true;
		ret.standbyEmission.sustain = getSmallSquareParticle(startColor, endColor);
		ret.standbyEmission.release = getSmallSquareParticle(startColor, endColor);
		ret.standbyEmission.release.particleLifeTime *= 1.5f;
		ret.standbyEmission.emitTimer = 0.025f;
		ret.standbyEmission.sustain.onCreateCount = 1;
		ret.standbyEmission.sustain.createApearence.size = {0.7f, 0.9f};
		ret.standbyEmission.sustain.endApearence.size = {0.7f, 0.9f};
		ret.standbyEmission.sustain.texture = getAssetManager().particleCircle;
		ret.standbyEmission.sustain.animationType = ParticleSettings::ANIMATION_TYPES::animationAtom;
		ret.standbyEmission.sustain.animationSpeed = {10.0f, 16.0f};
		ret.standbyEmission.sustain.animationScaleX = {PIXEL_SIZE * 2.0f, PIXEL_SIZE * 3.6f};
		ret.standbyEmission.sustain.animationScaleY = {PIXEL_SIZE * 2.0f, PIXEL_SIZE * 3.6f};
		ret.standbyEmission.sustain.animationPhase = {0.0f, 6.2831853f};
		ret.standbyEmission.sustain.folowParent = true;
		ret.standbyEmission.release.texture = getAssetManager().particleCircle;
		ret.standbyEmission.release.folowParent = false;
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

	inline SummonSpell getSummonWaterSlimeSpell()
	{
		SummonSpell ret;
		ret.element = Elements::Water;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto slime = std::make_unique<SlimeSummon>();
		slime->element = Elements::Water;
		slime->attackDamage = 5.0f;
		slime->statusAmount = 5.0f;
		slime->tileSet = getAssetManager().waterSlime;
		ret.summon = std::move(slime);
		ret.summonCount = 1;
		return ret;
	}

	inline SummonSpell getSummonFireSlimeSpell()
	{
		SummonSpell ret;
		ret.element = Elements::Fire;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto slime = std::make_unique<SlimeSummon>();
		slime->element = Elements::Fire;
		slime->attackDamage = 3.0f;
		slime->statusAmount = 2.0f;
		slime->tileSet = getAssetManager().fireSlime;
		ret.summon = std::move(slime);
		ret.summonCount = 1;
		return ret;
	}

	inline SummonSpell getSummonIceSlimeSpell()
	{
		SummonSpell ret;
		ret.element = Elements::Ice;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto slime = std::make_unique<SlimeSummon>();
		slime->element = Elements::Ice;
		slime->attackDamage = 4.0f;
		slime->statusAmount = 2.0f;
		slime->tileSet = getAssetManager().iceSlime;
		ret.summon = std::move(slime);
		ret.summonCount = 1;
		return ret;
	}

	inline HomingVolleySpell getCinderCompassSpell()
	{
		HomingVolleySpell ret;
		HitStats hitStats;
		hitStats.damage = 5.0f;
		hitStats.pushBack = 5.2f;
		ret.element = Elements::Fire;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto projectile = std::make_unique<HomingMagicMissle>(hitStats);
		projectile->element = Elements::Fire;
		ret.projectile = std::move(projectile);
		ret.throwVelocity = 6.0f;
		ret.directions = {
			{0.0f, -1.0f},
			{0.0f, 1.0f},
			{-1.0f, -0.4f},
			{-1.0f, 0.4f},
			{1.0f, -0.4f},
			{1.0f, 0.4f}
		};
		return ret;
	}

	inline ThornWallSpell getThornWallSpell()
	{
		ThornWallSpell ret;
		ret.element = Elements::Earth;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.thornCount = 15;
		ret.wallLength = 7.0f;
		ret.wallOffset = 1.2f;
		return ret;
	}

	inline WildGrowthSpell getWildGrowthSpell()
	{
		WildGrowthSpell ret;
		ret.element = Elements::Earth;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.maxThorns = 40;
		ret.wormCount = 40;
		ret.maxDistance = 28.0f;
		ret.maxDuration = 3.0f;
		ret.spawnInterval = 0.005f;
		return ret;
	}

	inline EarthTrapSpell getEarthTrapSpell()
	{
		EarthTrapSpell ret;
		ret.element = Elements::Earth;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.0f;
		ret.thornCount = 15;
		ret.minRadius = 0.25f;
		ret.maxRadius = 1.1f;
		ret.offsetJitter = 0.1f;
		ret.spawnAttempts = 12;
		ret.particleBurstCount = 3.0f;
		ret.thornDamage = 1.0f;
		return ret;
	}

	inline EarthTrapSpell getBigEarthTrapSpell()
	{
		auto ret = getEarthTrapSpell();
		ret.thornDamage = 2.0f;
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

	inline HealingSpell getHealingSpell()
	{
		HealingSpell ret;
		ret.element = Elements::Water;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.totalHealing = 3.0f;
		ret.healDuration = 2.0f;
		ret.continuousUpdateTimer = ret.healDuration;
		ret.particleInterval = 0.04f;
		ret.particleRadius = 0.5f;
		return ret;
	}

	inline ShieldSpell getShieldSpell()
	{
		ShieldSpell ret;
		ret.element = Elements::Water;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.totalShield = 2.0f;
		ret.shieldDuration = 2.0f;
		ret.continuousUpdateTimer = ret.shieldDuration;
		ret.particleInterval = 0.05f;
		ret.particleRadius = 0.5f;
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

	inline BasicMagicMissleSpell getPiercingBoltSpell(int element)
	{
		BasicMagicMissleSpell ret;
		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		ret.projectile = std::make_unique<PiercingBoltProjectile>();
		ret.projectile->element = element;
		ret.throwVelocity = 10.0f;
		return ret;
	}

	inline BasicMagicMissleSpell getAimableBoltSpell(int element)
	{
		BasicMagicMissleSpell ret;
		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto projectile = std::make_unique<AimableBoltProjectile>();
		projectile->element = element;
		projectile->hitStats.damage = 18.0f;
		projectile->hitStats.pushBack = 4.0f;
		projectile->moveSpeed = 5.5f;
		projectile->timeAlieve = 7.0f;
		ret.projectile = std::move(projectile);
		ret.throwVelocity = 5.5f;
		return ret;
	}

	inline BasicMagicMissleSpell getAimableEarthBoltSpell()
	{
		BasicMagicMissleSpell ret;
		ret.element = Elements::Earth;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto projectile = std::make_unique<AimableEarthBoltProjectile>();
		projectile->element = Elements::Earth;
		projectile->hitStats.damage = 18.0f;
		projectile->hitStats.pushBack = 4.0f;
		projectile->moveSpeed = 4.5f;
		projectile->timeAlieve = 7.0f;
		ret.projectile = std::move(projectile);
		ret.throwVelocity = 4.5f;
		return ret;
	}

	inline BasicMagicMissleSpell getWildMagicSpell()
	{
		BasicMagicMissleSpell ret;
		ret.element = Elements::NoneElement;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto projectile = std::make_unique<WildMagicBoltProjectile>();
		projectile->element = Elements::NoneElement;
		ret.projectile = std::move(projectile);
		ret.throwVelocity = 6.0f;
		return ret;
	}

	inline WaterSiphonSpell getWaterSiphonSpell()
	{
		WaterSiphonSpell ret;
		ret.element = Elements::Water;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.2f;
		return ret;
	}

	inline WaterSiphonSpell getSwordSpell(int element)
	{
		WaterSiphonSpell ret;
		ret.element = element;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.15f;
		ret.range = 2.4f;
		ret.beamWidth = 0.08f;
		ret.minDamage = 0.2f;
		ret.maxDamage = 1.6f;
		ret.statusAmount = 5.0f;
		ret.particleStartOffset = 0.55f;
		ret.particleSpawnCount = 6;
		ret.continuousUpdateTimer = 9.0f;
		return ret;
	}

	inline WaterSiphonSpell getIceSwordSpell()
	{
		return getSwordSpell(Elements::Ice);
	}

	inline WaterSiphonSpell getFireSwordSpell()
	{
		return getSwordSpell(Elements::Fire);
	}

	inline HomingBouldersSpell getHomingBouldersSpell()
	{
		HomingBouldersSpell ret;
		ret.element = Elements::NoneElement;
		ret.maxFireCount = 1;
		ret.triggerDelay = 0.1f;
		auto projectile = std::make_unique<HomingBoulderProjectile>();
		projectile->element = Elements::NoneElement;
		projectile->hitStats.damage = 8.0f;
		ret.projectile = std::move(projectile);
		ret.shotCount = 4;
		ret.throwVelocity = 9.0f;
		return ret;
	}

	inline TsunamiSpell getTsunamiSpell(int element)
	{
		TsunamiSpell ret;
		ret.element = element;
		ret.maxFireCount = 3;
		ret.triggerDelay = 0.18f;
		ret.bulletsPerWave = 16;
		ret.throwVelocity = 8.0f;
		ret.totalDamage = 70.0f;
		if (element == Elements::Fire)
		{
			ret.pushBack = 0.0f;
			ret.statusAmount = 3.0f;
		}
		else
		{
			ret.pushBack = 20.8f;
			ret.statusAmount = 0.0f;
		}
		ret.projectileSizeScale = 0.7f;
		ret.waveParticleCount = 6;
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
		iceStandbySimple,
		waterHomingStandby,
		iceStandby,
		fastWaterStandby,
		fastIceStandby,
		meteoritesStandby,
		summonWaterSlime,
		summonFireSlime,
		summonIceSlime,
		cinderCompass,
		thornWall,
		wildGrowth,
		earthWaterThorn,
		earthWaterHealing,
		earthWaterShield,
		iceBolt,
		dragonsBreath,
		waterDragonsBreath,
		earthDragonsBreath,
		iceTrap,
		fireTrap,
		bigFireTrap,
		bigIceTrap,
		bigWaterTrap,
		bigEarthTrap,
		waterTrap,
		earthTrap,
		fireHomingMissle,
		earthHomingMissle,
		iceHomingMissle,
		fastFireBolt,
		fastIceBolt,
		aimableFireBolt,
		aimableIceBolt,
		aimableEarthBolt,
		flameWall,
		iceWall,
		boulder,
		waterSiphon,
		waterTsunami,
		fireTsunami,
		homingBoulders,
		iceSword,
		fireSword,
		earthRicochet,
		earthRicochetIce,
			earthRicochetWater,
			bigIceBlock,
		iceDragonsBreath,
		bigIceDragonsBreath,
		bigWaterDragonsBreath,
		meteorite,
		iceMeteorite,
		meteoriteShower,
		iceMeteoriteShower,
		meteoriteApocalipse,
		inferno,
		homingMeteorites,
		piercingIceBolt,
		piercingFireBolt,
		piercingWaterBolt,
		piercingEarthBolt,

		SPELLS_COUNT
		};

	std::unique_ptr<Spell> getSpellFromRecepie(SpellRecepie recepie);

	std::unique_ptr<Spell> getSpell(int spellType);
	SpellRecepie getSpellRecepie(int spellType);
	const char *getSpellName(int spellType);


};

