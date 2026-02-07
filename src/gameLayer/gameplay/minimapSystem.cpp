#include <gameplay/minimapSystem.h>
#include <gameplay/map.h>
#include <gameplay/doors.h>
#include <gameplay/blocks.h>
#include <gameplay/Physics.h>
#include <gameplay/roomLightingSystem.h>
#include <worldGen/floorGen.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <unordered_set>

namespace
{
	gl2d::Color4f dimColor(gl2d::Color4f color, float factor)
	{
		color.r *= factor;
		color.g *= factor;
		color.b *= factor;
		return color;
	}

	bool isWallAt(Map &map, int x, int y)
	{
		if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y)
		{
			return false;
		}
		auto base = map.firstLayer.getBlockUnsafe(x, y).type;
		auto over = map.secondLayer.getBlockUnsafe(x, y).type;
		return isWall(base) || isWall(over);
	}

	bool isSolidAt(Map &map, int x, int y)
	{
		if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y)
		{
			return false;
		}
		auto base = map.firstLayer.getBlockUnsafe(x, y).type;
		auto over = map.secondLayer.getBlockUnsafe(x, y).type;
		return isBlockColidable(base) || isBlockColidable(over);
	}

	bool hasTile(Map &map, int x, int y)
	{
		if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y)
		{
			return false;
		}
		auto base = map.firstLayer.getBlockUnsafe(x, y).type;
		auto over = map.secondLayer.getBlockUnsafe(x, y).type;
		return base != Blocks::none || over != Blocks::none;
	}

	bool isFloorAt(Map &map, int x, int y)
	{
		if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y)
		{
			return false;
		}
		auto base = map.firstLayer.getBlockUnsafe(x, y).type;
		auto over = map.secondLayer.getBlockUnsafe(x, y).type;
		bool hasTile = base != Blocks::none || over != Blocks::none;
		return hasTile && !isSolidAt(map, x, y);
	}

	// Corridor floor is a walkable tile constrained by walls on one axis.
	bool isCorridorFloorAt(Map &map, int x, int y)
	{
		if (!isFloorAt(map, x, y))
		{
			return false;
		}

		bool wallsLeftRight = isSolidAt(map, x - 1, y) && isSolidAt(map, x + 1, y);
		bool wallsTopBottom = isSolidAt(map, x, y - 1) && isSolidAt(map, x, y + 1);
		return wallsLeftRight || wallsTopBottom;
	}

	bool isInGeneratedRoom(const FloorInfo *floorInfo, int x, int y)
	{
		if (!floorInfo) { return false; }
		for (const auto &room : floorInfo->rooms)
		{
			if (x >= room.pos.x && x < room.pos.x + room.size.x
				&& y >= room.pos.y && y < room.pos.y + room.size.y)
			{
				return true;
			}
		}
		return false;
	}

	bool isCorridorFloorFromFloorInfo(Map &map, const FloorInfo *floorInfo, int x, int y)
	{
		if (!floorInfo) { return isCorridorFloorAt(map, x, y); }
		return isFloorAt(map, x, y) && !isInGeneratedRoom(floorInfo, x, y);
	}

	bool isCorridorWallAt(Map &map, const FloorInfo *floorInfo, int x, int y)
	{
		if (!isWallAt(map, x, y))
		{
			return false;
		}

		return isCorridorFloorFromFloorInfo(map, floorInfo, x - 1, y)
			|| isCorridorFloorFromFloorInfo(map, floorInfo, x + 1, y)
			|| isCorridorFloorFromFloorInfo(map, floorInfo, x, y - 1)
			|| isCorridorFloorFromFloorInfo(map, floorInfo, x, y + 1);
	}

	void renderUnknownDoorMarker(gl2d::Renderer2D &renderer, glm::vec2 center,
		gl2d::Color4f boxColor, gl2d::Color4f markColor, float pixelSize = 1.0f)
	{
		// Editable 5x5 marker mask (0 = empty, 1 = colored pixel).
		// Change this array to tweak the question mark pixel art quickly.
		static const unsigned char markerMask[25] = {
			0, 1, 1, 1, 0,
			0, 0, 0, 1, 0,
			0, 0, 1, 0, 0,
			0, 0, 0, 0, 0,
			0, 0, 1, 0, 0
		};

		constexpr int markerSize = 5;

		pixelSize = std::max(0.001f, pixelSize);
		float markerWorldSize = markerSize * pixelSize;
		// Snap to tile grid so marker pixels align with map tiles.
		float snappedCenterX = std::floor(center.x / pixelSize) * pixelSize + pixelSize * 0.5f;
		float snappedCenterY = std::floor(center.y / pixelSize) * pixelSize + pixelSize * 0.5f;
		float left = snappedCenterX - markerWorldSize * 0.5f;
		float top = snappedCenterY - markerWorldSize * 0.5f;

		for (int py = 0; py < markerSize; py++)
		{
			for (int px = 0; px < markerSize; px++)
			{
				renderer.renderRectangle({left + px * pixelSize, top + py * pixelSize,
					pixelSize, pixelSize}, boxColor);
			}
		}

		for (int py = 0; py < markerSize; py++)
		{
			for (int px = 0; px < markerSize; px++)
			{
				int idx = px + py * markerSize;
				if (!markerMask[idx]) { continue; }
				renderer.renderRectangle({left + px * pixelSize, top + py * pixelSize,
					pixelSize, pixelSize}, markColor);
			}
		}

		// Border around the marker (outside the 5x5 content area).
		float borderLeft = left - pixelSize;
		float borderTop = top - pixelSize;
		float borderRight = left + markerSize * pixelSize;
		float borderBottom = top + markerSize * pixelSize;
		for (int i = 0; i <= markerSize + 1; i++)
		{
			float x = borderLeft + i * pixelSize;
			renderer.renderRectangle({x, borderTop, pixelSize, pixelSize}, boxColor);
			renderer.renderRectangle({x, borderBottom, pixelSize, pixelSize}, boxColor);
		}
		for (int i = 0; i <= markerSize + 1; i++)
		{
			float y = borderTop + i * pixelSize;
			renderer.renderRectangle({borderLeft, y, pixelSize, pixelSize}, boxColor);
			renderer.renderRectangle({borderRight, y, pixelSize, pixelSize}, boxColor);
		}
	}
}

