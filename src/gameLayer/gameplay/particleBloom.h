#pragma once

#include <gl2d/gl2d.h>

// Pixel-art bloom post process for particles.
// This runs as an optional offscreen SDL_gpu pass and is disabled on legacy backend.
struct ParticleBloomPostProcess
{
	bool enabled = true;

	void init();
	void cleanup();
	void reloadShaders();
	void updateWindowMetrics(gl2d::Renderer2D &renderer, int width, int height);
	bool apply(gl2d::Renderer2D &renderer, gl2d::FrameBuffer &sourceFbo);
	gl2d::Texture getOutputTexture(gl2d::FrameBuffer &sourceFbo) const;

	private:
	#if GL2D_USE_SDL_GPU
		gl2d::FrameBuffer bloomFbo;
		gl2d::FrameBuffer bloomBlurFbo;
		SDL_GPUShader *vertexShader = nullptr;
		SDL_GPUShader *fragmentShader = nullptr;
		SDL_GPUGraphicsPipeline *pipeline = nullptr;
		SDL_GPUSampler *nearestSampler = nullptr;

		bool ensureResources(gl2d::Renderer2D &renderer);
	#endif

		bool lastApplySucceeded = false;
};
