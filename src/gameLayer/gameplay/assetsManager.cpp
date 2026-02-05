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
		16, //caveFloor,
		16, //grassDecor,
		16, //wooden floor,
		16, //wooden wall 3D,
		16, //carpet decals,
		16, //woodenDecorations
		16, //exit
		16, //wallDecorations
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
		{8,1}, //caveFloor,
		{4,4}, //grassDecor,
		{4,1}, //wooden floor
		{6,4}, //wooden wall 3D
		{6,4}, //carpet decals
		{4,1}, //woodenDecorations
		{1,1}, //exit
		{16,1}, //wallDecorations

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


	auto loadCharacter = [&](TileSet &set, const char *path, int cellSize, glm::ivec2 atlasSize)
	{
		set.texture.loadFromFileWithPixelPadding(path, cellSize);
		auto size = set.texture.GetSize();
		set.atlas = gl2d::TextureAtlasPadding(atlasSize.x, atlasSize.y, size.x, size.y);
	};

	player.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/player.png",	48);
	auto s = player.texture.GetSize();
	player.atlas = gl2d::TextureAtlasPadding(6, 10, s.x, s.y);

	loadCharacter(skeleton, RESOURCES_PATH "characters/skeleton.png", 48, {6, 10});
	loadCharacter(templarOriginal, RESOURCES_PATH "characters/templarOriginal.png", 48, {6, 10});
	loadCharacter(earthTemplar, RESOURCES_PATH "characters/earthTemplar.png", 48, {6, 10});
	loadCharacter(fireTemplar, RESOURCES_PATH "characters/fireTemplar.png", 48, {6, 10});
	loadCharacter(iceTemplar, RESOURCES_PATH "characters/iceTemplar.png", 48, {6, 10});
	loadCharacter(waterTemplar, RESOURCES_PATH "characters/waterTemplar.png", 48, {6, 10});
	loadCharacter(goblinArcher, RESOURCES_PATH "characters/goblinArcher.png", 48, {6, 10});
	loadCharacter(goblinSpearman, RESOURCES_PATH "characters/goblinSpearman.png", 48, {6, 10});
	loadCharacter(orcArcher, RESOURCES_PATH "characters/orcArcher.png", 48, {6, 10});
	loadCharacter(goblinHeavy, RESOURCES_PATH "characters/goblinHeavy.png", 32, {6, 10});
	loadCharacter(goblinThief, RESOURCES_PATH "characters/golbinThief.png", 32, {6, 10});

	// dark angel uses variable column count, compute from texture width
	int darkCellSize = 64;
	darkAngel.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "characters/darkAngel.png", darkCellSize);
	s = darkAngel.texture.GetSize();
	int darkCols = std::max(1, (s.x + darkCellSize / 2) / darkCellSize);
	darkAngel.atlas = gl2d::TextureAtlasPadding(darkCols, 10, s.x, s.y);

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

	woodenChest.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "map/woodenChest.png", 16);
	s = woodenChest.texture.GetSize();
	woodenChest.atlas = gl2d::TextureAtlasPadding(4, 1, s.x, s.y);

	hearth.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "items/hearth.png", 16);
	s = hearth.texture.GetSize();
	hearth.atlas = gl2d::TextureAtlasPadding(1, 1, s.x, s.y);

	coin.texture.loadFromFileWithPixelPadding(RESOURCES_PATH "items/coin.png", 16);
	s = coin.texture.GetSize();
	coin.atlas = gl2d::TextureAtlasPadding(6, 1, s.x, s.y);

	doorClosedHorizontal.loadFromFile(RESOURCES_PATH "map/doorClosedHorizontal.png");
	doorOpenedHorizontal.loadFromFile(RESOURCES_PATH "map/doorOpenedHorizontal.png");
	doorClosedVertical.loadFromFile(RESOURCES_PATH "map/doorClosedVertical.png");
	doorOpenedVertical.loadFromFile(RESOURCES_PATH "map/doorOpenedVertical.png");

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

	// load button prompts for UI (xbox series + light keyboard/mouse)
	{
		std::string buttonsRoot = std::string(RESOURCES_PATH) + "buttons/";
		std::string controllerDir = buttonsRoot + "Xbox Series/";
		std::string keyboardDir = buttonsRoot + "Keyboard & Mouse/Light/";

		buttonSprites.controller.clear();
		buttonSprites.keyboard.clear();
		buttonSprites.mouse.clear();
		buttonSprites.controller.reserve(32);
		buttonSprites.keyboard.reserve(8);
		buttonSprites.mouse.reserve(8);

		auto loadButtonTexture = [&](std::unordered_map<std::string, gl2d::Texture> &out,
			const std::string &key, const std::string &path)
		{
			auto &tex = out[key];
			tex.loadFromFile(path.c_str());
			if (!tex.isValid())
			{
				out.erase(key);
				std::string err = "Error couldn't load texture: ";
				err += path;
				platform::log(err.c_str(), LogManager::logError);
			}
		};

		auto loadButtonFolder = [&](const std::string &directory,
			const std::string &prefix, std::unordered_map<std::string, gl2d::Texture> &out)
		{
			if (!std::filesystem::exists(directory))
			{
				std::string err = "Warning couldn't find button folder: ";
				err += directory;
				platform::log(err.c_str(), LogManager::logWarning);
				return;
			}

			for (auto &entry : std::filesystem::directory_iterator(directory))
			{
				if (!entry.is_regular_file()) { continue; }
				auto path = entry.path();
				if (path.extension() != ".png") { continue; }

				std::string key = path.stem().string();
				if (!prefix.empty() && key.rfind(prefix, 0) == 0)
				{
					key.erase(0, prefix.size());
					if (!key.empty() && key[0] == '_')
					{
						key.erase(0, 1);
					}
				}

				loadButtonTexture(out, key, path.string());
			}
		};

		loadButtonFolder(controllerDir, "XboxSeriesX", buttonSprites.controller);

		if (std::filesystem::exists(keyboardDir))
		{
			loadButtonTexture(buttonSprites.keyboard, "W", keyboardDir + "W_Key_Light.png");
			loadButtonTexture(buttonSprites.keyboard, "A", keyboardDir + "A_Key_Light.png");
			loadButtonTexture(buttonSprites.keyboard, "S", keyboardDir + "S_Key_Light.png");
			loadButtonTexture(buttonSprites.keyboard, "D", keyboardDir + "D_Key_Light.png");
			loadButtonTexture(buttonSprites.keyboard, "Q", keyboardDir + "Q_Key_Light.png");
			loadButtonTexture(buttonSprites.keyboard, "E", keyboardDir + "E_Key_Light.png");
			loadButtonTexture(buttonSprites.keyboard, "Tab", keyboardDir + "Tab_Key_Light.png");
			loadButtonTexture(buttonSprites.keyboard, "Arrow_Up", keyboardDir + "Arrow_Up_Key_Light.png");

			loadButtonTexture(buttonSprites.mouse, "Left", keyboardDir + "Mouse_Left_Key_Light.png");
			loadButtonTexture(buttonSprites.mouse, "Right", keyboardDir + "Mouse_Right_Key_Light.png");
			loadButtonTexture(buttonSprites.mouse, "Middle", keyboardDir + "Mouse_Middle_Key_Light.png");
			loadButtonTexture(buttonSprites.mouse, "Simple", keyboardDir + "Mouse_Simple_Key_Light.png");
		}
		else
		{
			std::string err = "Warning couldn't find button folder: ";
			err += keyboardDir;
			platform::log(err.c_str(), LogManager::logWarning);
		}
	}

	particleCircle.loadFromFile(RESOURCES_PATH "circle.png");
	particleCross.loadFromFile(RESOURCES_PATH "cross.png");
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
