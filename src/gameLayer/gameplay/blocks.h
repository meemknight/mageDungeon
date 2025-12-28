#pragma once
#include <platformTools.h>
#include <gl2d/gl2d.h>


namespace Blocks
{
	enum
	{
		none = 0,
		floor1,
		floor2,
		dungeonWall,
		grass,
		dirt,
		grassDecoration,
		dirtDecoration,
		fence,
		hill,
		smallTree,
		cobbleStoneWall,

		BLOCKS_COUNT

	};
}


using BlockType = unsigned short;

struct Block
{
	BlockType type = 0;


};


namespace TileSets
{
	enum TileSets
	{
		none = 0,
		dungeonTileSet,
		grass,
		dirt,
		grassDecals,
		dungeonWall,
		dungeonWall3D,
		fence,
		hill,
		hillWall3D,
		smallTree,
		stoneWall3D,
		TILE_SETS_COUNT
	};
}


struct BlockSettings
{


	bool collidable = 0;
	glm::ivec2 atlasPos = {};
	int tileSet = 0;
	bool canHaveGrassDecals = 0; //todo remove
	bool isGrass = 0;
	bool isWall = 0;
	bool isSmallWall = 0;
	bool chunkyTile = 0;
	int wall3DTileSet = 0; 


	BlockSettings &setCollidable()
	{
		collidable = true;
		return *this;
	}

	BlockSettings &setTileSet(int tileSet)
	{
		this->tileSet = tileSet;
		return *this;
	}

	BlockSettings &setWall3DTileSet(int wall3DTileSet)
	{
		this->wall3DTileSet = wall3DTileSet;
		return *this;
	}

	BlockSettings &setAtlasPos(glm::ivec2 pos)
	{
		this->atlasPos = pos;
		return *this;
	}

	BlockSettings &setCanHaveGrassDecals()
	{
		this->canHaveGrassDecals = true;
		return *this;
	}

	BlockSettings &setIsGrass()
	{
		this->isGrass = true;
		return *this;
	}

	BlockSettings &setIsWall()
	{
		this->isWall = true;
		return *this;
	}

	BlockSettings &setIsSmallWall()
	{
		this->isSmallWall = true;
		return *this;
	}

	BlockSettings &setChunkyTile()
	{
		chunkyTile = true;
		return *this;
	}

};



glm::ivec2 getBlockAtlasPos(BlockType block);
int getTileSetIndex(BlockType block);
int getWall3DTileSetIndex(BlockType block);
int isBlockColidable(BlockType block);

bool canHaveGrassDecals(BlockType block);
bool isGrass(BlockType block);
bool isWall(BlockType block);
bool isChunkyTile(BlockType block);
