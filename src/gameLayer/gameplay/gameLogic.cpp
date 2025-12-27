#include <gameplay/gameLogic.h>
#include <gameplay/map.h>
#include <imgui.h>
#include <platformInput.h>
#include <gameLayer.h>
#include <glui/glui.h>
#include <unordered_set>
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


	player.physical.getPos() = {35, 35};


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

	ImGui::DragFloat2("Position", &player.physical.getPos()[0], 0.01);
	ImGui::DragFloat("zoom", &zoom);

	if (ImGui::Button("Exit"))
	{
		exitDungeon = true;
	}

	ImGui::End();
#pragma endregion

#pragma region input
	glm::vec2 move = {};
	if (platform::isButtonHeld(platform::Button::A))
	{
		move.x -= 1;
	}
	if (platform::isButtonHeld(platform::Button::D))
	{
		move.x += 1;
	}
	if (platform::isButtonHeld(platform::Button::W))
	{
		move.y -= 1;
	}
	if (platform::isButtonHeld(platform::Button::S))
	{
		move.y += 1;
	}

	if (glm::length(move) != 0)
	{
		move = glm::normalize(move);
		move *= deltaTime * 6.f; //player speed
	}

	player.physical.getPos() += move;
	player.animator.setAnimationBasedOnMovement(move);



#pragma endregion

#pragma region updates

	player.physical.resolveConstrains(map);

	player.physical.updateMove();


	projectiles.update(deltaTime, map, particleSystem, rng);


	particleSystem.update(deltaTime);

