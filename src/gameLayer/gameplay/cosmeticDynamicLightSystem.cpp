#include <gameplay/cosmeticDynamicLightSystem.h>

#include <gameplay/map.h>
#include <gameplay/blocks.h>
#include <gameplay/Physics.h>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <vector>

namespace
{
	struct OccluderSegment
	{
		glm::vec2 a = {};
		glm::vec2 b = {};
		float transmission = 0.0f;
	};

	struct RayHit
	{
		float angle = 0.0f;
		glm::vec2 point = {};
		float transmission = 1.0f;
		float transmissionStartDistance = 100000.0f;
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

	bool readBinaryFile(const char *path, std::vector<Uint8> &outData)
	{
		outData.clear();
		if (!path) { return false; }

		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open()) { return false; }

		auto size = file.tellg();
		if (size <= 0) { return false; }
		file.seekg(0, std::ios::beg);

		outData.resize(static_cast<size_t>(size));
		file.read(reinterpret_cast<char *>(outData.data()), size);
		return file.good();
	}

	glm::vec2 worldToClip(const glm::vec4 &viewRect, const glm::vec2 worldPos)
	{
		float viewW = std::max(viewRect.z, 0.0001f);
		float viewH = std::max(viewRect.w, 0.0001f);

		float nx = (worldPos.x - viewRect.x) / viewW;
		float ny = (worldPos.y - viewRect.y) / viewH;

		return {nx * 2.0f - 1.0f, 1.0f - ny * 2.0f};
	}

	#if GL2D_USE_SDL_GPU
	struct LightMaskVertex
	{
		float clipPos[2] = {};
		float worldPos[2] = {};
		float lightCenter[2] = {};
		float lightRadius = 1.0f;
		float falloffPower = 1.0f;
		float lightColor[3] = {1, 1, 1};
		float transmission = 1.0f;
		float transmissionStartDistance = 100000.0f;
		float padding = 0.0f;
	};

