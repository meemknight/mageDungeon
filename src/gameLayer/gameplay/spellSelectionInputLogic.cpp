#include "spellSelectionInputLogic.h"
#include <gameplay/spells/spellTypes.h>
#include <gameplay/spells/spells.h>
#include <gameplay/player.h>
#include <gameplay/wand.h>
#include <gameplay/assetsManager.h>
#include <gameplay/elements.h>
#include <gameplay/Physics.h>
#include <glui/glui.h>
#include <platformInput.h>
#include <gameLayer.h>

void SleppSelectionInputLogic::update(float deltaTime, gl2d::Renderer2D &renderer,
	AssetsManager &assetsManager,
	SpellRecepie &spellRecepie,
	SpellsHolder &spellsHolder,
	Player &player,
	glm::vec2 fireDirection,
	bool usesController,
	const Wand &wand,
	platform::Input &input)
{
	const auto &controller = input.controller;
	glm::vec2 cursorPos = {static_cast<float>(input.mouseX), static_cast<float>(input.mouseY)};

	auto tryAddElement = [&](int element)
	{
		int maxElements = wand.maxElementsPerCast;
		if (maxElements < 1) { maxElements = 1; }
		if (maxElements > SpellRecepie::MAX_ELEMENTS) { maxElements = SpellRecepie::MAX_ELEMENTS; }
		spellRecepie.add(element, maxElements);
	};

	if (input.rMouse.pressed || controller.RTButton.pressed)
	{
		auto spell = SpellTypes::getSpellFromRecepie(spellRecepie);
		spellsHolder.addSpell(std::move(spell), player.physics.getPos(), fireDirection);
		spellRecepie.clear();
	}

	float cameraZoom = renderer.currentCamera.zoom;
	renderer.pushCamera();

	if (usesController)
	{
		float size = 15 * PIXEL_SIZE * cameraZoom;
		glm::vec2 pos = glm::vec2{renderer.windowW, renderer.windowH} / 2.f;
		pos += PIXEL_SIZE * cameraZoom * 30 * fireDirection;
		pos -= size / 2.f;
		glm::vec4 transform(pos, size, size);
		renderer.renderRectangle(transform, assetsManager.target, {1, 1, 1, 0.5f});
	}

	bool startSelectionButton = input.buttons[platform::Button::Q].held ||
		controller.LTButton.held ||
		controller.buttons[platform::Controller::RThumb].held;
	bool startDraw = input.lMouse.held;
	constexpr float TRAIL_TIMER = 0.55f;
	bool selectionActive = startSelectionButton || startDraw;

	glui::Frame screenFrame({0, 0, renderer.windowW, renderer.windowH});
	float selectorSize = PIXEL_SIZE * 96 * cameraZoom;

	glm::vec2 screenCenter = {renderer.windowW / 2.f, renderer.windowH / 2.f};
	glm::vec2 selectionCenter = screenCenter;
	glm::vec4 mainBox = {};
	glm::ivec4 mainBoxFrame = {};
	float selectLength = (selectorSize / 6.f) * 1.5f;
	const glm::vec3 baseTrailColor = {0.5f, 0.5f, 0.5f};

	auto getDragDirection = [&](glm::vec2 cursorVector)
	{
		if (glm::length(cursorVector) < selectLength)
		{
			return 0;
		}
		if (glm::abs(cursorVector.x) > glm::abs(cursorVector.y))
		{
			return cursorVector.x > 0 ? 4 : 3;
		}
		return cursorVector.y > 0 ? 2 : 1;
	};

	for (size_t i = 0; i < trail.size();)
	{
		auto &point = trail[i];
		point.timer -= deltaTime;
		if (point.timer <= 0.0f)
		{
			trail[i] = trail.back();
			trail.pop_back();
			continue;
		}
		++i;
	}

	if (selectionActive)
	{
		if (!executedFirstFrame)
		{
			executedFirstFrame = true;
			isDrawing = false;
			isClickSelection = false;

			if (startSelectionButton)
			{
				isClickSelection = true;
			}
			else if (startDraw)
			{
				isDrawing = true;
				mouseStart = cursorPos;
			}

			trail.clear();
			dragDirection = 0;
			trailColor = baseTrailColor;
			trailColorStart = baseTrailColor;
			trailTargetColor = baseTrailColor;
			trailColorTimer = 0.0f;
		}

		selectionCenter = isDrawing ? mouseStart : screenCenter;
		mainBox = {selectionCenter.x - selectorSize * 0.5f,
			selectionCenter.y - selectorSize * 0.5f,
			selectorSize, selectorSize};
		mainBoxFrame = {static_cast<int>(mainBox.x), static_cast<int>(mainBox.y),
			static_cast<int>(mainBox.z), static_cast<int>(mainBox.w)};

		bool selectedUp = false;
		bool selectedDown = false;
		bool selectedLeft = false;
		bool selectedRight = false;

		bool hoveredUp = false;
		bool hoveredDown = false;
		bool hoveredLeft = false;
		bool hoveredRight = false;
		glm::vec2 cursorVector = cursorPos - selectionCenter;
		int currentDragDir = 0;

		bool upEnabled = wand.up.type == WandSlotType::Element;
		bool downEnabled = wand.down.type == WandSlotType::Element;
		bool leftEnabled = wand.left.type == WandSlotType::Element;
		bool rightEnabled = wand.right.type == WandSlotType::Element;

		int upElement = wand.up.element;
		int downElement = wand.down.element;
		int leftElement = wand.left.element;
		int rightElement = wand.right.element;

		if (isClickSelection)
		{
			glm::vec2 upVector = glm::vec2(0, -1) * selectLength;
			glm::vec2 downVector = glm::vec2(0, +1) * selectLength;
			glm::vec2 leftVector = glm::vec2(-1, 0) * selectLength;
			glm::vec2 rightVector = glm::vec2(1, 0) * selectLength;

			if (glm::length(cursorVector) < selectorSize * 0.55f)
			{
				if (glm::dot(cursorVector, upVector) > selectLength * selectLength)
				{
					hoveredUp = true;
				}
				else if (glm::dot(cursorVector, downVector) > selectLength * selectLength)
				{
					hoveredDown = true;
				}
				else if (glm::dot(cursorVector, leftVector) > selectLength * selectLength)
				{
					hoveredLeft = true;
				}
				else if (glm::dot(cursorVector, rightVector) > selectLength * selectLength)
				{
					hoveredRight = true;
				}
			}
		}

		if (isClickSelection)
		{
			if (hoveredUp && input.lMouse.pressed) { selectedUp = true; }
			if (hoveredDown && input.lMouse.pressed) { selectedDown = true; }
			if (hoveredLeft && input.lMouse.pressed) { selectedLeft = true; }
			if (hoveredRight && input.lMouse.pressed) { selectedRight = true; }

			if (!selectedUp && !selectedDown && !selectedLeft && !selectedRight)
			{
				if (controller.RStickButtonUp.pressed) { selectedUp = true; } else
				if (controller.RStickButtonDown.pressed) { selectedDown = true; } else
				if (controller.RStickButtonLeft.pressed) { selectedLeft = true; } else
				if (controller.RStickButtonRight.pressed) { selectedRight = true; }
			}
		}
		else if (isDrawing)
		{
			int dragDir = getDragDirection(cursorVector);
			currentDragDir = dragDir;

			hoveredUp = dragDir == 1;
			hoveredDown = dragDir == 2;
			hoveredLeft = dragDir == 3;
			hoveredRight = dragDir == 4;

			if (dragDir == 0)
			{
				dragDirection = 0;
			}
			else if (dragDir != dragDirection)
			{
				dragDirection = dragDir;
				selectedUp = dragDir == 1;
				selectedDown = dragDir == 2;
				selectedLeft = dragDir == 3;
				selectedRight = dragDir == 4;
			}

			if (!upEnabled) { hoveredUp = false; selectedUp = false; }
			if (!downEnabled) { hoveredDown = false; selectedDown = false; }
			if (!leftEnabled) { hoveredLeft = false; selectedLeft = false; }
			if (!rightEnabled) { hoveredRight = false; selectedRight = false; }
		}

		glm::vec3 desiredTrailColor = baseTrailColor;
		glm::vec3 instantTintColor = baseTrailColor;
		bool trailTintActive = false;
		if (isDrawing)
		{
			bool trailEnabled = false;
			int trailElement = Elements::NoneElement;
			switch (currentDragDir)
			{
				case 1:
					trailEnabled = upEnabled;
					trailElement = upElement;
					break;
				case 2:
					trailEnabled = downEnabled;
					trailElement = downElement;
					break;
				case 3:
					trailEnabled = leftEnabled;
					trailElement = leftElement;
					break;
				case 4:
					trailEnabled = rightEnabled;
					trailElement = rightElement;
					break;
			}
			if (trailEnabled)
			{
				auto elementColor = elementToColor(trailElement);
				desiredTrailColor = {elementColor.x, elementColor.y, elementColor.z};
				instantTintColor = desiredTrailColor;
				trailTintActive = true;
			}
		}

		if (desiredTrailColor.x != trailTargetColor.x ||
			desiredTrailColor.y != trailTargetColor.y ||
			desiredTrailColor.z != trailTargetColor.z)
		{
			trailColorStart = trailColor;
			trailTargetColor = desiredTrailColor;
			trailColorTimer = 0.0f;
		}

		constexpr float TRAIL_COLOR_BLEND_TIME = 0.5f;
		trailColorTimer = glm::clamp(trailColorTimer + deltaTime, 0.0f, TRAIL_COLOR_BLEND_TIME);
		float trailBlend = TRAIL_COLOR_BLEND_TIME > 0.0f
			? trailColorTimer / TRAIL_COLOR_BLEND_TIME
			: 1.0f;
		trailColor = glm::mix(trailColorStart, trailTargetColor, trailBlend);

		if (isDrawing)
		{
			trail.push_back({cursorPos, TRAIL_TIMER, trailColor});
		}

		float opacity = 0.8f;

		auto updateCirclePiece = [&](CirclePiece &c, gl2d::Texture t, int element,
			float opacityValue, bool selected, bool hovered, bool enabled)
		{
			glm::vec3 color = enabled ? elementToColor(element) : glm::vec3{0.35f, 0.35f, 0.35f};
			c.animationTime -= deltaTime * 2;
			c.animationTime = glm::clamp(c.animationTime, 0.f, 1.f);
			if (selected && enabled)
			{
				c.animationTime = 1.f;
				tryAddElement(element);
			}
			if (hovered) { c.animationTime = std::max(c.animationTime, 0.5f); }
			glm::vec3 finalColor = glm::mix(color, glm::vec3{1, 1, 1}, glm::vec3(c.animationTime));
			float finalOpacity = enabled ? opacityValue : opacityValue * 0.4f;
			renderer.renderRectangle(mainBox, t, {finalColor, finalOpacity});
		};

		updateCirclePiece(upPiece, assetsManager.upCircle, upElement, opacity, selectedUp, hoveredUp, upEnabled);
		updateCirclePiece(downPiece, assetsManager.downCircle, downElement, opacity, selectedDown, hoveredDown, downEnabled);
		updateCirclePiece(leftPiece, assetsManager.leftCircle, leftElement, opacity, selectedLeft, hoveredLeft, leftEnabled);
		updateCirclePiece(rightPiece, assetsManager.rightCircle, rightElement, opacity, selectedRight, hoveredRight, rightEnabled);

		auto renderElementIcon = [&](int element, glm::vec2 dir)
		{
			float iconSize = PIXEL_SIZE * 14.0f * cameraZoom;
			float offset = selectLength * 1.55f;
			glm::vec2 center = selectionCenter + dir * offset;
			glm::vec4 rect = {center.x - iconSize * 0.5f, center.y - iconSize * 0.5f,
				iconSize, iconSize};
			renderer.renderRectangle(rect, assetsManager.elements.texture,
				{1, 1, 1, opacity}, {}, 0,
				assetsManager.elements.atlas.get(element, 0));
		};

		if (upEnabled) { renderElementIcon(upElement, {0, -1}); }
		if (downEnabled) { renderElementIcon(downElement, {0, 1}); }
		if (leftEnabled) { renderElementIcon(leftElement, {-1, 0}); }
		if (rightEnabled) { renderElementIcon(rightElement, {1, 0}); }

		int sizeInt = 4;
		float trailSize = PIXEL_SIZE * sizeInt * cameraZoom;
		for (const auto &point : trail)
		{
			float normalized = glm::clamp(point.timer / TRAIL_TIMER, 0.0f, 1.0f);
			float fade = glm::pow(normalized, 0.6f);
			glm::vec3 finalTrailColor = point.color;
			if (trailTintActive)
			{
				finalTrailColor = glm::mix(finalTrailColor, instantTintColor, 0.5f);
			}
			glm::vec4 drawColor = {finalTrailColor.x, finalTrailColor.y, finalTrailColor.z, 0.7f};
			drawColor.a *= fade;
			glm::vec4 pos = {point.pos.x, point.pos.y, trailSize, trailSize};
			renderer.renderRectangle(pos, drawColor);
		}
	}
	else
	{
		executedFirstFrame = false;
		isClickSelection = false;
		isDrawing = false;
		mouseStart = {};
		trail.clear();
		dragDirection = 0;
		trailColor = baseTrailColor;
		trailColorStart = baseTrailColor;
		trailTargetColor = baseTrailColor;
		trailColorTimer = 0.0f;

		selectionCenter = screenCenter;
		mainBox = {selectionCenter.x - selectorSize * 0.5f,
			selectionCenter.y - selectorSize * 0.5f,
			selectorSize, selectorSize};
		mainBoxFrame = {static_cast<int>(mainBox.x), static_cast<int>(mainBox.y),
			static_cast<int>(mainBox.z), static_cast<int>(mainBox.w)};
	}

	// render loaded elements
	{
		glui::Frame inCircle(mainBoxFrame);
		float elementSize = PIXEL_SIZE * 4 * cameraZoom;
		auto elementBox = glui::Box().xCenter().yCenter().xDimensionPixels(elementSize)
			.yDimensionPixels(elementSize)();
		elementBox.y -= PIXEL_SIZE * 16 * cameraZoom;
		elementBox.x -= PIXEL_SIZE * 8 * cameraZoom;

		for (int i = 0; i < spellRecepie.count; i++)
		{
			renderer.renderRectangle(elementBox, elementToColor(spellRecepie.elements[i]));
			elementBox.x += elementSize * 1.5f;
		}
	}

	renderer.popCamera();
}
