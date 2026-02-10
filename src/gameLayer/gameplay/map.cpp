#include "map.h"
#include <imgui.h>
#include <imguiTools.h>
#include <gameplay/doors.h>
#include <gameplay/Physics.h>
#include <gameplay/worldTextSystem.h>

static constexpr float DOOR_VERTICAL_Y_OFFSET = PIXEL_SIZE * -1.0f;
static constexpr float DOOR_VERTICAL_HEIGHT = 4.0f;

// Hash-based offsets for per-tile atlas variation.
static unsigned int hashPosition(int x, int y)
{
	unsigned int h = 2166136261u;
	h = (h ^ (unsigned int)x) * 16777619u;
	h = (h ^ (unsigned int)y) * 16777619u;
	return h;
}

static int pickAtlasOffset(unsigned int h, int maxOffset, unsigned int salt)
{
	if (maxOffset <= 0) { return 0; }
	h ^= salt + 0x9e3779b9u + (h << 6) + (h >> 2);
	return int(h % (unsigned int)(maxOffset + 1));
}

void Map::create(int sizeX, int sizeY)
{
	firstLayer.create(sizeX, sizeY);
	secondLayer.create(sizeX, sizeY);
	size = {sizeX,sizeY};
	textAnnotations.clear();

	//for (int x = 0; x < size.x; x++)
	//	for (int y = 0; y < size.y; y++)
	//	{
	//		blocks[x + y * size.x].type = Blocks::dirt;
	//	}
	//
	//
	//for (int x = 10; x < size.x; x++)
	//	for (int y = 0; y < 10; y++)
	//	{
	//		blocks[x + y * size.x].type = Blocks::floor1;
	//	}


	//getBlockUnsafe(2, 1).type = Blocks::grass;
	//getBlockUnsafe(3, 2).type = Blocks::grass;
	//getBlockUnsafe(1, 2).type = Blocks::grass;
	//getBlockUnsafe(2, 3).type = Blocks::grass;
	//
	//
	//getBlockUnsafe(5, 5).type = Blocks::grass;
	////getBlockUnsafe(6, 5).type = Blocks::grass;
	//getBlockUnsafe(7, 5).type = Blocks::grass;
	//getBlockUnsafe(8, 5).type = Blocks::grass;
	//getBlockUnsafe(5, 3).type = Blocks::grass;
	////getBlockUnsafe(6, 3).type = Blocks::grass;
	//getBlockUnsafe(7, 3).type = Blocks::grass;
	//getBlockUnsafe(8, 3).type = Blocks::grass;
	//
	//getBlockUnsafe(3, 6).type = Blocks::grass;
	//getBlockUnsafe(2, 8).type = Blocks::grass;
	//
	//
	//getBlockUnsafe(10, 10).type = Blocks::grass;
	//getBlockUnsafe(11, 9).type = Blocks::grass;
	//getBlockUnsafe(12, 10).type = Blocks::grass;
	//getBlockUnsafe(10, 11).type = Blocks::grass;
	//getBlockUnsafe(12, 11).type = Blocks::grass;
	//getBlockUnsafe(9, 11).type = Blocks::grass;
	//getBlockUnsafe(8, 12).type = Blocks::grass;
	//getBlockUnsafe(9, 13).type = Blocks::grass;
	//getBlockUnsafe(10, 13).type = Blocks::grass;
	//getBlockUnsafe(11, 14).type = Blocks::grass;
	//getBlockUnsafe(12, 13).type = Blocks::grass;
	//getBlockUnsafe(13, 12).type = Blocks::grass;
	//
	//
	//
	//getBlockUnsafe(5, 3).type = Blocks::dungeonWall;
	//getBlockUnsafe(5, 7).type = Blocks::dungeonWall;
	//getBlockUnsafe(4, 7).type = Blocks::dungeonWall;
	//getBlockUnsafe(6, 7).type = Blocks::dungeonWall;
	//
	//getBlockUnsafe(8, 5).type = Blocks::dungeonWall;
	//getBlockUnsafe(8, 6).type = Blocks::dungeonWall;
	//getBlockUnsafe(8, 7).type = Blocks::dungeonWall;
	//
	//
	//getBlockUnsafe(12, 5).type = Blocks::dungeonWall;
	//getBlockUnsafe(12, 6).type = Blocks::dungeonWall;
	//getBlockUnsafe(12, 7).type = Blocks::dungeonWall;
	//getBlockUnsafe(11, 6).type = Blocks::dungeonWall;
	//getBlockUnsafe(13, 6).type = Blocks::dungeonWall;
	//getBlockUnsafe(14, 6).type = Blocks::dungeonWall;
	//getBlockUnsafe(14, 5).type = Blocks::dungeonWall;
	//
	//
	//getBlockUnsafe(3, 11).type = Blocks::dungeonWall;
	//getBlockUnsafe(4, 11).type = Blocks::dungeonWall;
	//getBlockUnsafe(6, 11).type = Blocks::dungeonWall;
	//getBlockUnsafe(7, 11).type = Blocks::dungeonWall;
	//getBlockUnsafe(3, 12).type = Blocks::dungeonWall;
	//getBlockUnsafe(7, 12).type = Blocks::dungeonWall;
	//getBlockUnsafe(3, 15).type = Blocks::dungeonWall;
	//getBlockUnsafe(7, 15).type = Blocks::dungeonWall;
	//getBlockUnsafe(3, 16).type = Blocks::dungeonWall;
	//getBlockUnsafe(4, 16).type = Blocks::dungeonWall;
	//getBlockUnsafe(6, 16).type = Blocks::dungeonWall;
	//getBlockUnsafe(7, 16).type = Blocks::dungeonWall;
	//
	//
	//getBlockUnsafe(16, 16).type = Blocks::dungeonWall;
	//getBlockUnsafe(17, 16).type = Blocks::dungeonWall;
	//getBlockUnsafe(16, 17).type = Blocks::dungeonWall;
	//xgetBlockUnsafe(17, 17).type = Blocks::dungeonWall;



}

