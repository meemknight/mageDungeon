#pragma once
#include <gl2d/gl2d.h>
#include <randomStuff.h>
#include <vector>
#include "particleBloom.h"


struct ParticleApearence
{
	glm::vec2 size = {};
	glm::vec4 color1 = {};
	glm::vec4 color2 = {};
};

struct ParticleSettings
{
	enum TRANZITION_TYPES
	{
		none = 0,
		linear,
		curbe,
		abruptCurbe,
		wave,
		wave2,
		delay,
		delay2
	};

	enum ANIMATION_TYPES
	{
		animationNone = 0,
		animationCircle,
		animationAtom,
		animationZigZag,
		animationSpiral,
		animationWave,
		animationFigure8,
		animationBob
	};

	short onCreateCount = 0;

	//random offset
	glm::vec2 positionX = {};
	glm::vec2 positionY = {};

	glm::vec2 particleLifeTime = {};
	glm::vec2 velocityX = {};
	glm::vec2 velocityY = {};
	glm::vec2 dragX = {};
	glm::vec2 dragY = {};

	glm::vec2 rotation = {};
	glm::vec2 rotationSpeed = {};
	glm::vec2 rotationDrag = {};

	// additional motion layered on top of physics (radians/sec, applied in render)
	unsigned char animationType = ANIMATION_TYPES::animationNone;
	glm::vec2 animationSpeed = {};
	glm::vec2 animationAcceleration = {};
	glm::vec2 animationScaleX = {};
	glm::vec2 animationScaleY = {};
	glm::vec2 animationRotation = {}; // degrees
	glm::vec2 animationPhase = {}; // initial time/phase

	ParticleApearence createApearence = {};
	ParticleApearence endApearence = {};

	gl2d::Texture texture = {};

	unsigned char tranzitionType = TRANZITION_TYPES::linear;

	bool folowParent = true;
};

struct ParticleEmissionSettings
{
	
	ParticleSettings create;

	ParticleSettings sustain;

	ParticleSettings release;

	float emitTimer = 0.01;

};


struct ParticleInstance
{

	glm::vec2 pos = {};
	glm::vec2 parentPos = {};

	glm::vec2 drag = {};
	glm::vec2 velocity = {};

	float durationTotal = 0;
	float durationRemaining = 0;
	float rotation = 0; //in degrees
	float rotationSpeed = 0;
	float rotationDrag = 0;

	unsigned char tranzitionType = ParticleSettings::TRANZITION_TYPES::linear;

	glm::vec4 colorStart = {};
	glm::vec4 colorEnd = {};

	float sizeStart = 0;
	float sizeEnd = 0;

	gl2d::Texture texture = {};

	bool followParent = true;

	// animation state (offset is applied during render)
	unsigned char animationType = ParticleSettings::ANIMATION_TYPES::animationNone;
	float animationTime = 0;
	float animationSpeed = 0;
	float animationAcceleration = 0;
	glm::vec2 animationScale = {};
	float animationRotation = 0;
	glm::vec2 animationOffset = {};
};

struct ParticlePostProcessRenderer
{
	void init();
	void cleanup();

	gl2d::FrameBuffer fbo;
	// Particle bloom is optional and only runs on the SDL_gpu backend.
	ParticleBloomPostProcess bloom;
	bool bloomEnabled = true;
	// Tint used when compositing the particle framebuffer to the screen.
	gl2d::Color4f compositeColor = {1, 1, 1, 2};

	void updateWindowMetrics(gl2d::Renderer2D &renderer);

	void finalRender(gl2d::Renderer2D &renderer);

};

ParticlePostProcessRenderer &getParticlePostProcessRenderer();

struct Map;

struct ParticleSystem
{

	int maxCount = 150;

	void update(float deltaTime);

	void render(gl2d::Renderer2D &renderer, 
		ParticlePostProcessRenderer &postProcessRenderer,
		glm::vec2 parentPos);

	std::vector<ParticleInstance> particles;

	void emitParticles(const ParticleSettings &particle, glm::vec2 pos, 
		std::ranlux24_base &rng, glm::vec2 parentPos);

	// removes particles that hit collidable tiles (parentPos needed for followParent)
	void killParticlesColliding(Map &map, glm::vec2 parentPos);

	void copyParticles(ParticleSystem &other, std::ranlux24_base &rng, glm::vec2 parentPos)
	{

		for (auto p : other.particles)
		{
			if (particles.size() >= maxCount) { break; }

			p.durationRemaining *= 0.6;
			if (p.durationRemaining > 4)
			{
				p.durationRemaining = 3.5;
				p.durationTotal = 4;
			}

			if (p.followParent)
			{
				p.pos += -p.parentPos + parentPos;
			}

			p.parentPos = {};

			particles.push_back(p);
		}
	}

};

