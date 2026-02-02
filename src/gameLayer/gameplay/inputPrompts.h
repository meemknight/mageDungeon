#pragma once
#include <gameplay/assetsManager.h>
#include <glm/glm.hpp>

// Helper for rendering button prompts in world space.
struct ButtonPrompt
{
	const char *keyboard = "";
	const char *controller = "";
};

inline const gl2d::Texture *getPromptTexture(const AssetsManager &assetsManager,
	bool usesController, const ButtonPrompt &prompt)
{
	if (usesController)
	{
		return assetsManager.buttonSprites.getController(prompt.controller);
	}
	return assetsManager.buttonSprites.getKeyboard(prompt.keyboard);
}

inline void renderPrompt(gl2d::Renderer2D &renderer, const AssetsManager &assetsManager,
	bool usesController, const ButtonPrompt &prompt, glm::vec2 worldPos, float size, float alpha)
{
	const gl2d::Texture *texture = getPromptTexture(assetsManager, usesController, prompt);
	if (!texture || !texture->isValid()) { return; }

	glm::vec4 rect = {worldPos.x - size * 0.5f, worldPos.y - size * 0.5f, size, size};
	renderer.renderRectangle(rect, *texture, {1, 1, 1, alpha});
}