	SDL_GPUGraphicsPipeline *createMaskPipeline(SDL_GPUDevice *device,
		SDL_GPUShader *vertexShader,
		SDL_GPUShader *fragmentShader,
		SDL_GPUTextureFormat targetFormat)
	{
		if (!device || !vertexShader || !fragmentShader)
		{
			return nullptr;
		}

		SDL_GPUVertexBufferDescription vertexBufferDesc[1] = {};
		vertexBufferDesc[0].slot = 0;
		vertexBufferDesc[0].pitch = sizeof(LightMaskVertex);
		vertexBufferDesc[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
		vertexBufferDesc[0].instance_step_rate = 0;

		SDL_GPUVertexAttribute vertexAttributes[8] = {};
		vertexAttributes[0].location = 0;
		vertexAttributes[0].buffer_slot = 0;
		vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		vertexAttributes[0].offset = offsetof(LightMaskVertex, clipPos);

		vertexAttributes[1].location = 1;
		vertexAttributes[1].buffer_slot = 0;
		vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		vertexAttributes[1].offset = offsetof(LightMaskVertex, worldPos);

		vertexAttributes[2].location = 2;
		vertexAttributes[2].buffer_slot = 0;
		vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		vertexAttributes[2].offset = offsetof(LightMaskVertex, lightCenter);

		vertexAttributes[3].location = 3;
		vertexAttributes[3].buffer_slot = 0;
		vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
		vertexAttributes[3].offset = offsetof(LightMaskVertex, lightRadius);

		vertexAttributes[4].location = 4;
		vertexAttributes[4].buffer_slot = 0;
		vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
		vertexAttributes[4].offset = offsetof(LightMaskVertex, falloffPower);

		vertexAttributes[5].location = 5;
		vertexAttributes[5].buffer_slot = 0;
		vertexAttributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
		vertexAttributes[5].offset = offsetof(LightMaskVertex, lightColor);

		vertexAttributes[6].location = 6;
		vertexAttributes[6].buffer_slot = 0;
		vertexAttributes[6].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
		vertexAttributes[6].offset = offsetof(LightMaskVertex, transmission);

		vertexAttributes[7].location = 7;
		vertexAttributes[7].buffer_slot = 0;
		vertexAttributes[7].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
		vertexAttributes[7].offset = offsetof(LightMaskVertex, transmissionStartDistance);

		SDL_GPUVertexInputState vertexInput = {};
		vertexInput.vertex_buffer_descriptions = vertexBufferDesc;
		vertexInput.num_vertex_buffers = 1;
		vertexInput.vertex_attributes = vertexAttributes;
		vertexInput.num_vertex_attributes = 8;

		SDL_GPURasterizerState rasterizer = {};
		rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
		rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
		rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
		rasterizer.enable_depth_clip = false;

		SDL_GPUMultisampleState multisample = {};
		multisample.sample_count = SDL_GPU_SAMPLECOUNT_1;

		SDL_GPUDepthStencilState depthStencil = {};
		depthStencil.enable_depth_test = false;
		depthStencil.enable_depth_write = false;
		depthStencil.enable_stencil_test = false;

		SDL_GPUColorTargetBlendState blend = {};
		blend.enable_blend = true;
		blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
		blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
		blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
		blend.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
			SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

		SDL_GPUColorTargetDescription targetDesc = {};
		targetDesc.format = targetFormat;
		targetDesc.blend_state = blend;

		SDL_GPUGraphicsPipelineTargetInfo targetInfo = {};
		targetInfo.num_color_targets = 1;
		targetInfo.color_target_descriptions = &targetDesc;
		targetInfo.has_depth_stencil_target = false;

		SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.vertex_shader = vertexShader;
		pipelineInfo.fragment_shader = fragmentShader;
		pipelineInfo.vertex_input_state = vertexInput;
		pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineInfo.rasterizer_state = rasterizer;
		pipelineInfo.multisample_state = multisample;
		pipelineInfo.depth_stencil_state = depthStencil;
		pipelineInfo.target_info = targetInfo;

		return SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
	}
	#endif
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
	if (!map.isCollidableAtPosSafe(x, y)) { return false; }

	// Ignore the visual top-of-wall projection row as a shadow caster.
	if (!isWallAt(map, x, y) && isWallAt(map, x, y + 1))
	{
		return false;
	}

	return true;
}

bool CosmeticDynamicLightSystem::hasBreakableDecorationAt(int x, int y) const
{
	if (!inBounds(x, y)) { return false; }
	if (breakableDecorationMask.empty()) { return false; }
	return breakableDecorationMask[x + y * mapSize.x] != 0;
}

void CosmeticDynamicLightSystem::init()
{
	*this = {};
	#if GL2D_USE_SDL_GPU
	// Light mask is HDR so multiple colored lights can overlap additively.
	maskFbo.gpuTextureFormat = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
	#endif
	maskFbo.create(1, 1, true);
}

void CosmeticDynamicLightSystem::cleanup()
{
	#if GL2D_USE_SDL_GPU
	releaseGpuResources();
	#endif
	maskFbo.cleanup();
	*this = {};
}

void CosmeticDynamicLightSystem::resetForFloor(Map &map)
{
	mapSize = map.size;
	lights.clear();
	visibilityPolygons.clear();
	breakableDecorationMask.assign(std::max(0, mapSize.x * mapSize.y), 0);
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

void CosmeticDynamicLightSystem::setBreakableDecorations(const std::vector<glm::ivec2> &positions)
{
	if (mapSize.x <= 0 || mapSize.y <= 0) { return; }

	if (breakableDecorationMask.size() != (size_t)std::max(0, mapSize.x * mapSize.y))
	{
		breakableDecorationMask.assign(std::max(0, mapSize.x * mapSize.y), 0);
	}

	std::fill(breakableDecorationMask.begin(), breakableDecorationMask.end(), 0);
	for (const auto &pos : positions)
	{
		if (!inBounds(pos.x, pos.y)) { continue; }
		breakableDecorationMask[pos.x + pos.y * mapSize.x] = 1;
	}
}

void CosmeticDynamicLightSystem::addLight(glm::vec2 position, float radius, float intensity,
	float falloffPower, bool castsShadows, glm::vec3 color, bool forceShadowCasting)
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
	light.forceShadowCasting = forceShadowCasting;
	light.color = glm::max(color, glm::vec3(0.0f));
	lights.push_back(light);
}

void CosmeticDynamicLightSystem::buildLightMask(Map &map)
{
	visibilityPolygons.clear();
	if (mapSize.x <= 0 || mapSize.y <= 0) { return; }
	if (!enabled) { return; }
	if (lights.empty()) { return; }

	// Keep heavy shadow ray casting bounded for projectile storms.
	int shadowCastersUsed = 0;
	constexpr int shadowCasterLimit = 25;

	for (const auto &sourceLight : lights)
	{
		CosmeticDynamicLight light = sourceLight;

		if (light.castsShadows)
		{
			if (shadowCastersUsed >= shadowCasterLimit && !light.forceShadowCasting)
			{
				light.castsShadows = false;
			}

			if (light.castsShadows)
			{
				shadowCastersUsed++;
			}
		}

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
			constexpr float breakableDecorationTransmission = 0.5f;

			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					if (!isOccluderAt(map, x, y)) { continue; }

					if (!isOccluderAt(map, x - 1, y))
					{
						occluderSegments.push_back({{(float)x, (float)y}, {(float)x, (float)y + 1.0f}, 0.0f});
					}
					if (!isOccluderAt(map, x + 1, y))
					{
						occluderSegments.push_back({{(float)x + 1.0f, (float)y}, {(float)x + 1.0f, (float)y + 1.0f}, 0.0f});
					}
					if (!isOccluderAt(map, x, y - 1))
					{
						occluderSegments.push_back({{(float)x, (float)y}, {(float)x + 1.0f, (float)y}, 0.0f});
					}
					if (!isOccluderAt(map, x, y + 1))
					{
						occluderSegments.push_back({{(float)x, (float)y + 1.0f}, {(float)x + 1.0f, (float)y + 1.0f}, 0.0f});
					}
				}
			}

			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					if (!hasBreakableDecorationAt(x, y)) { continue; }

