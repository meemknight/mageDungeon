#include <gameplay/cosmeticDynamicLightSystem.h>

#include <gameplay/map.h>
#include <gameplay/blocks.h>
#include <gameplay/aStar.h>

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

int CosmeticDynamicLightSystem::toIndex(int x, int y) const
{
	return x + y * mapSize.x;
}

bool CosmeticDynamicLightSystem::inBounds(int x, int y) const
{
	return x >= 0 && y >= 0 && x < mapSize.x && y < mapSize.y;
}

bool CosmeticDynamicLightSystem::hasTileAt(Map &map, int x, int y) const
{
	if (!inBounds(x, y)) { return false; }
	auto base = map.firstLayer.getBlockUnsafe(x, y).type;
	auto over = map.secondLayer.getBlockUnsafe(x, y).type;
	return base != Blocks::none || over != Blocks::none;
}

bool CosmeticDynamicLightSystem::isWallAt(Map &map, int x, int y) const
{
	if (!inBounds(x, y)) { return false; }
	auto base = map.firstLayer.getBlockUnsafe(x, y).type;
	auto over = map.secondLayer.getBlockUnsafe(x, y).type;
	return isWall(base) || isWall(over);
}

void CosmeticDynamicLightSystem::init()
{
	*this = {};
	#if GL2D_USE_SDL_GPU
	// Store mask in HDR so extra light values above 1.0 survive sampling.
	maskFbo.gpuTextureFormat = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
	#endif
	maskFbo.create(1, 1, true);
}

void CosmeticDynamicLightSystem::cleanup()
{
	maskFbo.cleanup();
	*this = {};
}

void CosmeticDynamicLightSystem::resetForFloor(Map &map)
{
	mapSize = map.size;
	int tileCount = std::max(0, mapSize.x * mapSize.y);
	tileLight.assign(tileCount, 0.0f);
	lights.clear();
}

void CosmeticDynamicLightSystem::beginFrame(Map &map)
{
	if (mapSize != map.size || tileLight.size() != (size_t)std::max(0, map.size.x * map.size.y))
	{
		resetForFloor(map);
	}

	lights.clear();
	if (tileLight.empty()) { return; }

	float baseLight = enabled ? std::max(ambientLight, 0.0f) : 0.0f;
	std::fill(tileLight.begin(), tileLight.end(), baseLight);

	// Keep wall tiles and top-wall projections neutral so only floor/entities/player get extra light.
	for (int y = 0; y < mapSize.y; y++)
	{
		for (int x = 0; x < mapSize.x; x++)
		{
			if (!isWallAt(map, x, y)) { continue; }

			tileLight[toIndex(x, y)] = 0.0f;
			if (y > 0)
			{
				tileLight[toIndex(x, y - 1)] = 0.0f;
			}
		}
	}
}

void CosmeticDynamicLightSystem::addLight(glm::vec2 position, float radius, float intensity,
	float falloffPower, bool castsShadows)
{
	if (!enabled) { return; }
	if (radius <= 0.05f) { return; }
	if (intensity <= 0.001f) { return; }

	CosmeticDynamicLight light = {};
	light.position = position;
	light.radius = radius;
	light.intensity = intensity;
	light.falloffPower = std::max(falloffPower, 0.05f);
	light.castsShadows = castsShadows;
	lights.push_back(light);
}

void CosmeticDynamicLightSystem::buildLightMask(Map &map)
{
	if (!enabled) { return; }
	if (lights.empty()) { return; }
	if (tileLight.empty()) { return; }

	float baseLight = std::max(ambientLight, 0.0f);

	for (const auto &light : lights)
	{
		glm::ivec2 originTile = WorldToTile(light.position);
		float radius = std::max(light.radius, 0.05f);
		float radius2 = radius * radius;
		float falloffPower = std::max(light.falloffPower, 0.05f);

		int minX = (int)std::floor(light.position.x - radius) - 1;
		int minY = (int)std::floor(light.position.y - radius) - 1;
		int maxX = (int)std::ceil(light.position.x + radius) + 1;
		int maxY = (int)std::ceil(light.position.y + radius) + 1;

		minX = std::clamp(minX, 0, mapSize.x - 1);
		minY = std::clamp(minY, 0, mapSize.y - 1);
		maxX = std::clamp(maxX, 0, mapSize.x - 1);
		maxY = std::clamp(maxY, 0, mapSize.y - 1);

		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				if (!inBounds(x, y)) { continue; }
				if (!hasTileAt(map, x, y)) { continue; }
				if (isWallAt(map, x, y)) { continue; }

				glm::vec2 tileCenter = {(float)x + 0.5f, (float)y + 0.5f};
				glm::vec2 diff = tileCenter - light.position;
				float dist2 = glm::dot(diff, diff);
				if (dist2 > radius2) { continue; }

				if (light.castsShadows)
				{
					if (!HasLineOfSightGrid(map, originTile, {x, y}))
					{
						continue;
					}
				}

				float dist = std::sqrt(dist2);
				float t = 1.0f - (dist / radius);
				t = std::clamp(t, 0.0f, 1.0f);
				float smooth = std::pow(t, falloffPower);
				float lit = baseLight + smooth * light.intensity;

				int idx = toIndex(x, y);
				tileLight[idx] = std::max(tileLight[idx], lit);
			}
		}
	}
}

void CosmeticDynamicLightSystem::updateWindowMetrics(gl2d::Renderer2D &renderer)
{
	int width = std::max(renderer.windowW, 1);
	int height = std::max(renderer.windowH, 1);
	maskFbo.resize(width, height);
}

void CosmeticDynamicLightSystem::renderMask(gl2d::Renderer2D &renderer, Map &map)
{
	if (!maskFbo.texture.isValid()) { return; }
	if (tileLight.empty()) { return; }

	maskFbo.bind();
	renderer.clearScreen({0, 0, 0, 1});

	auto oldBlend = renderer.getBlendMode();
	renderer.setBlendMode(gl2d::Renderer2D::BlendMode_Alpha);

	auto viewRect = renderer.getViewRect();
	int minX = std::max(0, (int)std::floor(viewRect.x) - 2);
	int minY = std::max(0, (int)std::floor(viewRect.y) - 2);
	int maxX = std::min(mapSize.x, (int)std::ceil(viewRect.x + viewRect.z) + 2);
	int maxY = std::min(mapSize.y, (int)std::ceil(viewRect.y + viewRect.w) + 2);

	for (int y = minY; y < maxY; y++)
	{
		for (int x = minX; x < maxX; x++)
		{
			if (!hasTileAt(map, x, y)) { continue; }

			float value = std::max(tileLight[toIndex(x, y)], 0.0f);
			renderer.renderRectangle({(float)x, (float)y, 1.0f, 1.0f},
				{value, value, value, 1.0f});
		}
	}

	renderer.setBlendMode(oldBlend);
	maskFbo.unbind();
}
