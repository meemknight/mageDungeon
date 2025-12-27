#pragma once


#include "Physics.h"
#include "characterAnimator.h"
#include <gl2d/gl2d.h>
#include "assetsManager.h"


struct Player
{

	PhysicalEntity physical{ glm::vec2{12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE} };
	CharacterAnimator animator{ glm::vec2(48.f * PIXEL_SIZE,48.f * PIXEL_SIZE)};

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager);

	void update(float deltaTime);

};
