#pragma once
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <random>

struct ParticleSystem;

enum class StatusType
{
	Fire,
	Poison,
	Chill
};

struct StatusImmunities
{
	bool fire = false;
	bool poison = false;
	bool chill = false;
};

struct StatusEffects
{
	float fire = 0.0f;
	float poison = 0.0f;
	float chill = 0.0f;

	float fireParticleTimer = 0.0f;
	float poisonParticleTimer = 0.0f;
	float chillParticleTimer = 0.0f;

	float damageTickTimer = 0.0f;
	float damageAccumulator = 0.0f;
	float damageTickInterval = 0.5f;
};

struct StatusEffectTick
{
	float damage = 0.0f;
	float speedMultiplier = 1.0f;
	bool hasEffect = false;
};

void addStatusEffect(StatusEffects &effects, const StatusImmunities &immunities,
	StatusType type, float amount);

void addStatusEffectFromElement(StatusEffects &effects, const StatusImmunities &immunities,
	int element, float amount);

StatusEffectTick updateStatusEffects(StatusEffects &effects,
	const StatusImmunities &immunities, float deltaTime);

glm::vec4 getStatusTint(const StatusEffects &effects);

void updateStatusEffectParticles(StatusEffects &effects, ParticleSystem &particleSystem,
	std::ranlux24_base &rng, glm::vec2 pos, float deltaTime);
