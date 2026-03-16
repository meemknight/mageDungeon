#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>

#include "platformTools.h"
#include "platformInput.h"
#include "gameLayer.h"
#include "stringManipulation.h"

//#include <raudio.h>

#include "imguiTools.h"

#if REMOVE_IMGUI == 0
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#if GL2D_USE_SDL_GPU
#include "backends/imgui_impl_sdlgpu3.h"
#endif
#include "imguiThemes.h"
#include "IconsForkAwesome.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#undef min
#undef max

#pragma region globals
static SDL_Window *window = nullptr;
static SDL_Renderer *sdlRenderer = nullptr;

bool currentFullScreen = false;
bool fullScreen = false;
bool windowFocus = true;
int mouseMovedFlag = 0;

static bool gShouldQuit = false;
static std::chrono::high_resolution_clock::time_point gLast;
#if REMOVE_IMGUI == 0
static bool gUseImguiGpuRenderer = false;
#endif
#pragma endregion

static SDL_Renderer *createRendererPreferVulkan(SDL_Window *window)
{
	if (!window) { return nullptr; }

#if !GL2D_USE_SDL_GPU
	// Legacy gl2d backend keeps SDL renderer default selection.
	return SDL_CreateRenderer(window, nullptr);
#else

	// Prefer Vulkan so the gl2d GPU path can reuse the renderer's Vulkan device.
	int driverCount = SDL_GetNumRenderDrivers();
	for (int i = 0; i < driverCount; i++)
	{
		const char *driverName = SDL_GetRenderDriver(i);
		if (driverName && SDL_strcmp(driverName, "vulkan") == 0)
		{
			auto renderer = SDL_CreateRenderer(window, "vulkan");
			if (renderer)
			{
				return renderer;
			}

			std::cout << "SDL Vulkan renderer init failed, falling back: "
				<< SDL_GetError() << "\n";
			break;
		}
	}

	return SDL_CreateRenderer(window, nullptr);
#endif
}

static bool isGl2dGpuBackendActive()
{
#if GL2D_USE_SDL_GPU
	// gl2d presents through SDL_gpu when a GPU device is available.
	return getRenderer().gpuDevice != nullptr;
#else
	return false;
#endif
}

#if REMOVE_IMGUI == 0 && GL2D_USE_SDL_GPU
static void renderImguiInGpuPass(SDL_GPUCommandBuffer *commandBuffer,
	SDL_GPURenderPass *renderPass,
	void *)
{
	if (!gUseImguiGpuRenderer || !commandBuffer) { return; }

	ImDrawData *drawData = ImGui::GetDrawData();
	if (!drawData) { return; }

	// Prepare runs before pass (renderPass == nullptr), render runs inside pass.
	if (!renderPass)
	{
		ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);
	}
	else
	{
		ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, renderPass);
	}
}
#endif

namespace platform
{

	SDL_Renderer *getSdlRenderer()
	{
		return ::sdlRenderer;
	}

	void setRelMousePosition(int x, int y)
	{
		SDL_WarpMouseInWindow(window, (float)x, (float)y);
	}

	bool isFullScreen() { return fullScreen; }
	void setFullScreen(bool f) { fullScreen = f; }

	glm::ivec2 getFrameBufferSize()
	{
		int w, h;
		SDL_GetWindowSizeInPixels(window, &w, &h);
		return {w, h};
	}

	glm::ivec2 getRelMousePosition()
	{
		float x, y;
		SDL_GetMouseState(&x, &y);
		int windowW = 0;
		int windowH = 0;
		SDL_GetWindowSize(window, &windowW, &windowH);
		int pixelW = 0;
		int pixelH = 0;
		SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);

		float scaleX = (windowW > 0) ? ((float)pixelW / (float)windowW) : 1.0f;
		float scaleY = (windowH > 0) ? ((float)pixelH / (float)windowH) : 1.0f;
		return {(int)(x * scaleX), (int)(y * scaleY)};
	}

	glm::ivec2 getWindowSize()
	{
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		return {w, h};
	}

	bool showState = true;
	void showMouse(bool show)
	{
		if (show)
		{
			if (!showState)
			{
				showState = true;
				SDL_SetWindowRelativeMouseMode(window, false);
				SDL_ShowCursor();
			}
		}
		else
		{
			SDL_HideCursor();

			if (showState)
			{
				showState = false;
				SDL_SetWindowRelativeMouseMode(window, true);
				SDL_HideCursor();
			}
		}
	}

	bool hasFocused() { return windowFocus; }
	bool mouseMoved() { return mouseMovedFlag; }

	bool writeEntireFile(const char *name, void *buffer, size_t size)
	{
		std::ofstream f(name, std::ios::binary);
		if (!f.is_open()) return false;
		f.write((char *)buffer, size);
		return true;
	}
}