void Map::renderMap(gl2d::Renderer2D &renderer, AssetsManager &assetManager)
{

	firstLayer.renderMap(renderer, assetManager);
	secondLayer.renderMap(renderer, assetManager);

}

void Map::renderWallShadows(gl2d::Renderer2D &renderer, AssetsManager &assetManager)
{

	auto viewRect = renderer.getViewRect();
	glm::ivec4 viewRectInt = {};
	viewRectInt.x = int(viewRect.x) - 1;
	viewRectInt.y = int(viewRect.y) - 1;
	viewRectInt.z = int(viewRect.z + 0.5) + 2;
	viewRectInt.w = int(viewRect.w + 0.5) + 2;
	viewRectInt.z += viewRect.x;
	viewRectInt.w += viewRect.y;

	viewRectInt = glm::clamp(viewRectInt, {0,1,0,0}, {size.x - 1,size.y - 1,size.x - 1,size.y - 2});

	auto &shadow = assetManager.shadow;
	
	auto checkIsWall = [&](int x, int y)
	{
		return isWall(firstLayer.getBlockUnsafe(x, y).type) ||
			isWall(secondLayer.getBlockUnsafe(x, y).type);
	};

	for (int y = viewRectInt.y; y < viewRectInt.w; y++)
	{
		for (int x = viewRectInt.x; x < viewRectInt.z; x++)
		{

			if (!checkIsWall(x, y))
			{

				bool wallTop = checkIsWall(x, y - 1);
				bool wallRight = checkIsWall(x + 1, y);
				bool wallTopRight = checkIsWall(x+1, y - 1);
				glm::vec4 color = {1,1,1,0.35};

				if (wallTop && wallRight)
				{
					renderer.renderRectangle({x,y,1,1},
						shadow.texture,
						color,
						{},
						0,
						shadow.atlas.get(1, 2)
					);
				}else
				if (wallRight)
				{

					if (!wallTopRight)
					{
						renderer.renderRectangle({x,y,1,1},
							shadow.texture,
							color,
							{},
							0,
							shadow.atlas.get(0, 0)
						);
					}
					else
					{
						renderer.renderRectangle({x,y,1,1},
							shadow.texture,
							color,
							{},
							0,
							shadow.atlas.get(0, 1)
						);
					}

				}else
				if(wallTop)
				{

					if (!wallTopRight)
					{
						renderer.renderRectangle({x,y,1,1},
							shadow.texture,
							color,
							{},
							0,
							shadow.atlas.get(1, 1)
						);
					}
					else
					{
						renderer.renderRectangle({x,y,1,1},
							shadow.texture,
							color,
							{},
							0,
							shadow.atlas.get(1, 0)
						);
					}

				}
				else if (wallTopRight) //corner
				{
					renderer.renderRectangle({x,y,1,1},
						shadow.texture,
						color,
						{},
						0,
						shadow.atlas.get(0, 2)
					);
				}




			}

		}
	}

}

