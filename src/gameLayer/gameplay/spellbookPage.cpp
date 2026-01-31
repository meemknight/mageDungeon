#include <gameplay/spellbookPage.h>
#include <gameplay/assetsManager.h>
#include <gameplay/blocks.h>
#include <randomStuff.h>
#include <gl2d/gl2d.h>
#include <algorithm>
#include <cmath>

void SpellbookPage::init()
{
	if (!entries.empty()) { return; }
	entries.reserve(SpellTypes::SPELLS_COUNT + 1);

	for (int i = 1; i < SpellTypes::SPELLS_COUNT; i++)
	{
		SpellbookEntry entry;
		entry.spellType = i;
		entry.recipe = SpellTypes::getSpellRecepie(i);
		entry.name = SpellTypes::getSpellName(i);
		entries.push_back(entry);
	}

	SpellbookEntry unstable;
	unstable.spellType = -1;
	unstable.recipe = {};
	unstable.name = "Wild Magic";
	unstable.unstable = true;
	entries.push_back(unstable);
}

void SpellbookPage::update(float deltaTime, std::ranlux24_base &rng)
{
	previewTimer += deltaTime;
	unstableTimer -= deltaTime;
	if (unstableTimer <= 0.0f)
	{
		unstableTimer = 0.7f;
		int elements[4] = {Elements::Fire, Elements::Water, Elements::Earth, Elements::Ice};
		for (int i = 3; i > 0; i--)
		{
			int swapIndex = getRandomInt(rng, 0, i);
			std::swap(elements[i], elements[swapIndex]);
		}
		unstableRecipe.clear();
		for (int i = 0; i < 4; i++)
		{
			unstableRecipe.add(elements[i], 4);
		}
	}
}

