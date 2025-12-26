#include "map.h"

void Map::create()
{
	*this = {};



	blocks.resize(20 * 20);
	size = {20,20};


	for (int x = 0; x < size.x; x++)
		for (int y = 0; y < size.y; y++)
		{
			blocks[x + y * size.x].type = Blocks::dirt;
		}


	for (int x = 10; x < size.x; x++)
		for (int y = 0; y < 10; y++)
		{
			blocks[x + y * size.x].type = Blocks::floor1;
		}


	getBlockUnsafe(2, 1).type = Blocks::grass;
	getBlockUnsafe(3, 2).type = Blocks::grass;
	getBlockUnsafe(1, 2).type = Blocks::grass;
	getBlockUnsafe(2, 3).type = Blocks::grass;


	//getBlockUnsafe(5, 3).type = Blocks::dungeonWall;
	//getBlockUnsafe(5, 5).type = Blocks::dungeonWall;
	//getBlockUnsafe(5, 6).type = Blocks::dungeonWall;
	//getBlockUnsafe(6, 6).type = Blocks::dungeonWall;
	//getBlockUnsafe(6, 7).type = Blocks::dungeonWall;


	getBlockUnsafe(5, 5).type = Blocks::grass;
	//getBlockUnsafe(6, 5).type = Blocks::grass;
	getBlockUnsafe(7, 5).type = Blocks::grass;
	getBlockUnsafe(8, 5).type = Blocks::grass;
	getBlockUnsafe(5, 3).type = Blocks::grass;
	//getBlockUnsafe(6, 3).type = Blocks::grass;
	getBlockUnsafe(7, 3).type = Blocks::grass;
	getBlockUnsafe(8, 3).type = Blocks::grass;

	getBlockUnsafe(3, 6).type = Blocks::grass;
	getBlockUnsafe(2, 8).type = Blocks::grass;


	getBlockUnsafe(10, 10).type = Blocks::grass;
	getBlockUnsafe(11, 9).type = Blocks::grass;
	getBlockUnsafe(12, 10).type = Blocks::grass;
	getBlockUnsafe(10, 11).type = Blocks::grass;
	getBlockUnsafe(12, 11).type = Blocks::grass;
	getBlockUnsafe(9, 11).type = Blocks::grass;
	getBlockUnsafe(8, 12).type = Blocks::grass;
	getBlockUnsafe(9, 13).type = Blocks::grass;
	getBlockUnsafe(10, 13).type = Blocks::grass;
	getBlockUnsafe(11, 14).type = Blocks::grass;
	getBlockUnsafe(12, 13).type = Blocks::grass;
	getBlockUnsafe(13, 12).type = Blocks::grass;




}

Block &Map::getBlockUnsafe(int x, int y)
{
	return blocks[x + y * size.x];
}

Block *Map::getBlockSafe(int x, int y)
{
	if (x < 0 || y < 0 || x >= size.x || y >= size.y)
	{
		return nullptr;
	}

	return &blocks[x + y * size.x];
}


void Map::renderMap(gl2d::Renderer2D &renderer,
	AssetsManager &assetManager)
{


	for (int y = 0; y < size.y; y++)
	{
		for (int x = 0; x < size.x; x++)
		{

			Block b = getBlockUnsafe(x, y);

			if (b.type)
			{

				int tileSet = getTileSetIndex(b.type);
				auto uv = getBlockAtlasPos(b.type);

				auto tile = assetManager.tileSets[tileSet];

				if (tile.texture.isValid())
				{
					renderer.renderRectangle({x,y,1,1},
						 tile.texture,
						 Colors_White,
						{},
						0,
						 tile.atlas.get(uv.x, uv.y)
						);
				}

			};

		}


	}

	//render grass decals
	auto decals = assetManager.tileSets[TileSets::grassDecals];

	for (int y = 0; y < size.y; y++)
	{
		for (int x = 0; x < size.x; x++)
		{

			Block b = getBlockUnsafe(x, y);

			if (canHaveGrassDecals(b.type))
			{

				bool top = 0; 
				bool bottom = 0;
				bool left = 0;
				bool right = 0;

				bool topLeft = 0;
				bool topRight = 0;
				bool bottomLeft = 0;
				bool bottomRight = 0;

				auto b = getBlockSafe(x, y - 1); top = b ? isGrass(b->type) : 0;
					b = getBlockSafe(x, y + 1); bottom = b ? isGrass(b->type) : 0;
					b = getBlockSafe(x - 1, y); left = b ? isGrass(b->type) : 0;
					b = getBlockSafe(x + 1, y); right = b ? isGrass(b->type) : 0;
					b = getBlockSafe(x + 1, y + 1); bottomRight = b ? isGrass(b->type) : 0;
					b = getBlockSafe(x - 1, y + 1); bottomLeft = b ? isGrass(b->type) : 0;
					b = getBlockSafe(x + 1, y - 1); topRight = b ? isGrass(b->type) : 0;
					b = getBlockSafe(x - 1, y - 1); topLeft = b ? isGrass(b->type) : 0;


				if (top && bottom && left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						decals.texture,
						Colors_White,
						{},
						0,
						decals.atlas.get(0, 3)
					);
				}
				else if (top && left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						decals.texture,
						Colors_White,
						{},
						0,
						decals.atlas.get(0, 0)
					);
				}
				else if (left && top && bottom)
				{
					renderer.renderRectangle({x,y,1,1},
						decals.texture,
						Colors_White,
						{},
						0,
						decals.atlas.get(1, 3)
					);
				}
				else if (right && top && bottom)
				{
					renderer.renderRectangle({x,y,1,1},
						decals.texture,
						Colors_White,
						{},
						0,
						decals.atlas.get(3, 3)
					);
				}
				else if (bottom && left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						decals.texture,
						Colors_White,
						{},
						0,
						decals.atlas.get(0, 2)
					);
				}
				else if (top && bottom)
				{
					renderer.renderRectangle({x,y,1,1},
						decals.texture,
						Colors_White,
						{},
						0,
						decals.atlas.get(2, 3)
					);
				}
				else if (left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						decals.texture,
						Colors_White,
						{},
						0,
						decals.atlas.get(0, 1)
					);
				}

				else			
				{
					//can have corner pieces
					if (left && top)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(1, 0)
						);
					}
					else if (right && top)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(3, 0)
						);
					}
					else if (left && bottom)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(1, 2)
						);
					}
					else if (right && bottom)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(3, 2)
						);
					}

					else if (left)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(1, 1)
						);
					}
					else if (right)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(3, 1)
						);
					}
					else if (top)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(2, 0)
						);
					}
					else if (bottom)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(2, 2)
						);
					}


					if (!bottom && !left && bottomLeft)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(5, 0)
						);
					}

					if (!bottom && !right && bottomRight)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(4, 0)
						);
					}

					if (!top && !left && topLeft)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(5, 1)
						);
					}

					if (!top && !right && topRight)
					{
						renderer.renderRectangle({x,y,1,1},
							decals.texture,
							Colors_White,
							{},
							0,
							decals.atlas.get(4, 1)
						);
					}


				}

			};

		}


	}





}