void MinimapSystem::init()
{
	int size = std::max(1, pixelSize);
	fbo.create(size, size, true);
}

void MinimapSystem::update(gl2d::Renderer2D &renderer, Map &map, const DoorHolder &doorHolder,
	glm::vec2 playerPos, const FloorInfo *floorInfo,
	const RoomLightingSystem *lightingSystem,
	const glm::vec2 *cameraCenterOverride,
	const float *viewSizeOverride)
{
	float activeViewSize = viewSizeOverride ? *viewSizeOverride : viewSize;
	if (pixelSize <= 0 || activeViewSize <= 0.0f) { return; }

	if (fbo.w <= 0 || fbo.h <= 0)
	{
		fbo.create(pixelSize, pixelSize, true);
	}
	else if (fbo.w != pixelSize || fbo.h != pixelSize)
	{
		fbo.resize(pixelSize, pixelSize);
	}

	fbo.clear();
	fbo.bind();

	glm::ivec2 oldSize = {renderer.windowW, renderer.windowH};
	renderer.updateWindowMetrics(fbo.w, fbo.h);

	gl2d::Camera miniCam = {};
	float viewWidth = activeViewSize;
	float viewHeight = activeViewSize;
	float maxX = std::max(0.0f, (float)map.size.x - viewWidth);
	float maxY = std::max(0.0f, (float)map.size.y - viewHeight);
	glm::vec2 cameraCenter = cameraCenterOverride ? *cameraCenterOverride : playerPos;
	glm::vec2 viewPos = {cameraCenter.x - viewWidth * 0.5f, cameraCenter.y - viewHeight * 0.5f};
	viewPos.x = std::clamp(viewPos.x, 0.0f, maxX);
	viewPos.y = std::clamp(viewPos.y, 0.0f, maxY);
	miniCam.position = viewPos;
	miniCam.zoom = (float)fbo.w / viewWidth;

	renderer.pushCamera(miniCam);

	std::vector<glm::vec2> unknownDoorMarkers;
	if (lightingSystem && floorInfo)
	{
		auto isUnknownExplorableTile = [&](int x, int y)
		{
			if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y) { return false; }
			if (!hasTile(map, x, y)) { return false; }
			if (isSolidAt(map, x, y)) { return false; }
			return !lightingSystem->isTileVisible(map, x, y);
		};

		auto unknownScoreAhead = [&](glm::ivec2 a, glm::ivec2 b, glm::ivec2 step)
		{
			int score = 0;
			for (int i = 1; i <= 4; i++)
			{
				glm::ivec2 ta = a + step * i;
				glm::ivec2 tb = b + step * i;
				if (isUnknownExplorableTile(ta.x, ta.y)) { score++; }
				if (tb != ta && isUnknownExplorableTile(tb.x, tb.y)) { score++; }
			}
			return score;
		};

		auto doorIsLit = [&](glm::ivec2 doorPos)
		{
			return lightingSystem->isTileVisible(map, doorPos.x, doorPos.y)
				|| lightingSystem->isTileVisible(map, doorPos.x + 1, doorPos.y)
				|| lightingSystem->isTileVisible(map, doorPos.x, doorPos.y + 1)
				|| lightingSystem->isTileVisible(map, doorPos.x + 1, doorPos.y + 1);
		};

		std::unordered_set<glm::ivec2, DoorPosHash, DoorPosEqual> processedDoors;
		for (const auto &room : floorInfo->rooms)
		{
			for (const auto &doorPos : room.doorPositions)
			{
				if (!processedDoors.insert(doorPos).second) { continue; }
				if (!doorIsLit(doorPos)) { continue; }

				int scoreNorth = unknownScoreAhead(
					{doorPos.x, doorPos.y}, {doorPos.x + 1, doorPos.y}, {0, -1});
				int scoreSouth = unknownScoreAhead(
					{doorPos.x, doorPos.y + 1}, {doorPos.x + 1, doorPos.y + 1}, {0, 1});
				int scoreWest = unknownScoreAhead(
					{doorPos.x, doorPos.y}, {doorPos.x, doorPos.y + 1}, {-1, 0});
				int scoreEast = unknownScoreAhead(
					{doorPos.x + 1, doorPos.y}, {doorPos.x + 1, doorPos.y + 1}, {1, 0});

				int bestScore = 0;
				glm::vec2 bestMarker = {};
				if (scoreNorth > bestScore)
				{
					bestScore = scoreNorth;
					bestMarker = {doorPos.x + 1.0f, doorPos.y - 4.0f};
				}
				if (scoreSouth > bestScore)
				{
					bestScore = scoreSouth;
					bestMarker = {doorPos.x + 1.0f, doorPos.y + 5.0f};
				}
				if (scoreWest > bestScore)
				{
					bestScore = scoreWest;
					bestMarker = {doorPos.x - 4.0f, doorPos.y + 1.0f};
				}
				if (scoreEast > bestScore)
				{
					bestScore = scoreEast;
					bestMarker = {doorPos.x + 5.0f, doorPos.y + 1.0f};
				}

				if (bestScore > 0)
				{
					unknownDoorMarkers.push_back(bestMarker);
				}
			}
		}
	}

	int minX = std::max(0, (int)std::floor(viewPos.x));
	int minY = std::max(0, (int)std::floor(viewPos.y));
	int maxTileX = std::min(map.size.x, (int)std::ceil(viewPos.x + viewWidth));
	int maxTileY = std::min(map.size.y, (int)std::ceil(viewPos.y + viewHeight));

	for (int y = minY; y < maxTileY; y++)
	{
		for (int x = minX; x < maxTileX; x++)
		{
			if (lightingSystem && !lightingSystem->isTileVisible(map, x, y))
			{
				// Hidden tiles use wall color on the minimap.
				renderer.renderRectangle({(float)x, (float)y, 1.0f, 1.0f}, wallColor);
				continue;
			}

			gl2d::Color4f color = {};
			bool isCorridorTile = false;
			if (doorHolder.doors.find({x, y}) != doorHolder.doors.end())
			{
				color = doorColor;
			}
			else if (isSolidAt(map, x, y))
			{
				bool isWallTile = isWallAt(map, x, y);
				if (isWallTile)
				{
					bool edge = !isWallAt(map, x + 1, y) || !isWallAt(map, x - 1, y)
						|| !isWallAt(map, x, y + 1) || !isWallAt(map, x, y - 1);
					color = edge ? wallEdgeColor : wallColor;
					isCorridorTile = isCorridorWallAt(map, floorInfo, x, y);
				}
				else
				{
					color = wallColor;
				}
			}
			else if (isFloorAt(map, x, y))
			{
				color = floorColor;
				isCorridorTile = isCorridorFloorFromFloorInfo(map, floorInfo, x, y);
			}
			else
			{
				continue;
			}

			if (isCorridorTile)
			{
				color = dimColor(color, corridorDimFactor);
			}

			renderer.renderRectangle({(float)x, (float)y, 1.0f, 1.0f}, color);
		}
	}

	for (const auto &marker : unknownDoorMarkers)
	{
		renderUnknownDoorMarker(renderer, marker, unknownDoorBoxColor, unknownDoorMarkColor);
	}

	// Keep player marker visible even when minimap view is zoomed out.
	float minMarkerPixels = 2.0f;
	float markerMinWorldSize = minMarkerPixels / std::max(miniCam.zoom, 0.001f);
	float markerSize = std::max(playerMarkerSize, markerMinWorldSize);
	glm::vec4 marker = {
		playerPos.x - markerSize * 0.5f,
		playerPos.y - markerSize * 0.5f,
		markerSize,
		markerSize
	};
	renderer.renderRectangle(marker, playerColor);

	renderer.popCamera();
	renderer.updateWindowMetrics(oldSize.x, oldSize.y);
	fbo.unbind();
}

