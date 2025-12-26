#pragma once
#include <gameplay/map.h>
#include <gameplay/Physics.h>

//this is an instance of the game.
//This shouldn't load things like textures, those should be load outside
struct GameLogic
{


	Map map;
	PhysicalEntity player{glm::vec2{0.9, 0.9}};

	//returns false on fail
	bool init();

	//returns false on fail
	bool update(float deltaTime, gl2d::Renderer2D &renderer,
		AssetsManager &assetsManager);

	void close();

	float zoom = 60;
	bool inGame = 0;

};
