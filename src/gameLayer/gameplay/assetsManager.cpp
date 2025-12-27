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

}