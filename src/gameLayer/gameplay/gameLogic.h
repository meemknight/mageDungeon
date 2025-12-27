#pragma once
#include <gameplay/map.h>
#include <gameplay/Physics.h>
#include <gameplay/player.h>
#include <gameplay/projectiles.h>
#include "particleSystem.h"

//this is an instance of the game.
//This shouldn't load things like textures, those should be load outside
struct GameLogic
{


	Map map;
	Player player;
	ProjectileHolder projectiles;

	ParticleSystem particleSystem;

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
