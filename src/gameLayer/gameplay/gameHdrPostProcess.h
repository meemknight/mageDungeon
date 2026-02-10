#pragma once

#include <gl2d/gl2d.h>

// Full-frame HDR post process for gameplay rendering.
// In SDL_gpu mode we render to HDR (R11G11B10) then apply a selectable tonemapper.
// Legacy backend keeps this disabled and rendering stays unchanged.
struct GameHdrPostProcess
{
	bool enabled = true;

	// Tonemapper mode used by the fullscreen HDR resolve pass.
	enum ToneMapper
	{
		ToneMapper_ACESFitted = 0,
		ToneMapper_AGX,
		ToneMapper_ZCAM,
		ToneMapper_Uncharted2,
		ToneMapper_PBRNeutral,
		ToneMapper_Count,
	};

	int toneMapper = ToneMapper_PBRNeutral;
	float exposure = 1.35f;

	// Extra pre-tonemap color grading controls.
	float saturation = 1.0f;
	float vibrance = 0.95f;
	float gamma = 1.0f;
	float shadowBoost = -0.5f;
	float highlightBoost = 0.15f;
	float vignette = 0.15f;
	glm::vec3 lift = {0.0f, 0.0f, 0.0f};
	glm::vec3 gain = {1.0f, 1.0f, 1.0f};

	void init();
	void cleanup();
	void reloadShaders();
	void updateWindowMetrics(gl2d::Renderer2D &renderer);

	bool beginScene(gl2d::Renderer2D &renderer);
	void endScene(gl2d::Renderer2D &renderer);

	private:
	#if GL2D_USE_SDL_GPU
		gl2d::FrameBuffer hdrFbo;
		gl2d::FrameBuffer toneMappedFbo;
		SDL_GPUShader *vertexShader = nullptr;
		SDL_GPUShader *fragmentShader = nullptr;
		SDL_GPUGraphicsPipeline *pipeline = nullptr;
		SDL_GPUSampler *nearestSampler = nullptr;

		bool frameActive = false;

		bool ensureResources(gl2d::Renderer2D &renderer);
		bool applyToneMapping(gl2d::Renderer2D &renderer);
	#endif
};
