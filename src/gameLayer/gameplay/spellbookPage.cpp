#include <gameplay/spellbookPage.h>
#include <gameplay/assetsManager.h>
#include <gameplay/blocks.h>
#include <gameplay/wand.h>
#include <gameLayer.h>
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
		entries.emplace_back();
		auto &entry = entries.back();
		entry.spellType = i;
		entry.recipe = SpellTypes::getSpellRecepie(i);
		entry.name = SpellTypes::getSpellName(i);
	}

	entries.emplace_back();
	auto &unstable = entries.back();
	unstable.spellType = -1;
	unstable.recipe = {};
	unstable.name = "Wild Magic";
	unstable.unstable = true;
}

// Initializes the tiny simulation used for spell previews.
static void initPreviewContext(SpellPreviewContext &ctx, std::ranlux24_base &rng)
{
	if (ctx.initialized) { return; }
	ctx.initialized = true;
	ctx.particleRenderer.init();
	ctx.map.create(6, 4);
	for (int y = 0; y < ctx.map.size.y; y++)
	{
		for (int x = 0; x < ctx.map.size.x; x++)
		{
			int variant = getRandomInt(rng, 0, 4);
			ctx.map.firstLayer.getBlockUnsafe(x, y).type =
				variant == 0 ? Blocks::grassDecoration : Blocks::grass;
		}
	}
	ctx.player.physics.teleport({ctx.map.size.x * 0.5f, ctx.map.size.y * 0.45f});
	ctx.player.animator.setAnimationBasedOnMovement({0.0f, 1.0f});
	ctx.player.resetHealth();
	ctx.previewWand = makeStarterWand(rng);
}

static void resetPreviewContext(SpellPreviewContext &ctx, int spellType, std::ranlux24_base &rng)
{
	ctx.spellType = spellType;
	ctx.spells.spells.clear();
	ctx.projectiles.projectiles.clear();
	ctx.projectiles.pendingProjectiles.clear();
	ctx.particleSystem.particles.clear();
	ctx.summons.clear();
	ctx.standbyProjectiles.standbyProjectiles.clear();
	ctx.standbyProjectiles.insertIndex = 1;
	ctx.entities.entities.clear();
	ctx.damageViewer = {};
	ctx.castTimer = 0.0f;
	ctx.standbyFireTimer = 0.0f;
	ctx.player.physics.teleport({ctx.map.size.x * 0.5f, ctx.map.size.y * 0.45f});
	ctx.player.animator.setAnimationBasedOnMovement({0.0f, 1.0f});
	ctx.player.resetHealth();
}

static void updatePreviewContext(SpellPreviewContext &ctx, float deltaTime, std::ranlux24_base &rng,
	int spellType, bool unstable)
{
	ctx.castTimer -= deltaTime;
	ctx.standbyFireTimer -= deltaTime;
	if (ctx.castTimer <= 0.0f)
	{
		ctx.castTimer = 1.1f;
		if (unstable)
		{
			auto spell = SpellTypes::getWildMagicSpell();
			ctx.spells.addSpell(std::move(spell), ctx.player.physics.getPos(), ctx.aimDirection);
		}
		else
		{
			auto spell = SpellTypes::getSpell(spellType);
			ctx.spells.addSpell(std::move(spell), ctx.player.physics.getPos(), ctx.aimDirection);
		}
	}

	ctx.spells.update(deltaTime, ctx.map, ctx.particleSystem,
		ctx.projectiles, rng, ctx.player, ctx.entities, ctx.aimDirection);

	if (ctx.standbyFireTimer <= 0.0f)
	{
		ctx.standbyFireTimer = 0.9f;
		ctx.standbyProjectiles.tryFire(ctx.map, ctx.projectiles, ctx.player, ctx.entities, ctx.aimDirection);
	}
	ctx.standbyProjectiles.update(deltaTime, ctx.map, ctx.projectiles, rng,
		ctx.player, ctx.entities, ctx.aimDirection, true);

	ctx.projectiles.update(deltaTime, ctx.map, ctx.particleSystem, rng, ctx.entities);
	ctx.particleSystem.update(deltaTime);
	ctx.summons.update(deltaTime, ctx.map, ctx.particleSystem, ctx.projectiles, rng, ctx.player, ctx.entities);
	ctx.player.update(deltaTime);
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
	std::ranlux24_base &rng, const glm::vec4 &bookRect, const glm::vec2 &cursorPos, bool click)
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

		SpellbookEntry &entry = entries[entryIndex];
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

		initPreviewContext(entry.preview, rng);
		if (entry.preview.spellType != entry.spellType)
		{
			resetPreviewContext(entry.preview, entry.spellType, rng);
		}
		entry.preview.aimDirection = {1.0f, 0.0f};
		setSpellPreviewContext(&entry.preview);
		updatePreviewContext(entry.preview, 0.016f, rng, entry.spellType, entry.unstable);

		// preview camera
		float viewWidth = 3.2f;
		gl2d::Camera previewCam = renderer.currentCamera;
		previewCam.zoom = previewRect.z / viewWidth;
		float viewHeight = previewRect.w / previewCam.zoom;
		glm::vec2 screenCenter = {renderer.windowW * 0.5f, renderer.windowH * 0.5f};
		glm::vec2 previewCenter = {previewRect.x + previewRect.z * 0.5f, previewRect.y + previewRect.w * 0.5f};
		glm::vec2 offset = (previewCenter - screenCenter) / previewCam.zoom;
		glm::vec2 worldCenter = entry.preview.player.physics.getPos();
		previewCam.position = {worldCenter.x - offset.x, worldCenter.y + offset.y - viewHeight * 0.18f};
		renderer.pushCamera(previewCam);

		entry.preview.map.renderMap(renderer, assetsManager);
		entry.preview.spells.renderBeforeEntities(renderer, entry.preview.particleRenderer);
		entry.preview.projectiles.render(renderer, assetsManager, entry.preview.particleRenderer);
		entry.preview.summons.render(renderer, entry.preview.particleRenderer);
		entry.preview.player.render(renderer, assetsManager, entry.preview.previewWand, entry.preview.aimDirection);

		renderer.popCamera();
		clearSpellPreviewContext();
		renderer.stopSchisor();

		float nameSize = slotRect.w * 0.16f;
		glm::vec2 namePos = {slotRect.x + slotRect.z * 0.5f, slotRect.y + slotRect.w * 0.82f};
		renderer.renderText(namePos, entry.name, assetsManager.font,
			{0.95f, 0.95f, 0.95f, 0.95f}, nameSize, 4, 3, true);
	}
}
