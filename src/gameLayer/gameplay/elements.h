#pragma once
#include <glm/vec4.hpp>


enum Elements
{
	NoneElement,
	Fire,
	Water,
	Earth,
	Ice,
};


inline glm::vec4 elementToColor(int element)
{
	static const glm::vec4 colors[] =
	{
		{0.5f, 0.5f, 0.5f, 1.0f}, // neutral
		{1.0f, 0.2f, 0.1f, 1.0f}, // fire (red/orange)
		{0.2f, 0.6f, 1.0f, 1.0f}, // water (lighter cyan-blue)
		{0.2f, 0.8f, 0.3f, 1.0f}, // earth (green)
		{0.6f, 0.9f, 1.0f, 1.0f}  // ice (icy cyan)
	};

	return colors[element];
}

inline glm::vec4 elementToSecondaryColor(int element)
{
	static const glm::vec4 colors[] =
	{
		{0.7f, 0.7f, 0.7f, 1.0f}, // neutral (lighter gray)
		{1.0f, 0.9f, 0.2f, 1.0f}, // fire (yellow)
		{0.6f, 0.8f, 1.0f, 1.0f}, // water (light blue)
		{0.55f, 0.35f, 0.15f, 1.0f}, // earth (brown)
		{0.8f, 1.0f, 1.0f, 1.0f}  // ice (very light cyan)
	};

	return colors[element];
}