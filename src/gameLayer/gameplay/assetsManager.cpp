#include <gameplay/assetsManager.h>
#include <gameplay/paletteEffect.h>
#include <logs.h>
#include <magic_enum.hpp>
#include <stb_image/stb_image.h>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>


void AssetsManager::loadAllAssets()
{

	font.createFromFile(RESOURCES_PATH "font/pixel.ttf");

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
		16, //stoneWall3D
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
		{6, 4}, //stone wall 3D

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


	skeleton.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/skeleton.png", 48);
	s = skeleton.texture.GetSize();
	skeleton.atlas = gl2d::TextureAtlasPadding(6, 10, s.x, s.y);

	waterSlime.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/waterSlime.png", 32);
	s = waterSlime.texture.GetSize();
	waterSlime.atlas = gl2d::TextureAtlasPadding(6, 7, s.x, s.y);

	iceSlime.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/iceSlime.png", 32);
	s = iceSlime.texture.GetSize();
	iceSlime.atlas = gl2d::TextureAtlasPadding(6, 7, s.x, s.y);

	fireSlime.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/fireSlime.png", 32);
	s = fireSlime.texture.GetSize();
	fireSlime.atlas = gl2d::TextureAtlasPadding(6, 7, s.x, s.y);

	elements.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "ui/elements.png", 16);
	s = elements.texture.GetSize();
	elements.atlas = gl2d::TextureAtlasPadding(5, 1, s.x, s.y);

	shadow.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "map/shadow.png", 16);
	s = shadow.texture.GetSize();
	shadow.atlas = gl2d::TextureAtlasPadding(2, 3, s.x, s.y);

	wands.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "items/wands.png", 16);
	s = wands.texture.GetSize();
	wands.atlas = gl2d::TextureAtlasPadding(32, 1, s.x, s.y);

	PaletteEffect palette;
	palette.loadPalette();
	bool hasPalette = palette.hasPalette();
	auto loadPalettedTexture = [&](gl2d::Texture &tex, const std::string &path)
	{
		int w = 0;
		int h = 0;
		int channels = 0;
		unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 4);
		if (!data)
		{
			std::string err = "Error couldn't load texture: ";
			err += path;
			platform::log(err.c_str(), LogManager::logError);
			return false;
		}

		std::vector<unsigned char> buffer(data, data + (size_t)w * (size_t)h * 4);
		STBI_FREE(data);
		if (hasPalette)
		{
			palette.applyToBuffer(buffer);
		}
		tex.createFromBuffer((const char *)buffer.data(), w, h, true, true);
		if (!tex.isValid())
		{
			std::string err = "Error couldn't load texture: ";
			err += path;
			platform::log(err.c_str(), LogManager::logError);
			return false;
		}
		return true;
	};

	upCircle.loadFromFile(RESOURCES_PATH "ui/upCircle.png");
	downCircle.loadFromFile(RESOURCES_PATH "ui/downCircle.png");
	leftCircle.loadFromFile(RESOURCES_PATH "ui/leftCircle.png");
	rightCircle.loadFromFile(RESOURCES_PATH "ui/rightCircle.png");

	// palette the inventory book texture so it matches the UI palette
	loadPalettedTexture(book, RESOURCES_PATH "ui/book.png");

	// load per-wand textures for the inventory UI
	{
		std::string wandDirectory = std::string(RESOURCES_PATH) + "wands/";
		int maxIndex = -1;
		if (std::filesystem::exists(wandDirectory))
		{
			for (auto &entry : std::filesystem::directory_iterator(wandDirectory))
			{
				if (!entry.is_regular_file()) { continue; }
				auto path = entry.path();
				if (path.extension() != ".png") { continue; }
				std::string stem = path.stem().string();
				if (stem.empty()) { continue; }
				bool allDigits = std::all_of(stem.begin(), stem.end(), [](unsigned char c)
				{
					return std::isdigit(c) != 0;
				});
				if (!allDigits) { continue; }
				int index = std::stoi(stem);
				maxIndex = std::max(maxIndex, index);
			}
		}

		if (maxIndex < 0) { maxIndex = 0; }
		loadPalettedTexture(wandFallback, wandDirectory + "0.png");
		wandIcons.clear();
		wandIcons.resize(maxIndex + 1);
		for (int i = 0; i <= maxIndex; i++)
		{
			std::string path = wandDirectory + std::to_string(i) + ".png";
			if (std::filesystem::exists(path))
			{
				loadPalettedTexture(wandIcons[i], path);
			}
		}
	}

	particleCircle.loadFromFile(RESOURCES_PATH "circle.png");
	particleSmoke.loadFromFile(RESOURCES_PATH "smoke.png");
	target.loadFromFile(RESOURCES_PATH "target.png");
	thorn.loadFromFile(RESOURCES_PATH "thorn.png");

}

gl2d::Texture &AssetsManager::getWandIcon(int index)
{
	if (index < 0 || index >= (int)wandIcons.size())
	{
		return wandFallback;
	}
	if (!wandIcons[index].isValid())
	{
		return wandFallback;
	}
	return wandIcons[index];
}
