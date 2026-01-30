#pragma once
#include <gl2d/gl2d.h>
#include <gameplay/blocks.h>
#include <vector>

struct TileSet
{
	gl2d::Texture texture;
	gl2d::TextureAtlasPadding atlas;
};

//this structu will load all the assets at the begining of the game.
//so idealy it shouldn't do much other than hold them
struct AssetsManager
{
	
	TileSet tileSets[TileSets::TILE_SETS_COUNT];
	gl2d::Font font;

	TileSet player;
	TileSet skeleton;
	TileSet waterSlime;
	TileSet fireSlime;
	TileSet iceSlime;

	TileSet elements;
	TileSet shadow;

	TileSet wands;


	gl2d::Texture upCircle;
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

	


	void loadAllAssets();
	gl2d::Texture &getWandIcon(int index);


};

AssetsManager &getAssetManager();
