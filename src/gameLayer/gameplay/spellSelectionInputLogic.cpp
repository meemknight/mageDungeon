#include "spellSelectionInputLogic.h"
#include <gameplay/spells/spellTypes.h>
#include <gameplay/spells/spells.h>
#include <gameplay/player.h>
#include <gameplay/wand.h>
#include <gameplay/assetsManager.h>
#include <gameplay/elements.h>
#include <gameplay/Physics.h>
#include <gameplay/projectiles/projectiles.h>
#include <glui/glui.h>
#include <platformInput.h>
#include <gameLayer.h>
#include <cstdio>

void SleppSelectionInputLogic::update(float deltaTime, gl2d::Renderer2D &renderer,
	AssetsManager &assetsManager,
	SpellRecepie &spellRecepie,
	SpellsHolder &spellsHolder,
	Map &map,
	ProjectileHolder &projectileHolder,
	EntityHolder &entityHolder,
	Player &player,
	glm::vec2 fireDirection,
	bool usesController,
	const Wand &wand,
	platform::Input &input)
{
	noManaFeedback = false;
	noManaDisplayTimer = std::max(0.0f, noManaDisplayTimer - deltaTime);
	noManaShakeTimer = std::max(0.0f, noManaShakeTimer - deltaTime);
	const auto &controller = input.controller;
	glm::vec2 cursorPos = {static_cast<float>(input.mouseX), static_cast<float>(input.mouseY)};
	constexpr float CAST_COOLDOWN = 0.7f;

	auto getMaxElements = [&]()
	{
		int maxElements = wand.maxElementsPerCast;
		if (maxElements < 1) { maxElements = 1; }
		if (maxElements > SpellRecepie::MAX_ELEMENTS) { maxElements = SpellRecepie::MAX_ELEMENTS; }
		return maxElements;
	};

	auto resetCastState = [&]()
	{
		remainingUp = wand.up.type == WandSlotType::Element ? wand.up.castCount : 0;
		remainingDown = wand.down.type == WandSlotType::Element ? wand.down.castCount : 0;
		remainingLeft = wand.left.type == WandSlotType::Element ? wand.left.castCount : 0;
		remainingRight = wand.right.type == WandSlotType::Element ? wand.right.castCount : 0;
		remainingAlwaysCast = wand.alwaysCast.type == WandSlotType::Element ? 1 : 0;
		alwaysCastUsedThisCast = false;
	};

	auto slotEqual = [](const WandSlot &a, const WandSlot &b)
	{
		return a.type == b.type && a.element == b.element && a.castCount == b.castCount;
	};

	auto wandEqual = [&](const Wand &a, const Wand &b)
	{
		return slotEqual(a.up, b.up) && slotEqual(a.down, b.down) &&
			slotEqual(a.left, b.left) && slotEqual(a.right, b.right) &&
			slotEqual(a.alwaysCast, b.alwaysCast) &&
			a.maxMana == b.maxMana && a.manaChargeSpeed == b.manaChargeSpeed &&
			a.maxElementsPerCast == b.maxElementsPerCast && a.wandSprite == b.wandSprite;
	};

	bool wandChanged = !hasWandState || !wandEqual(lastWand, wand);
	if (wandChanged)
	{
		lastWand = wand;
		hasWandState = true;
		spellRecepie.clear();
		resetCastState();
	}

	if (!manaInitialized || wandChanged)
	{
		currentMana = 0.0f;
		manaInitialized = true;
		castCooldownTimer = 0.0f;
	}

	float manaDelta = pauseManaCharge ? 0.0f : deltaTime;
	pauseManaCharge = false;
	currentMana += wand.manaChargeSpeed * manaDelta;
	currentMana = glm::clamp(currentMana, 0.0f, (float)wand.maxMana);
	castCooldownTimer = std::max(0.0f, castCooldownTimer - deltaTime);

	auto applyAlwaysCast = [&]()
	{
		if (wand.alwaysCast.type != WandSlotType::Element) { return; }
		if (alwaysCastUsedThisCast) { return; }
		if (spellRecepie.count > 0) { return; }

		int maxElements = getMaxElements();
		if (spellRecepie.add(wand.alwaysCast.element, maxElements))
		{
			alwaysCastUsedThisCast = true;
		}
	};

	applyAlwaysCast();

	auto tryUseSlotElement = [&](int element, int &remaining)
	{
		int maxElements = getMaxElements();
		if (remaining <= 0) { return false; }
		if (currentMana < 1.0f) { noManaFeedback = true; return false; }
		if (spellRecepie.count >= maxElements) { return false; }
		if (spellRecepie.add(element, maxElements))
		{
			remaining--;
			currentMana -= 1.0f;
			if (currentMana < 0.0f) { currentMana = 0.0f; }
			return true;
		}
		return false;
	};

	auto buildQuickRecipe = [&](const QuickAction &action)
	{
		SpellRecepie fullRecipe;
		int maxElements = getMaxElements();
		if (wand.alwaysCast.type == WandSlotType::Element)
		{
			fullRecipe.add(wand.alwaysCast.element, maxElements);
		}
		for (int i = 0; i < action.count; i++)
		{
			fullRecipe.add(action.elements[i], maxElements);
		}
		return fullRecipe;
	};

	auto tryConsumeElement = [&](int element, int &ru, int &rd, int &rl, int &rr)
	{
		if (wand.up.type == WandSlotType::Element && wand.up.element == element && ru > 0)
		{
			ru--;
			return true;
		}
		if (wand.down.type == WandSlotType::Element && wand.down.element == element && rd > 0)
		{
			rd--;
			return true;
		}
		if (wand.left.type == WandSlotType::Element && wand.left.element == element && rl > 0)
		{
			rl--;
			return true;
		}
		if (wand.right.type == WandSlotType::Element && wand.right.element == element && rr > 0)
		{
			rr--;
			return true;
		}
		return false;
	};

	auto tryApplyElement = [&](int element)
	{
		if (wand.up.type == WandSlotType::Element && wand.up.element == element && remainingUp > 0)
		{
			return tryUseSlotElement(element, remainingUp);
		}
		if (wand.down.type == WandSlotType::Element && wand.down.element == element && remainingDown > 0)
		{
			return tryUseSlotElement(element, remainingDown);
		}
		if (wand.left.type == WandSlotType::Element && wand.left.element == element && remainingLeft > 0)
		{
			return tryUseSlotElement(element, remainingLeft);
		}
		if (wand.right.type == WandSlotType::Element && wand.right.element == element && remainingRight > 0)
		{
			return tryUseSlotElement(element, remainingRight);
		}
		return false;
	};

	auto tryQuickCast = [&](const QuickAction &action) -> bool
	{
		int maxElements = getMaxElements();
		int alwaysCastCount = wand.alwaysCast.type == WandSlotType::Element ? 1 : 0;
		if (action.count + alwaysCastCount > maxElements) { return false; }
		if (action.count <= 0 && alwaysCastCount == 0) { return false; }
		SpellRecepie fullRecipe = buildQuickRecipe(action);
		if (fullRecipe.count == 0 || fullRecipe.count > maxElements) { return false; }
		if (spellRecepie.count > fullRecipe.count) { return false; }
		for (int i = 0; i < spellRecepie.count; i++)
		{
			if (spellRecepie.elements[i] != fullRecipe.elements[i]) { return false; }
		}
		if (spellRecepie.count == fullRecipe.count)
		{
			return true;
		}

		float tempMana = currentMana;
		int tempUp = remainingUp;
		int tempDown = remainingDown;
		int tempLeft = remainingLeft;
		int tempRight = remainingRight;
		SpellRecepie tempRecipe = spellRecepie;

		for (int i = spellRecepie.count; i < fullRecipe.count; i++)
		{
			if (tempRecipe.count >= maxElements) { return false; }
			if (tempMana < 1.0f) { noManaFeedback = true; return false; }
			if (!tryConsumeElement(fullRecipe.elements[i], tempUp, tempDown, tempLeft, tempRight))
			{
				return false;
			}
			tempRecipe.add(fullRecipe.elements[i], maxElements);
			tempMana -= 1.0f;
		}

		for (int i = spellRecepie.count; i < fullRecipe.count; i++)
		{
			if (!tryApplyElement(fullRecipe.elements[i])) { return false; }
		}
		return spellRecepie.count == fullRecipe.count;
	};

	int quickIndex = -1;
	if (controller.buttons[platform::Controller::Up].pressed) { quickIndex = 0; }
	if (controller.buttons[platform::Controller::Down].pressed) { quickIndex = 1; }
	if (controller.buttons[platform::Controller::Left].pressed) { quickIndex = 2; }
	if (controller.buttons[platform::Controller::Right].pressed) { quickIndex = 3; }
	if (quickIndex >= 0)
	{
		bool quickReady = tryQuickCast(wand.quickActions[quickIndex]);
		if (kQuickCastInstant && quickReady)
		{
			if (spellRecepie.count > 0)
			{
				auto spell = SpellTypes::getSpellFromRecepie(spellRecepie);
				spellsHolder.addSpell(std::move(spell), player.physics.getPos(), fireDirection);
				spellRecepie.clear();
				resetCastState();
				applyAlwaysCast();
				castCooldownTimer = CAST_COOLDOWN;
			}
		}
	}
	if (noManaFeedback)
	{
		noManaDisplayTimer = 0.5f;
		noManaShakeTimer = 0.2f;
	}

	if (input.rMouse.held || controller.RTButton.held)
	{
		if (castCooldownTimer > 0.0f)
		{
			// still on cooldown
		}
		else
		{
			bool hasRecipe = spellRecepie.count > 0;
			bool onlyAlwaysCast = alwaysCastUsedThisCast && spellRecepie.count == 1;
			bool castedSpell = false;
			bool firedStandby = false;

			if (hasRecipe)
			{
				auto spell = SpellTypes::getSpellFromRecepie(spellRecepie);
				spellsHolder.addSpell(std::move(spell), player.physics.getPos(), fireDirection);
				castedSpell = true;
			}

			if (!hasRecipe || onlyAlwaysCast)
			{
				firedStandby = getStandbyProjectilesSystem().tryFire(map, projectileHolder,
					player, entityHolder, fireDirection);
			}

			if (!hasRecipe && !firedStandby)
			{
				auto spell = SpellTypes::getSpellFromRecepie(spellRecepie);
				spellsHolder.addSpell(std::move(spell), player.physics.getPos(), fireDirection);
				castedSpell = true;
			}

			if (castedSpell || firedStandby)
			{
				spellRecepie.clear();
				resetCastState();
				applyAlwaysCast();
				float cooldownRatio = firedStandby ? 0.7f : 1.0f;
				castCooldownTimer = CAST_COOLDOWN * cooldownRatio;
			}
		}
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

		int maxElements = getMaxElements();
		bool hasMana = currentMana >= 1.0f;
		bool upEnabled = wand.up.type == WandSlotType::Element;
		bool downEnabled = wand.down.type == WandSlotType::Element;
		bool leftEnabled = wand.left.type == WandSlotType::Element;
		bool rightEnabled = wand.right.type == WandSlotType::Element;
		bool upSelectable = upEnabled && remainingUp > 0 && hasMana;
		bool downSelectable = downEnabled && remainingDown > 0 && hasMana;
		bool leftSelectable = leftEnabled && remainingLeft > 0 && hasMana;
		bool rightSelectable = rightEnabled && remainingRight > 0 && hasMana;

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

			if (!upSelectable) { hoveredUp = false; selectedUp = false; }
			if (!downSelectable) { hoveredDown = false; selectedDown = false; }
			if (!leftSelectable) { hoveredLeft = false; selectedLeft = false; }
			if (!rightSelectable) { hoveredRight = false; selectedRight = false; }
		}

		if (!upSelectable) { hoveredUp = false; selectedUp = false; }
		if (!downSelectable) { hoveredDown = false; selectedDown = false; }
		if (!leftSelectable) { hoveredLeft = false; selectedLeft = false; }
		if (!rightSelectable) { hoveredRight = false; selectedRight = false; }

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
				trailEnabled = upSelectable;
				trailElement = upElement;
				break;
			case 2:
				trailEnabled = downSelectable;
				trailElement = downElement;
				break;
			case 3:
				trailEnabled = leftSelectable;
				trailElement = leftElement;
				break;
			case 4:
				trailEnabled = rightSelectable;
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
			float opacityValue, bool selected, bool hovered, bool enabled, bool selectable, int &remaining)
		{
			glm::vec3 color = enabled ? elementToColor(element) : glm::vec3{0.35f, 0.35f, 0.35f};
			if (enabled && !selectable)
			{
				color = glm::mix(color, glm::vec3{0.2f, 0.2f, 0.2f}, 0.6f);
			}
			c.animationTime -= deltaTime * 2;
			c.animationTime = glm::clamp(c.animationTime, 0.f, 1.f);
			if (selected && selectable)
			{
				if (tryUseSlotElement(element, remaining))
				{
					c.animationTime = 1.f;
				}
			}
			if (hovered) { c.animationTime = std::max(c.animationTime, 0.5f); }
			glm::vec3 finalColor = glm::mix(color, glm::vec3{1, 1, 1}, glm::vec3(c.animationTime));
			float finalOpacity = enabled ? opacityValue : opacityValue * 0.4f;
			if (enabled && !selectable) { finalOpacity *= 0.6f; }
			renderer.renderRectangle(mainBox, t, {finalColor, finalOpacity});
		};

		updateCirclePiece(upPiece, assetsManager.upCircle, upElement, opacity, selectedUp, hoveredUp,
			upEnabled, upSelectable, remainingUp);
		updateCirclePiece(downPiece, assetsManager.downCircle, downElement, opacity, selectedDown, hoveredDown,
			downEnabled, downSelectable, remainingDown);
		updateCirclePiece(leftPiece, assetsManager.leftCircle, leftElement, opacity, selectedLeft, hoveredLeft,
			leftEnabled, leftSelectable, remainingLeft);
		updateCirclePiece(rightPiece, assetsManager.rightCircle, rightElement, opacity, selectedRight, hoveredRight,
			rightEnabled, rightSelectable, remainingRight);

		auto renderElementIcon = [&](int element, glm::vec2 dir, float iconOpacity, int remaining)
		{
			float iconSize = PIXEL_SIZE * 14.0f * cameraZoom;
			float offset = selectLength * 1.55f;
			glm::vec2 center = selectionCenter + dir * offset;
			glm::vec4 rect = {center.x - iconSize * 0.5f, center.y - iconSize * 0.5f,
				iconSize, iconSize};
			renderer.renderRectangle(rect, assetsManager.elements.texture,
				{1, 1, 1, iconOpacity}, {}, 0,
				assetsManager.elements.atlas.get(element, 0));

			if (remaining >= 0)
			{
				char countText[8] = {};
				snprintf(countText, sizeof(countText), "%d", remaining);
				glm::vec2 textPos = {center.x + iconSize * 0.6f, center.y + iconSize * 0.35f};
				renderer.renderText(textPos, countText, assetsManager.font,
					{1, 1, 1, iconOpacity}, iconSize * 0.9f, 4, 3, false);
			}
		};

		if (upEnabled) { renderElementIcon(upElement, {0, -1}, upSelectable ? opacity : opacity * 0.4f, remainingUp); }
		if (downEnabled) { renderElementIcon(downElement, {0, 1}, downSelectable ? opacity : opacity * 0.4f, remainingDown); }
		if (leftEnabled) { renderElementIcon(leftElement, {-1, 0}, leftSelectable ? opacity : opacity * 0.4f, remainingLeft); }
		if (rightEnabled) { renderElementIcon(rightElement, {1, 0}, rightSelectable ? opacity : opacity * 0.4f, remainingRight); }

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

		int maxElements = getMaxElements();
		int totalSlots = selectionActive ? maxElements : spellRecepie.count;
		for (int i = 0; i < totalSlots; i++)
		{
			if (i < spellRecepie.count)
			{
				renderer.renderRectangle(elementBox, elementToColor(spellRecepie.elements[i]));
			}
			else
			{
				renderer.renderRectangle(elementBox, {0.1f, 0.1f, 0.1f, 0.5f});
				renderer.renderRectangleOutline(elementBox, {0.6f, 0.6f, 0.6f, 0.6f},
					PIXEL_SIZE * 0.8f * cameraZoom);
			}
			elementBox.x += elementSize * 1.5f;
		}
	}

	// mana bar (right side)
	int manaSlots = wand.maxMana;
	if (manaSlots < 1) { manaSlots = 1; }
	float segmentSize = PIXEL_SIZE * 6 * cameraZoom;
	float segmentSpacing = PIXEL_SIZE * 2 * cameraZoom;
	float barRight = renderer.windowW - PIXEL_SIZE * 6 * cameraZoom;
	float barY = PIXEL_SIZE * 10 * cameraZoom;
	float outlineWidth = PIXEL_SIZE * cameraZoom;
	if (outlineWidth < 1.0f) { outlineWidth = 1.0f; }

	glm::vec4 background = {0.05f, 0.08f, 0.15f, 0.6f};
	glm::vec4 fillColor = {0.2f, 0.5f, 1.0f, 0.9f};
	glm::vec4 outlineColor = {0.12f, 0.25f, 0.55f, 0.9f};

	float barWidth = manaSlots * segmentSize + (manaSlots - 1) * segmentSpacing;
	for (int i = 0; i < manaSlots; i++)
	{
		float segmentX = barRight - (i + 1) * segmentSize - i * segmentSpacing;
		glm::vec4 segment = {segmentX, barY, segmentSize, segmentSize};
		renderer.renderRectangle(segment, background);
		renderer.renderRectangleOutline(segment, outlineColor, outlineWidth);

		float fill = glm::clamp(currentMana - i, 0.0f, 1.0f);
		if (fill > 0.0f)
		{
			float fillWidth = segment.z * fill;
			glm::vec4 fillRect = {segment.x + (segment.z - fillWidth), segment.y,
				fillWidth, segment.w};
			renderer.renderRectangle(fillRect, fillColor);
		}
	}

	int maxElements = getMaxElements();
	float maxElementsBoxSize = segmentSize * 0.7f;
	float maxElementsBoxSpacing = PIXEL_SIZE * 1.2f * cameraZoom;
	float maxElementsRowWidth = maxElements * maxElementsBoxSize + (maxElements - 1) * maxElementsBoxSpacing;
	float maxElementsRowX = barRight - maxElementsRowWidth;
	float maxElementsRowY = barY + segmentSize + PIXEL_SIZE * 6 * cameraZoom;
	float outlineWidthSmall = PIXEL_SIZE * 0.6f * cameraZoom;
	if (outlineWidthSmall < PIXEL_SIZE * 0.3f) { outlineWidthSmall = PIXEL_SIZE * 0.3f; }

	// max elements per cast (right side, under mana)
	{
		for (int i = 0; i < maxElements; i++)
		{
			glm::vec4 box = {maxElementsRowX + i * (maxElementsBoxSize + maxElementsBoxSpacing),
				maxElementsRowY, maxElementsBoxSize, maxElementsBoxSize};
			renderer.renderRectangle(box, {0.08f, 0.08f, 0.08f, 0.5f});
			renderer.renderRectangleOutline(box, {0.6f, 0.6f, 0.6f, 0.7f}, outlineWidthSmall);
		}
	}

	// wand ring (right side, under mana)
	{
		glm::vec2 ringCenter = {maxElementsRowX + maxElementsRowWidth * 0.5f - PIXEL_SIZE * 18 * cameraZoom,
			maxElementsRowY + maxElementsBoxSize + PIXEL_SIZE * 24 * cameraZoom};
		float ringSize = segmentSize * 6.1f;
		float ringOffset = ringSize * 0.4f;
		float iconSize = ringSize * 0.34f;
		float textSize = iconSize * 0.55f;
		float textOffset = iconSize * 0.6f;

		glm::vec4 ringRect = {ringCenter.x - ringSize * 0.5f, ringCenter.y - ringSize * 0.5f,
			ringSize, ringSize};

		auto renderRingPiece = [&](gl2d::Texture t, const WandSlot &slot, int remaining)
		{
			bool hasElement = slot.type == WandSlotType::Element;
			glm::vec3 baseColor = hasElement ? elementToColor(slot.element) : glm::vec3{0.25f, 0.25f, 0.25f};
			if (hasElement && remaining <= 0)
			{
				baseColor = glm::mix(baseColor, glm::vec3{0.2f, 0.2f, 0.2f}, 0.6f);
			}
			float opacity = hasElement ? 0.9f : 0.4f;
			renderer.renderRectangle(ringRect, t, {baseColor, opacity});
		};

		renderRingPiece(assetsManager.upCircle, wand.up, remainingUp);
		renderRingPiece(assetsManager.downCircle, wand.down, remainingDown);
		renderRingPiece(assetsManager.leftCircle, wand.left, remainingLeft);
		renderRingPiece(assetsManager.rightCircle, wand.right, remainingRight);

		auto renderRingIcon = [&](const WandSlot &slot, int remaining, glm::vec2 dir)
		{
			if (slot.type != WandSlotType::Element) { return; }
			glm::vec2 center = ringCenter + dir * ringOffset;
			glm::vec4 iconRect = {center.x - iconSize * 0.5f, center.y - iconSize * 0.5f,
				iconSize, iconSize};
			renderer.renderRectangle(iconRect, assetsManager.elements.texture,
				{1, 1, 1, 0.9f}, {}, 0,
				assetsManager.elements.atlas.get(slot.element, 0));

			char countText[8] = {};
			snprintf(countText, sizeof(countText), "%d", std::max(0, remaining));
			glm::vec2 textPos = {center.x + textOffset, center.y + textSize * 0.35f};
			float textAlpha = remaining > 0 ? 0.9f : 0.5f;
			renderer.renderText(textPos, countText, assetsManager.font,
				{1, 1, 1, textAlpha}, textSize, 4, 3, false);
		};

		renderRingIcon(wand.up, remainingUp, {0, -1});
		renderRingIcon(wand.down, remainingDown, {0, 1});
		renderRingIcon(wand.left, remainingLeft, {-1, 0});
		renderRingIcon(wand.right, remainingRight, {1, 0});

		if (wand.alwaysCast.type == WandSlotType::Element)
		{
			float centerSize = ringSize * 0.38f;
			glm::vec4 centerRect = {ringCenter.x - centerSize * 0.5f, ringCenter.y - centerSize * 0.5f,
				centerSize, centerSize};
			renderer.renderRectangle(centerRect, assetsManager.elements.texture,
				{1, 1, 1, 0.9f}, {}, 0,
				assetsManager.elements.atlas.get(wand.alwaysCast.element, 0));
		}

		// quick action ring (only set actions)
		{
			bool hasAlwaysCast = wand.alwaysCast.type == WandSlotType::Element;
			float quickRingSize = ringSize * 0.82f;
			float quickRingOffset = quickRingSize * 0.4f;
			float quickIconSize = quickRingSize * 0.34f;
			glm::vec2 quickCenter = ringCenter + glm::vec2(0.0f, ringSize * 1.12f);
			glm::vec4 quickRingRect = {quickCenter.x - quickRingSize * 0.5f, quickCenter.y - quickRingSize * 0.5f,
				quickRingSize, quickRingSize};

			auto getQuickInfo = [&](const QuickAction &action, int &outElement, int &outCount)
			{
				if (action.count <= 0) { return false; }
				outElement = action.elements[0];
				outCount = action.count + (hasAlwaysCast ? 1 : 0);
				return outCount > 0;
			};

			auto renderQuickPiece = [&](gl2d::Texture t, const QuickAction &action)
			{
				int element = Elements::NoneElement;
				int count = 0;
				if (!getQuickInfo(action, element, count)) { return; }
				renderer.renderRectangle(quickRingRect, t, {0.25f, 0.25f, 0.25f, 0.85f});
			};

			renderQuickPiece(assetsManager.upCircle, wand.quickActions[0]);
			renderQuickPiece(assetsManager.downCircle, wand.quickActions[1]);
			renderQuickPiece(assetsManager.leftCircle, wand.quickActions[2]);
			renderQuickPiece(assetsManager.rightCircle, wand.quickActions[3]);

			auto renderQuickIcon = [&](const QuickAction &action, glm::vec2 dir)
			{
				int element = Elements::NoneElement;
				int count = 0;
				if (!getQuickInfo(action, element, count)) { return; }
				glm::vec2 center = quickCenter + dir * quickRingOffset;
				int maxElements = getMaxElements();
				if (maxElements < 1) { maxElements = 1; }
				float boxSize = quickIconSize * 0.18f;
				float boxGap = boxSize * 0.4f;
				float totalWidth = maxElements * boxSize + (maxElements - 1) * boxGap;
				float startX = center.x - totalWidth * 0.5f;
				float y = center.y + boxSize * 0.2f;
				for (int i = 0; i < maxElements; i++)
				{
					glm::vec4 boxRect = {startX + i * (boxSize + boxGap), y, boxSize, boxSize};
					if (i < count)
					{
						int elementIndex = i;
						if (hasAlwaysCast)
						{
							if (i == 0)
							{
								renderer.renderRectangle(boxRect, elementToColor(wand.alwaysCast.element));
								continue;
							}
							elementIndex = i - 1;
						}
						if (elementIndex >= 0 && elementIndex < action.count)
						{
							renderer.renderRectangle(boxRect, elementToColor(action.elements[elementIndex]));
						}
						else
						{
							renderer.renderRectangle(boxRect, {0.2f, 0.2f, 0.2f, 0.7f});
						}
					}
					else
					{
						renderer.renderRectangle(boxRect, {0.2f, 0.2f, 0.2f, 0.7f});
					}
				}
			};

			renderQuickIcon(wand.quickActions[0], {0, -1});
			renderQuickIcon(wand.quickActions[1], {0, 1});
			renderQuickIcon(wand.quickActions[2], {-1, 0});
			renderQuickIcon(wand.quickActions[3], {1, 0});
		}
	}

	renderer.popCamera();

	bool showManaBar = selectionActive || noManaDisplayTimer > 0.0f;
	// mana bar under player while selecting or failing due to no mana
	if (showManaBar)
	{
		int manaSlots = wand.maxMana;
		if (manaSlots < 1) { manaSlots = 1; }
		float segmentSize = PIXEL_SIZE * 3.5f;
		float segmentSpacing = PIXEL_SIZE * 1.0f;
		float outlineWidth = PIXEL_SIZE * 0.4f;
		if (outlineWidth < PIXEL_SIZE * 0.2f) { outlineWidth = PIXEL_SIZE * 0.2f; }

		glm::vec4 background = {0.05f, 0.08f, 0.15f, 0.6f};
		glm::vec4 fillColor = {0.2f, 0.5f, 1.0f, 0.9f};
		glm::vec4 outlineColor = {0.12f, 0.25f, 0.55f, 0.9f};

		float totalWidth = manaSlots * segmentSize + (manaSlots - 1) * segmentSpacing;
		glm::vec2 basePos = player.physics.getPos();
		float barX = basePos.x - totalWidth * 0.5f;
		float barY = basePos.y + player.physics.transform.size.y * 0.5f + PIXEL_SIZE * 2.5f;
		if (noManaShakeTimer > 0.0f)
		{
			float strength = std::min(1.0f, noManaShakeTimer / 0.2f);
			float phase = (0.2f - noManaShakeTimer) * 40.0f;
			float shake = std::sin(phase) * (PIXEL_SIZE * 1.4f) * strength;
			barX += shake;
		}

		for (int i = 0; i < manaSlots; i++)
		{
			float segmentX = barX + totalWidth - (i + 1) * segmentSize - i * segmentSpacing;
			glm::vec4 segment = {segmentX, barY, segmentSize, segmentSize};
			renderer.renderRectangle(segment, background);
			renderer.renderRectangleOutline(segment, outlineColor, outlineWidth);

			float fill = glm::clamp(currentMana - i, 0.0f, 1.0f);
			if (fill > 0.0f)
			{
				float fillWidth = segment.z * fill;
				glm::vec4 fillRect = {segment.x + (segment.z - fillWidth), segment.y,
					fillWidth, segment.w};
				renderer.renderRectangle(fillRect, fillColor);
			}
		}

	}

	if (castCooldownTimer > 0.0f)
	{
		int manaSlots = wand.maxMana;
		if (manaSlots < 1) { manaSlots = 1; }
		float segmentSize = PIXEL_SIZE * 3.5f;
		float segmentSpacing = PIXEL_SIZE * 1.0f;
		float totalWidth = manaSlots * segmentSize + (manaSlots - 1) * segmentSpacing;

		glm::vec2 basePos = player.physics.getPos();
		float barX = basePos.x - totalWidth * 0.5f;
		float barY = basePos.y + player.physics.transform.size.y * 0.5f + PIXEL_SIZE * 2.5f;

		float cooldownWidth = totalWidth;
		float cooldownHeight = PIXEL_SIZE * 1.4f;
		float cooldownY = showManaBar
			? barY + segmentSize + PIXEL_SIZE * 1.6f
			: barY + PIXEL_SIZE * 1.6f;
		float cooldownRatio = glm::clamp(castCooldownTimer / CAST_COOLDOWN, 0.0f, 1.0f);
		glm::vec4 cooldownRect = {barX, cooldownY, cooldownWidth, cooldownHeight};
		renderer.renderRectangle(cooldownRect, {0.1f, 0.1f, 0.1f, 0.5f});
		renderer.renderRectangleOutline(cooldownRect, {0.4f, 0.4f, 0.4f, 0.7f},
			PIXEL_SIZE * 0.3f);

		float fillWidth = cooldownWidth * cooldownRatio;
		glm::vec4 fillRect = {barX, cooldownY, fillWidth, cooldownHeight};
		renderer.renderRectangle(fillRect, {0.9f, 0.9f, 0.9f, 0.8f});
	}
}

