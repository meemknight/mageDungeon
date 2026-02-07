#pragma once
#include <gl2d/gl2d.h>
#include <glm/vec2.hpp>

struct Map;
struct DoorHolder;
struct FloorInfo;
struct RoomLightingSystem;

// Renders a tiny map preview into a square framebuffer for UI display.
struct MinimapSystem
{
	gl2d::FrameBuffer fbo;

	int pixelSize = 48; // minimap framebuffer size in pixels (square)
	float viewSize = 48.f; // world tiles visible across the minimap
	float uiSize = 0.0f; // override UI size in pixels (0 = use default)
	float opacity = 0.8f; // final minimap opacity in UI

	gl2d::Color4f floorColor = {0.35f, 0.35f, 0.35f, 1.0f};
	gl2d::Color4f wallColor = {0.05f, 0.05f, 0.05f, 1.0f};
	gl2d::Color4f wallEdgeColor = {0.45f, 0.75f, 0.95f, 1.0f};
	gl2d::Color4f doorColor = {0.55f, 0.55f, 0.55f, 1.0f};
	gl2d::Color4f playerColor = {1.0f, 0.08f, 0.08f, 1.0f};
	gl2d::Color4f unknownDoorBoxColor = {0.25f, 0.25f, 0.25f, 1.0f};
	gl2d::Color4f unknownDoorMarkColor = {0.72f, 0.28f, 0.86f, 1.0f};
	float corridorDimFactor = 0.62f; // corridor floor/walls are slightly dimmer
	float playerMarkerSize = 0.9f; // world tile size for the player dot

	void init();
	void update(gl2d::Renderer2D &renderer, Map &map, const DoorHolder &doorHolder,
		glm::vec2 playerPos, const FloorInfo *floorInfo = nullptr,
		const RoomLightingSystem *lightingSystem = nullptr,
		const glm::vec2 *cameraCenterOverride = nullptr,
		const float *viewSizeOverride = nullptr);
	void render(gl2d::Renderer2D &renderer);
	// Draw minimap texture into an arbitrary UI rect (fullscreen map viewer uses this).
	void renderAt(gl2d::Renderer2D &renderer, glm::vec4 rect, float finalOpacity = -1.0f);
};
