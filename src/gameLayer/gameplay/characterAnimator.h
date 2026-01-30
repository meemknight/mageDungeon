#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp> // for glm::length2


struct CharacterAnimator
{
	CharacterAnimator() {};
	CharacterAnimator(glm::ivec2 textureSize) : textureSize(textureSize) {};

	//the texture size in game!
	glm::ivec2 textureSize = {};

	int positionX = 0;
	int positionY = 0;
	bool flipX = 0;

	float timer = 0;
	
	void update(float deltaTime, float frameDuration, int framesCount)
	{
		timer -= deltaTime;

		if (timer <= 0)
		{
			timer += frameDuration;
			positionX++;
		}

		positionX %= framesCount;

	}

	void setAnimation(int animation)
	{
		if (positionY != animation)
		{
			positionX = 0;
			positionY = animation;
		}
	}


	void setAnimationBasedOnMovement(glm::vec2 movement)
	{
		// Treat tiny movement as zero to avoid jitter
		constexpr float kEps = 1e-6f;
		constexpr int moveFrames = 6;
	
		if (glm::length2(movement) <= kEps)
		{
			// No movement: keep / set idle animation like you already do
			if (positionY > 2)
			{
				if (positionX >= moveFrames - 1)
				{
					setAnimation(positionY - 3);
				}
			}
			return;
		}
	
		glm::vec2 dir = glm::normalize(movement);
	
		// Cardinal directions
		const glm::vec2 right( 1.f,  0.f);
		const glm::vec2 left (-1.f,  0.f);
		const glm::vec2 up   ( 0.f, -1.f); // matches your "movement.y < 0" => up
		const glm::vec2 down ( 0.f,  1.f); // matches your "movement.y > 0" => down
	
		float dRight = glm::dot(dir, right);
		float dLeft  = glm::dot(dir, left);
		float dUp    = glm::dot(dir, up);
		float dDown  = glm::dot(dir, down);
	
		// Find strongest alignment
		float best = dRight;
		int bestDir = 0; // 0=right,1=left,2=up,3=down
	
		if (dLeft > best) { best = dLeft; bestDir = 1; }
		if (dUp   > best) { best = dUp;   bestDir = 2; }
		if (dDown > best) { best = dDown; bestDir = 3; }
	
		switch (bestDir)
		{
			case 0: // right
				setAnimation(4);
				flipX = 0;
				break;
			case 1: // left
				setAnimation(4);
				flipX = 1;
				break;
			case 2: // up
				setAnimation(5);
				flipX = 0;
				break;
			case 3: // down
				setAnimation(3);
				flipX = 0;
				break;
		}
	}

};