					if (!hasBreakableDecorationAt(x - 1, y))
					{
						occluderSegments.push_back({{(float)x, (float)y}, {(float)x, (float)y + 1.0f},
							breakableDecorationTransmission});
					}
					if (!hasBreakableDecorationAt(x + 1, y))
					{
						occluderSegments.push_back({{(float)x + 1.0f, (float)y}, {(float)x + 1.0f, (float)y + 1.0f},
							breakableDecorationTransmission});
					}
					if (!hasBreakableDecorationAt(x, y - 1))
					{
						occluderSegments.push_back({{(float)x, (float)y}, {(float)x + 1.0f, (float)y},
							breakableDecorationTransmission});
					}
					if (!hasBreakableDecorationAt(x, y + 1))
					{
						occluderSegments.push_back({{(float)x, (float)y + 1.0f}, {(float)x + 1.0f, (float)y + 1.0f},
							breakableDecorationTransmission});
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
			float transmission = 1.0f;
			float transmissionStartDistance = radius + 1.0f;

			if (light.castsShadows)
			{
				for (const auto &segment : occluderSegments)
				{
					float t = 0.0f;
					if (!intersectRayWithSegment(light.position, direction, segment, t)) { continue; }

					if (segment.transmission <= 0.001f)
					{
						if (t < nearestDistance)
						{
							nearestDistance = t;
						}
					}
					else
					{
						// Breakable decorations are partial occluders: keep at least 50% light.
						if (t <= nearestDistance + 0.0001f)
						{
							transmission = std::min(transmission, segment.transmission);
							transmissionStartDistance = std::min(transmissionStartDistance, t);
						}
					}
				}
			}

			RayHit hit = {};
			hit.angle = angle;
			hit.point = light.position + direction * nearestDistance;
			hit.transmission = transmission;
			hit.transmissionStartDistance = transmissionStartDistance;
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
				glm::vec2 d = hit.point - polygon.points.back().point;
				if (glm::dot(d, d) < 0.000001f)
				{
					polygon.points.back().transmission = std::min(polygon.points.back().transmission,
						hit.transmission);
					polygon.points.back().transmissionStartDistance = std::min(
						polygon.points.back().transmissionStartDistance,
						hit.transmissionStartDistance);
					continue;
				}
			}

