#include <gameplay/worldTextSystem.h>
#include <gameplay/assetsManager.h>
#include <gameplay/Physics.h>
#include <algorithm>

namespace
{
	void trimTag(std::string &tag)
	{
		if (tag.empty()) { return; }
		size_t first = tag.find_first_not_of(" \t");
		if (first == std::string::npos)
		{
			tag.clear();
			return;
		}
		size_t last = tag.find_last_not_of(" \t");
		tag = tag.substr(first, last - first + 1);
	}

	const gl2d::Texture *resolvePromptTexture(const AssetsManager &assetsManager,
		const WorldTextSystem::AnnotationPrompt &prompt, bool usesController)
	{
		auto isValid = [](const gl2d::Texture *texture)
		{
			return texture && texture->isValid();
		};

		if (usesController)
		{
			if (!prompt.controller.empty())
			{
				if (auto *texture = assetsManager.buttonSprites.getController(prompt.controller))
				{
					if (isValid(texture)) { return texture; }
				}
			}
			if (!prompt.mouse.empty())
			{
				if (auto *texture = assetsManager.buttonSprites.getMouse(prompt.mouse))
				{
					if (isValid(texture)) { return texture; }
				}
			}
			if (!prompt.keyboard.empty())
			{
				if (auto *texture = assetsManager.buttonSprites.getKeyboard(prompt.keyboard))
				{
					if (isValid(texture)) { return texture; }
				}
			}
		}
		else
		{
			if (!prompt.mouse.empty())
			{
				if (auto *texture = assetsManager.buttonSprites.getMouse(prompt.mouse))
				{
					if (isValid(texture)) { return texture; }
				}
			}
			if (!prompt.keyboard.empty())
			{
				if (auto *texture = assetsManager.buttonSprites.getKeyboard(prompt.keyboard))
				{
					if (isValid(texture)) { return texture; }
				}
			}
			if (!prompt.controller.empty())
			{
				if (auto *texture = assetsManager.buttonSprites.getController(prompt.controller))
				{
					if (isValid(texture)) { return texture; }
				}
			}
		}

		return nullptr;
	}
}

WorldTextSystem::WorldTextSystem()
{
	defaultTextSize = PIXEL_SIZE * 10.0f;
	setPrompt("shoot_button", {"", "Right", "RT"});
}

int WorldTextSystem::addText(const std::string &text, glm::vec2 pos,
	const gl2d::Color4f &color, float size, float spacing, float lineSpacing, bool center)
{
	Entry entry;
	entry.pos = pos;
	entry.color = color;
	entry.size = size > 0.0f ? size : defaultTextSize;
	entry.spacing = spacing;
	entry.lineSpacing = lineSpacing;
	entry.center = center;
	entry.tokens = parseText(text);
	entries.push_back(std::move(entry));
	return (int)entries.size() - 1;
}

void WorldTextSystem::clear()
{
	entries.clear();
}

bool WorldTextSystem::removeText(int index)
{
	if (index < 0 || index >= (int)entries.size())
	{
		return false;
	}
	entries[index] = entries.back();
	entries.pop_back();
	return true;
}

void WorldTextSystem::setPrompt(const std::string &tag, const AnnotationPrompt &prompt)
{
	if (tag.empty()) { return; }
	prompts[tag] = prompt;
}

std::vector<WorldTextSystem::Token> WorldTextSystem::parseText(const std::string &text) const
{
	std::vector<Token> tokens;
	std::string buffer;

	auto flushText = [&]()
	{
		if (!buffer.empty())
		{
			tokens.push_back({Token::Type::Text, buffer});
			buffer.clear();
		}
	};

	for (size_t i = 0; i < text.size(); i++)
	{
		char c = text[i];
		if (c == '\n')
		{
			flushText();
			tokens.push_back({Token::Type::NewLine, {}});
			continue;
		}

		if (c == '{')
		{
			size_t close = text.find('}', i + 1);
			if (close != std::string::npos)
			{
				std::string tag = text.substr(i + 1, close - i - 1);
				trimTag(tag);
				flushText();
				if (!tag.empty())
				{
					tokens.push_back({Token::Type::Prompt, tag});
				}
				else
				{
					buffer += "{}";
				}
				i = close;
				continue;
			}
		}

		buffer += c;
	}

	flushText();
	return tokens;
}

