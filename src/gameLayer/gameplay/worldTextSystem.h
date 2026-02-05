#pragma once
#include <gl2d/gl2d.h>
#include <glm/vec2.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct AssetsManager;

// Stores world-space text with inline button annotations like {shoot_button}.
struct WorldTextSystem
{
	struct Token
	{
		enum class Type
		{
			Text,
			Prompt,
			NewLine
		};

		Type type = Type::Text;
		std::string text = {};
	};

	struct AnnotationPrompt
	{
		std::string keyboard = {};
		std::string mouse = {};
		std::string controller = {};
	};

	struct Entry
	{
		glm::vec2 pos = {};
		gl2d::Color4f color = {1, 1, 1, 1};
		float size = 0.0f;
		float spacing = 4.0f;
		float lineSpacing = 3.0f;
		bool center = false;
		std::vector<Token> tokens;
	};

	std::vector<Entry> entries;
	std::unordered_map<std::string, AnnotationPrompt> prompts;
	float defaultTextSize = 0.0f;
	float iconScale = 1.0f;
	float tokenPaddingScale = 0.25f;
	float iconYOffsetScale = 1.3f; // pushes button icons up relative to text size
	float lineGapScale = 0.35f; // extra line spacing as a size fraction

	WorldTextSystem();

	int addText(const std::string &text, glm::vec2 pos,
		const gl2d::Color4f &color = {1, 1, 1, 1},
		float size = 0.0f, float spacing = 4.0f, float lineSpacing = 3.0f, bool center = false);
	void clear();
	bool removeText(int index);

	void setPrompt(const std::string &tag, const AnnotationPrompt &prompt);
	void render(gl2d::Renderer2D &renderer, const AssetsManager &assetsManager, bool usesController);

private:
	std::vector<Token> parseText(const std::string &text) const;
};
