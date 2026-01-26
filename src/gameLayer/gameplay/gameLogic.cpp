#include <gameplay/gameLogic.h>
#include <gameplay/map.h>
#include <imgui.h>
#include <platformInput.h>
#include <gameLayer.h>
#include <glui/glui.h>
#include <iostream>
#include <gameplay/entities/entity.h>
#include <gameplay/entities/enemyTypes.h>
#include <gameplay/spells/spellTypes.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <gameplay/elements.h>

#include <worldGen/roomGen.h>

bool GameLogic::init()
{
	
	RoomGenerator roomGenerator;
	roomGenerator.init();
	
	roomGenerator.generatePlainsRoom(70, 70, map, 1234);

	roomGenerator.clear();

	particlePostProcessRenderer.init();

	player.physics.getPos() = {35, 35};


	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {20, 20});
	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {22, 20});
	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {23, 25});
	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {25, 24});
	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {22, 25});
	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {23, 26});
	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {24, 18});
	entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), {25, 18});



	inGame = true;
	return true;
}

bool GameLogic::update(float deltaTime,
	gl2d::Renderer2D &renderer,
	AssetsManager &assetsManager)
{
	bool exitDungeon = false;


#pragma region imgui
	//ImGui::ShowDemoWindow();
	ImGui::Begin("Game Debug");

	ImGui::DragFloat2("Position", &player.physics.getPos()[0], 0.01);
	ImGui::DragFloat("zoom", &zoom);

	if (ImGui::Button("Exit"))
	{
		exitDungeon = true;
	}

	ImGui::End();
#pragma endregion

	glm::vec2 cursorPos = platform::getRelMousePosition();
	glm::vec4 viewRect = renderer.getViewRect();
	glm::vec2 screenCenter = {renderer.windowW / 2.f, renderer.windowH / 2.f};
	static bool usesController = 0;

#pragma region input
	{

		glm::vec2 move = {};
		if (platform::isButtonHeld(platform::Button::A))
		{
			move.x -= 1;
			usesController = false;
		}
		if (platform::isButtonHeld(platform::Button::D))
		{
			move.x += 1;
			usesController = false;
		}
		if (platform::isButtonHeld(platform::Button::W))
		{
			move.y -= 1;
			usesController = false;
		}
		if (platform::isButtonHeld(platform::Button::S))
		{
			move.y += 1;
			usesController = false;
		}

		if (glm::length(platform::getControllerButtons().LStick) > 0.1 || glm::length(platform::getControllerButtons().RStick) > 0.1)
		{
			usesController = true;
		}

		if (platform::isLMouseHeld() || platform::isRMouseHeld() || platform::mouseMoved())
		{
			usesController = false;
		}

		move += platform::getControllerButtons().LStick * glm::vec2(1, -1);

		if (glm::length(move) != 0)
		{
			move = glm::normalize(move);
			move *= deltaTime * 6.f; //player speed
		}

		player.physics.getPos() += move;
		player.animator.setAnimationBasedOnMovement(move);

		//fire dirrection
		{
			if (!usesController)
			{
				fireDirection = cursorPos - screenCenter;
			}
			else
			{
				auto c = platform::getControllerButtons().RStick;
				float l = glm::length(c);
				if (l > 0.4)
				{
					fireDirection = c * glm::vec2(1,-1);
				}
			}

			float l = glm::length(fireDirection);
			if (l <= 0.000000001)
			{
				fireDirection = {1,0};
			}
			else
			{
				fireDirection /= l;
			}
		}

		platform::showMouse(!usesController);

	}
#pragma endregion

#pragma region updates

	player.physics.resolveConstrains(map);

	player.physics.updateMove();


	projectiles.update(deltaTime, map, particleSystem, rng, entityHolder);


	particleSystem.update(deltaTime);
	damageViewerSystem.update(deltaTime);

#pragma endregion


	renderer.currentCamera.zoom = zoom;
	renderer.currentCamera.follow(player.physics.transform.getCenter(),
		deltaTime * 4.f, 0.00001, 0,
		renderer.windowW, renderer.windowH);

	particlePostProcessRenderer.updateWindowMetrics(renderer);

#pragma region rendering

	map.renderMap(renderer, assetsManager);

	map.renderWallShadows(renderer, assetsManager);

	spellsHolder.renderBeforeEntities(renderer, particlePostProcessRenderer);

	entityHolder.update(deltaTime, map, particleSystem, rng, player);
	entityHolder.render(renderer, particlePostProcessRenderer);


	//renderer.renderRectangle(player.physical.getAABB(), Colors_Red);
	player.update(deltaTime);
	player.render(renderer, assetsManager);

	projectiles.render(renderer, assetsManager, particlePostProcessRenderer);

	particleSystem.render(renderer, particlePostProcessRenderer, {});

	particlePostProcessRenderer.finalRender(renderer);

	map.renderMapAfterEntities(renderer, assetsManager);
	damageViewerSystem.render(renderer, assetsManager.font);

#pragma endregion

	// magic ui, todo move
	{


		auto tryAddElement = [&](int element)
		{
			spellRecepie.add(element, 4);
		};


		if (platform::isRMousePressed() || platform::getControllerButtons().RTButton.pressed)
		{

			//if (elementsLoaded.size())
			{
				auto spell =SpellTypes::getSpellFromRecepie(spellRecepie);

				spellsHolder.addSpell(std::move(spell),
				player.physics.getPos(), fireDirection);

				spellRecepie.clear();
			}


		}

		float cameraZoom = renderer.currentCamera.zoom;
		renderer.pushCamera();


		if(usesController)
		{
			float size = 15 * PIXEL_SIZE * cameraZoom;

			glm::vec2 pos = glm::vec2{renderer.windowW, renderer.windowH} / 2.f;
			pos += PIXEL_SIZE * cameraZoom * 30 * fireDirection;
			pos -= size / 2.f;

			glm::vec4 transform(pos,size,size);
			
			renderer.renderRectangle(transform, assetsManager.target, {1,1,1,0.5});
		}


		bool startSelectionButton = platform::isButtonHeld(platform::Button::Q) || platform::getControllerButtons().LTButton.held
			|| platform::getControllerButtons().buttons[platform::Controller::RThumb].held
			
			;
		bool startDraw = platform::isLMouseHeld();
		constexpr float TRAIL_TIMER = 0.55f;
		bool selectionActive = startSelectionButton || startDraw;
		auto &selection = spellSelectionState;

		{


			glui::Frame screenFrame({0,0,renderer.windowW, renderer.windowH});
			float selectorSize = PIXEL_SIZE * 96 * cameraZoom;

			//float selectorSize = std::min(renderer.windowW, renderer.windowH);
			//selectorSize /= 2;
			glm::vec2 selectionCenter = screenCenter;
			glm::vec4 mainBox = {};
			glm::ivec4 mainBoxFrame = {};
			float selectLength = (selectorSize / 6.f) * 1.5f;
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


			//0 1 2 3 4 -> none, up down left right
			auto detectTrailDirection = [&]()
			{
				glm::vec2 mouseEnd = platform::getRelMousePosition();
				return getDragDirection(mouseEnd - selection.mouseStart);
			};

			for (size_t i = 0; i < selection.trail.size();)
			{
				auto &point = selection.trail[i];
				point.timer -= deltaTime;
				if (point.timer <= 0.0f)
				{
					selection.trail[i] = selection.trail.back();
					selection.trail.pop_back();
					continue;
				}
				++i;
			}

			if (selectionActive)
			{

				if (!selection.executedFirstFrame)
				{
					selection.executedFirstFrame = true;

					selection.isDrawing = 0;
					selection.isClickSelection = 0;

					if (startSelectionButton)
					{
						selection.isClickSelection = true;
					}
					else if(startDraw)
					{
						selection.isDrawing = true;
						selection.mouseStart = platform::getRelMousePosition();
					}

					//platform::setRelMousePosition(screenCenter.x, screenCenter.y);
					selection.trail.clear();
					selection.dragDirection = 0;
				}

				if (selection.isDrawing)
				{
					selection.trail.push_back({platform::getRelMousePosition(), TRAIL_TIMER});
				}

				selectionCenter = selection.isDrawing ? selection.mouseStart : screenCenter;
				mainBox = {selectionCenter.x - selectorSize * 0.5f,
					selectionCenter.y - selectorSize * 0.5f,
					selectorSize, selectorSize};
				mainBoxFrame = {static_cast<int>(mainBox.x), static_cast<int>(mainBox.y),
					static_cast<int>(mainBox.z), static_cast<int>(mainBox.w)};

			#pragma region detect selections
				bool selectedUp = 0;
				bool selectedDown = 0;
				bool selectedLeft = 0;
				bool selectedRight = 0;

				bool hoveredUp = 0;
				bool hoveredDown = 0;
				bool hoveredLeft = 0;
				bool hoveredRight = 0;
				glm::vec2 cursorVector = cursorPos - selectionCenter;

				if (selection.isClickSelection)
				{
					//v1
					//glm::vec2 cursorVector = cursorPos - screenCenter;
					//
					//float selectLength = 200;
					//
					//glm::vec2 upVector = glm::vec2(0, -1) * selectLength;
					//glm::vec2 downVector = glm::vec2(0, +1) * selectLength;
					//glm::vec2 leftVector = glm::vec2(-1, 0) * selectLength;
					//glm::vec2 rightVector = glm::vec2(1, 0) * selectLength;
					//
					//
					//if (glm::dot(cursorVector, upVector) > selectLength * selectLength)
					//{
					//	platform::setRelMousePosition(screenCenter.x, screenCenter.y);
					//	selectedUp = true;
					//}else if (glm::dot(cursorVector, downVector) > selectLength * selectLength)
					//{
					//	platform::setRelMousePosition(screenCenter.x, screenCenter.y);
					//	selectedDown = true;
					//}
					//else if (glm::dot(cursorVector, leftVector) > selectLength * selectLength)
					//{
					//	platform::setRelMousePosition(screenCenter.x, screenCenter.y);
					//	selectedLeft = true;
					//}
					//else if (glm::dot(cursorVector, rightVector) > selectLength * selectLength)
					//{
					//	platform::setRelMousePosition(screenCenter.x, screenCenter.y);
					//	selectedRight = true;
					//}

					//v2
					glm::vec2 upVector = glm::vec2(0, -1) * selectLength;
					glm::vec2 downVector = glm::vec2(0, +1) * selectLength;
					glm::vec2 leftVector = glm::vec2(-1, 0) * selectLength;
					glm::vec2 rightVector = glm::vec2(1, 0) * selectLength;

					if (glm::length(cursorVector) < selectorSize * 0.55)
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

				if (selection.isClickSelection)
				{
					if (hoveredUp && platform::isLMousePressed()) { selectedUp = true; }
					if (hoveredDown && platform::isLMousePressed()) { selectedDown = true; }
					if (hoveredLeft && platform::isLMousePressed()) { selectedLeft = true; }
					if (hoveredRight && platform::isLMousePressed()) { selectedRight = true; }

					if (!selectedUp && !selectedDown && !selectedLeft && !selectedRight)
					{
						auto c = platform::getControllerButtons();
						
						if (c.RStickButtonUp.pressed) { selectedUp = true; } else
						if (c.RStickButtonDown.pressed) { selectedDown = true; } else
						if (c.RStickButtonLeft.pressed) { selectedLeft = true; } else
						if (c.RStickButtonRight.pressed) { selectedRight = true; }
					}
				}
				else if (selection.isDrawing)
				{
					int dragDir = getDragDirection(cursorVector);

					hoveredUp = dragDir == 1;
					hoveredDown = dragDir == 2;
					hoveredLeft = dragDir == 3;
					hoveredRight = dragDir == 4;

					if (dragDir == 0)
					{
						selection.dragDirection = 0;
					}
					else if (dragDir != selection.dragDirection)
					{
						selection.dragDirection = dragDir;
						selectedUp = dragDir == 1;
						selectedDown = dragDir == 2;
						selectedLeft = dragDir == 3;
						selectedRight = dragDir == 4;
					}
				}

			#pragma endregion


				//renderer.renderRectangle(mainBox, {1,0,0,0.1});


				{
					float opacity = 0.8;

					//glui::Frame mainBoxFrame(mainBox);

					struct CirclePiece
					{
						float animationTime = 0;


					};

					auto updateCirclePiece = [&](
						CirclePiece &c,
						gl2d::Texture t, int element, float opacity, bool selected, bool hovered)
					{
						glm::vec3 color = elementToColor(element);

						c.animationTime -= deltaTime * 2;
						c.animationTime = glm::clamp(c.animationTime, 0.f, 1.f);

						if (selected)
						{
							c.animationTime = 1.f;
							tryAddElement(element);
						}

						if (hovered) { c.animationTime = std::max(c.animationTime, 0.5f); }


						glm::vec3 finalColor = glm::mix(color, glm::vec3{1,1,1}, glm::vec3(c.animationTime));

						renderer.renderRectangle(mainBox, t, {finalColor, opacity});

					};

					static CirclePiece up;
					static CirclePiece down;
					static CirclePiece left;
					static CirclePiece right;

					updateCirclePiece(up, assetsManager.upCircle, Fire, opacity, selectedUp, hoveredUp);
					updateCirclePiece(down, assetsManager.downCircle, Earth, opacity, selectedDown, hoveredDown);
					updateCirclePiece(left, assetsManager.leftCircle, Ice, opacity, selectedLeft, hoveredLeft);
					updateCirclePiece(right, assetsManager.rightCircle, Water, opacity, selectedRight, hoveredRight);


				}

				//trail
				{
					int trailDir = detectTrailDirection();

					glm::vec4 color = {0.5,0.5,0.5,1};

					switch (trailDir)
					{
						case 1: color = elementToColor(Fire); break;
						case 2: color = elementToColor(Earth); break;
						case 3: color = elementToColor(Ice); break;
						case 4: color = elementToColor(Water); break;
					}

					color.a = 0.7;

					int sizeInt = 4;
					float trailSize = PIXEL_SIZE * sizeInt * cameraZoom;

					for (const auto &point : selection.trail)
					{
						//if (e.x % sizeInt != 0 || e.y % sizeInt != 0) { continue; }

						float normalized = glm::clamp(point.timer / TRAIL_TIMER, 0.0f, 1.0f);
						float fade = glm::pow(normalized, 0.6);
						glm::vec4 drawColor = color;
						drawColor.a *= fade;
						glm::vec4 pos = {point.pos.x, point.pos.y, trailSize, trailSize};

						//pos.x -= trailSize / 2.f;
						//pos.y -= trailSize / 2.f;

						renderer.renderRectangle(pos, drawColor);
					}
				}


			}
			else
			{
				selection.executedFirstFrame = false;
				selection.isClickSelection = false;
				selection.isDrawing = false;
				selection.mouseStart = {};
				selection.trail.clear();
				selection.dragDirection = 0;

				selectionCenter = screenCenter;
				mainBox = {selectionCenter.x - selectorSize * 0.5f,
					selectionCenter.y - selectorSize * 0.5f,
					selectorSize, selectorSize};
				mainBoxFrame = {static_cast<int>(mainBox.x), static_cast<int>(mainBox.y),
					static_cast<int>(mainBox.z), static_cast<int>(mainBox.w)};
			}


			//render loaded elements
			{
				glui::Frame inCircle(mainBoxFrame);

				float elementSize = PIXEL_SIZE * 4 * cameraZoom;

				auto elementBox = glui::Box().xCenter().yCenter().xDimensionPixels(elementSize).
					yDimensionPixels(elementSize)();

				elementBox.y -= PIXEL_SIZE * 16 * cameraZoom;
				elementBox.x -= PIXEL_SIZE * 8 * cameraZoom;

				for (int i=0; i< spellRecepie.count; i++)
				{
					renderer.renderRectangle(elementBox, elementToColor(spellRecepie.elements[i]));
					elementBox.x += elementSize * 1.5;
				}
			}


		}

		renderer.popCamera();
	}


	//we want the first frame of the spell to happen in the same frame it was cast
	spellsHolder.update(deltaTime, map, particleSystem,
		projectiles, rng, player, fireDirection);


	renderer.flush();
	return !exitDungeon;
}

void GameLogic::close()
{

	*this = {};
	inGame = 0;
}
