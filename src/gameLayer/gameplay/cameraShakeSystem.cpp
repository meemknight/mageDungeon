#include <gameplay/cameraShakeSystem.h>
#include <randomStuff.h>
#include <glm/common.hpp>
#include <cmath>

namespace
{
	constexpr float PI2 = 6.2831853071f;
}

void CameraShakeSystem::clear()
{
	activeShakes.clear();
	current = {};
}

void CameraShakeSystem::addShake(CameraShakeType type, float intensity, float duration, std::ranlux24_base &rng)
{
	intensity = glm::clamp(intensity, 0.0f, 3.0f);
	duration = glm::clamp(duration, 0.01f, 5.0f);
	if (intensity <= 0.0f) { return; }

	CameraShakeInstance instance;
	instance.type = type;
	instance.intensity = intensity;
	instance.duration = duration;
	instance.timeRemaining = duration;
	instance.timeElapsed = 0.0f;
	instance.phaseX = getRandomFloat(rng, 0.0f, PI2);
	instance.phaseY = getRandomFloat(rng, 0.0f, PI2);
	instance.phaseRot = getRandomFloat(rng, 0.0f, PI2);

	if (type == CameraShakeType::Impact)
	{
		instance.freqX = getRandomFloat(rng, 20.0f, 32.0f);
		instance.freqY = getRandomFloat(rng, 23.0f, 36.0f);
		instance.freqRot = getRandomFloat(rng, 16.0f, 24.0f);
	}
	else
	{
		instance.freqX = getRandomFloat(rng, 8.0f, 13.0f);
		instance.freqY = getRandomFloat(rng, 9.0f, 14.0f);
		instance.freqRot = getRandomFloat(rng, 6.0f, 10.0f);
	}

	activeShakes.push_back(instance);
}

float CameraShakeSystem::getStrength(const CameraShakeInstance &instance) const
{
	if (instance.duration <= 0.0f) { return 0.0f; }
	float remaining = glm::clamp(instance.timeRemaining / instance.duration, 0.0f, 1.0f);
	float envelope = 0.0f;
	if (instance.type == CameraShakeType::Impact)
	{
		envelope = remaining * remaining;
	}
	else
	{
		envelope = remaining * (0.55f + 0.45f * remaining);
	}
	return instance.intensity * envelope;
}

CameraShakeResult CameraShakeSystem::sampleShake(const CameraShakeInstance &instance) const
{
	CameraShakeResult result;
	float t = instance.timeElapsed;
	float strength = getStrength(instance);
	if (strength <= 0.0f) { return result; }

	float nx = std::sin(t * instance.freqX + instance.phaseX) * 0.70f
		+ std::sin(t * (instance.freqX * 1.91f) + instance.phaseX * 0.63f) * 0.30f;
	float ny = std::sin(t * instance.freqY + instance.phaseY) * 0.68f
		+ std::sin(t * (instance.freqY * 1.73f) + instance.phaseY * 0.51f) * 0.32f;
	float nr = std::sin(t * instance.freqRot + instance.phaseRot) * 0.66f
		+ std::sin(t * (instance.freqRot * 1.67f) + instance.phaseRot * 0.57f) * 0.34f;

	float offsetUnit = instance.type == CameraShakeType::Impact ? 0.075f : 0.045f;
	float rotationUnit = instance.type == CameraShakeType::Impact ? 0.50f : 0.28f;

	result.offset = glm::vec2(nx, ny) * offsetUnit * strength;
	result.rotation = nr * rotationUnit * strength;
	return result;
}

void CameraShakeSystem::update(float deltaTime)
{
	if (deltaTime < 0.0f) { deltaTime = 0.0f; }

	for (auto &instance : activeShakes)
	{
		instance.timeElapsed += deltaTime;
		instance.timeRemaining -= deltaTime;
	}

	for (int i = 0; i < (int)activeShakes.size();)
	{
		if (activeShakes[i].timeRemaining <= 0.0f)
		{
			activeShakes[i] = activeShakes.back();
			activeShakes.pop_back();
		}
		else
		{
			i++;
		}
	}

	current = {};
	if (activeShakes.empty()) { return; }

	int strongestIndex = -1;
	float strongest = 0.0f;
	for (int i = 0; i < (int)activeShakes.size(); i++)
	{
		float s = getStrength(activeShakes[i]);
		if (s > strongest)
		{
			strongest = s;
			strongestIndex = i;
		}
	}

	if (strongestIndex >= 0)
	{
		current = sampleShake(activeShakes[strongestIndex]);
	}
}
