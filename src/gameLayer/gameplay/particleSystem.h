#pragma once
#include <gl2d/gl2d.h>
#include <randomStuff.h>
#include <vector>


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

	ParticleApearence createApearence = {};
	ParticleApearence endApearence = {};

	gl2d::Texture texture = {};

	unsigned char tranzitionType = TRANZITION_TYPES::linear;


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

};


struct ParticleSystem
{

	int maxCount = 50;

	void update(float deltaTime);

	void render(gl2d::Renderer2D &renderer, glm::vec2 parentPos);

	std::vector<ParticleInstance> particles;

	void emitParticles(const ParticleSettings &particle, glm::vec2 pos, 
		std::ranlux24_base &rng, glm::vec2 parentPos);


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

			p.pos += -p.parentPos + parentPos;

			p.parentPos = {};

			particles.push_back(p);
		}
	}

};