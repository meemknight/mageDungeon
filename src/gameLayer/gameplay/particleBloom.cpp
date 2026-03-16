#include "particleBloom.h"

#include <fstream>
#include <vector>
#include <cstring>

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

	SDL_GPUGraphicsPipeline *createPipelineForBloomPass(SDL_GPUDevice *device,
		SDL_GPUShader *vertexShader,
		SDL_GPUShader *fragmentShader)
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
		targetDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
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

	struct BloomPassUniformData
	{
		// 0 = horizontal extract blur, 1 = vertical blur.
		float passMode = 0.0f;
		float padding0 = 0.0f;
		float padding1 = 0.0f;
		float padding2 = 0.0f;
	};
}
#endif

void ParticleBloomPostProcess::init()
{
	lastApplySucceeded = false;

	#if GL2D_USE_SDL_GPU
	// Bloom runs into dedicated low-res intermediate + final targets.
	bloomFbo.create(1, 1, true);
	bloomBlurFbo.create(1, 1, true);
	#endif
}

void ParticleBloomPostProcess::cleanup()
{
	lastApplySucceeded = false;

	#if GL2D_USE_SDL_GPU
	reloadShaders();

	if (bloomFbo.texture.gpuDevice && nearestSampler)
	{
		auto *device = bloomFbo.texture.gpuDevice;
		SDL_ReleaseGPUSampler(device, nearestSampler);
		nearestSampler = nullptr;
	}

	bloomFbo.cleanup();
	bloomBlurFbo.cleanup();
	#endif
}

