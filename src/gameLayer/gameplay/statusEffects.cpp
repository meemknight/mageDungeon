#include "statusEffects.h"
#include <gameplay/particleSystem.h>
#include <gameplay/elements.h>
#include <particles/particleCreator.h>
#include <algorithm>
#include <glm/glm.hpp>

static float getCompressedDuration(float amount)
{
	if (amount <= 3.0f)
	{
		return amount;
	}
	return 3.0f + (amount - 3.0f) * 0.15f;
}

static float consumeEffect(float &amount, float deltaTime)
{
	if (amount <= 0.0f)
	{
		return 0.0f;
	}

	float duration = getCompressedDuration(amount);
	float rate = amount / std::max(duration, 0.001f);
	float damage = rate * deltaTime;
	amount = std::max(0.0f, amount - damage);
	return damage;
}

void addStatusEffect(StatusEffects &effects, const StatusImmunities &immunities,
	StatusType type, float amount)
{
	if (amount <= 0.0f)
	{
		return;
	}

	switch (type)
	{
		case StatusType::Fire:
			if (!immunities.fire) { effects.fire = std::max(effects.fire, amount); }
			break;
		case StatusType::Poison:
			if (!immunities.poison) { effects.poison = std::max(effects.poison, amount); }
			break;
		case StatusType::Chill:
			if (!immunities.chill) { effects.chill = std::max(effects.chill, amount); }
			break;
	}
}

void addStatusEffectFromElement(StatusEffects &effects, const StatusImmunities &immunities,
	int element, float amount)
{
	if (element == Elements::Fire)
	{
		addStatusEffect(effects, immunities, StatusType::Fire, amount);
	}
	else if (element == Elements::Ice)
	{
		addStatusEffect(effects, immunities, StatusType::Chill, amount);
	}
}

StatusEffectTick updateStatusEffects(StatusEffects &effects,
	const StatusImmunities &immunities, float deltaTime)
{
	StatusEffectTick tick;

	if (immunities.fire) { effects.fire = 0.0f; }
	if (immunities.poison) { effects.poison = 0.0f; }
	if (immunities.chill) { effects.chill = 0.0f; }

	const bool hasDamageEffect = effects.fire > 0.0f || effects.poison > 0.0f;
	if (hasDamageEffect || effects.chill > 0.0f)
	{
		tick.hasEffect = true;
	}

	if (hasDamageEffect && effects.damageTickTimer <= 0.0f && effects.damageAccumulator <= 0.0f)
	{
		effects.damageTickTimer = effects.damageTickInterval;
	}

	float damageThisFrame = 0.0f;

	if (effects.fire > 0.0f)
	{
		damageThisFrame += consumeEffect(effects.fire, deltaTime);
	}
	if (effects.poison > 0.0f)
	{
		damageThisFrame += consumeEffect(effects.poison, deltaTime);
	}
	if (effects.chill > 0.0f)
	{
		consumeEffect(effects.chill, deltaTime);
		float chillStrength = std::min(1.0f, effects.chill / 5.0f);
		tick.speedMultiplier = 1.0f - 0.5f * chillStrength;
	}

	if (hasDamageEffect)
	{
		effects.damageAccumulator += damageThisFrame;
		effects.damageTickTimer -= deltaTime;
		if (effects.damageTickTimer <= 0.0f)
		{
			effects.damageTickTimer += effects.damageTickInterval;
			tick.damage = effects.damageAccumulator;
			effects.damageAccumulator = 0.0f;
		}
	}
	else
	{
		if (effects.damageAccumulator > 0.0f)
		{
			tick.damage = effects.damageAccumulator;
			effects.damageAccumulator = 0.0f;
		}
		effects.damageTickTimer = 0.0f;
	}

	return tick;
}

glm::vec4 getStatusTint(const StatusEffects &effects)
{
	glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};

	float fireWeight = effects.fire > 0.0f ? 0.65f : 0.0f;
	float poisonWeight = effects.poison > 0.0f ? 0.65f : 0.0f;
	float chillWeight = effects.chill > 0.0f ? 0.65f : 0.0f;

	glm::vec4 fireTint = {1.0f, 0.6f, 0.5f, 1.0f};
	glm::vec4 poisonTint = {0.6f, 1.0f, 0.6f, 1.0f};
	glm::vec4 chillTint = {0.6f, 0.8f, 1.0f, 1.0f};

	if (fireWeight > 0.0f)
	{
		tint = glm::mix(tint, fireTint, fireWeight);
	}
	if (poisonWeight > 0.0f)
	{
		tint = glm::mix(tint, poisonTint, poisonWeight);
	}
	if (chillWeight > 0.0f)
	{
		tint = glm::mix(tint, chillTint, chillWeight);
	}

	return tint;
}

void updateStatusEffectParticles(StatusEffects &effects, ParticleSystem &particleSystem,
	std::ranlux24_base &rng, glm::vec2 pos, float deltaTime)
{
	if (effects.fire > 0.0f)
	{
		effects.fireParticleTimer -= deltaTime;
		if (effects.fireParticleTimer <= 0.0f)
		{
			effects.fireParticleTimer = 0.1f;
			auto particle = getStatusFireParticle({1.0f, 0.45f, 0.2f, 0.9f}, {1.0f, 0.85f, 0.35f, 0.7f});
			particleSystem.emitParticles(particle, pos, rng, pos);
		}
	}

	if (effects.poison > 0.0f)
	{
		effects.poisonParticleTimer -= deltaTime;
		if (effects.poisonParticleTimer <= 0.0f)
		{
			effects.poisonParticleTimer = 0.2f;
			auto particle = getStatusPoisonParticle({0.4f, 0.9f, 0.3f, 0.8f}, {0.2f, 0.6f, 0.2f, 0.6f});
			particleSystem.emitParticles(particle, pos, rng, pos);
		}
	}

	if (effects.chill > 0.0f)
	{
		effects.chillParticleTimer -= deltaTime;
		if (effects.chillParticleTimer <= 0.0f)
		{
			effects.chillParticleTimer = 0.16f;
			auto particle = getStatusChillParticle({0.95f, 0.98f, 1.0f, 0.92f}, {0.8f, 0.92f, 1.0f, 0.75f});
			particleSystem.emitParticles(particle, pos, rng, pos);
		}
	}
}
