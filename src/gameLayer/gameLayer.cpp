#define GLM_ENABLE_EXPERIMENTAL
#include "gameLayer.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "platformInput.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <platformTools.h>
#include <logs.h>
#include <SDL3/SDL.h>
#include <gl2d/gl2d.h>
#include <glui/glui.h>

#include <imguiTools.h>

#include <gameplay/gameLogic.h>
#include <gameplay/spellPreviewContext.h>
#include <gameplay/assetsManager.h>
#include "gameplay/aStar.h"
#include <particles/particleCreator.h>



static gl2d::Renderer2D renderer;
static GameLogic game;
static AssetsManager assetsManager;
static glui::RendererUi uirenderer;
static SpellPreviewContext *spellPreviewContext = nullptr;

// Rebuilds shader binaries in development and reloads GPU shader objects.
static void tryHotReloadShaders()
{
#if defined(_WIN32) && defined(DEVELOPLEMT_BUILD) && (DEVELOPLEMT_BUILD == 1)
	std::string scriptPath = std::string(RESOURCES_PATH) + "shaders/compile_all_shaders.bat";
	for (char &c : scriptPath)
	{
		if (c == '/') { c = '\\'; }
	}

	std::ifstream scriptFile(scriptPath);
	if (!scriptFile.is_open())
	{
		platform::log("Shader reload failed: missing resources\\shaders\\compile_all_shaders.bat",
			LogManager::logError);
		return;
	}

	const std::string command = std::string("cmd.exe /C call \"") + scriptPath + "\"";
	int result = std::system(command.c_str());
	if (result != 0)
	{
		platform::log("Shader reload failed: compile_all_shaders.bat returned an error",
			LogManager::logError);
		return;
	}

	renderer.reloadGpuShaders();
	game.gameHdrPostProcess.reloadShaders();
	game.particlePostProcessRenderer.bloom.reloadShaders();
	if (spellPreviewContext)
	{
		spellPreviewContext->particleRenderer.bloom.reloadShaders();
	}

	platform::log("Hot reloaded shaders", LogManager::logNormal);
#else
	platform::log("Shader reload is only enabled on Windows development builds", LogManager::logWarning);
#endif
}

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

