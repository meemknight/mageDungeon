#include "gameplay/gameHdrPostProcess.h"

#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>

#if GL2D_USE_SDL_GPU
namespace
{
	bool readBinaryFile(const char *path, std::vector<Uint8> &outData)
	{
		outData.clear();
		if (!path) { return false; }

		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open()) { return false; }

		auto size = file.tellg();
		if (size <= 0) { return false; }
		file.seekg(0, std::ios::beg);

		outData.resize(static_cast<size_t>(size));
		file.read(reinterpret_cast<char *>(outData.data()), size);
		return file.good();
	}

	SDL_GPUGraphicsPipeline *createToneMapPipeline(SDL_GPUDevice *device,
		SDL_GPUShader *vertexShader,
		SDL_GPUShader *fragmentShader,
		SDL_GPUTextureFormat targetFormat)
	{
		if (!device || !vertexShader || !fragmentShader)
		{
			return nullptr;
		}

		SDL_GPUVertexInputState vertexInput = {};

		SDL_GPURasterizerState rasterizer = {};
		rasterizer.fill_mode = SDL_GPU_FILLMODE_FILL;
		rasterizer.cull_mode = SDL_GPU_CULLMODE_NONE;
		rasterizer.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
		rasterizer.enable_depth_clip = false;

		SDL_GPUMultisampleState multisample = {};
		multisample.sample_count = SDL_GPU_SAMPLECOUNT_1;

		SDL_GPUDepthStencilState depthStencil = {};
		depthStencil.enable_depth_test = false;
		depthStencil.enable_depth_write = false;
		depthStencil.enable_stencil_test = false;

		SDL_GPUColorTargetBlendState blend = {};
		blend.enable_blend = false;
		blend.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
			SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

		SDL_GPUColorTargetDescription targetDesc = {};
		targetDesc.format = targetFormat;
		targetDesc.blend_state = blend;

		SDL_GPUGraphicsPipelineTargetInfo targetInfo = {};
		targetInfo.num_color_targets = 1;
		targetInfo.color_target_descriptions = &targetDesc;
		targetInfo.has_depth_stencil_target = false;

		SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.vertex_shader = vertexShader;
		pipelineInfo.fragment_shader = fragmentShader;
		pipelineInfo.vertex_input_state = vertexInput;
		pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineInfo.rasterizer_state = rasterizer;
		pipelineInfo.multisample_state = multisample;
		pipelineInfo.depth_stencil_state = depthStencil;
		pipelineInfo.target_info = targetInfo;

		return SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
	}

	struct ToneMapUniformData
	{
		// x: tonemapper index, y: exposure, z: saturation, w: vibrance
		float toneMapData[4] = {};
		// x: gamma, y: shadowBoost, z: highlightBoost, w: vignette
		float gradingData[4] = {};
		// xyz: lift
		float lift[4] = {};
		// xyz: gain
		float gain[4] = {};
		// x: hasCosmeticLightMask
		float extraData[4] = {};
	};
}
#endif

void GameHdrPostProcess::init()
{
	#if GL2D_USE_SDL_GPU
	// Reset to struct defaults so tone/grading values stay defined in one place.
	*this = {};

	// Main scene target in HDR format for wide luminance range.
	hdrFbo.gpuTextureFormat = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;

	// Tone mapped output stays in regular LDR for final sprite compose.
	toneMappedFbo.gpuTextureFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	#endif
}

void GameHdrPostProcess::setCosmeticLightMaskTexture(gl2d::Texture texture)
{
	#if GL2D_USE_SDL_GPU
	cosmeticLightMaskTexture = texture;
	#else
	(void)texture;
	#endif
}

void GameHdrPostProcess::cleanup()
{
	#if GL2D_USE_SDL_GPU
	reloadShaders();

	if (hdrFbo.texture.gpuDevice && nearestSampler)
	{
		auto *device = hdrFbo.texture.gpuDevice;
		SDL_ReleaseGPUSampler(device, nearestSampler);
		nearestSampler = nullptr;
	}

	hdrFbo.cleanup();
	toneMappedFbo.cleanup();
	frameActive = false;
	#endif
}

