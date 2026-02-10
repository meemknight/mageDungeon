#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <gl2d/gl2d.h>

struct Map;

// Runtime point light used by the cosmetic dynamic light pass.
struct CosmeticDynamicLight
{
	glm::vec2 position = {};
	float radius = 9.0f;
	float intensity = 1.0f;
	float falloffPower = 1.6f;
	glm::vec3 color = {1.0f, 1.0f, 1.0f};
	bool castsShadows = true;
	bool forceShadowCasting = false;
};

// Shared defaults used by projectile lights unless a projectile overrides behavior.
struct CosmeticProjectileLightDefaults
{
	bool enabled = true;
	float radius = 4.0f;
	float intensity = 1.0f;
	float falloffPower = 1.4f;
	float colorScale = 0.7f;
	bool castsShadows = true;
};

// Cosmetic-only LOS based lighting mask (does not affect gameplay visibility/minimap).
struct CosmeticDynamicLightSystem
{
	bool enabled = true;
	// Extra global light added on top of the normal world brightness.
	float ambientLight = 0.0f;
	float playerLightRadius = 12.0f;
	float playerLightIntensity = 0.54f; 
	float playerLightFalloffPower = 0.4f; //lower means stronger light at the edges
	// Extra downscale on top of camera pixel-scale mask sizing.
	bool useHalfResolution = true;
	CosmeticProjectileLightDefaults projectileLightDefaults;

	gl2d::FrameBuffer maskFbo;

	void init();
	void cleanup();
	void resetForFloor(Map &map);

	void beginFrame(Map &map);
	void addLight(glm::vec2 position, float radius, float intensity,
		float falloffPower = 1.6f, bool castsShadows = true,
		glm::vec3 color = {1.0f, 1.0f, 1.0f}, bool forceShadowCasting = false);
	void buildLightMask(Map &map);

	void updateWindowMetrics(gl2d::Renderer2D &renderer);
	void renderMask(gl2d::Renderer2D &renderer, Map &map);
	gl2d::Texture getMaskTexture() const { return maskFbo.texture; }

	private:
	struct VisibilityPolygon
	{
		CosmeticDynamicLight light = {};
		std::vector<glm::vec2> points;
	};

	glm::ivec2 mapSize = {};
	std::vector<CosmeticDynamicLight> lights;
	std::vector<VisibilityPolygon> visibilityPolygons;

	bool inBounds(int x, int y) const;
	bool hasTileAt(Map &map, int x, int y) const;
	bool isWallAt(Map &map, int x, int y) const;
	bool isOccluderAt(Map &map, int x, int y) const;

	#if GL2D_USE_SDL_GPU
		SDL_GPUShader *vertexShader = nullptr;
		SDL_GPUShader *fragmentShader = nullptr;
		SDL_GPUGraphicsPipeline *pipeline = nullptr;
		SDL_GPUBuffer *vertexBuffer = nullptr;
		SDL_GPUTransferBuffer *vertexTransferBuffer = nullptr;
		uint32_t vertexBufferSize = 0;
		SDL_GPUTextureFormat pipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

		bool ensureResources(gl2d::Renderer2D &renderer);
		bool ensureVertexBufferCapacity(gl2d::Renderer2D &renderer, uint32_t requiredBytes);
		void releaseGpuResources();
	#endif
};