void Map::renderMapAfterEntities(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	const DoorHolder *doorHolder, WorldTextSystem *textSystem, bool usesController)
{

	firstLayer.renderMapAfterEntities(renderer, assetManager, doorHolder,
		nullptr, false, &wallFaceGradientSettings);
	secondLayer.renderMapAfterEntities(renderer, assetManager, nullptr,
		nullptr, false, &wallFaceGradientSettings);

	if (!textAnnotations.empty())
	{
		auto viewRect = renderer.getViewRect();
		float textSize = PIXEL_SIZE * 12.0f;
		float padding = 3.0f;
		float left = viewRect.x - padding;
		float right = viewRect.x + viewRect.z + padding;
		float top = viewRect.y - padding;
		float bottom = viewRect.y + viewRect.w + padding;
		gl2d::Color4f textColor = {1.0f, 1.0f, 1.0f, 1.0f};
		gl2d::Color4f shadowColor = {0.1f, 0.1f, 0.1f, 1.0f};

		if (textSystem)
		{
			textSystem->clear();
			for (const auto &entry : textAnnotations)
			{
				const glm::vec2 &pos = entry.first;
				if (pos.x < left || pos.x > right || pos.y < top || pos.y > bottom)
				{
					continue;
				}
				textSystem->addText(entry.second, pos, textColor, textSize, 4, 3, true);
			}
			textSystem->render(renderer, assetManager, usesController);
		}
		else
		{
			for (const auto &entry : textAnnotations)
			{
				const glm::vec2 &pos = entry.first;
				if (pos.x < left || pos.x > right || pos.y < top || pos.y > bottom)
				{
					continue;
				}
				renderer.renderText(pos, entry.second.c_str(), assetManager.font,
					textColor, textSize, 4, 3, true, shadowColor);
			}
		}
	}

}

