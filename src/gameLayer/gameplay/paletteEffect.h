#pragma once

#include <gl2d/gl2d.h>
#include <glm/gtc/type_precision.hpp>
#include <vector>
#include <string>

// Applies a CPU color palette to rendered textures.
struct PaletteEffect
{
	bool enabledParticles = false;
	bool enabledGame = false;
	float strength = 0.5f; // 0..1 palette blend strength
	bool loaded = false;
	std::string palettePath;
	glm::ivec2 paletteSize = {0, 0};
	std::vector<glm::u8vec4> colors;
	std::vector<unsigned char> readBuffer;
	std::vector<unsigned char> writeBuffer;
	gl2d::Texture particlesTexture;
	gl2d::Texture gameTexture;
	glm::ivec2 particlesSize = {0, 0};
	glm::ivec2 gameSize = {0, 0};

	void loadPalette();
	bool hasPalette() const { return loaded && !colors.empty(); }
	bool applyToBuffer(std::vector<unsigned char> &rgba);
	bool applyToTexture(gl2d::Renderer2D &renderer, gl2d::Texture &src,
		gl2d::Texture &dst, glm::ivec2 &dstSize, glm::ivec2 size);
};