void ParticleBloomPostProcess::reloadShaders()
{
	lastApplySucceeded = false;

	#if GL2D_USE_SDL_GPU
	SDL_GPUDevice *device = bloomFbo.texture.gpuDevice;
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

void ParticleBloomPostProcess::updateWindowMetrics(gl2d::Renderer2D &, int width, int height)
{
	#if GL2D_USE_SDL_GPU
	if (width <= 0 || height <= 0) { return; }
	bloomFbo.resize(width, height);
	bloomBlurFbo.resize(width, height);
	#endif
}

bool ParticleBloomPostProcess::apply(gl2d::Renderer2D &renderer, gl2d::FrameBuffer &sourceFbo)
{
	lastApplySucceeded = false;

	#if GL2D_USE_SDL_GPU
	if (!enabled || !renderer.gpuDevice)
	{
		return false;
	}

	if (!sourceFbo.texture.gpuTexture || sourceFbo.w <= 0 || sourceFbo.h <= 0)
	{
		return false;
	}

	updateWindowMetrics(renderer, sourceFbo.w, sourceFbo.h);
	if (!bloomFbo.texture.gpuTexture || !bloomBlurFbo.texture.gpuTexture)
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

	SDL_GPUViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.w = static_cast<float>(bloomFbo.w);
	viewport.h = static_cast<float>(bloomFbo.h);
	viewport.min_depth = 0.f;
	viewport.max_depth = 1.f;

	SDL_Rect scissor = {0, 0, bloomFbo.w, bloomFbo.h};

	// First pass: horizontal blur + bright extraction.
	SDL_GPUColorTargetInfo horizontalTarget = {};
	horizontalTarget.texture = bloomBlurFbo.texture.gpuTexture;
	horizontalTarget.load_op = SDL_GPU_LOADOP_CLEAR;
	horizontalTarget.store_op = SDL_GPU_STOREOP_STORE;
	horizontalTarget.clear_color = {0, 0, 0, 0};

	SDL_GPURenderPass *horizontalPass = SDL_BeginGPURenderPass(commandBuffer, &horizontalTarget, 1, nullptr);
	if (!horizontalPass)
	{
		SDL_CancelGPUCommandBuffer(commandBuffer);
		return false;
	}

	SDL_BindGPUGraphicsPipeline(horizontalPass, pipeline);
	SDL_SetGPUViewport(horizontalPass, &viewport);
	SDL_SetGPUScissor(horizontalPass, &scissor);

	SDL_GPUTextureSamplerBinding sourceBinding = {};
	sourceBinding.texture = sourceFbo.texture.gpuTexture;
	sourceBinding.sampler = nearestSampler;
	SDL_BindGPUFragmentSamplers(horizontalPass, 0, &sourceBinding, 1);

	BloomPassUniformData horizontalUniform = {};
	horizontalUniform.passMode = 0.0f;
	SDL_PushGPUFragmentUniformData(commandBuffer, 0, &horizontalUniform, sizeof(horizontalUniform));

	SDL_DrawGPUPrimitives(horizontalPass, 3, 1, 0, 0);
	SDL_EndGPURenderPass(horizontalPass);

	// Second pass: vertical blur from intermediate texture.
	SDL_GPUColorTargetInfo verticalTarget = {};
	verticalTarget.texture = bloomFbo.texture.gpuTexture;
	verticalTarget.load_op = SDL_GPU_LOADOP_CLEAR;
	verticalTarget.store_op = SDL_GPU_STOREOP_STORE;
	verticalTarget.clear_color = {0, 0, 0, 0};

	SDL_GPURenderPass *verticalPass = SDL_BeginGPURenderPass(commandBuffer, &verticalTarget, 1, nullptr);
	if (!verticalPass)
	{
		SDL_CancelGPUCommandBuffer(commandBuffer);
		return false;
	}

	SDL_BindGPUGraphicsPipeline(verticalPass, pipeline);
	SDL_SetGPUViewport(verticalPass, &viewport);
	SDL_SetGPUScissor(verticalPass, &scissor);

	SDL_GPUTextureSamplerBinding bloomBinding = {};
	bloomBinding.texture = bloomBlurFbo.texture.gpuTexture;
	bloomBinding.sampler = nearestSampler;
	SDL_BindGPUFragmentSamplers(verticalPass, 0, &bloomBinding, 1);

	BloomPassUniformData verticalUniform = {};
	verticalUniform.passMode = 1.0f;
	SDL_PushGPUFragmentUniformData(commandBuffer, 0, &verticalUniform, sizeof(verticalUniform));

	SDL_DrawGPUPrimitives(verticalPass, 3, 1, 0, 0);
	SDL_EndGPURenderPass(verticalPass);

	if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
	{
		return false;
	}

	lastApplySucceeded = true;
	return true;
	#else
	(void)renderer;
	(void)sourceFbo;
	return false;
	#endif
}

gl2d::Texture ParticleBloomPostProcess::getOutputTexture(gl2d::FrameBuffer &sourceFbo) const
{
	#if GL2D_USE_SDL_GPU
	if (lastApplySucceeded && bloomFbo.texture.isValid())
	{
		return bloomFbo.texture;
	}
	#endif

	return sourceFbo.texture;
}

#if GL2D_USE_SDL_GPU
bool ParticleBloomPostProcess::ensureResources(gl2d::Renderer2D &renderer)
{
	if (!renderer.gpuDevice)
	{
		return false;
	}

	if (pipeline && vertexShader && fragmentShader && nearestSampler)
	{
		return true;
	}

	if ((SDL_GetGPUShaderFormats(renderer.gpuDevice) & SDL_GPU_SHADERFORMAT_SPIRV) == 0)
	{
		return false;
	}

	std::vector<Uint8> vertexCode;
	std::vector<Uint8> fragmentCode;
	if (!readBinaryFile(RESOURCES_PATH "shaders/particle_bloom.vert.spv", vertexCode)
		|| !readBinaryFile(RESOURCES_PATH "shaders/particle_bloom.frag.spv", fragmentCode))
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
		fragmentInfo.num_samplers = 1;
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
		pipeline = createPipelineForBloomPass(renderer.gpuDevice,
			vertexShader, fragmentShader);
	}

	return pipeline && vertexShader && fragmentShader && nearestSampler;
}
#endif