Player &getPlayer()
{
	if (spellPreviewContext) { return spellPreviewContext->player; }
	return game.player;
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

// Burst of brown chips when a decoration breaks.
static void emitDecorationBreakParticles(ParticleSystem &system,
	std::ranlux24_base &rng, glm::vec2 pos)
{
	glm::vec4 startColor = {0.52f, 0.34f, 0.18f, 0.9f};
	glm::vec4 endColor = {0.28f, 0.18f, 0.1f, 0.6f};
	ParticleSettings chips = getSmallSquareParticle(startColor, endColor);
	chips.onCreateCount = 6;
	chips.particleLifeTime = {0.22f, 0.4f};
	chips.velocityX = glm::vec2{-18.0f, 18.0f} * PIXEL_SIZE;
	chips.velocityY = glm::vec2{-10.0f, -4.0f} * PIXEL_SIZE;
	chips.dragX = glm::vec2{-25.0f, -45.0f} * PIXEL_SIZE;
	chips.dragY = glm::vec2{-25.0f, -45.0f} * PIXEL_SIZE;
	chips.createApearence.size = glm::vec2{3.2f, 4.2f} * PIXEL_SIZE;
	chips.endApearence.size = glm::vec2{2.0f, 3.0f} * PIXEL_SIZE;
	chips.positionX = glm::vec2{-4.0f, 4.0f} * PIXEL_SIZE;
	chips.positionY = glm::vec2{-4.0f, 4.0f} * PIXEL_SIZE;
	system.emitParticles(chips, pos, rng, pos);
}

static void emitSpikeTrapTriggerParticles(ParticleSystem &system,
	std::ranlux24_base &rng, glm::vec2 pos)
{
	glm::vec4 startColor = {1.0f, 0.35f, 0.15f, 0.9f};
	glm::vec4 endColor = {0.85f, 0.15f, 0.05f, 0.5f};
	ParticleSettings sparks = getSmallSquareParticle(startColor, endColor);
	sparks.onCreateCount = 8;
	sparks.particleLifeTime = {0.2f, 0.4f};
	sparks.velocityX = glm::vec2{-25.0f, 25.0f} * PIXEL_SIZE;
	sparks.velocityY = glm::vec2{-18.0f, -4.0f} * PIXEL_SIZE;
	sparks.dragX = glm::vec2{-80.0f, -140.0f} * PIXEL_SIZE;
	sparks.dragY = glm::vec2{-80.0f, -140.0f} * PIXEL_SIZE;
	sparks.positionX = glm::vec2{-3.0f, 3.0f} * PIXEL_SIZE;
	sparks.positionY = glm::vec2{-3.0f, 3.0f} * PIXEL_SIZE;
	system.emitParticles(sparks, pos, rng, pos);
}

// Breakable decorations are small map markers that can be destroyed on contact.
int breakDecorationsAtCollider(const Transform2D &collider)
{
	if (spellPreviewContext) { return 0; }
	auto &decorations = game.breakableDecorations.positions;
	if (decorations.empty()) { return 0; }

	Transform2D decorationCollider = {};
	decorationCollider.size = {1.0f, 1.0f};
	decorationCollider.isCircleCollider = true;

	int broken = 0;
	for (size_t i = 0; i < decorations.size(); )
	{
		glm::ivec2 tilePos = decorations[i];
		glm::vec2 breakPos = {tilePos.x + 0.5f, tilePos.y + 0.5f};
		decorationCollider.pos = breakPos;
		if (decorationCollider.intersectTransform(collider))
		{
			emitDecorationBreakParticles(game.particleSystem, game.rng, breakPos);
			decorations[i] = decorations.back();
			decorations.pop_back();
			broken++;
			continue;
		}
		++i;
	}

	return broken;
}

int breakDecorationsInRadius(Map *map, glm::vec2 center, float radius, bool useLineOfSight)
{
	if (spellPreviewContext) { return 0; }
	if (radius <= 0.0f) { return 0; }
	auto &decorations = game.breakableDecorations.positions;
	if (decorations.empty()) { return 0; }

	float radius2 = radius * radius;
	glm::ivec2 originTile = WorldToTile(center);
	int broken = 0;
	for (size_t i = 0; i < decorations.size(); )
	{
		glm::ivec2 tilePos = decorations[i];
		glm::vec2 breakPos = {tilePos.x + 0.5f, tilePos.y + 0.5f};
		glm::vec2 diff = breakPos - center;
		if (glm::dot(diff, diff) <= radius2)
		{
			if (useLineOfSight && map && !HasLineOfSightGrid(*map, originTile, WorldToTile(breakPos)))
			{
				++i;
				continue;
			}
			emitDecorationBreakParticles(game.particleSystem, game.rng, breakPos);
			decorations[i] = decorations.back();
			decorations.pop_back();
			broken++;
			continue;
		}
		++i;
	}

	return broken;
}

int triggerSpikeTrapsInRadius(glm::vec2 center, float radius)
{
	if (spellPreviewContext) { return 0; }
	if (radius <= 0.0f) { return 0; }
	auto &spikes = game.trapSpikes.spikes;
	if (spikes.empty()) { return 0; }

	float radius2 = radius * radius;
	int triggered = 0;
	for (auto &spike : spikes)
	{
		glm::vec2 pos = {spike.pos.x + 0.5f, spike.pos.y + 0.5f};
		glm::vec2 diff = pos - center;
		if (glm::dot(diff, diff) > radius2) { continue; }

		if (spike.state == TrapSpikeElement::State::Closed)
		{
			spike.state = TrapSpikeElement::State::OpeningDelay;
			spike.stateTimer = SpikeTrapSettings::OpenDelaySeconds;
			spike.queuedOpen = false;
			emitSpikeTrapTriggerParticles(game.particleSystem, game.rng, pos);
		}
		else if (spike.state == TrapSpikeElement::State::OpeningDelay)
		{
			spike.stateTimer = SpikeTrapSettings::OpenDelaySeconds;
		}
		else if (spike.state == TrapSpikeElement::State::Open)
		{
			spike.stateTimer = SpikeTrapSettings::OpenHoldSeconds;
		}
		else if (spike.state == TrapSpikeElement::State::Closing)
		{
			spike.queuedOpen = true;
		}
		triggered++;
	}

	return triggered;
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
	#if GL2D_USE_SDL_GPU
	if (!renderer.gpuDevice)
	{
		platform::log("SDL_gpu device unavailable on this platform; GPU post-process effects are disabled.",
			LogManager::logWarning);
	}
	#endif
	//uirenderer.SetAlignModeFixedSizeWidgets({0,150});
	assetsManager.loadAllAssets();

	//game.init();


	return true;
}


bool gameLogic(float deltaTime, platform::Input &input, SDL_Renderer *sdlRenderer)
{



#pragma region init stuff
	int w = 0; int h = 0;
	w = platform::getFrameBufferSizeX(); //window w
	h = platform::getFrameBufferSizeY(); //window h
	
	renderer.updateWindowMetrics(w, h);

	if (input.isButtonPressed(platform::Button::F1))
	{
		platform::setFullScreen(!platform::isFullScreen());
	}

	if (input.isButtonPressed(platform::Button::F5))
	{
		tryHotReloadShaders();
	}

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

		uirenderer.Text("Mages Dungeon", Colors_White);
		uirenderer.Text("Work in progress", Colors_White);
		uirenderer.Text("The game is hard and it has lots of spells!\nI don't want you to miss on the fun.", Colors_Gray);
		uirenderer.Text("You can press F6 to give yourself\nan OP wand and have fun!", Colors_Gray);
		uirenderer.Text("You can also press F10 to see the debug menu with all the spells! ", Colors_White);
		uirenderer.Text("F1 for full screen! ", Colors_White);
		

		//todo (LLGD): add a nice texture here for the button.
		if (uirenderer.Button("Play", Colors_White))
		{
			game.init();

		}

		uirenderer.End();


		uirenderer.renderFrame(renderer, assetsManager.font,
			platform::getRelMousePosition(),
			platform::isLMousePressed(),
			platform::isLMouseHeld(), platform::isLMouseReleased(),
			platform::isButtonReleased(platform::Button::Escape),
			platform::getTypedInput(), deltaTime);


		#if GL2D_USE_SDL_GPU
		if (!renderer.gpuDevice)
		#endif
		{
			renderer.flush();
		}

	}





	return true;
#pragma endregion

}

//This function might not be be called if the program is forced closed
void closeGame()
{



}
