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
	glm::vec4 colors[] = {{0.5,0.5,0.5,1}, { 1,0,0,1 }, {0,0,1,1}, {0,1,0,1}, {0,1,1,1}};
	return colors[element];
};