void WorldTextSystem::render(gl2d::Renderer2D &renderer, const AssetsManager &assetsManager, bool usesController)
{
	struct ResolvedToken
	{
		Token::Type type = Token::Type::Text;
		const std::string *text = nullptr;
		std::string fallback = {};
		const gl2d::Texture *texture = nullptr;
		float width = 0.0f;
	};

	for (const auto &entry : entries)
	{
		if (entry.tokens.empty()) { continue; }

		float size = entry.size > 0.0f ? entry.size : defaultTextSize;
		float tokenPadding = std::max(size * tokenPaddingScale, PIXEL_SIZE * 2.0f);
		float iconSize = size * iconScale;
		glm::vec2 singleSize = renderer.getTextSize("Ag", assetsManager.font, size,
			entry.spacing, entry.lineSpacing);
		glm::vec2 doubleSize = renderer.getTextSize("Ag\nAg", assetsManager.font, size,
			entry.spacing, entry.lineSpacing);
		float lineHeight = std::max(singleSize.y, iconSize);
		float lineAdvance = doubleSize.y - singleSize.y;
		if (lineAdvance <= 0.0f)
		{
			lineAdvance = lineHeight;
		}

		std::vector<ResolvedToken> resolved;
		resolved.reserve(entry.tokens.size());
		for (const auto &token : entry.tokens)
		{
			ResolvedToken resolvedToken;
			resolvedToken.type = token.type;
			resolvedToken.text = &token.text;
			if (token.type == Token::Type::Text)
			{
				resolvedToken.width = renderer.getTextSize(token.text.c_str(), assetsManager.font,
					size, entry.spacing, entry.lineSpacing).x;
			}
			else if (token.type == Token::Type::Prompt)
			{
				auto it = prompts.find(token.text);
				if (it != prompts.end())
				{
					resolvedToken.texture = resolvePromptTexture(assetsManager, it->second, usesController);
				}
				if (resolvedToken.texture)
				{
					resolvedToken.width = iconSize;
				}
				else
				{
					resolvedToken.fallback = "{" + token.text + "}";
					resolvedToken.width = renderer.getTextSize(resolvedToken.fallback.c_str(),
						assetsManager.font, size, entry.spacing, entry.lineSpacing).x;
				}
			}
			resolved.push_back(std::move(resolvedToken));
		}

		std::vector<float> lineWidths;
		lineWidths.reserve(4);
		float lineWidth = 0.0f;
		bool firstInLine = true;
		for (const auto &token : resolved)
		{
			if (token.type == Token::Type::NewLine)
			{
				lineWidths.push_back(lineWidth);
				lineWidth = 0.0f;
				firstInLine = true;
				continue;
			}
			if (!firstInLine)
			{
				lineWidth += tokenPadding;
			}
			lineWidth += token.width;
			firstInLine = false;
		}
		lineWidths.push_back(lineWidth);

		size_t lineIndex = 0;
		float cursorX = entry.pos.x;
		float cursorY = entry.pos.y;
		if (entry.center && !lineWidths.empty())
		{
			cursorX -= lineWidths[0] * 0.5f;
		}
		firstInLine = true;

		for (const auto &token : resolved)
		{
			if (token.type == Token::Type::NewLine)
			{
				lineIndex++;
				cursorY += lineAdvance;
				cursorX = entry.pos.x;
				if (entry.center && lineIndex < lineWidths.size())
				{
					cursorX -= lineWidths[lineIndex] * 0.5f;
				}
				firstInLine = true;
				continue;
			}

			if (!firstInLine)
			{
				cursorX += tokenPadding;
			}

			if (token.type == Token::Type::Text)
			{
				gl2d::Color4f shadow = {0.1f, 0.1f, 0.1f, entry.color.a};
				renderer.renderText({cursorX, cursorY}, token.text->c_str(), assetsManager.font,
					entry.color, size, entry.spacing, entry.lineSpacing, false, shadow);
			}
			else if (token.type == Token::Type::Prompt)
			{
				if (token.texture)
				{
					float iconY = cursorY + lineHeight * 0.5f - iconSize * 0.5f;
					glm::vec4 rect = {cursorX, iconY, iconSize, iconSize};
					renderer.renderRectangle(rect, *token.texture, {1, 1, 1, entry.color.a});
				}
				else
				{
					gl2d::Color4f shadow = {0.1f, 0.1f, 0.1f, entry.color.a};
					renderer.renderText({cursorX, cursorY}, token.fallback.c_str(), assetsManager.font,
						entry.color, size, entry.spacing, entry.lineSpacing, false, shadow);
				}
			}

			cursorX += token.width;
			firstInLine = false;
		}
	}
}
