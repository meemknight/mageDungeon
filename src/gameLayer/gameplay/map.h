#pragma once
#include <gameplay/blocks.h>
#include <vector>
#include <gameplay/assetsManager.h>


struct MapLayer
{
	void create(int sizeX, int sizeY)
	{
		*this = {};
		blocks.resize(sizeX * sizeY);
		size = {sizeX, sizeY};
	}

	std::vector<Block> blocks;
	glm::ivec2 size = {};

	Block &getBlockUnsafe(int x, int y);

	Block *getBlockSafe(int x, int y);

	void renderMap(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager);

	//for rendering tall walls and stuff
	void renderMapAfterEntities(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager);
};

struct Map
{

	void create(int sizeX, int sizeY);


	MapLayer firstLayer;
	MapLayer secondLayer;
	glm::ivec2 size = {};

	
	bool isCollidableAtPosSafe(int x, int y)
	{
		auto b = firstLayer.getBlockSafe(x, y);
		if (!b) { return false; }

		if (isBlockColidable(b->type)) { return true; }
		
		b = secondLayer.getBlockSafe(x, y);
		if (isBlockColidable(b->type)) { return true; }

		return false;
	}

	bool isCollidableAtPosUnsafe(int x, int y)
	{
		auto b = firstLayer.getBlockUnsafe(x, y);
		if (isBlockColidable(b.type)) { return true; }

		b = secondLayer.getBlockUnsafe(x, y);
		if (isBlockColidable(b.type)) { return true; }

		return false;
	}

	void renderMap(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager);

	//for rendering tall walls and stuff
	void renderMapAfterEntities(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager);

};