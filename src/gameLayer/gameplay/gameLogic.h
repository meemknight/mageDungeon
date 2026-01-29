#pragma once
#include <gameplay/map.h>
#include <gameplay/Physics.h>
#include <gameplay/player.h>
#include <gameplay/projectiles/projectiles.h>
#include "particleSystem.h"
#include <gameplay/entities/entity.h>
#include <gameplay/spells/spells.h>
#include <gameplay/spells/spellTypes.h>
#include <gameplay/damageViewerSystem.h>
#include <worldGen/floorGen.h>
#include <gameplay/wand.h>
#include <gameplay/droppedItems.h>
#include "spellSelectionInputLogic.h"

//this is an instance of the game.
//This shouldn't load things like textures, those should be load outside
struct GameLogic
{
	Map map;
	Player player;
	ProjectileHolder projectiles;
	EntityHolder entityHolder;
	SpellsHolder spellsHolder;
	SpellRecepie spellRecepie; //current spell recepie;
	SleppSelectionInputLogic spellSelectionInputLogic; //spell selection input and UI
	DamageViewerSystem damageViewerSystem;
	FloorInfo floorInfo;
	Wand currentWand;
	DroppedItemSystem droppedItems; // dropped items like wands

	ParticleSystem particleSystem;
	ParticlePostProcessRenderer particlePostProcessRenderer;

	glm::vec2 fireDirection = {1,0};

	//returns false on fail
	bool init();

	//returns false on fail
	bool update(float deltaTime, gl2d::Renderer2D &renderer,
		AssetsManager &assetsManager, platform::Input &input);

	void close();

	float zoom = 100;
	bool inGame = 0;

	std::ranlux24_base rng{std::random_device()()};

};