void GameHdrPostProcess::reloadShaders()
{
	#if GL2D_USE_SDL_GPU
	frameActive = false;

	SDL_GPUDevice *device = hdrFbo.texture.gpuDevice;
	if (!device)
	{
		pipeline = nullptr;
		vertexShader = nullptr;
		fragmentShader = nullptr;
		return;
	}

	if (pipeline)
	{
		SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
		pipeline = nullptr;
	}

	if (vertexShader)
	{
		SDL_ReleaseGPUShader(device, vertexShader);
		vertexShader = nullptr;
	}

	if (fragmentShader)
	{
		SDL_ReleaseGPUShader(device, fragmentShader);
		fragmentShader = nullptr;
	}
	#endif
}

void GameHdrPostProcess::updateWindowMetrics(gl2d::Renderer2D &renderer)
{
	#if GL2D_USE_SDL_GPU
	if (!renderer.gpuDevice) { return; }

	int w = std::max(renderer.windowW, 1);
	int h = std::max(renderer.windowH, 1);
	hdrFbo.resize(w, h);
	toneMappedFbo.resize(w, h);
	#else
	(void)renderer;
	#endif
}

bool GameHdrPostProcess::beginScene(gl2d::Renderer2D &renderer)
{
	#if GL2D_USE_SDL_GPU
	if (!enabled || !renderer.gpuDevice)
	{
		frameActive = false;
		return false;
	}

	updateWindowMetrics(renderer);
	if (!hdrFbo.texture.gpuTexture)
	{
		frameActive = false;
		return false;
	}

	hdrFbo.bind();
	// Keep out-of-map pixels black so additive cosmetic light cannot turn them white.
	renderer.clearScreen({0, 0, 0, 1});
	frameActive = true;
	return true;
	#else
	(void)renderer;
	return false;
	#endif
}

void GameHdrPostProcess::endScene(gl2d::Renderer2D &renderer)
{
	#if GL2D_USE_SDL_GPU
	if (!frameActive)
	{
		return;
	}

	frameActive = false;
	if (!renderer.gpuDevice)
	{
		return;
	}

	// Resolve all batched scene draws into the HDR target before tone mapping.
	renderer.flush(true);
	hdrFbo.unbind();

	const bool toneMapOk = applyToneMapping(renderer);

	auto oldBlendMode = renderer.getBlendMode();
	renderer.pushCamera();
	renderer.setBlendMode(gl2d::Renderer2D::BlendMode_Alpha);

	if (toneMapOk)
	{
		// Output from this custom pass is Y-flipped, so we flip UVs here.
		renderer.renderRectangle({0, 0, (float)renderer.windowW, (float)renderer.windowH},
			toneMappedFbo.texture, {1, 1, 1, 1}, {}, {}, {0, 1, 1, 0});
	}
	else
	{
		renderer.renderRectangle({0, 0, (float)renderer.windowW, (float)renderer.windowH},
			hdrFbo.texture, {1, 1, 1, 1}, {}, {}, {0, 0, 1, 1});
	}

	renderer.setBlendMode(oldBlendMode);
	renderer.popCamera();
	#else
	(void)renderer;
	#endif
}