void MapLayer::renderMap(gl2d::Renderer2D &renderer,
	AssetsManager &assetManager)
{

	auto viewRect = renderer.getViewRect();
	glm::ivec4 viewRectInt = {};
	viewRectInt.x = int(viewRect.x) - 1;
	viewRectInt.y = int(viewRect.y) - 1;
	viewRectInt.z = int(viewRect.z + 1.5) + 2;
	viewRectInt.w = int(viewRect.w + 1.5) + 2;
	viewRectInt.z += viewRect.x;
	viewRectInt.w += viewRect.y;
	viewRectInt = glm::clamp(viewRectInt, {0,0,0,0}, {size.x - 1,size.y - 1,size.x - 1,size.y - 1});

	for (int y = viewRectInt.y; y < viewRectInt.w; y++)
	{
		for (int x = viewRectInt.x; x < viewRectInt.z; x++)
		{

			Block b = getBlockUnsafe(x, y);

			if (b.type)
			{

			int tileSet = getTileSetIndex(b.type);
			auto uv = getBlockAtlasPos(b.type);
			auto randomOffsets = getRandomAtlasOffsets(b.type);

			auto tile = assetManager.tileSets[tileSet];

			if (tile.texture.isValid())
			{

					if (isChunkyTile(b.type))
					{

						glm::vec4 aabb{x,y,2,1};
						aabb.x -= 0.5;

						renderer.renderRectangle(aabb,
							tile.texture,
							Colors_White,
							{},
							0,
							{0,0.5,1,0}
						);
					}
				else
				{
					if (randomOffsets.x != 0 || randomOffsets.y != 0)
					{
						unsigned int h = hashPosition(x, y);
						uv.x += pickAtlasOffset(h, randomOffsets.x, 0x68bc21ebu);
						uv.y += pickAtlasOffset(h, randomOffsets.y, 0x9e3779b9u);
					}

					renderer.renderRectangle({x,y,1,1},
						tile.texture,
						Colors_White,
							{},
							0,
							tile.atlas.get(uv.x, uv.y)
						);
					}
				}

			};


		}


	}

	//render grass decals
	auto decals = assetManager.tileSets[TileSets::grassDecals];
	auto carpetDecals = assetManager.tileSets[TileSets::carpetDecals];

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

	//render carpet decals
	for (int y = 0; y < size.y; y++)
	{
		for (int x = 0; x < size.x; x++)
		{

			Block b = getBlockUnsafe(x, y);

			if (canHaveCarpetDecals(b.type))
			{

				bool top = 0;
				bool bottom = 0;
				bool left = 0;
				bool right = 0;

				bool topLeft = 0;
				bool topRight = 0;
				bool bottomLeft = 0;
				bool bottomRight = 0;

				auto b = getBlockSafe(x, y - 1); top = b ? isCarpet(b->type) : 0;
					b = getBlockSafe(x, y + 1); bottom = b ? isCarpet(b->type) : 0;
					b = getBlockSafe(x - 1, y); left = b ? isCarpet(b->type) : 0;
					b = getBlockSafe(x + 1, y); right = b ? isCarpet(b->type) : 0;
					b = getBlockSafe(x + 1, y + 1); bottomRight = b ? isCarpet(b->type) : 0;
					b = getBlockSafe(x - 1, y + 1); bottomLeft = b ? isCarpet(b->type) : 0;
					b = getBlockSafe(x + 1, y - 1); topRight = b ? isCarpet(b->type) : 0;
					b = getBlockSafe(x - 1, y - 1); topLeft = b ? isCarpet(b->type) : 0;


				if (top && bottom && left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						carpetDecals.texture,
						Colors_White,
						{},
						0,
						carpetDecals.atlas.get(0, 3)
					);
				}
				else if (top && left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						carpetDecals.texture,
						Colors_White,
						{},
						0,
						carpetDecals.atlas.get(0, 0)
					);
				}
				else if (left && top && bottom)
				{
					renderer.renderRectangle({x,y,1,1},
						carpetDecals.texture,
						Colors_White,
						{},
						0,
						carpetDecals.atlas.get(1, 3)
					);
				}
				else if (right && top && bottom)
				{
					renderer.renderRectangle({x,y,1,1},
						carpetDecals.texture,
						Colors_White,
						{},
						0,
						carpetDecals.atlas.get(3, 3)
					);
				}
				else if (bottom && left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						carpetDecals.texture,
						Colors_White,
						{},
						0,
						carpetDecals.atlas.get(0, 2)
					);
				}
				else if (top && bottom)
				{
					renderer.renderRectangle({x,y,1,1},
						carpetDecals.texture,
						Colors_White,
						{},
						0,
						carpetDecals.atlas.get(2, 3)
					);
				}
				else if (left && right)
				{
					renderer.renderRectangle({x,y,1,1},
						carpetDecals.texture,
						Colors_White,
						{},
						0,
						carpetDecals.atlas.get(0, 1)
					);
				}

				else			
				{
					//can have corner pieces
					if (left && top)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(1, 0)
						);
					}
					else if (right && top)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(3, 0)
						);
					}
					else if (left && bottom)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(1, 2)
						);
					}
					else if (right && bottom)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(3, 2)
						);
					}

					else if (left)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(1, 1)
						);
					}
					else if (right)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(3, 1)
						);
					}
					else if (top)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(2, 0)
						);
					}
					else if (bottom)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(2, 2)
						);
					}


					if (!bottom && !left && bottomLeft)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(5, 0)
						);
					}

					if (!bottom && !right && bottomRight)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(4, 0)
						);
					}

					if (!top && !left && topLeft)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(5, 1)
						);
					}

					if (!top && !right && topRight)
					{
						renderer.renderRectangle({x,y,1,1},
							carpetDecals.texture,
							Colors_White,
							{},
							0,
							carpetDecals.atlas.get(4, 1)
						);
					}


				}

			}

		};

	}





}

