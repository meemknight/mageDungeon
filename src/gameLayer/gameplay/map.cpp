#include "map.h"
#include <imgui.h>

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



	getBlockUnsafe(5, 3).type = Blocks::dungeonWall;
	getBlockUnsafe(5, 7).type = Blocks::dungeonWall;
	getBlockUnsafe(4, 7).type = Blocks::dungeonWall;
	getBlockUnsafe(6, 7).type = Blocks::dungeonWall;

	getBlockUnsafe(8, 5).type = Blocks::dungeonWall;
	getBlockUnsafe(8, 6).type = Blocks::dungeonWall;
	getBlockUnsafe(8, 7).type = Blocks::dungeonWall;


	getBlockUnsafe(12, 5).type = Blocks::dungeonWall;
	getBlockUnsafe(12, 6).type = Blocks::dungeonWall;
	getBlockUnsafe(12, 7).type = Blocks::dungeonWall;
	getBlockUnsafe(11, 6).type = Blocks::dungeonWall;
	getBlockUnsafe(13, 6).type = Blocks::dungeonWall;
	getBlockUnsafe(14, 6).type = Blocks::dungeonWall;
	getBlockUnsafe(14, 5).type = Blocks::dungeonWall;


	getBlockUnsafe(3, 11).type = Blocks::dungeonWall;
	getBlockUnsafe(4, 11).type = Blocks::dungeonWall;
	getBlockUnsafe(6, 11).type = Blocks::dungeonWall;
	getBlockUnsafe(7, 11).type = Blocks::dungeonWall;
	getBlockUnsafe(3, 12).type = Blocks::dungeonWall;
	getBlockUnsafe(7, 12).type = Blocks::dungeonWall;
	getBlockUnsafe(3, 15).type = Blocks::dungeonWall;
	getBlockUnsafe(7, 15).type = Blocks::dungeonWall;
	getBlockUnsafe(3, 16).type = Blocks::dungeonWall;
	getBlockUnsafe(4, 16).type = Blocks::dungeonWall;
	getBlockUnsafe(6, 16).type = Blocks::dungeonWall;
	getBlockUnsafe(7, 16).type = Blocks::dungeonWall;


	getBlockUnsafe(16, 16).type = Blocks::dungeonWall;
	getBlockUnsafe(17, 16).type = Blocks::dungeonWall;
	getBlockUnsafe(16, 17).type = Blocks::dungeonWall;
	getBlockUnsafe(17, 17).type = Blocks::dungeonWall;



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

void Map::renderMapAfterEntities(gl2d::Renderer2D &renderer, 
	AssetsManager &assetManager)
{


	auto dungeonWall = assetManager.tileSets[TileSets::dungeonWall3D];


	static float opacity = 1;

	ImGui::Begin("Game Debug");
	ImGui::SliderFloat("Opacity", &opacity, 0, 1);
	ImGui::End();

	glm::vec4 color = {1,1,1,opacity};

	for (int y = 0; y < size.y; y++)
	{
		for (int x = 0; x < size.x; x++)
		{

			auto current = getBlockUnsafe(x, y);
			if (!isWall(current.type))
			{
				continue;
			}

			bool top = 0;
			bool top2 = 0;
			bool bottom = 0;
			bool left = 0;
			bool right = 0;

			bool topLeft = 0;
			bool topRight = 0;
			bool bottomLeft = 0;
			bool bottomRight = 0;

			auto b = getBlockSafe(x, y - 1); top = b ? isWall(b->type) : 0;
			b = getBlockSafe(x, y - 2); top2 = b ? isWall(b->type) : 0;
			b = getBlockSafe(x, y + 1); bottom = b ? isWall(b->type) : 0;
			b = getBlockSafe(x - 1, y); left = b ? isWall(b->type) : 0;
			b = getBlockSafe(x + 1, y); right = b ? isWall(b->type) : 0;
			b = getBlockSafe(x + 1, y + 1); bottomRight = b ? isWall(b->type) : 0;
			b = getBlockSafe(x - 1, y + 1); bottomLeft = b ? isWall(b->type) : 0;
			b = getBlockSafe(x + 1, y - 1); topRight = b ? isWall(b->type) : 0;
			b = getBlockSafe(x - 1, y - 1); topLeft = b ? isWall(b->type) : 0;

			if (left && right && top && bottom)
			{
				//fully inclosed
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(2, 1)
				);
			
			}else

			if (left && top)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(3, 2)
				);

			}else if(right &&top)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(1, 2)
				);

			}
			else if(right && bottom)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(1, 0)
				);

			}
			else if (left && bottom)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(3, 0)
				);

			}
			else


			if (left && right)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(2, 3)
				);

			}
			else if (left)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(3, 3)
				);
			}
			else if (right)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(1, 3)
				);
			}else
			if (bottom && top)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(0, 1)
				);

			}
			else
			if (bottom)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(0, 0)
				);

			}else
			if (top)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(0, 2)
				);

			}
			else
			if (!bottom && !left && !right && !top)
			{
				//fully open
				renderer.renderRectangle({x,y-1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(0, 3)
				);
			}


			//corners
			if (bottom && left && !bottomLeft)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(5, 0)
				);
			}
			if (bottom && right && !bottomRight)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(4, 0)
				);
			}
			if (top && left && !topLeft)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(5, 1)
				);
			}
			if (top && right && !topRight)
			{
				renderer.renderRectangle({x,y - 1,1,1},
					dungeonWall.texture,
					color,
					{},
					0,
					dungeonWall.atlas.get(4, 1)
				);
			}


		}

	}


}