			VisibilityPoint point = {};
			point.point = hit.point;
			point.transmission = hit.transmission;
			point.transmissionStartDistance = hit.transmissionStartDistance;
			polygon.points.push_back(point);
		}

		if (polygon.points.size() < 3) { continue; }

		glm::vec2 d = polygon.points.front().point - polygon.points.back().point;
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
	// Match particle post-process sizing so light mask aligns with world pixel grid.
	int pixelateFactor = (PIXEL_SIZE * renderer.currentCamera.zoom);
	pixelateFactor = std::max(pixelateFactor, 2);
	if (useHalfResolution)
	{
		pixelateFactor *= 2;
	}

	int width = std::max((renderer.windowW + pixelateFactor - 1) / pixelateFactor, 1);
	int height = std::max((renderer.windowH + pixelateFactor - 1) / pixelateFactor, 1);
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

	float ambient = enabled ? std::max(ambientLight, 0.0f) : 0.0f;

	#if GL2D_USE_SDL_GPU
	if (renderer.gpuDevice && maskFbo.texture.gpuTexture)
	{
		std::vector<LightMaskVertex> vertices;
		vertices.reserve(4096);

		auto viewRect = renderer.getViewRect();
		const float geometryExtension = PIXEL_SIZE * 8.0f;

		auto appendFan = [&](const CosmeticDynamicLight &light,
			const std::vector<VisibilityPoint> &points)
		{
			if (points.size() < 3) { return; }

			glm::vec3 lightColor = glm::max(light.color, glm::vec3(0.0f)) * std::max(light.intensity, 0.0f);
			float radius = std::max(light.radius, 0.05f);
			float falloff = std::max(light.falloffPower, 0.05f);
			float edgeExpand = PIXEL_SIZE * 0.75f;
			float noTransmissionStart = radius + geometryExtension;

			auto appendTriangle = [&](glm::vec2 a, glm::vec2 b, glm::vec2 c,
				float transmissionA, float transmissionB, float transmissionC,
				float startA, float startB, float startC)
			{
				LightMaskVertex tri[3] = {};

				glm::vec2 clipA = worldToClip(viewRect, a);
				tri[0].clipPos[0] = clipA.x;
				tri[0].clipPos[1] = clipA.y;
				tri[0].worldPos[0] = a.x;
				tri[0].worldPos[1] = a.y;

				glm::vec2 clipB = worldToClip(viewRect, b);
				tri[1].clipPos[0] = clipB.x;
				tri[1].clipPos[1] = clipB.y;
				tri[1].worldPos[0] = b.x;
				tri[1].worldPos[1] = b.y;

				glm::vec2 clipC = worldToClip(viewRect, c);
				tri[2].clipPos[0] = clipC.x;
				tri[2].clipPos[1] = clipC.y;
				tri[2].worldPos[0] = c.x;
				tri[2].worldPos[1] = c.y;

				for (auto &v : tri)
				{
					v.lightCenter[0] = light.position.x;
					v.lightCenter[1] = light.position.y;
					v.lightRadius = radius;
					v.falloffPower = falloff;
					v.lightColor[0] = lightColor.x;
					v.lightColor[1] = lightColor.y;
					v.lightColor[2] = lightColor.z;
				}

				tri[0].transmission = std::clamp(transmissionA, 0.0f, 1.0f);
				tri[1].transmission = std::clamp(transmissionB, 0.0f, 1.0f);
				tri[2].transmission = std::clamp(transmissionC, 0.0f, 1.0f);
				tri[0].transmissionStartDistance = std::max(startA, 0.0f);
				tri[1].transmissionStartDistance = std::max(startB, 0.0f);
				tri[2].transmissionStartDistance = std::max(startC, 0.0f);

				vertices.push_back(tri[0]);
				vertices.push_back(tri[1]);
				vertices.push_back(tri[2]);
			};

			for (size_t i = 0; i < points.size(); i++)
			{
				const VisibilityPoint &p1Info = points[i];
				const VisibilityPoint &p2Info = points[(i + 1) % points.size()];
				const glm::vec2 &p1raw = p1Info.point;
				const glm::vec2 &p2raw = p2Info.point;

				glm::vec2 d1 = p1raw - light.position;
				glm::vec2 d2 = p2raw - light.position;
				float l1 = glm::length(d1);
				float l2 = glm::length(d2);
				if (l1 <= 0.0001f || l2 <= 0.0001f) { continue; }

				// Preserve occluder silhouette: blocked rays stay near blockers,
				// open rays expand a bit beyond radius so falloff clipping happens in shader.
				float p1Distance = l1 + edgeExpand;
				float p2Distance = l2 + edgeExpand;
				if (l1 >= radius - edgeExpand)
				{
					p1Distance = radius + geometryExtension;
				}
				if (l2 >= radius - edgeExpand)
				{
					p2Distance = radius + geometryExtension;
				}

				glm::vec2 p1 = light.position + d1 * (p1Distance / l1);
				glm::vec2 p2 = light.position + d2 * (p2Distance / l2);

				float trans1 = std::clamp(p1Info.transmission, 0.0f, 1.0f);
				float trans2 = std::clamp(p2Info.transmission, 0.0f, 1.0f);
				bool hasPartialTransmission = (trans1 < 0.999f) || (trans2 < 0.999f);

				if (!hasPartialTransmission)
				{
					appendTriangle(light.position, p1, p2,
						1.0f, 1.0f, 1.0f,
						noTransmissionStart, noTransmissionStart, noTransmissionStart);
					continue;
				}

				// Split transmitted wedges into an inner full-light section and an outer attenuated section.
				float start1 = std::clamp(p1Info.transmissionStartDistance, 0.0f, p1Distance);
				float start2 = std::clamp(p2Info.transmissionStartDistance, 0.0f, p2Distance);

				glm::vec2 n1 = light.position + d1 * (start1 / l1);
				glm::vec2 n2 = light.position + d2 * (start2 / l2);

				appendTriangle(light.position, n1, n2,
					1.0f, 1.0f, 1.0f,
					noTransmissionStart, noTransmissionStart, noTransmissionStart);

				appendTriangle(n1, p1, p2,
					1.0f, trans1, trans2,
					start1, start1, start2);

				appendTriangle(n1, p2, n2,
					1.0f, trans2, 1.0f,
					start1, start2, start2);
			}
		};

		for (const auto &polygon : visibilityPolygons)
		{
			appendFan(polygon.light, polygon.points);
		}

		if (vertices.empty())
		{
			const int fallbackSegments = useHalfResolution ? 56 : 96;
			for (const auto &light : lights)
			{
				std::vector<VisibilityPoint> circlePoints;
				circlePoints.reserve(fallbackSegments);
				for (int i = 0; i < fallbackSegments; i++)
				{
					float angle = (glm::two_pi<float>() * (float)i) / (float)fallbackSegments;
					glm::vec2 dir = {std::cos(angle), std::sin(angle)};
					VisibilityPoint point = {};
					point.point = light.position + dir * std::max(light.radius, 0.05f);
					point.transmission = 1.0f;
					circlePoints.push_back(point);
				}
				appendFan(light, circlePoints);
			}
		}

		if (maskFbo.texture.gpuTexture && ensureResources(renderer))
		{
			// Custom GPU pass writes straight into maskFbo and does not need batched sprite flush.

			uint32_t uploadBytes = static_cast<uint32_t>(vertices.size() * sizeof(LightMaskVertex));
			bool canUpload = uploadBytes == 0 || ensureVertexBufferCapacity(renderer, uploadBytes);

			if (canUpload)
			{
				if (uploadBytes > 0)
				{
					void *mapped = SDL_MapGPUTransferBuffer(renderer.gpuDevice, vertexTransferBuffer, true);
					if (mapped)
					{
						std::memcpy(mapped, vertices.data(), uploadBytes);
						SDL_UnmapGPUTransferBuffer(renderer.gpuDevice, vertexTransferBuffer);
					}
					else
					{
						uploadBytes = 0;
					}
				}

				SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(renderer.gpuDevice);
				if (commandBuffer)
				{
					if (uploadBytes > 0)
					{
						SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
						if (copyPass)
						{
							SDL_GPUTransferBufferLocation src = {};
							src.transfer_buffer = vertexTransferBuffer;
							src.offset = 0;

							SDL_GPUBufferRegion dst = {};
							dst.buffer = vertexBuffer;
							dst.offset = 0;
							dst.size = uploadBytes;

							SDL_UploadToGPUBuffer(copyPass, &src, &dst, true);
							SDL_EndGPUCopyPass(copyPass);
						}
					}

					SDL_GPUColorTargetInfo colorTarget = {};
					colorTarget.texture = maskFbo.texture.gpuTexture;
					colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
					colorTarget.store_op = SDL_GPU_STOREOP_STORE;
					colorTarget.clear_color = {ambient, ambient, ambient, 1.0f};

					SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
					if (renderPass)
					{
						SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

						SDL_GPUViewport viewport = {};
						viewport.x = 0;
						viewport.y = 0;
						viewport.w = static_cast<float>(maskFbo.w);
						viewport.h = static_cast<float>(maskFbo.h);
						viewport.min_depth = 0.0f;
						viewport.max_depth = 1.0f;
						SDL_SetGPUViewport(renderPass, &viewport);

						SDL_Rect scissor = {0, 0, maskFbo.w, maskFbo.h};
						SDL_SetGPUScissor(renderPass, &scissor);

						if (uploadBytes > 0)
						{
							SDL_GPUBufferBinding vertexBinding = {};
							vertexBinding.buffer = vertexBuffer;
							vertexBinding.offset = 0;
							SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
							SDL_DrawGPUPrimitives(renderPass, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
						}

						SDL_EndGPURenderPass(renderPass);
						SDL_SubmitGPUCommandBuffer(commandBuffer);
					}
					else
					{
						SDL_CancelGPUCommandBuffer(commandBuffer);
					}
				}
			}
		}
		else
		{
			maskFbo.bind();
			renderer.clearScreen({ambient, ambient, ambient, 1.0f});
			maskFbo.unbind();
		}

		// Walls are excluded from cosmetic lighting by writing black after the additive pass.
		maskFbo.bind();
		auto oldBlend = renderer.getBlendMode();
		renderer.setBlendMode(gl2d::Renderer2D::BlendMode_Alpha);

		int tileMinX = std::max(0, (int)std::floor(viewRect.x) - 2);
		int tileMinY = std::max(0, (int)std::floor(viewRect.y) - 2);
		int tileMaxX = std::min(mapSize.x, (int)std::ceil(viewRect.x + viewRect.z) + 2);
		int tileMaxY = std::min(mapSize.y, (int)std::ceil(viewRect.y + viewRect.w) + 2);

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

		renderer.setBlendMode(oldBlend);
		maskFbo.unbind();
		return;
	}
	#endif

	maskFbo.bind();
	renderer.clearScreen({ambient, ambient, ambient, 1.0f});
	maskFbo.unbind();
}

