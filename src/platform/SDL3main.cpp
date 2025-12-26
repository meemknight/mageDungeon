#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <iostream>
#include <chrono>
#include <fstream>

#include "platformTools.h"
#include "platformInput.h"
#include "gameLayer.h"
#include "stringManipulation.h"

#include <raudio.h>

#include "imguiTools.h"

#if REMOVE_IMGUI == 0
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imguiThemes.h"
#include "IconsForkAwesome.h"
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
#pragma endregion

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
		return {(int)x, (int)y};
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

	// ----- file helpers unchanged -----
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
	exit(0);

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
		//SDL_Keymod  mod = e.key.mod;
		//SDL_Scancode sc = e.key.scancode;

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

int main(int, char **)
{
	permaAssertComment(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD),
		"SDL init failed");

	window = SDL_CreateWindow(
		"geam",
		500, 500,
		SDL_WINDOW_RESIZABLE
	);

	permaAssertComment(window, "SDL window creation failed");

	sdlRenderer = SDL_CreateRenderer(window, nullptr);
	
	permaAssertComment(sdlRenderer, "SDL renderer creation failed");

	SDL_StartTextInput(window);

#pragma region audio
	//InitAudioDevice();

	//Music m = LoadMusicStream(RESOURCES_PATH "target.ogg");
	//Music m = {};
	//UpdateMusicStream(m);
	//StopMusicStream(m);
	//PlayMusicStream(m);

#pragma endregion

#pragma region imgui
#if REMOVE_IMGUI == 0
	ImGui::CreateContext();
	imguiThemes::embraceTheDarkness();

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGuiStyle &style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg].w = 0.5f;
	style.FontScaleMain = 3;

	ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer);
	ImGui_ImplSDLRenderer3_Init(sdlRenderer);

	if (1) //load font awesome
	{
		//https://pixtur.github.io/mkdocs-for-imgui/site/FONTS/
		//https://github.com/juliettef/IconFontCppHeaders
		//https://fontawesome.com/v4/icons/


		if (1)
		{
			// use the default font
			io.Fonts->AddFontDefault();
		}
		else
		{
			//load custom font
			io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "arial.ttf", 16);
		}



		// Merge FontAwesome into the default font
		ImFontConfig config;
		config.MergeMode = true;
		config.PixelSnapH = true;
		config.GlyphMinAdvanceX = 16.0f; // adjust as needed

		static const ImWchar icon_ranges[] = {ICON_MIN_FK, ICON_MAX_FK, 0};
		io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "fontawesome-webfont.ttf", 16.0f, &config, icon_ranges);

		// Build fonts after all additions
		io.Fonts->Build();

		//make one icon larger example
		//{
		//	ImVector<ImWchar> ranges;
		//	ImFontGlyphRangesBuilder builder;
		//	builder.AddChar(0xf016);//ICON_FK_FILE_O
		//	builder.AddChar(0xf114);//ICON_FK_FOLDER_O
		//	builder.BuildRanges(&ranges);
		//
		//	io.Fonts->AddFontFromFileTTF(RESOURCES_PATH "fontawesome-webfont.ttf",
		//		150.f / io.FontGlobalScale, 0, ranges.Data);
		//}

	}
#endif
#pragma endregion


	if (!initGame(sdlRenderer))
	{
		return 0;
	}

	auto last = std::chrono::high_resolution_clock::now();

	while (true)
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
		float dt = std::chrono::duration<float>(now - last).count();
		last = now;
		dt = std::min(dt, 1.f / 10.f);

	#pragma region imgui
	#if REMOVE_IMGUI == 0
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {});
		ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, {});
		ImGui::DockSpaceOverViewport();
		ImGui::PopStyleColor(2);
	#endif
	#pragma endregion

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


		SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
		SDL_RenderClear(sdlRenderer);

		if (!gameLogic(dt, input, sdlRenderer))
		{
			break;
		}

		mouseMovedFlag = 0;
		platform::internal::updateAllButtons(dt);
		platform::internal::resetTypedInput();

	#if REMOVE_IMGUI == 0
		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(
			ImGui::GetDrawData(), sdlRenderer);
	#endif

		SDL_RenderPresent(sdlRenderer);
	}

	closeGame();

#if REMOVE_IMGUI == 0
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
#endif

	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