void MapLayer::renderMapAfterEntities(gl2d::Renderer2D &renderer,
	AssetsManager &assetManager, const DoorHolder *doorHolder,
	WorldTextSystem *textSystem, bool usesController,
	const WallFaceGradientSettings *wallFaceGradientSettings)
{
	(void)textSystem;
	(void)usesController;



	static float opacity = 1;

	if (ImGui::isImguiWindowOpen())
	{
		ImGui::Begin("Game Debug");
		ImGui::SliderFloat("Opacity", &opacity, 0, 1);
		ImGui::End();
	}

	glm::vec4 color = {1,1,1,opacity};

	auto viewRect = renderer.getViewRect();
	glm::ivec4 viewRectInt = {};
	viewRectInt.x = int(viewRect.x) - 1;
	viewRectInt.y = int(viewRect.y) - 1;
	viewRectInt.z = int(viewRect.z + 1.5) + 2;
	viewRectInt.w = int(viewRect.w + 1.5) + 2;
	viewRectInt.z += viewRect.x;
	viewRectInt.w += viewRect.y;
	viewRectInt = glm::clamp(viewRectInt, {0,0,0,0}, {size.x - 1,size.y - 1,size.x - 1,size.y - 1});

	for (int y = viewRectInt.y; y < viewRectInt.w; y++)
	{
		for (int x = viewRectInt.x; x < viewRectInt.z; x++)
		{
			if (doorHolder)
			{
				auto it = doorHolder->doors.find({x, y});
				if (it != doorHolder->doors.end())
				{
					const Door &door = it->second;
					if (door.orientation == Door::Orientation::Vertical)
					{
						gl2d::Texture &sprite = door.open
							? assetManager.doorOpenedVertical
							: assetManager.doorClosedVertical;
						if (sprite.isValid())
						{
							// Vertical door is drawn taller but keeps the same bottom anchor.
							float doorBottom = (float)y + 1.0f - DOOR_VERTICAL_Y_OFFSET;
							glm::vec4 rect = {(float)x, doorBottom - DOOR_VERTICAL_HEIGHT, 1.0f, DOOR_VERTICAL_HEIGHT};
							renderer.renderRectangle(rect, sprite, Colors_White);
						}
					}
				}
			}

			auto current = getBlockUnsafe(x, y);
			if (isWall(current.type))
			{
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

				auto wall = assetManager.tileSets[getWall3DTileSetIndex(current.type)];

				if (left && right && top && bottom)
				{
					//fully inclosed
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(2, 1)
					);
				
				}
				else

					//thin walls
					if (left && right && !top && !bottom)
					{
						renderer.renderRectangle({x,y - 1,1,1},
							wall.texture,
							color,
							{},
							0,
							wall.atlas.get(2, 3)
						);

					}
					else
					if (bottom && top && !left && !right)
					{
						renderer.renderRectangle({x,y - 1,1,1},
							wall.texture,
							color,
							{},
							0,
							wall.atlas.get(0, 1)
						);

					}else

					//simple walls
				if (bottom && top && !right)
				{
					//right wall
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(3, 1)
					);

				}else if (bottom && top && !left)
				{
					//right wall
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(1, 1)
					);

				}
				else if (left && right && !top)
				{
					//top wall
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(2, 0)
					);

				}
				else if (left && right && !bottom)
				{
					//bottom wall
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(2, 2)
					);

				}
				else

				if (left && top)
				{
					//bottom left corner
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(3, 2)
					);

				}else if(right &&top)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(1, 2)
					);

				}
				else if(right && bottom)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(1, 0)
					);

				}
				else if (left && bottom)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(3, 0)
					);

				}


				else if (left)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(3, 3)
					);
				}
				else if (right)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(1, 3)
					);
				}
				else
				if (bottom)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(0, 0)
					);

				}else
				if (top)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(0, 2)
					);

				}
				else
				if (!bottom && !left && !right && !top)
				{
					//fully open
					renderer.renderRectangle({x,y-1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(0, 3)
					);
				}


				//corners
				if (bottom && left && !bottomLeft)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(5, 0)
					);
				}
				if (bottom && right && !bottomRight)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(4, 0)
					);
				}
				if (top && left && !topLeft)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(5, 1)
					);
				}
				if (top && right && !topRight)
				{
					renderer.renderRectangle({x,y - 1,1,1},
						wall.texture,
						color,
						{},
						0,
						wall.atlas.get(4, 1)
					);
				}

				if (wallFaceGradientSettings && wallFaceGradientSettings->enabled && !bottom)
				{
					float topStrength = glm::clamp(wallFaceGradientSettings->topRimStrength, 0.0f, 1.0f);
					float bottomStrength = glm::clamp(wallFaceGradientSettings->bottomShadeStrength, 0.0f, 1.0f);
					float span = glm::clamp(wallFaceGradientSettings->gradientSpan, 0.05f, 1.0f);

					auto oldBlend = renderer.getBlendMode();

					if (topStrength > 0.0001f)
					{
						renderer.setBlendMode(gl2d::Renderer2D::BlendMode_Additive);
						gl2d::Color4f topColors[4] = {
							{topStrength, topStrength, topStrength, topStrength},
							{0.0f, 0.0f, 0.0f, 0.0f},
							{0.0f, 0.0f, 0.0f, 0.0f},
							{topStrength, topStrength, topStrength, topStrength},
						};
						// Wall faces render at y-1, so gradient overlays the same face tile.
						renderer.renderRectangle({(float)x, (float)y - 1.0f, 1.0f, span}, topColors);
					}

					if (bottomStrength > 0.0001f)
					{
						renderer.setBlendMode(gl2d::Renderer2D::BlendMode_Alpha);
						gl2d::Color4f bottomColors[4] = {
							{0.0f, 0.0f, 0.0f, 0.0f},
							{0.0f, 0.0f, 0.0f, bottomStrength},
							{0.0f, 0.0f, 0.0f, bottomStrength},
							{0.0f, 0.0f, 0.0f, 0.0f},
						};
						renderer.renderRectangle({(float)x, (float)y - span, 1.0f, span}, bottomColors);
					}

					renderer.setBlendMode(oldBlend);
				}
			}
			else if (isChunkyTile(current.type))
			{
				int tileSet = getTileSetIndex(current.type);
				auto tile = assetManager.tileSets[tileSet];

				glm::vec4 aabb{x,y,2,1};
				aabb.x -= 0.5;
				aabb.y -= 1;

				renderer.renderRectangle(aabb,
					tile.texture,
					Colors_White,
					{},
					0,
					{0,1,1,0.5}
				);
			}


		}

	}


}

Block &MapLayer::getBlockUnsafe(int x, int y)
{
	return blocks[x + y * size.x];
}

Block *MapLayer::getBlockSafe(int x, int y)
{
	if (x < 0 || y < 0 || x >= size.x || y >= size.y)
	{
		return nullptr;
	}

	return &blocks[x + y * size.x];
}