void SleppSelectionInputLogic::resetSelectionForWand(const Wand &wand, SpellRecepie &spellRecepie, bool resetMana)
{
	spellRecepie.clear();
	executedFirstFrame = false;
	isDrawing = false;
	isClickSelection = false;
	mouseStart = {};
	dragDirection = 0;
	trail.clear();
	trailColor = {0.5f, 0.5f, 0.5f};
	trailColorStart = trailColor;
	trailTargetColor = trailColor;
	trailColorTimer = 0.0f;
	upPiece = {};
	downPiece = {};
	leftPiece = {};
	rightPiece = {};
	remainingUp = wand.up.type == WandSlotType::Element ? wand.up.castCount : 0;
	remainingDown = wand.down.type == WandSlotType::Element ? wand.down.castCount : 0;
	remainingLeft = wand.left.type == WandSlotType::Element ? wand.left.castCount : 0;
	remainingRight = wand.right.type == WandSlotType::Element ? wand.right.castCount : 0;
	remainingAlwaysCast = wand.alwaysCast.type == WandSlotType::Element ? 1 : 0;
	alwaysCastUsedThisCast = false;
	lastWand = wand;
	hasWandState = true;
	if (resetMana)
	{
		currentMana = 0.0f;
		manaInitialized = true;
		castCooldownTimer = 0.0f;
		pauseManaCharge = false;
	}
}