void MinimapSystem::renderFullscreenDirect(gl2d::Renderer2D &renderer, Map &map, const DoorHolder &doorHolder,
	glm::vec2 playerPos, const FloorInfo *floorInfo,
	const RoomLightingSystem *lightingSystem,
	const glm::vec2 *cameraCenterOverride,
	const float *viewSizeOverride,
	float mapAlpha)
{
	float activeViewHeight = viewSizeOverride ? *viewSizeOverride : viewSize;
	if (activeViewHeight <= 0.0f) { return; }
	mapAlpha = glm::clamp(mapAlpha, 0.0f, 1.0f);

	auto applyAlpha = [&](gl2d::Color4f color)
	{
		color.a *= mapAlpha;
		return color;
	};

	float zoom = renderer.windowH / activeViewHeight;
	if (zoom <= 0.0f) { return; }
	float activeViewWidth = renderer.windowW / zoom;

	float maxX = std::max(0.0f, (float)map.size.x - activeViewWidth);
	float maxY = std::max(0.0f, (float)map.size.y - activeViewHeight);
	glm::vec2 cameraCenter = cameraCenterOverride ? *cameraCenterOverride : playerPos;
	glm::vec2 viewPos = {cameraCenter.x - activeViewWidth * 0.5f, cameraCenter.y - activeViewHeight * 0.5f};
	viewPos.x = std::clamp(viewPos.x, 0.0f, maxX);
	viewPos.y = std::clamp(viewPos.y, 0.0f, maxY);

	renderer.pushCamera();

	auto worldToScreen = [&](glm::vec2 p)
	{
		return glm::vec2{
			(p.x - viewPos.x) * zoom,
			(p.y - viewPos.y) * zoom
		};
	};

	std::vector<glm::vec2> unknownDoorMarkers;
	if (lightingSystem && floorInfo)
	{
		auto isUnknownExplorableTile = [&](int x, int y)
		{
			if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y) { return false; }
			if (!hasTile(map, x, y)) { return false; }
			if (isSolidAt(map, x, y)) { return false; }
			return !lightingSystem->isTileVisible(map, x, y);
		};

		auto unknownScoreAhead = [&](glm::ivec2 a, glm::ivec2 b, glm::ivec2 step)
		{
			int score = 0;
			for (int i = 1; i <= 4; i++)
			{
				glm::ivec2 ta = a + step * i;
				glm::ivec2 tb = b + step * i;
				if (isUnknownExplorableTile(ta.x, ta.y)) { score++; }
				if (tb != ta && isUnknownExplorableTile(tb.x, tb.y)) { score++; }
			}
			return score;
		};

		auto doorIsLit = [&](glm::ivec2 doorPos)
		{
			return lightingSystem->isTileVisible(map, doorPos.x, doorPos.y)
				|| lightingSystem->isTileVisible(map, doorPos.x + 1, doorPos.y)
				|| lightingSystem->isTileVisible(map, doorPos.x, doorPos.y + 1)
				|| lightingSystem->isTileVisible(map, doorPos.x + 1, doorPos.y + 1);
		};

		std::unordered_set<glm::ivec2, DoorPosHash, DoorPosEqual> processedDoors;
		for (const auto &room : floorInfo->rooms)
		{
			for (const auto &doorPos : room.doorPositions)
			{
				if (!processedDoors.insert(doorPos).second) { continue; }
				if (!doorIsLit(doorPos)) { continue; }

				int scoreNorth = unknownScoreAhead(
					{doorPos.x, doorPos.y}, {doorPos.x + 1, doorPos.y}, {0, -1});
				int scoreSouth = unknownScoreAhead(
					{doorPos.x, doorPos.y + 1}, {doorPos.x + 1, doorPos.y + 1}, {0, 1});
				int scoreWest = unknownScoreAhead(
					{doorPos.x, doorPos.y}, {doorPos.x, doorPos.y + 1}, {-1, 0});
				int scoreEast = unknownScoreAhead(
					{doorPos.x + 1, doorPos.y}, {doorPos.x + 1, doorPos.y + 1}, {1, 0});

				int bestScore = 0;
				glm::vec2 bestMarker = {};
				if (scoreNorth > bestScore)
				{
					bestScore = scoreNorth;
					bestMarker = {doorPos.x + 1.0f, doorPos.y - 4.0f};
				}
				if (scoreSouth > bestScore)
				{
					bestScore = scoreSouth;
					bestMarker = {doorPos.x + 1.0f, doorPos.y + 5.0f};
				}
				if (scoreWest > bestScore)
				{
					bestScore = scoreWest;
					bestMarker = {doorPos.x - 4.0f, doorPos.y + 1.0f};
				}
				if (scoreEast > bestScore)
				{
					bestScore = scoreEast;
					bestMarker = {doorPos.x + 5.0f, doorPos.y + 1.0f};
				}

				if (bestScore > 0)
				{
					unknownDoorMarkers.push_back(bestMarker);
				}
			}
		}
	}

	int minX = std::max(0, (int)std::floor(viewPos.x));
	int minY = std::max(0, (int)std::floor(viewPos.y));
	int maxTileX = std::min(map.size.x, (int)std::ceil(viewPos.x + activeViewWidth));
	int maxTileY = std::min(map.size.y, (int)std::ceil(viewPos.y + activeViewHeight));

	for (int y = minY; y < maxTileY; y++)
	{
		for (int x = minX; x < maxTileX; x++)
		{
			glm::vec2 sp = worldToScreen({(float)x, (float)y});
			glm::vec4 rect = {sp.x, sp.y, zoom, zoom};

			if (lightingSystem && !lightingSystem->isTileVisible(map, x, y))
			{
				renderer.renderRectangle(rect, applyAlpha(wallColor));
				continue;
			}

			gl2d::Color4f color = {};
			bool isCorridorTile = false;
			if (doorHolder.doors.find({x, y}) != doorHolder.doors.end())
			{
				color = doorColor;
			}
			else if (isSolidAt(map, x, y))
			{
				bool isWallTile = isWallAt(map, x, y);
				if (isWallTile)
				{
					bool edge = !isWallAt(map, x + 1, y) || !isWallAt(map, x - 1, y)
						|| !isWallAt(map, x, y + 1) || !isWallAt(map, x, y - 1);
					color = edge ? wallEdgeColor : wallColor;
					isCorridorTile = isCorridorWallAt(map, floorInfo, x, y);
				}
				else
				{
					color = wallColor;
				}
			}
			else if (isFloorAt(map, x, y))
			{
				color = floorColor;
				isCorridorTile = isCorridorFloorFromFloorInfo(map, floorInfo, x, y);
			}
			else
			{
				continue;
			}

			if (isCorridorTile)
			{
				color = dimColor(color, corridorDimFactor);
			}

			renderer.renderRectangle(rect, applyAlpha(color));
		}
	}

	for (const auto &marker : unknownDoorMarkers)
	{
		renderUnknownDoorMarker(renderer, worldToScreen(marker),
			applyAlpha(unknownDoorBoxColor), applyAlpha(unknownDoorMarkColor), zoom);
	}

	float markerSize = std::max(2.0f, playerMarkerSize * zoom);
	glm::vec2 playerScreen = worldToScreen(playerPos);
	glm::vec4 marker = {
		playerScreen.x - markerSize * 0.5f,
		playerScreen.y - markerSize * 0.5f,
		markerSize,
		markerSize
	};
	renderer.renderRectangle(marker, applyAlpha(playerColor));

	renderer.popCamera();
}

void MinimapSystem::render(gl2d::Renderer2D &renderer)
{
	if (!fbo.texture.isValid()) { return; }

	const float uiBaseZoom = 100.0f;
	float cameraZoom = uiBaseZoom;
	float padding = PIXEL_SIZE * 3.0f * cameraZoom;
	float size = uiSize > 0.0f ? uiSize : (PIXEL_SIZE * 34.0f * cameraZoom);
	float x = renderer.windowW - padding - size;
	float y = renderer.windowH - padding - size;
	renderAt(renderer, {x, y, size, size});
}


void MinimapSystem::renderAt(gl2d::Renderer2D &renderer, glm::vec4 rect, float finalOpacity)
{
	if (!fbo.texture.isValid()) { return; }
	if (finalOpacity < 0.0f)
	{
		finalOpacity = opacity;
	}
	finalOpacity = std::clamp(finalOpacity, 0.0f, 1.0f);
	renderer.pushCamera();
	renderer.renderRectangle(rect, fbo.texture, {1, 1, 1, finalOpacity}, {}, 0, {0, 0, 1, 1});
	renderer.popCamera();
}
