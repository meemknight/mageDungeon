#pragma once

#include <gameplay/map.h>
#include <gameplay/player.h>
#include <gameplay/projectiles/projectiles.h>
#include <gameplay/summons.h>
#include <gameplay/damageViewerSystem.h>
#include <gameplay/spells/spells.h>
#include <gameplay/particleSystem.h>

// Holds a miniature simulation for spell previews.
struct SpellPreviewContext
{
	SpellPreviewContext() = default;
	SpellPreviewContext(const SpellPreviewContext &) = delete;
	SpellPreviewContext &operator=(const SpellPreviewContext &) = delete;
	SpellPreviewContext(SpellPreviewContext &&) noexcept = default;
	SpellPreviewContext &operator=(SpellPreviewContext &&) noexcept = default;

	Map map;
	Player player;
	EntityHolder entities;
	SummonHolder summons;
	ProjectileHolder projectiles;
	StandbyProjectileSystem standbyProjectiles;
	ParticleSystem particleSystem;
	ParticlePostProcessRenderer particleRenderer;
	DamageViewerSystem damageViewer;
	SpellsHolder spells;
	Wand previewWand;

	glm::vec2 aimDirection = {1.0f, 0.0f};
	float castTimer = 0.0f;
	float standbyFireTimer = 0.0f;
	int spellType = -1;
	bool initialized = false;
};
