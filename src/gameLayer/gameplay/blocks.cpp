#include "blocks.h"


BlockSettings blockSettings[]

{

	BlockSettings{}, //none
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({2,3}), //floor
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({2,4}), //floor
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({1,1}).setCollidable(), //dungeonWall
	
	BlockSettings{}.setTileSet(TileSets::grass).setAtlasPos({0,0}).setIsGrass(), //grass
	BlockSettings{}.setTileSet(TileSets::dirt).setAtlasPos({0,0}).setCanHaveGrassDecals(), //dirt



};


static_assert(sizeof(blockSettings) / sizeof(blockSettings[0]) ==
	Blocks::BLOCKS_COUNT);


glm::ivec2 getBlockAtlasPos(BlockType block)
{
	permaAssert(block < Blocks::BLOCKS_COUNT);
	permaAssert(block >= 0);

	return blockSettings[block].atlasPos;
}

int getTileSetIndex(BlockType block)
{
	permaAssert(block < Blocks::BLOCKS_COUNT);
	permaAssert(block >= 0);

	return blockSettings[block].tileSet;
}

int isBlockColidable(BlockType block)
{
	permaAssert(block < Blocks::BLOCKS_COUNT);
	permaAssert(block >= 0);

	return blockSettings[block].collidable;
}

//TODO is wall
bool canHaveGrassDecals(BlockType block)
{
	permaAssert(block < Blocks::BLOCKS_COUNT);
	permaAssert(block >= 0);

	return (!blockSettings[block].collidable &&
		!blockSettings[block].isGrass
		);
}

bool isGrass(BlockType block)
{
	permaAssert(block < Blocks::BLOCKS_COUNT);
	permaAssert(block >= 0);

	return blockSettings[block].isGrass;
}