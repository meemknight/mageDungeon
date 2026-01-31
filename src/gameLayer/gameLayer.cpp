#define GLM_ENABLE_EXPERIMENTAL
#include "gameLayer.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "platformInput.h"
#include <iostream>
#include <sstream>
#include <platformTools.h>
#include <logs.h>
#include <SDL3/SDL.h>
#include <gl2d/gl2d.h>
#include <glui/glui.h>

#include <imguiTools.h>

#include <gameplay/gameLogic.h>
#include <gameplay/spellPreviewContext.h>
#include <gameplay/assetsManager.h>



static gl2d::Renderer2D renderer;
static GameLogic game;
static AssetsManager assetsManager;
static glui::RendererUi uirenderer;
static SpellPreviewContext *spellPreviewContext = nullptr;

AssetsManager &getAssetManager()
{
	return assetsManager;
}

ParticlePostProcessRenderer &getParticlePostProcessRenderer()
{
	if (spellPreviewContext) { return spellPreviewContext->particleRenderer; }
	return game.particlePostProcessRenderer;
}

DamageViewerSystem &getDamageViewerSystem()
{
	if (spellPreviewContext) { return spellPreviewContext->damageViewer; }
	return game.damageViewerSystem;
}

ProjectileHolder &getProjectileHolder()
{
	if (spellPreviewContext) { return spellPreviewContext->projectiles; }
	return game.projectiles;
}

SummonHolder &getSummonHolder()
{
	if (spellPreviewContext) { return spellPreviewContext->summons; }
	return game.summons;
}

glm::vec2 getFireDirection()
{
	return game.fireDirection;
}

glm::vec2 getFireTargetPos()
{
	return game.fireTargetPos;
}

StandbyProjectileSystem &getStandbyProjectilesSystem()
{
	if (spellPreviewContext) { return spellPreviewContext->standbyProjectiles; }
	return game.standbyProjectiles;
}

void setSpellPreviewContext(SpellPreviewContext *context)
{
	spellPreviewContext = context;
}

void clearSpellPreviewContext()
{
	spellPreviewContext = nullptr;
}

gl2d::Renderer2D &getRenderer()
{
	return renderer;
}


bool initGame(SDL_Renderer *sdlRenderer)
{

	gl2d::init();


	renderer.create(sdlRenderer);
	uirenderer.SetAlignModeFixedSizeWidgets({0,150});
	assetsManager.loadAllAssets();

	game.init();


	return true;
}


bool gameLogic(float deltaTime, platform::Input &input, SDL_Renderer *sdlRenderer)
{



#pragma region init stuff
	int w = 0; int h = 0;
	w = platform::getFrameBufferSizeX(); //window w
	h = platform::getFrameBufferSizeY(); //window h
	
	renderer.updateWindowMetrics(w, h);

	renderer.clearScreen();

#pragma endregion


	if (game.inGame)
	{
		if (!game.update(deltaTime,
			renderer, assetsManager, input))
		{
			game.close();
		}

	}
	else
	{

		uirenderer.Begin(1);

		uirenderer.Text("Lowest Level Dungeon XD", Colors_White);

		//todo (LLGD): add a nice texture here for the button.
		if (uirenderer.Button("Play", Colors_White))
		{
			game.init();

		}

		uirenderer.BeginMenu("Load game...", Colors_White, {});
		{
			uirenderer.Text("Ther's nothing here :((", Colors_White);


		}
		uirenderer.EndMenu();

		uirenderer.End();


		uirenderer.renderFrame(renderer, assetsManager.font,
			platform::getRelMousePosition(),
			platform::isLMousePressed(),
			platform::isLMouseHeld(), platform::isLMouseReleased(),
			platform::isButtonReleased(platform::Button::Escape),
			platform::getTypedInput(), deltaTime);


		renderer.flush();

	}





	return true;
#pragma endregion

}

//This function might not be be called if the program is forced closed
void closeGame()
{



}
