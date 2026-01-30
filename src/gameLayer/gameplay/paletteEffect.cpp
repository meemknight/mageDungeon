#include "gameplay/paletteEffect.h"
#include <gameLayer.h>
#include <logs.h>
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include <limits>

static inline glm::u8vec4 toColor(const unsigned char *data, int index)
{
	return {data[index], data[index + 1], data[index + 2], data[index + 3]};
}

static inline int colorDistanceSq(const glm::u8vec4 &a, const glm::u8vec4 &b)
{
	int dr = (int)a.r - (int)b.r;
	int dg = (int)a.g - (int)b.g;
	int db = (int)a.b - (int)b.b;
	return dr * dr + dg * dg + db * db;
}

static inline float clamp01(float value)
{
	if (value < 0.0f) { return 0.0f; }
	if (value > 1.0f) { return 1.0f; }
	return value;
}

static inline unsigned char clampByte(int value)
{
	if (value < 0) { return 0; }
	if (value > 255) { return 255; }
	return (unsigned char)value;
}

static inline unsigned char blendChannel(unsigned char src, unsigned char pal, float strength)
{
	float out = (float)src * (1.0f - strength) + (float)pal * strength;
	int rounded = (int)(out + 0.5f);
	return clampByte(rounded);
}

static glm::u8vec4 findClosestColor(const glm::u8vec4 &color,
	const std::vector<glm::u8vec4> &palette)
{
	int bestDist = std::numeric_limits<int>::max();
	glm::u8vec4 best = palette.front();
	for (auto &p : palette)
	{
		int dist = colorDistanceSq(color, p);
		if (dist < bestDist)
		{
			bestDist = dist;
			best = p;
		}
	}
	return best;
}

bool PaletteEffect::applyToBuffer(std::vector<unsigned char> &rgba)
{
	if (!hasPalette() || rgba.size() < 4)
	{
		return false;
	}

	float paletteStrength = clamp01(strength);
	const size_t pixelCount = rgba.size() / 4;
	for (size_t i = 0; i < pixelCount; i++)
	{
		size_t idx = i * 4;
		glm::u8vec4 srcColor = {rgba[idx], rgba[idx + 1], rgba[idx + 2], rgba[idx + 3]};
		if (srcColor.a == 0)
		{
			continue;
		}
		glm::u8vec4 palColor = findClosestColor(srcColor, colors);
		rgba[idx] = blendChannel(srcColor.r, palColor.r, paletteStrength);
		rgba[idx + 1] = blendChannel(srcColor.g, palColor.g, paletteStrength);
		rgba[idx + 2] = blendChannel(srcColor.b, palColor.b, paletteStrength);
		rgba[idx + 3] = srcColor.a;
	}

	return true;
}

static bool ensureOutputTexture(gl2d::Texture &tex, glm::ivec2 &currentSize,
	glm::ivec2 newSize, bool pixelated)
{
	if (newSize.x <= 0 || newSize.y <= 0)
	{
		return false;
	}

	if (!tex.isValid() || currentSize != newSize)
	{
		tex.cleanup();
		tex.tex = SDL_CreateTexture(platform::getSdlRenderer(), SDL_PIXELFORMAT_ABGR8888,
			SDL_TEXTUREACCESS_STATIC, newSize.x, newSize.y);
		if (!tex.tex)
		{
			return false;
		}
		SDL_SetTextureBlendMode(tex.tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(tex.tex, pixelated ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
		currentSize = newSize;
	}

	return true;
}

void PaletteEffect::loadPalette()
{
	loaded = false;
	colors.clear();
	palettePath.clear();

	std::string directory = std::string(RESOURCES_PATH) + "palette/";
	if (!std::filesystem::exists(directory))
	{
		platform::log("Palette directory missing", LogManager::logWarning);
		return;
	}

	for (auto &entry : std::filesystem::directory_iterator(directory))
	{
		if (!entry.is_regular_file()) { continue; }
		auto path = entry.path();
		if (path.extension() == ".png")
		{
			palettePath = path.string();
			break;
		}
	}

	if (palettePath.empty())
	{
		platform::log("No palette PNG found", LogManager::logWarning);
		return;
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	unsigned char *data = stbi_load(palettePath.c_str(), &width, &height, &channels, 4);
	if (!data)
	{
		platform::log("Failed to load palette PNG", LogManager::logWarning);
		return;
	}

	paletteSize = {width, height};
	colors.reserve(width * height);
	std::unordered_set<unsigned int> seen;
	for (int i = 0; i < width * height; i++)
	{
		glm::u8vec4 c = toColor(data, i * 4);
		if (c.a == 0) { continue; }
		unsigned int key = (unsigned int)c.r | ((unsigned int)c.g << 8) | ((unsigned int)c.b << 16);
		if (seen.insert(key).second)
		{
			colors.push_back(c);
		}
	}

	STBI_FREE(data);
	loaded = !colors.empty();
}

bool PaletteEffect::applyToTexture(gl2d::Renderer2D &renderer, gl2d::Texture &src,
	gl2d::Texture &dst, glm::ivec2 &dstSize, glm::ivec2 size)
{
	if (!hasPalette() || !src.isValid())
	{
		return false;
	}

	float paletteStrength = clamp01(strength);
	SDL_Renderer *sdlRenderer = renderer.sdlRenderer;
	SDL_Texture *oldTarget = SDL_GetRenderTarget(sdlRenderer);
	SDL_SetRenderTarget(sdlRenderer, src.tex);
	SDL_FlushRenderer(sdlRenderer);
	SDL_Surface *surface = SDL_RenderReadPixels(sdlRenderer, nullptr);
	SDL_SetRenderTarget(sdlRenderer, oldTarget);
	if (!surface)
	{
		return false;
	}

	SDL_Surface *converted = surface;
	if (surface->format != SDL_PIXELFORMAT_ABGR8888)
	{
		converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
		SDL_DestroySurface(surface);
		if (!converted)
		{
			return false;
		}
	}

	glm::ivec2 texSize = {converted->w, converted->h};
	if (!ensureOutputTexture(dst, dstSize, texSize, true))
	{
		SDL_DestroySurface(converted);
		return false;
	}

	const size_t byteCount = (size_t)texSize.x * (size_t)texSize.y * 4;
	writeBuffer.resize(byteCount);

	unsigned char *pixels = static_cast<unsigned char *>(converted->pixels);
	int pitch = converted->pitch;
	for (int y = 0; y < texSize.y; y++)
	{
		unsigned char *row = pixels + y * pitch;
		for (int x = 0; x < texSize.x; x++)
		{
			unsigned char *p = row + x * 4;
			glm::u8vec4 srcColor = {p[0], p[1], p[2], p[3]};
			glm::u8vec4 outColor = srcColor;
			if (srcColor.a != 0)
			{
				glm::u8vec4 palColor = findClosestColor(srcColor, colors);
				outColor = {
					blendChannel(srcColor.r, palColor.r, paletteStrength),
					blendChannel(srcColor.g, palColor.g, paletteStrength),
					blendChannel(srcColor.b, palColor.b, paletteStrength),
					srcColor.a
				};
			}
			size_t dstIndex = (size_t)(y * texSize.x + x) * 4;
			writeBuffer[dstIndex] = outColor.r;
			writeBuffer[dstIndex + 1] = outColor.g;
			writeBuffer[dstIndex + 2] = outColor.b;
			writeBuffer[dstIndex + 3] = outColor.a;
		}
	}

	SDL_DestroySurface(converted);

	SDL_UpdateTexture(dst.tex, nullptr, writeBuffer.data(), texSize.x * 4);
	return true;
}
