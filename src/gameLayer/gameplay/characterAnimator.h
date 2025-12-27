#pragma once

#include <glm/glm.hpp>



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

		if (movement.x > 0)
		{
			setAnimation(4);
			flipX = 0;
		}
		else if (movement.x < 0)
		{
			setAnimation(4);
			flipX = 1;
		}
		else if (movement.y < 0)
		{
			setAnimation(5);
			flipX = 0;
		}
		else if(movement.y > 0)
		{
			setAnimation(3);
			flipX = 0;
		}
		else
		{
			if (positionY > 2)
			{
				setAnimation(positionY - 3);
			}
		}

	}

};