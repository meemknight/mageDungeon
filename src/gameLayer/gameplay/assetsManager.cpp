#include <gameplay/assetsManager.h>
#include <logs.h>
#include <magic_enum.hpp>


void AssetsManager::loadAllAssets()
{

	font.createFromFile(RESOURCES_PATH "font/roboto_black.TTF");

	//RESOURCES_PATH "map/Damp Dungeon Tileset.png"


	//this represents the size of one block
	static const int blockSize[] =
	{
		0,
		16,
		16,
		16,
		16,
		16, //dungeon wall
		16, //dungeon wall 3D
		16, //fence
		16, //hill
		16, //hill3D
		0, //smallTree   0 for no texture atlas
	};

	//if you see an error that means you added a sprite but forgot to add
	//a block size to it! ^^^^
	static_assert(sizeof(blockSize) / sizeof(blockSize[0])
		== TileSets::TILE_SETS_COUNT);


	//how many blocks are there in the texture atlas
	static const glm::ivec2 textureAtlasSizes[] =
	{
		{},
		{7, 8},
		{2, 1},
		{2, 1},
		{6, 4},
		{2, 1}, //dungeon wall
		{6, 4}, //dungeon wall 3D
		{4, 4}, //fence
		{1, 1}, //hill
		{6, 4}, //hill3D
		{1, 1}, //smallTree

	};

	//if you see an error that means you added a sprite but forgot to add
	//am texture atlas size to it! ^^^^
	static_assert(sizeof(textureAtlasSizes) / sizeof(textureAtlasSizes[0])
		== TileSets::TILE_SETS_COUNT);


	for (int i = 1; i < TileSets::TILE_SETS_COUNT; i++)
	{

		std::string name = RESOURCES_PATH;
		name += "map/";
		name += magic_enum::enum_name((TileSets::TileSets)i);
		name += ".png";

		if (blockSize[i] == 0)
		{

			tileSets[i].texture.loadFromFile(name.c_str(), true, true);

			if (!tileSets[i].texture.isValid())
			{
				std::string err = "Error couldn't load texture: ";
				err += name;
				platform::log(err.c_str(), LogManager::logError);
			}
			else
			{
				auto size = tileSets[i].texture.GetSize();

				tileSets[i].atlas =
					gl2d::TextureAtlasPadding(
					textureAtlasSizes[i].x, textureAtlasSizes[i].y,
					1, 1);
			}

		}
		else
		{

			tileSets[i].texture.loadFromFileWithPixelPadding(name.c_str(),
				blockSize[i], true, true
			);

			if (!tileSets[i].texture.isValid())
			{
				std::string err = "Error couldn't load texture: ";
				err += name;
				platform::log(err.c_str(), LogManager::logError);
			}
			else
			{
				auto size = tileSets[i].texture.GetSize();

				tileSets[i].atlas =
					gl2d::TextureAtlasPadding(
					textureAtlasSizes[i].x, textureAtlasSizes[i].y,
					size.x, size.y);

			}

		}


	}


	player.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/player.png",	48);
	auto s = player.texture.GetSize();
	player.atlas = gl2d::TextureAtlasPadding(6, 10, s.x, s.y);


	elements.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/elements.png", 16);
	s = elements.texture.GetSize();
	elements.atlas = gl2d::TextureAtlasPadding(4, 1, s.x, s.y);


	upCircle.loadFromFile(RESOURCES_PATH "ui/upCircle.png");
	downCircle.loadFromFile(RESOURCES_PATH "ui/downCircle.png");
	leftCircle.loadFromFile(RESOURCES_PATH "ui/leftCircle.png");
	rightCircle.loadFromFile(RESOURCES_PATH "ui/rightCircle.png");


}