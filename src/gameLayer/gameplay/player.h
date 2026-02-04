#pragma once


#include "Physics.h"
#include "characterAnimator.h"
#include <gl2d/gl2d.h>
#include "assetsManager.h"
#include "statusEffects.h"
#include "wand.h"
#include <algorithm>


struct Player
{

	PhysicalEntity physics{ glm::vec2{12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE}, true };
	CharacterAnimator animator{ glm::vec2(48.f * PIXEL_SIZE,48.f * PIXEL_SIZE)};
	StatusEffects statusEffects;
	StatusImmunities statusImmunities;
	float statusSpeedMultiplier = 1.0f;
	float life = 10.f;
	float maxLife = 10.0f;
	// Spell healing is a small reserve that takes damage before life.
	float spellHealing = 0.0f;
	float maxSpellHealing = 3.0f;
	// Shield is temporary health that takes damage before spell healing.
	float shield = 0.0f;
	// last aim direction from input (world space, normalized)
	glm::vec2 aimDirection = {1.0f, 0.0f};
	// 0..1, how strongly the player is aiming
	float aimStrength = 0.0f;
	// shake timer for failed casts
	float wandFailTimer = 0.0f;

	// Resets life and clears temporary healing/shield.
	void resetHealth()
	{
		life = maxLife;
		spellHealing = 0.0f;
		shield = 0.0f;
	}

	// Adds spell healing up to its cap and missing life.
	void addSpellHealing(float amount)
	{
		if (amount <= 0.0f) { return; }
		float missingLife = maxLife - life;
		if (missingLife <= 0.0f) { return; }
		float cap = std::min(maxSpellHealing, missingLife);
		spellHealing = std::min(cap, spellHealing + amount);
	}

	// Applies regular healing and pushes spell healing out if needed.
	void healLife(float amount)
	{
		if (amount <= 0.0f) { return; }
		life = std::min(maxLife, life + amount);
		float overHeal = (life + spellHealing) - maxLife;
		if (overHeal > 0.0f)
		{
			spellHealing = std::max(0.0f, spellHealing - overHeal);
		}
	}

	// Shield replaces the current value if higher.
	void addShield(float amount)
	{
		if (amount <= 0.0f) { return; }
		shield = std::max(shield, amount);
	}

	// Damage order: shield -> spell healing -> life.
	void applyDamage(float damage)
	{
		if (damage <= 0.0f) { return; }
		float remaining = damage;
		if (shield > 0.0f)
		{
			float absorbed = std::min(shield, remaining);
			shield -= absorbed;
			remaining -= absorbed;
		}
		if (remaining > 0.0f && spellHealing > 0.0f)
		{
			float absorbed = std::min(spellHealing, remaining);
			spellHealing -= absorbed;
			remaining -= absorbed;
		}
		if (remaining > 0.0f)
		{
			life -= remaining;
		}
	}

	void render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
		const Wand &wand, glm::vec2 aimDirection);

	void update(float deltaTime);

};

// Access the active player (preview or game).
Player &getPlayer();
