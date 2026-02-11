#pragma once
#include <gameplay/blocks.h>
#include <vector>
#include <gameplay/assetsManager.h>
#include <unordered_map>
#include <string>
#include <functional>

struct DoorHolder;
struct WorldTextSystem;

struct WallFaceGradientSettings
{
	bool enabled = true;
	float topRimStrength = 0.07f;
	float bottomShadeStrength = 0.23f;
	float gradientSpan = 0.25f;
};

// Hash helper for using glm::vec2 in unordered containers.
struct Vec2Hash
{
	std::size_t operator()(const glm::vec2 &value) const
	{
		std::size_t hx = std::hash<float>{}(value.x);
		std::size_t hy = std::hash<float>{}(value.y);
		return hx ^ (hy + 0x9e3779b9 + (hx << 6) + (hx >> 2));
	}
};

struct Vec2Equal
{
	bool operator()(const glm::vec2 &a, const glm::vec2 &b) const
	{
		return a.x == b.x && a.y == b.y;
	}
};


struct MapLayer
{
	void create(int sizeX, int sizeY)
	{
		*this = {};
		blocks.resize(sizeX * sizeY);
		size = {sizeX, sizeY};
	}

	std::vector<Block> blocks;
	glm::ivec2 size = {};

	Block &getBlockUnsafe(int x, int y);

	Block *getBlockSafe(int x, int y);

	void renderMap(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager,
		const WallFaceGradientSettings *wallFaceGradientSettings = nullptr,
		MapLayer *otherLayer = nullptr);

	//for rendering tall walls and stuff
	void renderMapAfterEntities(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager, const DoorHolder *doorHolder = nullptr,
		WorldTextSystem *textSystem = nullptr, bool usesController = false);
};

struct Map
{

	void create(int sizeX, int sizeY);


	MapLayer firstLayer;
	MapLayer secondLayer;
	glm::ivec2 size = {};
	std::unordered_map<glm::vec2, std::string, Vec2Hash, Vec2Equal> textAnnotations; // world-space text notes
	WallFaceGradientSettings wallFaceGradientSettings = {};

	
	bool isCollidableAtPosSafe(int x, int y)
	{
		auto b = firstLayer.getBlockSafe(x, y);
		if (!b) { return false; }

		if (isBlockColidable(b->type)) { return true; }
		
		b = secondLayer.getBlockSafe(x, y);
		if (isBlockColidable(b->type)) { return true; }

		return false;
	}

	bool isCollidableAtPosUnsafe(int x, int y)
	{
		auto b = firstLayer.getBlockUnsafe(x, y);
		if (isBlockColidable(b.type)) { return true; }

		b = secondLayer.getBlockUnsafe(x, y);
		if (isBlockColidable(b.type)) { return true; }

		return false;
	}

	void renderMap(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager);

	void renderWallShadows(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager);

	//for rendering tall walls and stuff
	void renderMapAfterEntities(gl2d::Renderer2D &renderer,
		AssetsManager &assetManager, const DoorHolder *doorHolder = nullptr,
		WorldTextSystem *textSystem = nullptr, bool usesController = false);

};
