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
		grassDecorationStones,
		grassDecorationFlowers,
		grassDecorationMushrooms,

		dirtDecoration,
		fence,
		hill,
		smallTree,
		cobbleStoneWall,

		floorBigTileTopLeft,
		floorBigTileTopRight,
		floorBigTileBottomLeft,
		floorBigTileBottomRight,

		floorPatern1,

		caveFloor,

		woodenFloor,
		woodenWall,
		carpetFloor,

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
		caveFloor,
		grassDecor,
		woodenFloor,
		woodenWall3D,
		carpetDecals,

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
	bool isCarpet = 0;
	bool isWall = 0;
	bool isSmallWall = 0;
	bool chunkyTile = 0;
	int wall3DTileSet = 0; 

	//if this is not 0, than the block can have up to
	// this extra offsets selected randomly
	glm::ivec2 randomAtlasOffsets = {};


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

	BlockSettings &setRandomAtlasOffsets(glm::ivec2 pos)
	{
		this->randomAtlasOffsets = pos;
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

	BlockSettings &setIsCarpet()
	{
		this->isCarpet = true;
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
glm::ivec2 getRandomAtlasOffsets(BlockType block);
int getTileSetIndex(BlockType block);
int getWall3DTileSetIndex(BlockType block);
int isBlockColidable(BlockType block);

bool canHaveGrassDecals(BlockType block);
bool canHaveCarpetDecals(BlockType block);
bool isGrass(BlockType block);
bool isCarpet(BlockType block);
bool isWall(BlockType block);
bool isChunkyTile(BlockType block);
