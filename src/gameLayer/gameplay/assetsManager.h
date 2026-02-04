#pragma once
#include <gl2d/gl2d.h>
#include <gameplay/blocks.h>
#include <vector>
#include <string>
#include <unordered_map>

struct TileSet
{
	gl2d::Texture texture;
	gl2d::TextureAtlasPadding atlas;
};

//button prompt textures grouped by device type
struct ButtonSprites
{
	std::unordered_map<std::string, gl2d::Texture> controller;
	std::unordered_map<std::string, gl2d::Texture> keyboard;
	std::unordered_map<std::string, gl2d::Texture> mouse;

	const gl2d::Texture *getController(const std::string &name) const
	{
		auto it = controller.find(name);
		if (it == controller.end()) { return nullptr; }
		return &it->second;
	}

	const gl2d::Texture *getKeyboard(const std::string &name) const
	{
		auto it = keyboard.find(name);
		if (it == keyboard.end()) { return nullptr; }
		return &it->second;
	}

	const gl2d::Texture *getMouse(const std::string &name) const
	{
		auto it = mouse.find(name);
		if (it == mouse.end()) { return nullptr; }
		return &it->second;
	}
};

//this structu will load all the assets at the begining of the game.
//so idealy it shouldn't do much other than hold them
struct AssetsManager
{
	
	TileSet tileSets[TileSets::TILE_SETS_COUNT];
	gl2d::Font font;

	TileSet player;
	TileSet skeleton;
	TileSet templarOriginal;
	TileSet earthTemplar;
	TileSet fireTemplar;
	TileSet iceTemplar;
	TileSet waterTemplar;
	TileSet goblinArcher;
	TileSet goblinSpearman;
	TileSet goblinHeavy;
	TileSet goblinThief;
	TileSet orcArcher;
	TileSet darkAngel;
	TileSet waterSlime;
	TileSet fireSlime;
	TileSet iceSlime;

	TileSet elements;
	TileSet shadow;

	TileSet wands;
	TileSet woodenChest;
	TileSet hearth;
	TileSet coin;
	TileSet carpetDecals;
	ButtonSprites buttonSprites;


	gl2d::Texture upCircle;
	gl2d::Texture particleCross;
	gl2d::Texture downCircle;
	gl2d::Texture leftCircle;
	gl2d::Texture rightCircle;
	gl2d::Texture book;
	std::vector<gl2d::Texture> wandIcons; // per-sprite textures for UI
	gl2d::Texture wandFallback;

	gl2d::Texture particleCircle;
	gl2d::Texture particleSmoke;
	gl2d::Texture target;
	gl2d::Texture thorn;
	gl2d::Texture doorClosedHorizontal;
	gl2d::Texture doorOpenedHorizontal;

	


	void loadAllAssets();
	gl2d::Texture &getWandIcon(int index);


};

AssetsManager &getAssetManager();
