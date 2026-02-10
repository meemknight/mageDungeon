#include <gameplay/cosmeticDynamicLightSystem.h>

#include <gameplay/map.h>
#include <gameplay/blocks.h>
#include <gameplay/Physics.h>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace
{
	struct OccluderSegment
	{
		glm::vec2 a = {};
		glm::vec2 b = {};
	};

	struct RayHit
	{
		float angle = 0.0f;
		glm::vec2 point = {};
	};

	float cross2d(const glm::vec2 &a, const glm::vec2 &b)
	{
		return a.x * b.y - a.y * b.x;
	}

	float normalizeAngle(float angle)
	{
		const float twoPi = glm::two_pi<float>();
		while (angle <= -glm::pi<float>()) { angle += twoPi; }
		while (angle > glm::pi<float>()) { angle -= twoPi; }
		return angle;
	}

	bool intersectRayWithSegment(const glm::vec2 &origin,
		const glm::vec2 &direction,
		const OccluderSegment &segment,
		float &outDistance)
	{
		glm::vec2 edge = segment.b - segment.a;
		float denom = cross2d(direction, edge);
		if (std::abs(denom) < 0.000001f) { return false; }

		glm::vec2 fromRay = segment.a - origin;
		float t = cross2d(fromRay, edge) / denom;
		float u = cross2d(fromRay, direction) / denom;
		if (t < 0.0f) { return false; }
		if (u < 0.0f || u > 1.0f) { return false; }

		outDistance = t;
		return true;
	}

	bool pointInPolygon(const std::vector<glm::vec2> &polygon, const glm::vec2 point)
	{
		if (polygon.size() < 3) { return false; }

		bool inside = false;
		for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
		{
			const glm::vec2 &a = polygon[i];
			const glm::vec2 &b = polygon[j];

			bool intersects = ((a.y > point.y) != (b.y > point.y));
			if (!intersects) { continue; }

			float t = (point.y - a.y) / (b.y - a.y + 0.000001f);
			float hitX = a.x + (b.x - a.x) * t;
			if (point.x < hitX)
			{
				inside = !inside;
			}
		}

		return inside;
	}
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

bool CosmeticDynamicLightSystem::isOccluderAt(Map &map, int x, int y) const
{
	if (!inBounds(x, y)) { return false; }
	return map.isCollidableAtPosSafe(x, y);
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
	lights.clear();
	visibilityPolygons.clear();
}

void CosmeticDynamicLightSystem::beginFrame(Map &map)
{
	if (mapSize != map.size)
	{
		resetForFloor(map);
	}

	lights.clear();
	visibilityPolygons.clear();
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
	visibilityPolygons.clear();
	if (mapSize.x <= 0 || mapSize.y <= 0) { return; }
	if (!enabled) { return; }
	if (lights.empty()) { return; }

	for (const auto &light : lights)
	{
		float radius = std::max(light.radius, 0.05f);
		float radiusPad = radius + 1.5f;

		int minX = (int)std::floor(light.position.x - radiusPad) - 1;
		int minY = (int)std::floor(light.position.y - radiusPad) - 1;
		int maxX = (int)std::ceil(light.position.x + radiusPad) + 1;
		int maxY = (int)std::ceil(light.position.y + radiusPad) + 1;

		minX = std::clamp(minX, 0, mapSize.x - 1);
		minY = std::clamp(minY, 0, mapSize.y - 1);
		maxX = std::clamp(maxX, 0, mapSize.x - 1);
		maxY = std::clamp(maxY, 0, mapSize.y - 1);

		std::vector<OccluderSegment> occluderSegments;
		occluderSegments.reserve((maxX - minX + 1) * (maxY - minY + 1) * 2);

		if (light.castsShadows)
		{
			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					if (!isOccluderAt(map, x, y)) { continue; }

					if (!isOccluderAt(map, x - 1, y))
					{
						occluderSegments.push_back({{(float)x, (float)y}, {(float)x, (float)y + 1.0f}});
					}
					if (!isOccluderAt(map, x + 1, y))
					{
						occluderSegments.push_back({{(float)x + 1.0f, (float)y}, {(float)x + 1.0f, (float)y + 1.0f}});
					}
					if (!isOccluderAt(map, x, y - 1))
					{
						occluderSegments.push_back({{(float)x, (float)y}, {(float)x + 1.0f, (float)y}});
					}
					if (!isOccluderAt(map, x, y + 1))
					{
						occluderSegments.push_back({{(float)x, (float)y + 1.0f}, {(float)x + 1.0f, (float)y + 1.0f}});
					}
				}
			}
		}

		std::vector<float> rayAngles;
		rayAngles.reserve(256 + occluderSegments.size() * 6);

		const int baseRayCount = useHalfResolution ? 96 : 160;
		const float twoPi = glm::two_pi<float>();
		for (int i = 0; i < baseRayCount; i++)
		{
			float a = (twoPi * (float)i) / (float)baseRayCount;
			rayAngles.push_back(normalizeAngle(a));
		}

		if (light.castsShadows)
		{
			const float cornerRayEpsilon = 0.0009f;
			for (const auto &segment : occluderSegments)
			{
				glm::vec2 endpoints[2] = {segment.a, segment.b};
				for (const auto &endpoint : endpoints)
				{
					glm::vec2 toCorner = endpoint - light.position;
					float dist2 = glm::dot(toCorner, toCorner);
					if (dist2 > (radiusPad * radiusPad)) { continue; }

					float angle = std::atan2(toCorner.y, toCorner.x);
					rayAngles.push_back(normalizeAngle(angle - cornerRayEpsilon));
					rayAngles.push_back(normalizeAngle(angle));
					rayAngles.push_back(normalizeAngle(angle + cornerRayEpsilon));
				}
			}
		}

		std::sort(rayAngles.begin(), rayAngles.end());
		rayAngles.erase(std::unique(rayAngles.begin(), rayAngles.end(), [](float a, float b)
		{
			return std::abs(a - b) < 0.0001f;
		}), rayAngles.end());

		std::vector<RayHit> hits;
		hits.reserve(rayAngles.size());
		for (float angle : rayAngles)
		{
			glm::vec2 direction = {std::cos(angle), std::sin(angle)};
			float nearestDistance = radius;

			if (light.castsShadows)
			{
				for (const auto &segment : occluderSegments)
				{
					float t = 0.0f;
					if (!intersectRayWithSegment(light.position, direction, segment, t)) { continue; }
					if (t < nearestDistance)
					{
						nearestDistance = t;
					}
				}
			}

			RayHit hit = {};
			hit.angle = angle;
			hit.point = light.position + direction * nearestDistance;
			hits.push_back(hit);
		}

		if (hits.size() < 3) { continue; }

		std::sort(hits.begin(), hits.end(), [](const RayHit &a, const RayHit &b)
		{
			return a.angle < b.angle;
		});

		VisibilityPolygon polygon = {};
		polygon.light = light;
		polygon.points.reserve(hits.size());
		for (const auto &hit : hits)
		{
			if (!polygon.points.empty())
			{
				glm::vec2 d = hit.point - polygon.points.back();
				if (glm::dot(d, d) < 0.000001f)
				{
					continue;
				}
			}
			polygon.points.push_back(hit.point);
		}

		if (polygon.points.size() < 3) { continue; }

		glm::vec2 d = polygon.points.front() - polygon.points.back();
		if (glm::dot(d, d) < 0.000001f)
		{
			polygon.points.pop_back();
		}

		if (polygon.points.size() >= 3)
		{
			visibilityPolygons.push_back(std::move(polygon));
		}
	}
}