#pragma endregion


	renderer.currentCamera.zoom = zoom;
	renderer.currentCamera.follow(player.physical.transform.getCenter(),
		deltaTime * 4.f, 0.00001, 0,
		renderer.windowW, renderer.windowH);


	map.renderMap(renderer, assetsManager);


	//renderer.renderRectangle(player.physical.getAABB(), Colors_Red);
	player.update(deltaTime);
	player.render(renderer, assetsManager);

	projectiles.render(renderer, assetsManager);

	particleSystem.render(renderer, {});


	map.renderMapAfterEntities(renderer, assetsManager);



	// magic ui, todo move
	{

		auto fireProjectile = [&](glm::vec2 dir)
		{
			float l = glm::length(dir);
			if (l <= 0.000000001)
			{
				dir = {1,0};
			}
			else
			{
				dir /= l;
			}

			Projectile p;
			p.physics.teleport(player.physical.getPos());
			p.physics.velocity = dir * 10.f;

			projectiles.projectiles.push_back(p);

		};


		static std::vector<int> elementsLoaded;
		auto tryAddElement = [&](int element)
		{
			if(elementsLoaded.size() < 4)
				elementsLoaded.push_back(element);
		};

		glm::vec2 cursorPos = platform::getRelMousePosition();
		glm::vec4 viewRect = renderer.getViewRect();
		glm::vec2 screenCenter = {renderer.windowW / 2.f, renderer.windowH / 2.f};

		if (platform::isRMousePressed())
		{
			//cast spell
			fireProjectile(cursorPos - screenCenter);

			elementsLoaded.clear();
		}

		float cameraZoom = renderer.currentCamera.zoom;
		renderer.pushCamera();

	

		bool startSelectionButton = platform::isButtonHeld(platform::Button::Q);
		bool startDraw = platform::isLMouseHeld();

		static bool executedFirstFrame = 0;

		static bool isDrawing = 0;
		static bool isClickSelection = 0;
		static glm::vec2 mouseStart = {};

		static std::unordered_set<glm::ivec2> trail;

		{


			glui::Frame screenFrame({0,0,renderer.windowW, renderer.windowH});
			float selectorSize = PIXEL_SIZE * 96 * cameraZoom;

			//float selectorSize = std::min(renderer.windowW, renderer.windowH);
			//selectorSize /= 2;

			auto mainBox = glui::Box().xCenter().yCenter()
				.xDimensionPixels(selectorSize).yDimensionPixels(selectorSize)();


			//0 1 2 3 4 -> none, up down left right
			auto detectTrailDirection = [&]()
			{
				glm::vec2 mouseEnd = platform::getRelMousePosition();

				glm::vec2 cursorVector = mouseEnd - mouseStart;

				float selectLength = 150;

				glm::vec2 upVector = glm::vec2(0, -1) * selectLength;
				glm::vec2 downVector = glm::vec2(0, +1) * selectLength;
				glm::vec2 leftVector = glm::vec2(-1, 0) * selectLength;
				glm::vec2 rightVector = glm::vec2(1, 0) * selectLength;


				if (glm::dot(cursorVector, upVector) > selectLength * selectLength)
				{
					return 1;
				}
				else if (glm::dot(cursorVector, downVector) > selectLength * selectLength)
				{
					return 2;
				}
				else if (glm::dot(cursorVector, leftVector) > selectLength * selectLength)
				{
					return 3;
				}
				else if (glm::dot(cursorVector, rightVector) > selectLength * selectLength)
				{
					return 4;
				}

				return 0;
			};

			//end drawing
			if (isDrawing && !startDraw)
			{
				int dir = detectTrailDirection();

				switch (dir)
				{
					case 1: tryAddElement(Fire); break;
					case 2: tryAddElement(Earth); break;
					case 3: tryAddElement(Ice); break;
					case 4: tryAddElement(Water); break;
				}

			}


			if (startSelectionButton || startDraw)
			{

				if (!executedFirstFrame)
				{
					executedFirstFrame = true;

					isDrawing = 0;
					isClickSelection = 0;

					if (startSelectionButton)
					{
						isClickSelection = true;
					}
					else if(startDraw)
					{
						isDrawing = true;
						mouseStart = platform::getRelMousePosition();
					}

					//platform::setRelMousePosition(screenCenter.x, screenCenter.y);
					trail = {};
				}

				if (isDrawing)
				{
					trail.insert(platform::getRelMousePosition());
				}

			#pragma region detect selections
				bool selectedUp = 0;
				bool selectedDown = 0;
				bool selectedLeft = 0;
				bool selectedRight = 0;

				bool hoveredUp = 0;
				bool hoveredDown = 0;
				bool hoveredLeft = 0;
				bool hoveredRight = 0;

				if(isClickSelection)
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
					float selectLength = (selectorSize / 6.f) * 1.5;
					glm::vec2 cursorVector = cursorPos - screenCenter;

					glm::vec2 upVector = glm::vec2(0, -1) * selectLength;
					glm::vec2 downVector = glm::vec2(0, +1) * selectLength;
					glm::vec2 leftVector = glm::vec2(-1, 0) * selectLength;
					glm::vec2 rightVector = glm::vec2(1, 0) * selectLength;

					if (glm::length(cursorVector) < selectorSize * 0.55)
					{
						if (glm::dot(cursorVector, upVector) > selectLength * selectLength)
						{
							hoveredUp = true;

							if (platform::isLMousePressed())
							{
								selectedUp = true;
							}

						}
						else if (glm::dot(cursorVector, downVector) > selectLength * selectLength)
						{
							hoveredDown = true;

							if (platform::isLMousePressed())
							{
								selectedDown = true;
							}
						}
						else if (glm::dot(cursorVector, leftVector) > selectLength * selectLength)
						{
							hoveredLeft = true;

							if (platform::isLMousePressed())
							{
								selectedLeft = true;
							}
						}
						else if (glm::dot(cursorVector, rightVector) > selectLength * selectLength)
						{
							hoveredRight = true;

							if (platform::isLMousePressed())
							{
								selectedRight = true;
							}
						}
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

					for (auto e : trail)
					{
						//if (e.x % sizeInt != 0 || e.y % sizeInt != 0) { continue; }

						glm::vec4 pos = {e, trailSize, trailSize};

						//pos.x -= trailSize / 2.f;
						//pos.y -= trailSize / 2.f;

						renderer.renderRectangle(pos, color);
					}

				}


			}
			else
			{
				executedFirstFrame = false;
				isClickSelection = false;
				isDrawing = false;
				mouseStart = {};
				trail = {};
			}


			//render loaded elements
			{
				glui::Frame inCircle(mainBox);

				float elementSize = PIXEL_SIZE * 4 * cameraZoom;

				auto elementBox = glui::Box().xCenter().yCenter().xDimensionPixels(elementSize).
					yDimensionPixels(elementSize)();

				elementBox.y -= PIXEL_SIZE * 16 * cameraZoom;
				elementBox.x -= PIXEL_SIZE * 8 * cameraZoom;

				for (auto e : elementsLoaded)
				{
					renderer.renderRectangle(elementBox, elementToColor(e));
					elementBox.x += elementSize * 1.5;
				}
			}


		}

		renderer.popCamera();
	}



	renderer.flush();
	return !exitDungeon;
}

void GameLogic::close()
{

	*this = {};
	inGame = 0;
}
