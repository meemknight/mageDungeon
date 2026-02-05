#pragma once

#include <vector>
#include <random>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <gameplay/spells/spellTypes.h>
#include <gameplay/spellPreviewContext.h>

struct AssetsManager;

namespace gl2d
{
	struct Renderer2D;
}

// Book page for browsing spell previews and recipes.
struct SpellbookEntry
{
	SpellbookEntry() = default;
	SpellbookEntry(const SpellbookEntry &) = delete;
	SpellbookEntry &operator=(const SpellbookEntry &) = delete;
	SpellbookEntry(SpellbookEntry &&) noexcept = default;
	SpellbookEntry &operator=(SpellbookEntry &&) noexcept = default;

	int spellType = -1;
	SpellRecepie recipe = {};
	const char *name = "";
	bool unstable = false;
	SpellPreviewContext preview;
};

struct SpellbookPage
{
	std::vector<SpellbookEntry> entries;
	int pageIndex = 0;
	float previewTimer = 0.0f;
	float unstableTimer = 0.0f;
	SpellRecepie unstableRecipe = {};

	void init();
	void update(float deltaTime, std::ranlux24_base &rng);
	void render(gl2d::Renderer2D &renderer, AssetsManager &assetsManager,
		std::ranlux24_base &rng, const glm::vec4 &bookRect,
		const glm::vec2 &cursorPos, bool click);
};