static void handleSDLEvent(const SDL_Event &e)
{
	switch (e.type)
	{
	case SDL_EVENT_QUIT:
#ifdef __EMSCRIPTEN__
	gShouldQuit = true;
#else
	exit(0);
#endif
	break;

	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	windowFocus = true;
	break;

	case SDL_EVENT_WINDOW_FOCUS_LOST:
	windowFocus = false;
	platform::internal::resetInputsToZero();
	break;

	case SDL_EVENT_MOUSE_MOTION:
	mouseMovedFlag = 1;
	break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	{
		bool state = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
		if (e.button.button == SDL_BUTTON_LEFT)
			platform::internal::setLeftMouseState(state);
		if (e.button.button == SDL_BUTTON_RIGHT)
			platform::internal::setRightMouseState(state);
	} break;

	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
	{
		bool state = (e.type == SDL_EVENT_KEY_DOWN);

		SDL_Keycode key = e.key.key;

		if (key >= SDLK_A && key <= SDLK_Z)
			platform::internal::setButtonState(
			platform::Button::A + (key - SDLK_A), state);

		else if (key >= SDLK_0 && key <= SDLK_9)
			platform::internal::setButtonState(
			platform::Button::NR0 + (key - SDLK_0), state);

		else
		{
			if (key == SDLK_SPACE) platform::internal::setButtonState(platform::Button::Space, state);
			if (key == SDLK_RETURN) platform::internal::setButtonState(platform::Button::Enter, state);
			if (key == SDLK_ESCAPE) platform::internal::setButtonState(platform::Button::Escape, state);
			if (key == SDLK_UP) platform::internal::setButtonState(platform::Button::Up, state);
			if (key == SDLK_DOWN) platform::internal::setButtonState(platform::Button::Down, state);
			if (key == SDLK_LEFT) platform::internal::setButtonState(platform::Button::Left, state);
			if (key == SDLK_RIGHT) platform::internal::setButtonState(platform::Button::Right, state);
			if (key == SDLK_LCTRL) platform::internal::setButtonState(platform::Button::LeftCtrl, state);
			if (key == SDLK_TAB) platform::internal::setButtonState(platform::Button::Tab, state);
			if (key == SDLK_LSHIFT) platform::internal::setButtonState(platform::Button::LeftShift, state);
			if (key == SDLK_LALT) platform::internal::setButtonState(platform::Button::LeftAlt, state);
			if (key == SDLK_F1) platform::internal::setButtonState(platform::Button::F1, state);
			if (key == SDLK_F6) platform::internal::setButtonState(platform::Button::F6, state);
			if (key == SDLK_F5) platform::internal::setButtonState(platform::Button::F5, state);
			if (key == SDLK_F7) platform::internal::setButtonState(platform::Button::F7, state);
			if (key == SDLK_F8) platform::internal::setButtonState(platform::Button::F8, state);
			if (key == SDLK_F9) platform::internal::setButtonState(platform::Button::F9, state);
			if (key == SDLK_F10) platform::internal::setButtonState(platform::Button::F10, state);
		}
	} break;

	case SDL_EVENT_TEXT_INPUT:
	{
		char c = e.text.text[0];
		if (c < 127)
			platform::internal::addToTypedInput(c);
	} break;
	}
}

void updateFullscreen()
{
	if (currentFullScreen == fullScreen)
		return;

	currentFullScreen = fullScreen;

	if (fullScreen)
	{
		SDL_SetWindowFullscreen(window, 1);
	}
	else
	{
		SDL_SetWindowFullscreen(window, 0);
	}
}

static bool tickOneFrame()
{
	updateFullscreen();

	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
	#if REMOVE_IMGUI == 0
		ImGui_ImplSDL3_ProcessEvent(&e);
	#endif
		handleSDLEvent(e);
	}

	auto now = std::chrono::high_resolution_clock::now();
	float dt = std::chrono::duration<float>(now - gLast).count();
	gLast = now;
	dt = std::min(dt, 1.f / 10.f);

	bool gl2dGpuBackendActive = isGl2dGpuBackendActive();

#if REMOVE_IMGUI == 0
	#if GL2D_USE_SDL_GPU
	if (gUseImguiGpuRenderer)
	{
		ImGui_ImplSDLGPU3_NewFrame();
	}
	else
	#endif
	{
		ImGui_ImplSDLRenderer3_NewFrame();
	}
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGui::PushStyleColor(ImGuiCol_WindowBg, {});
	ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, {});
	ImGui::DockSpaceOverViewport();
	ImGui::PopStyleColor(2);