void SpellbookPage::render(gl2d::Renderer2D &renderer, AssetsManager &assetsManager,
	const glm::vec4 &bookRect, const glm::vec2 &cursorPos, bool click)
{
	if (entries.empty()) { return; }
	const int perPage = 4;
	int maxPages = (int)((entries.size() + perPage - 1) / perPage);
	pageIndex = std::max(0, std::min(pageIndex, maxPages - 1));

	float contentTop = bookRect.y + bookRect.w * 0.12f;
	float contentHeight = bookRect.w * 0.72f;
	float rowHeight = contentHeight / 2.0f;
	float rowGap = rowHeight * 0.08f;
	float slotHeight = rowHeight - rowGap;
	float columnWidth = bookRect.z * 0.38f;
	float leftX = bookRect.x + bookRect.z * 0.07f;
	float rightX = bookRect.x + bookRect.z * 0.55f;

	float arrowSize = bookRect.w * 0.065f;
	float arrowY = bookRect.y + bookRect.w * 0.90f;
	glm::vec4 leftArrow = {bookRect.x + bookRect.z * 0.44f, arrowY, arrowSize, arrowSize};
	glm::vec4 rightArrow = {bookRect.x + bookRect.z * 0.52f, arrowY, arrowSize, arrowSize};
	bool hoverLeft = cursorPos.x >= leftArrow.x && cursorPos.x <= leftArrow.x + leftArrow.z &&
		cursorPos.y >= leftArrow.y && cursorPos.y <= leftArrow.y + leftArrow.w;
	bool hoverRight = cursorPos.x >= rightArrow.x && cursorPos.x <= rightArrow.x + rightArrow.z &&
		cursorPos.y >= rightArrow.y && cursorPos.y <= rightArrow.y + rightArrow.w;
	if (click)
	{
		if (hoverLeft)
		{
			pageIndex = std::max(0, pageIndex - 1);
		}
		if (hoverRight)
		{
			pageIndex = std::min(maxPages - 1, pageIndex + 1);
		}
	}

	gl2d::Color4f leftColor = hoverLeft ? gl2d::Color4f{0.42f, 0.38f, 0.3f, 0.9f}
		: gl2d::Color4f{0.25f, 0.23f, 0.2f, 0.7f};
	gl2d::Color4f rightColor = hoverRight ? gl2d::Color4f{0.42f, 0.38f, 0.3f, 0.9f}
		: gl2d::Color4f{0.25f, 0.23f, 0.2f, 0.7f};
	renderer.renderRectangle(leftArrow, leftColor);
	renderer.renderRectangle(rightArrow, rightColor);
	float arrowTextSize = arrowSize * 0.8f;
	renderer.renderText({leftArrow.x + arrowSize * 0.25f, leftArrow.y + arrowSize * 0.05f},
		"<", assetsManager.font, {0.95f, 0.95f, 0.95f, 0.9f}, arrowTextSize, 4, 3, false);
	renderer.renderText({rightArrow.x + arrowSize * 0.25f, rightArrow.y + arrowSize * 0.05f},
		">", assetsManager.font, {0.95f, 0.95f, 0.95f, 0.9f}, arrowTextSize, 4, 3, false);

	int startIndex = pageIndex * perPage;
	for (int i = 0; i < perPage; i++)
	{
		int entryIndex = startIndex + i;
		if (entryIndex >= (int)entries.size()) { break; }

		int column = i / 2;
		int row = i % 2;
		float slotX = column == 0 ? leftX : rightX;
		float slotY = contentTop + row * rowHeight;
		glm::vec4 slotRect = {slotX, slotY, columnWidth, slotHeight};
		renderer.renderRectangle(slotRect, {0.16f, 0.13f, 0.1f, 0.2f});

		const SpellbookEntry &entry = entries[entryIndex];
		SpellRecepie recipe = entry.unstable ? unstableRecipe : entry.recipe;

		float previewPad = slotRect.z * 0.04f;
		glm::vec4 previewRect = {slotRect.x + previewPad, slotRect.y + previewPad,
			slotRect.z - previewPad * 2.0f, slotRect.w * 0.62f};

		float recipeBox = previewRect.z * 0.08f;
		float recipeGap = recipeBox * 0.3f;
		if (recipe.count > 0)
		{
			float recipeWidth = recipe.count * recipeBox + (recipe.count - 1) * recipeGap;
			float recipeX = previewRect.x + (previewRect.z - recipeWidth) * 0.5f;
			float recipeY = previewRect.y + recipeBox * 0.2f;
			for (int r = 0; r < recipe.count; r++)
			{
				glm::vec4 box = {recipeX + r * (recipeBox + recipeGap), recipeY, recipeBox, recipeBox};
				renderer.renderRectangle(box, elementToColor(recipe.elements[r]));
			}
		}

		renderer.schisor(previewRect);
		renderer.renderRectangle(previewRect, {0.12f, 0.18f, 0.12f, 0.9f});

		auto &grass = assetsManager.tileSets[TileSets::grass];
		float tileSize = previewRect.z / 4.0f;
		for (int ty = 0; ty < 2; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				glm::vec4 tileRect = {previewRect.x + tx * tileSize, previewRect.y + previewRect.w - (ty + 1) * tileSize,
					tileSize, tileSize};
				int variant = ((entryIndex + tx + ty * 3) % 5 == 0) ? 1 : 0;
				renderer.renderRectangle(tileRect, grass.texture, {1, 1, 1, 0.9f}, {}, 0,
					grass.atlas.get(variant, 0));
			}
		}

		float playerSize = previewRect.w * 0.48f;
		glm::vec2 playerPos = {previewRect.x + previewRect.z * 0.5f, previewRect.y + previewRect.w * 0.70f};
		glm::vec4 playerRect = {playerPos.x - playerSize * 0.5f, playerPos.y - playerSize * 0.5f,
			playerSize, playerSize};
		renderer.renderRectangle(playerRect, assetsManager.player.texture,
			{1, 1, 1, 1}, {}, 0, assetsManager.player.atlas.get(0, 0));

		// actual spell preview will be rendered here
		renderer.stopSchisor();

		float nameSize = slotRect.w * 0.16f;
		glm::vec2 namePos = {slotRect.x + slotRect.z * 0.5f, slotRect.y + slotRect.w * 0.82f};
		renderer.renderText(namePos, entry.name, assetsManager.font,
			{0.95f, 0.95f, 0.95f, 0.95f}, nameSize, 4, 3, true);
	}
}
