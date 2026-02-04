#include "blocks.h"


BlockSettings blockSettings[]

{

	BlockSettings{}, //none
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({2,3}), //floor
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({2,4}), //floor
	BlockSettings{}.setTileSet(TileSets::dungeonWall).setAtlasPos({0,0}).setCollidable().setIsWall().setWall3DTileSet(TileSets::dungeonWall3D), //dungeonWall

	BlockSettings{}.setTileSet(TileSets::grass).setAtlasPos({0,0}).setIsGrass(), //grass
	BlockSettings{}.setTileSet(TileSets::dirt).setAtlasPos({0,0}).setCanHaveGrassDecals(), //dirt

	BlockSettings{}.setTileSet(TileSets::grassDecor).setAtlasPos({0,0}).setIsGrass().setRandomAtlasOffsets({3, 0}), //grass decoration
	BlockSettings{}.setTileSet(TileSets::grassDecor).setAtlasPos({0,1}).setIsGrass().setRandomAtlasOffsets({3, 0}), //grass decoration
	BlockSettings{}.setTileSet(TileSets::grassDecor).setAtlasPos({0,2}).setIsGrass().setRandomAtlasOffsets({3, 0}), //grass decoration
	BlockSettings{}.setTileSet(TileSets::grassDecor).setAtlasPos({0,3}).setIsGrass().setRandomAtlasOffsets({3, 0}), //grass decoration

	BlockSettings{}.setTileSet(TileSets::dirt).setAtlasPos({1,0}).setCanHaveGrassDecals(), //dirt decoration

	BlockSettings{}.setTileSet(TileSets::fence).setAtlasPos({0,3}).setCollidable().setIsSmallWall() , //fence
	BlockSettings{}.setTileSet(TileSets::hill).setAtlasPos({0,3}).setCollidable().setIsWall().setWall3DTileSet(TileSets::hillWall3D), //hill

	BlockSettings{}.setTileSet(TileSets::smallTree).setAtlasPos({0,3}).setCollidable().setChunkyTile(), //smallTree
	BlockSettings{}.setTileSet(TileSets::stoneWall3D).setAtlasPos({5,3}).setCollidable().setIsWall().setWall3DTileSet(TileSets::stoneWall3D), //cobble stone

	//big floor tile

	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({3,3}),
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({4,3}),
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({3,4}),
	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({4,4}),

	BlockSettings{}.setTileSet(TileSets::dungeonTileSet).setAtlasPos({1,3}).setRandomAtlasOffsets({0, 3}),
	BlockSettings{}.setTileSet(TileSets::caveFloor).setAtlasPos({0,0}).setRandomAtlasOffsets({8, 0}), //cave floor

	BlockSettings{}.setTileSet(TileSets::woodenFloor).setAtlasPos({0,0}).setRandomAtlasOffsets({4, 0}), //wooden floor
	BlockSettings{}.setTileSet(TileSets::woodenWall3D).setAtlasPos({4,2}).setCollidable().setIsWall().setWall3DTileSet(TileSets::woodenWall3D).setRandomAtlasOffsets({1, 1}), //wooden wall
	BlockSettings{}.setTileSet(TileSets::carpetDecals).setAtlasPos({4,2}).setIsCarpet().setRandomAtlasOffsets({1, 1}), //carpet
	BlockSettings{}.setTileSet(TileSets::none).setCollidable(), //door collision (invisible)

	BlockSettings{}.setTileSet(TileSets::none).setCollidableOnlyForProjectiles().setAtlasPos({0,0}).setBreakableDecoration().setRandomAtlasOffsets({3,0}), //breakable wood decoration marker (rendered manually)
	BlockSettings{}.setTileSet(TileSets::exit).setAtlasPos({0,0}), //exit
	BlockSettings{}.setTileSet(TileSets::wallDecorations).setAtlasPos({0,0}).setRandomAtlasOffsets({15,0}), //wall decorations

	
};


static_assert(sizeof(blockSettings) / sizeof(blockSettings[0]) ==
	Blocks::BLOCKS_COUNT);


glm::ivec2 getBlockAtlasPos(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].atlasPos;
}

glm::ivec2 getRandomAtlasOffsets(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].randomAtlasOffsets;
}

int getTileSetIndex(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].tileSet;
}

int getWall3DTileSetIndex(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].wall3DTileSet;
}

int isBlockColidable(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].collidable;
}

//TODO is wall
bool canHaveGrassDecals(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return (!blockSettings[block].collidable &&
		!blockSettings[block].isGrass
		);
}

bool canHaveCarpetDecals(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return (!blockSettings[block].collidable &&
		!blockSettings[block].isWall &&
		!blockSettings[block].isCarpet
		);
}

bool isGrass(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].isGrass;
}

bool isCarpet(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].isCarpet;
}

bool isWall(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].isWall;
}

bool isChunkyTile(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].chunkyTile;
}

bool isBreakableDecoration(BlockType block)
{
	permaAssertDevelopement(block < Blocks::BLOCKS_COUNT);
	permaAssertDevelopement(block >= 0);

	return blockSettings[block].isBreakableDecorations;
}
