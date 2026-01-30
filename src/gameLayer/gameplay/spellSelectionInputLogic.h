#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <gameplay/wand.h>

struct AssetsManager;
struct Map;
struct Player;
struct ProjectileHolder;
struct SpellsHolder;
struct SpellRecepie;
struct EntityHolder;

namespace platform
{
	struct Input;
}

namespace gl2d
{
	struct Renderer2D;
}

// Handles the spell selector input and UI for a wand.
struct SleppSelectionInputLogic
{
	struct TrailPoint
	{
		glm::vec2 pos = {};
		float timer = 0.0f;
		glm::vec3 color = {0.5f, 0.5f, 0.5f};
	};

	struct CirclePiece
	{
		float animationTime = 0.0f;
	};

	bool executedFirstFrame = false;
	bool isDrawing = false;
	bool isClickSelection = false;
	glm::vec2 mouseStart = {};
	int dragDirection = 0;
	std::vector<TrailPoint> trail;

	CirclePiece upPiece;
	CirclePiece downPiece;
	CirclePiece leftPiece;
	CirclePiece rightPiece;

	// Trail color blending for smooth transitions between elements.
	glm::vec3 trailColor = {0.5f, 0.5f, 0.5f};
	glm::vec3 trailColorStart = {0.5f, 0.5f, 0.5f};
	glm::vec3 trailTargetColor = {0.5f, 0.5f, 0.5f};
	float trailColorTimer = 0.0f;

	// Per-cast mana and slot use tracking.
	float currentMana = 0.0f;
	bool manaInitialized = false;
	bool alwaysCastUsedThisCast = false;
	float castCooldownTimer = 0.0f;
	bool pauseManaCharge = false; // set true to skip mana charge this frame
	int remainingUp = 0;
	int remainingDown = 0;
	int remainingLeft = 0;
	int remainingRight = 0;
	int remainingAlwaysCast = 0;
	Wand lastWand = {};
	bool hasWandState = false;

	// resets loaded spells and selection state for a wand
	void resetSelectionForWand(const Wand &wand, SpellRecepie &spellRecepie, bool resetMana);

	void update(float deltaTime, gl2d::Renderer2D &renderer,
		AssetsManager &assetsManager,
		SpellRecepie &spellRecepie,
		SpellsHolder &spellsHolder,
		Map &map,
		ProjectileHolder &projectileHolder,
		EntityHolder &entityHolder,
		Player &player,
		glm::vec2 fireDirection,
		bool usesController,
		const Wand &wand,
		platform::Input &input);
};
