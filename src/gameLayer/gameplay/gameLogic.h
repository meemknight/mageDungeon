#pragma once
#include <gameplay/map.h>
#include <gameplay/Physics.h>
#include <gameplay/player.h>
#include <gameplay/projectiles/projectiles.h>
#include "particleSystem.h"
#include <gameplay/entities/entity.h>
#include <gameplay/spells/spells.h>
#include <gameplay/spells/spellTypes.h>
#include <vector>
#include <gameplay/damageViewerSystem.h>
#include <worldGen/floorGen.h>
#include <gameplay/wand.h>

//this is an instance of the game.
//This shouldn't load things like textures, those should be load outside
struct GameLogic
{
	struct SpellSelectionState
	{
		struct TrailPoint
		{
			glm::vec2 pos = {};
			float timer = 0.0f;
		};

		bool executedFirstFrame = false;
		bool isDrawing = false;
		bool isClickSelection = false;
		glm::vec2 mouseStart = {};
		int dragDirection = 0;
		std::vector<TrailPoint> trail;
	};


	Map map;
	Player player;
	ProjectileHolder projectiles;
	EntityHolder entityHolder;
	SpellsHolder spellsHolder;
	SpellRecepie spellRecepie; //current spell recepie;
	SpellSelectionState spellSelectionState;
	DamageViewerSystem damageViewerSystem;
	FloorInfo floorInfo;
	Wand currentWand;

	ParticleSystem particleSystem;
	ParticlePostProcessRenderer particlePostProcessRenderer;

	glm::vec2 fireDirection = {1,0};

	//returns false on fail
	bool init();

	//returns false on fail
	bool update(float deltaTime, gl2d::Renderer2D &renderer,
		AssetsManager &assetsManager);

	void close();

	float zoom = 100;
	bool inGame = 0;

	std::ranlux24_base rng{std::random_device()()};

};