#if GL2D_USE_SDL_GPU
bool GameHdrPostProcess::ensureResources(gl2d::Renderer2D &renderer)
{
	if (!renderer.gpuDevice)
	{
		return false;
	}

	if (pipeline && vertexShader && fragmentShader && nearestSampler)
	{
		return true;
	}

	const char *driver = SDL_GetGPUDeviceDriver(renderer.gpuDevice);
	if (!driver || strcmp(driver, "vulkan") != 0)
	{
		return false;
	}

	std::vector<Uint8> vertexCode;
	std::vector<Uint8> fragmentCode;
	if (!readBinaryFile(RESOURCES_PATH "shaders/game_tonemap.vert.spv", vertexCode)
		|| !readBinaryFile(RESOURCES_PATH "shaders/game_tonemap.frag.spv", fragmentCode))
	{
		return false;
	}

	if (!vertexShader)
	{
		SDL_GPUShaderCreateInfo vertexInfo = {};
		vertexInfo.code_size = vertexCode.size();
		vertexInfo.code = vertexCode.data();
		vertexInfo.entrypoint = "main";
		vertexInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
		vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
		vertexInfo.num_samplers = 0;
		vertexInfo.num_storage_textures = 0;
		vertexInfo.num_storage_buffers = 0;
		vertexInfo.num_uniform_buffers = 0;
		vertexShader = SDL_CreateGPUShader(renderer.gpuDevice, &vertexInfo);
	}

	if (!fragmentShader)
	{
		SDL_GPUShaderCreateInfo fragmentInfo = {};
		fragmentInfo.code_size = fragmentCode.size();
		fragmentInfo.code = fragmentCode.data();
		fragmentInfo.entrypoint = "main";
		fragmentInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
		fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
		fragmentInfo.num_samplers = 2;
		fragmentInfo.num_storage_textures = 0;
		fragmentInfo.num_storage_buffers = 0;
		fragmentInfo.num_uniform_buffers = 1;
		fragmentShader = SDL_CreateGPUShader(renderer.gpuDevice, &fragmentInfo);
	}

	if (!nearestSampler)
	{
		SDL_GPUSamplerCreateInfo samplerInfo = {};
		samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
		samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
		samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
		samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		nearestSampler = SDL_CreateGPUSampler(renderer.gpuDevice, &samplerInfo);
	}

	if (!pipeline && vertexShader && fragmentShader)
	{
		SDL_GPUTextureFormat targetFormat = toneMappedFbo.texture.gpuFormat;
		if (targetFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
		{
			targetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		}

		pipeline = createToneMapPipeline(renderer.gpuDevice, vertexShader, fragmentShader, targetFormat);
	}

	return pipeline && vertexShader && fragmentShader && nearestSampler;
}

bool GameHdrPostProcess::applyToneMapping(gl2d::Renderer2D &renderer)
{
	if (!renderer.gpuDevice || !hdrFbo.texture.gpuTexture || !toneMappedFbo.texture.gpuTexture)
	{
		return false;
	}

	if (!ensureResources(renderer))
	{
		return false;
	}

	SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(renderer.gpuDevice);
	if (!commandBuffer)
	{
		return false;
	}

	SDL_GPUColorTargetInfo colorTarget = {};
	colorTarget.texture = toneMappedFbo.texture.gpuTexture;
	colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
	colorTarget.store_op = SDL_GPU_STOREOP_STORE;
	colorTarget.clear_color = {0, 0, 0, 1};

	SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
	if (!renderPass)
	{
		SDL_CancelGPUCommandBuffer(commandBuffer);
		return false;
	}

	SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

	SDL_GPUViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.w = static_cast<float>(toneMappedFbo.w);
	viewport.h = static_cast<float>(toneMappedFbo.h);
	viewport.min_depth = 0.0f;
	viewport.max_depth = 1.0f;
	SDL_SetGPUViewport(renderPass, &viewport);

	SDL_Rect scissor = {0, 0, toneMappedFbo.w, toneMappedFbo.h};
	SDL_SetGPUScissor(renderPass, &scissor);

	SDL_GPUTextureSamplerBinding samplerBindings[2] = {};
	samplerBindings[0].texture = hdrFbo.texture.gpuTexture;
	samplerBindings[0].sampler = nearestSampler;
	samplerBindings[1].texture = cosmeticLightMaskTexture.gpuTexture ?
		cosmeticLightMaskTexture.gpuTexture : hdrFbo.texture.gpuTexture;
	samplerBindings[1].sampler = nearestSampler;
	SDL_BindGPUFragmentSamplers(renderPass, 0, samplerBindings, 2);

	ToneMapUniformData toneMapUniform = {};
	toneMapUniform.toneMapData[0] = (float)std::clamp(toneMapper, 0, ToneMapper_Count - 1);
	toneMapUniform.toneMapData[1] = std::max(exposure, 0.0f);
	toneMapUniform.toneMapData[2] = saturation;
	toneMapUniform.toneMapData[3] = vibrance;
	toneMapUniform.gradingData[0] = std::max(gamma, 0.001f);
	toneMapUniform.gradingData[1] = shadowBoost;
	toneMapUniform.gradingData[2] = highlightBoost;
	toneMapUniform.gradingData[3] = std::clamp(vignette, 0.0f, 1.0f);
	toneMapUniform.lift[0] = lift.x;
	toneMapUniform.lift[1] = lift.y;
	toneMapUniform.lift[2] = lift.z;
	toneMapUniform.gain[0] = gain.x;
	toneMapUniform.gain[1] = gain.y;
	toneMapUniform.gain[2] = gain.z;
	toneMapUniform.extraData[0] = cosmeticLightMaskTexture.gpuTexture ? 1.0f : 0.0f;
	SDL_PushGPUFragmentUniformData(commandBuffer, 0, &toneMapUniform, sizeof(toneMapUniform));

	SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
	SDL_EndGPURenderPass(renderPass);

	if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
	{
		return false;
	}

	return true;
}
#endif