#endif

	platform::Input input = {};
	input.deltaTime = dt;
	input.hasFocus = platform::hasFocused();
	memcpy(input.buttons, platform::getAllButtons(), sizeof(input.buttons));
	input.mouseX = platform::getRelMousePosition().x;
	input.mouseY = platform::getRelMousePosition().y;
	input.lMouse = platform::getLMouseButton();
	input.rMouse = platform::getRMouseButton();
	strlcpy(input.typedInput, platform::getTypedInput(), sizeof(input.typedInput));

	input.controller = platform::getControllerButtons();

	for (int i = 0; i < 4; i++)
	{
		input.controllers[i] = platform::getControllerButtonsAtIndex(i);
	}

	if (!gl2dGpuBackendActive)
	{
		SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
		SDL_RenderClear(sdlRenderer);
	}

	if (!gameLogic(dt, input, sdlRenderer))
	{
		return false;
	}

	mouseMovedFlag = 0;
	platform::internal::updateAllButtons(dt);
	platform::internal::resetTypedInput();

#if REMOVE_IMGUI == 0
	ImGui::Render();
	if (!gUseImguiGpuRenderer)
	{
		ImGui_ImplSDLRenderer3_RenderDrawData(
			ImGui::GetDrawData(), sdlRenderer);
	}
#endif

	// Submit all queued gl2d GPU work after ImGui has finalized its draw data.
	getRenderer().flush();

	if (!gl2dGpuBackendActive)
	{
		SDL_RenderPresent(sdlRenderer);
	}

	if (gShouldQuit) return false;

	return true;
}

#ifdef __EMSCRIPTEN__
static void mainLoop()
{
	if (!tickOneFrame())
	{
		emscripten_cancel_main_loop();

		closeGame();

	#if REMOVE_IMGUI == 0
		if (gUseImguiGpuRenderer)
		{
		#if GL2D_USE_SDL_GPU
			ImGui_ImplSDLGPU3_Shutdown();
		#endif
		}
		else
		{
			ImGui_ImplSDLRenderer3_Shutdown();
		}
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	#endif

		SDL_DestroyRenderer(sdlRenderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}
}
#endif

int main(int, char **)
{
	permaAssertComment(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD),
		"SDL init failed");

	#ifdef __EMSCRIPTEN__
	SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#canvas");
	SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
	#endif

	window = SDL_CreateWindow(
		"Mages Dungeon",
		1200, 900,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
	);

	permaAssertComment(window, "SDL window creation failed");

	sdlRenderer = createRendererPreferVulkan(window);

	permaAssertComment(sdlRenderer, "SDL renderer creation failed");

	SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

	SDL_StartTextInput(window);

#if REMOVE_IMGUI == 0
	ImGui::CreateContext();
	imguiThemes::embraceTheDarkness();

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGuiStyle &style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg].w = 0.5f;
	style.FontScaleMain = 3;

	if (1)
	{
		if (1)
		{
			io.Fonts->AddFontDefault();
		}
		else
		{
			io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "font/arial.ttf", 16);
		}

		ImFontConfig config;
		config.MergeMode = true;
		config.PixelSnapH = true;
		config.GlyphMinAdvanceX = 16.0f;

		static const ImWchar icon_ranges[] = {ICON_MIN_FK, ICON_MAX_FK, 0};
		io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "font/fontawesome-webfont.ttf", 16.0f, &config, icon_ranges);
	}
#endif

	if (!initGame(sdlRenderer))
	{
		return 0;
	}

#if REMOVE_IMGUI == 0
	gUseImguiGpuRenderer = false;
#if GL2D_USE_SDL_GPU
	gUseImguiGpuRenderer = isGl2dGpuBackendActive();
#endif

	if (gUseImguiGpuRenderer)
	{
	#if GL2D_USE_SDL_GPU
		permaAssertComment(ImGui_ImplSDL3_InitForSDLGPU(window),
			"ImGui SDL3 platform init for SDL_gpu failed");

		ImGui_ImplSDLGPU3_InitInfo initInfo = {};
		initInfo.Device = getRenderer().gpuDevice;
		initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(getRenderer().gpuDevice, window);

		permaAssertComment(ImGui_ImplSDLGPU3_Init(&initInfo),
			"ImGui SDL_gpu renderer init failed");

		getRenderer().setGpuPassCallback(renderImguiInGpuPass, nullptr);
	#endif
	}
	else
	{
		permaAssertComment(ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer),
			"ImGui SDL3 platform init for SDL renderer failed");
		permaAssertComment(ImGui_ImplSDLRenderer3_Init(sdlRenderer),
			"ImGui SDL renderer init failed");
	}
#endif

	gLast = std::chrono::high_resolution_clock::now();

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(mainLoop, 0, 1);
	return 0;
#else
	while (true)
	{
		if (!tickOneFrame()) break;
	}

	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(window);

	closeGame();

#if REMOVE_IMGUI == 0
	if (gUseImguiGpuRenderer)
	{
	#if GL2D_USE_SDL_GPU
		ImGui_ImplSDLGPU3_Shutdown();
	#endif
	}
	else
	{
		ImGui_ImplSDLRenderer3_Shutdown();
	}
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
#endif


	SDL_Quit();
#endif
}
