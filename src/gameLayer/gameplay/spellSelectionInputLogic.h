#pragma once
#include <glm/vec2.hpp>
#include <vector>

struct AssetsManager;
struct Player;
struct SpellsHolder;
struct SpellRecepie;
struct Wand;

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

	void update(float deltaTime, gl2d::Renderer2D &renderer,
		AssetsManager &assetsManager,
		SpellRecepie &spellRecepie,
		SpellsHolder &spellsHolder,
		Player &player,
		glm::vec2 fireDirection,
		bool usesController,
		const Wand &wand,
		platform::Input &input);
};
