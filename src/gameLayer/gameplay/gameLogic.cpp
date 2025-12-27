#include <gameplay/gameLogic.h>
#include <gameplay/map.h>
#include <imgui.h>
#include <platformInput.h>


bool GameLogic::init()
{
	
	map.create();

	player.physical.getPos() = {1, 1};


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


	player.physical.resolveConstrains(map);

	player.physical.updateMove();


	renderer.currentCamera.zoom = zoom;
	renderer.currentCamera.follow(player.physical.transform.getCenter(),
		deltaTime * 4.f, 0.01, 0.5,
		renderer.windowW, renderer.windowH);


	map.renderMap(renderer, assetsManager);


	//renderer.renderRectangle(player.physical.getAABB(), Colors_Red);
	player.update(deltaTime);
	player.render(renderer, assetsManager);


	map.renderMapAfterEntities(renderer, assetsManager);


	renderer.flush();
	return !exitDungeon;
}

void GameLogic::close()
{

	*this = {};
	inGame = 0;
}