#if GL2D_USE_SDL_GPU
bool CosmeticDynamicLightSystem::ensureVertexBufferCapacity(gl2d::Renderer2D &renderer, uint32_t requiredBytes)
{
	if (!renderer.gpuDevice) { return false; }
	if (requiredBytes == 0) { return true; }

	if (vertexBuffer && vertexTransferBuffer && vertexBufferSize >= requiredBytes)
	{
		return true;
	}

	if (vertexBuffer)
	{
		SDL_ReleaseGPUBuffer(renderer.gpuDevice, vertexBuffer);
		vertexBuffer = nullptr;
	}
	if (vertexTransferBuffer)
	{
		SDL_ReleaseGPUTransferBuffer(renderer.gpuDevice, vertexTransferBuffer);
		vertexTransferBuffer = nullptr;
	}

	vertexBufferSize = std::max(requiredBytes, static_cast<uint32_t>(64 * 1024));

	SDL_GPUBufferCreateInfo bufferInfo = {};
	bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
	bufferInfo.size = vertexBufferSize;
	vertexBuffer = SDL_CreateGPUBuffer(renderer.gpuDevice, &bufferInfo);
	if (!vertexBuffer)
	{
		return false;
	}

	SDL_GPUTransferBufferCreateInfo transferInfo = {};
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferInfo.size = vertexBufferSize;
	vertexTransferBuffer = SDL_CreateGPUTransferBuffer(renderer.gpuDevice, &transferInfo);
	if (!vertexTransferBuffer)
	{
		SDL_ReleaseGPUBuffer(renderer.gpuDevice, vertexBuffer);
		vertexBuffer = nullptr;
		return false;
	}

	return true;
}