void CosmeticDynamicLightSystem::updateWindowMetrics(gl2d::Renderer2D &renderer)
{
	int divider = useHalfResolution ? 2 : 1;
	int width = std::max((renderer.windowW + divider - 1) / divider, 1);
	int height = std::max((renderer.windowH + divider - 1) / divider, 1);
	maskFbo.resize(width, height);
}

void CosmeticDynamicLightSystem::renderMask(gl2d::Renderer2D &renderer, Map &map)
{
	if (!maskFbo.texture.isValid()) { return; }
	if (mapSize != map.size)
	{
		resetForFloor(map);
	}
	if (mapSize.x <= 0 || mapSize.y <= 0) { return; }

	maskFbo.bind();

	float ambient = enabled ? std::max(ambientLight, 0.0f) : 0.0f;
	renderer.clearScreen({ambient, ambient, ambient, 1});
	if (ambient <= 0.0001f && visibilityPolygons.empty())
	{
		maskFbo.unbind();
		return;
	}

	auto oldBlend = renderer.getBlendMode();
	renderer.setBlendMode(gl2d::Renderer2D::BlendMode_Alpha);

	auto viewRect = renderer.getViewRect();
	int tileMinX = std::max(0, (int)std::floor(viewRect.x) - 2);
	int tileMinY = std::max(0, (int)std::floor(viewRect.y) - 2);
	int tileMaxX = std::min(mapSize.x, (int)std::ceil(viewRect.x + viewRect.z) + 2);
	int tileMaxY = std::min(mapSize.y, (int)std::ceil(viewRect.y + viewRect.w) + 2);

	// Keep walls and their top projection row neutral (no cosmetic light).
	for (int y = tileMinY; y < tileMaxY; y++)
	{
		for (int x = tileMinX; x < tileMaxX; x++)
		{
			if (!isWallAt(map, x, y)) { continue; }
			renderer.renderRectangle({(float)x, (float)y, 1.0f, 1.0f}, {0, 0, 0, 1});
			if (y > 0)
			{
				renderer.renderRectangle({(float)x, (float)y - 1.0f, 1.0f, 1.0f}, {0, 0, 0, 1});
			}
		}
	}

	renderer.setBlendMode(gl2d::Renderer2D::BlendMode_Additive);

	float sampleStep = PIXEL_SIZE * (useHalfResolution ? 2.0f : 1.0f);
	sampleStep = std::max(sampleStep, PIXEL_SIZE);

	// Fallback to simple radial sampling if polygon build yielded nothing.
	if (visibilityPolygons.empty() && !lights.empty())
	{
		for (const auto &light : lights)
		{
			float radius = std::max(light.radius, 0.05f);
			float radius2 = radius * radius;
			float falloffPower = std::max(light.falloffPower, 0.05f);

			float minX = std::max(viewRect.x - 1.0f, light.position.x - radius - sampleStep);
			float minY = std::max(viewRect.y - 1.0f, light.position.y - radius - sampleStep);
			float maxX = std::min(viewRect.x + viewRect.z + 1.0f, light.position.x + radius + sampleStep);
			float maxY = std::min(viewRect.y + viewRect.w + 1.0f, light.position.y + radius + sampleStep);

			float startX = std::floor(minX / sampleStep) * sampleStep;
			float startY = std::floor(minY / sampleStep) * sampleStep;

			for (float y = startY; y < maxY; y += sampleStep)
			{
				for (float x = startX; x < maxX; x += sampleStep)
				{
					glm::vec2 samplePos = {x + sampleStep * 0.5f, y + sampleStep * 0.5f};
					int tx = (int)std::floor(samplePos.x);
					int ty = (int)std::floor(samplePos.y);

					if (!inBounds(tx, ty)) { continue; }
					if (!hasTileAt(map, tx, ty)) { continue; }
					if (isWallAt(map, tx, ty) || isWallAt(map, tx, ty + 1)) { continue; }

					glm::vec2 diff = samplePos - light.position;
					float dist2 = glm::dot(diff, diff);
					if (dist2 > radius2) { continue; }

					float t = 1.0f - std::sqrt(dist2) / radius;
					t = std::clamp(t, 0.0f, 1.0f);
					float contribution = std::pow(t, falloffPower) * light.intensity;
					if (contribution <= 0.0005f) { continue; }

					renderer.renderRectangle({x, y, sampleStep, sampleStep},
						{contribution, contribution, contribution, 1.0f});
				}
			}
		}
	}

	for (const auto &polygon : visibilityPolygons)
	{
		if (polygon.points.size() < 3) { continue; }

		float radius = std::max(polygon.light.radius, 0.05f);
		float radius2 = radius * radius;
		float falloffPower = std::max(polygon.light.falloffPower, 0.05f);

		float minX = std::max(viewRect.x - 1.0f, polygon.light.position.x - radius - sampleStep);
		float minY = std::max(viewRect.y - 1.0f, polygon.light.position.y - radius - sampleStep);
		float maxX = std::min(viewRect.x + viewRect.z + 1.0f, polygon.light.position.x + radius + sampleStep);
		float maxY = std::min(viewRect.y + viewRect.w + 1.0f, polygon.light.position.y + radius + sampleStep);

		float startX = std::floor(minX / sampleStep) * sampleStep;
		float startY = std::floor(minY / sampleStep) * sampleStep;

		for (float y = startY; y < maxY; y += sampleStep)
		{
			for (float x = startX; x < maxX; x += sampleStep)
			{
				glm::vec2 samplePos = {x + sampleStep * 0.5f, y + sampleStep * 0.5f};
				int tx = (int)std::floor(samplePos.x);
				int ty = (int)std::floor(samplePos.y);

				if (!inBounds(tx, ty)) { continue; }
				if (!hasTileAt(map, tx, ty)) { continue; }
				if (isWallAt(map, tx, ty) || isWallAt(map, tx, ty + 1)) { continue; }

				if (polygon.light.castsShadows && !pointInPolygon(polygon.points, samplePos))
				{
					continue;
				}

				glm::vec2 diff = samplePos - polygon.light.position;
				float dist2 = glm::dot(diff, diff);
				if (dist2 > radius2) { continue; }

				float t = 1.0f - std::sqrt(dist2) / radius;
				t = std::clamp(t, 0.0f, 1.0f);
				float contribution = std::pow(t, falloffPower) * polygon.light.intensity;
				if (contribution <= 0.0005f) { continue; }

				renderer.renderRectangle({x, y, sampleStep, sampleStep},
					{contribution, contribution, contribution, 1.0f});
			}
		}
	}

	renderer.setBlendMode(oldBlend);
	maskFbo.unbind();
}
