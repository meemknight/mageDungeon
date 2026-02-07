#pragma once
#include <glm/vec2.hpp>
#include <vector>
#include <random>

enum class CameraShakeType : unsigned char
{
	Impact,
	Rumble
};

struct CameraShakeResult
{
	glm::vec2 offset = {};
	float rotation = 0.0f;
};

struct CameraShakeInstance
{
	CameraShakeType type = CameraShakeType::Impact;
	float intensity = 0.0f;
	float duration = 0.0f;
	float timeRemaining = 0.0f;
	float timeElapsed = 0.0f;
	float phaseX = 0.0f;
	float phaseY = 0.0f;
	float phaseRot = 0.0f;
	float freqX = 0.0f;
	float freqY = 0.0f;
	float freqRot = 0.0f;
};

// Keeps active gameplay shakes and outputs the strongest one each frame.
struct CameraShakeSystem
{
	std::vector<CameraShakeInstance> activeShakes;
	CameraShakeResult current = {};

	void clear();
	void addShake(CameraShakeType type, float intensity, float duration, std::ranlux24_base &rng);
	void update(float deltaTime);
	CameraShakeResult getCurrent() const { return current; }

private:
	float getStrength(const CameraShakeInstance &instance) const;
	CameraShakeResult sampleShake(const CameraShakeInstance &instance) const;
};