bool CosmeticDynamicLightSystem::ensureResources(gl2d::Renderer2D &renderer)
{
	if (!renderer.gpuDevice)
	{
		return false;
	}

	SDL_GPUTextureFormat targetFormat = maskFbo.texture.gpuFormat;
	if (targetFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
	{
		targetFormat = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
	}

	if (pipeline && vertexShader && fragmentShader && pipelineFormat == targetFormat)
	{
		return true;
	}

	const char *driver = SDL_GetGPUDeviceDriver(renderer.gpuDevice);
	if (!driver || strcmp(driver, "vulkan") != 0)
	{
		return false;
	}

	std::vector<Uint8> vertexCode;
	std::vector<Uint8> fragmentCode;
	if (!readBinaryFile(RESOURCES_PATH "shaders/cosmetic_light_mask.vert.spv", vertexCode)
		|| !readBinaryFile(RESOURCES_PATH "shaders/cosmetic_light_mask.frag.spv", fragmentCode))
	{
		return false;
	}

	if (!vertexShader)
	{
		SDL_GPUShaderCreateInfo vertexInfo = {};
		vertexInfo.code_size = vertexCode.size();
		vertexInfo.code = vertexCode.data();
		vertexInfo.entrypoint = "main";
		vertexInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
		vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
		vertexInfo.num_samplers = 0;
		vertexInfo.num_storage_textures = 0;
		vertexInfo.num_storage_buffers = 0;
		vertexInfo.num_uniform_buffers = 0;
		vertexShader = SDL_CreateGPUShader(renderer.gpuDevice, &vertexInfo);
	}

	if (!fragmentShader)
	{
		SDL_GPUShaderCreateInfo fragmentInfo = {};
		fragmentInfo.code_size = fragmentCode.size();
		fragmentInfo.code = fragmentCode.data();
		fragmentInfo.entrypoint = "main";
		fragmentInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
		fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
		fragmentInfo.num_samplers = 0;
		fragmentInfo.num_storage_textures = 0;
		fragmentInfo.num_storage_buffers = 0;
		fragmentInfo.num_uniform_buffers = 0;
		fragmentShader = SDL_CreateGPUShader(renderer.gpuDevice, &fragmentInfo);
	}

	if (!pipeline || pipelineFormat != targetFormat)
	{
		if (pipeline)
		{
			SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, pipeline);
			pipeline = nullptr;
		}

		pipeline = createMaskPipeline(renderer.gpuDevice, vertexShader, fragmentShader, targetFormat);
		pipelineFormat = targetFormat;
	}

	return pipeline && vertexShader && fragmentShader;
}

void CosmeticDynamicLightSystem::releaseGpuResources()
{
	SDL_GPUDevice *device = maskFbo.texture.gpuDevice;
	if (!device)
	{
		vertexShader = nullptr;
		fragmentShader = nullptr;
		pipeline = nullptr;
		vertexBuffer = nullptr;
		vertexTransferBuffer = nullptr;
		vertexBufferSize = 0;
		pipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
		return;
	}

	if (pipeline)
	{
		SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
		pipeline = nullptr;
	}

	if (vertexShader)
	{
		SDL_ReleaseGPUShader(device, vertexShader);
		vertexShader = nullptr;
	}

	if (fragmentShader)
	{
		SDL_ReleaseGPUShader(device, fragmentShader);
		fragmentShader = nullptr;
	}

	if (vertexBuffer)
	{
		SDL_ReleaseGPUBuffer(device, vertexBuffer);
		vertexBuffer = nullptr;
	}

	if (vertexTransferBuffer)
	{
		SDL_ReleaseGPUTransferBuffer(device, vertexTransferBuffer);
		vertexTransferBuffer = nullptr;
	}

	vertexBufferSize = 0;
	pipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
}
#endif
