#pragma once


#include "Physics.h"
#include "characterAnimator.h"
#include <gl2d/gl2d.h>
#include "assetsManager.h"
#include "statusEffects.h"


struct Player
{

	PhysicalEntity physics{ glm::vec2{12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE}, true };
	CharacterAnimator animator{ glm::vec2(48.f * PIXEL_SIZE,48.f * PIXEL_SIZE)};
	StatusEffects statusEffects;
	StatusImmunities statusImmunities;
	float statusSpeedMultiplier = 1.0f;
	float life = 20.0f;
	float maxLife = 20.0f;

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager);

	void update(float deltaTime);

};
