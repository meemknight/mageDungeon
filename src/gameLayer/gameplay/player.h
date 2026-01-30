#pragma once


#include "Physics.h"
#include "characterAnimator.h"
#include <gl2d/gl2d.h>
#include "assetsManager.h"
#include "statusEffects.h"
#include "wand.h"


struct Player
{

	PhysicalEntity physics{ glm::vec2{12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE}, true };
	CharacterAnimator animator{ glm::vec2(48.f * PIXEL_SIZE,48.f * PIXEL_SIZE)};
	StatusEffects statusEffects;
	StatusImmunities statusImmunities;
	float statusSpeedMultiplier = 1.0f;
	float life = 10.0f;
	float maxLife = 10.0f;
	// last aim direction from input (world space, normalized)
	glm::vec2 aimDirection = {1.0f, 0.0f};
	// 0..1, how strongly the player is aiming
	float aimStrength = 0.0f;
	// shake timer for failed casts
	float wandFailTimer = 0.0f;

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		const Wand &wand, glm::vec2 aimDirection);

	void update(float deltaTime);

};
