#pragma once
#include <gl2d/gl2d.h>
#include <glm/vec2.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <randomStuff.h>
#include <sstream>
#include <string>
#include <vector>

struct DamageViewerSystem
{
	struct DamageEntry
	{
		glm::vec2 pos = {};
		glm::vec2 offset = {};
		float timer = 0.0f;
		float value = 0.0f;
		float rotation = 0.0f;
		std::string text = {};
	};

	std::vector<DamageEntry> entries;
	float maxTimer = 0.9f;
	float textSize = 0.6f;
	std::ranlux24_base rng{std::random_device()()};

	static std::string formatDamage(float value)
	{
		float rounded = std::round(value);
		std::ostringstream s;
		if (std::abs(value - rounded) < 0.05f)
		{
			s << static_cast<int>(rounded);
		}
		else
		{
			s << std::fixed << std::setprecision(1) << value;
		}
		return s.str();
	}

	void addDamage(float value, glm::vec2 pos)
	{
		DamageEntry entry;
		entry.pos = pos;
		float offsetRange = 0.2f;
		entry.offset = {
			getRandomFloat(rng, -offsetRange, offsetRange),
			getRandomFloat(rng, -offsetRange, offsetRange)
		};
		entry.timer = maxTimer;
		entry.value = value;
		entry.rotation = getRandomFloat(rng, -15.0f, 15.0f);
		entry.text = formatDamage(value);
		entries.push_back(std::move(entry));
	}

	void update(float deltaTime)
	{
		for (size_t i = 0; i < entries.size();)
		{
			auto &entry = entries[i];
			entry.timer -= deltaTime;
			if (entry.timer <= 0.0f)
			{
				entries[i] = entries.back();
				entries.pop_back();
				continue;
			}
			++i;
		}
	}

	void render(gl2d::Renderer2D &renderer, gl2d::Font &font)
	{
		for (const auto &entry : entries)
		{
			float normalized = std::clamp(entry.timer / maxTimer, 0.0f, 1.0f);
			gl2d::Color4f startColor = {1.0f, 0.95f, 0.2f, 1.0f};
			gl2d::Color4f endColor = {1.0f, 0.15f, 0.1f, 1.0f};
			gl2d::Color4f color = startColor * normalized + endColor * (1.0f - normalized);
			color.a *= std::sqrt(normalized);

			glm::vec2 pos = entry.pos + entry.offset;
			pos.y -= (1.0f - normalized) * 0.6f;
			renderer.renderText(pos, entry.text.c_str(), font, color, textSize, 4, 3, true,
				{0.1f, 0.1f, 0.1f, color.a}, {}, 0.0f, entry.rotation);
		}
	}
};

DamageViewerSystem &getDamageViewerSystem();
