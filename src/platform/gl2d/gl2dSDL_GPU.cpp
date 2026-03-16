#include "gl2d/gl2d.h"

#if GL2D_USE_SDL_GPU

#include <gameLayer.h>
#include <cmath>

//THIS IS A SDL PORT OF GL2D

/////////////////////////////////////////////////////////////////
//gl2d.cpp				1.6.4 work in progress
//Copyright(c) 2020 - 2025 Luta Vlad
//https://github.com/meemknight/gl2d
// 
//notes: 
// 1.2.1
// fixed alpha in the particle system with the 
// post process
// 
// 1.2.2
// added default values to structs
// added some more error reporting
// added option to change gl version and 
//  shader presision
// vsynk independend of gl loader
// 
// 1.2.3
// small problems fixes
// texture load flags
// working on 9pathces
// 
// 1.2.4
// working at fixing get text size
// 
// 1.2.5
// push pop shaders and camera
// added getViewRect
// 
// 1.2.6
// updated camera.follow
// removed TextureRegion
// 
// 1.3.0
// polished using custom shader api
// fixed camera follow
// moved the particle system into another file
// added a proper cmake
// used the proper stbi free function
// added a default fbo support
// added proper error reporting (with uer defined data)
// 
// 1.4.0
// much needed api refactoring
// removed capacity render limit
// added some more comments
// 
// 1.4.1
// line rendering
// rect outline rendering
// circle outline rendering
// 
// 1.5.0
// started to add some more needed text functions
// needed to be tested tho
// 
// 1.5.1
// fixed the follow function
// 
// 1.5.2
// read texture data + report error if opengl not loaded
// 
// 1.6.0
// Added post processing API + Improvements in custom shaders usage
// 
// 1.6.1
// trying to fix some text stuff
//
// 1.6.2
// finally fixed font rendering problems
// 
// 1.6.3
// added depth option to the framebuffer
// 
// 1.6.4 work in progress
// work in progress for 3D camera
// 
////////////////////////////////////////////////////////////////////////


//	todo
//
//	add particle demo
//	refactor particle system to woth with the new post process api
//	add matrices transforms
//	flags for vbos
//	add render circle
//	
//

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <gl2d/gl2d.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>

//if you are not using visual studio make shure you link to "Opengl32.lib"
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4244 4305 4267 4996 4018)
#pragma comment(lib, "Opengl32.lib")
#endif

#undef max


namespace gl2d
{

	static Camera defaultCamera = {};
	static Texture white1pxSquareTexture = {};
	

	static errorFuncType *errorFunc = defaultErrorFunc;

	void defaultErrorFunc(const char *msg, void *userDefinedData)
	{
		std::cerr << "gl2d error: " << msg << "\n";
	}

	void *userDefinedData = 0;
	void setUserDefinedData(void *data)
	{
		userDefinedData = data;
	}

	errorFuncType *setErrorFuncCallback(errorFuncType *newFunc)
	{
		auto a = errorFunc;
		errorFunc = newFunc;
		return a;
	}

	namespace
	{
		// Active renderer pointer used by FrameBuffer bind/unbind hooks.
		Renderer2D *activeRendererInstance = nullptr;
		// Global GPU device used by texture/framebuffer helpers.
		SDL_GPUDevice *globalGpuDevice = nullptr;
		// Owned devices are released on gl2d::cleanup after texture cleanup.
		SDL_GPUDevice *deferredOwnedGpuDevice = nullptr;
		// Last clear color used by framebuffer clear() helper.
		Color4f lastFrameBufferClearColor = {};

		struct BatchVertex
		{
			float pos[2] = {};
			float uv[2] = {};
			// Keep color in float to preserve overbright (>1) tints in shader math.
			float color[4] = {};
		};

		static inline bool rectEquals(const SDL_Rect &a, const SDL_Rect &b)
		{
			return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
		}

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

		void runDevelopmentShaderCompileScriptOnce()
		{
		#if defined(_WIN32) && defined(DEVELOPLEMT_BUILD) && (DEVELOPLEMT_BUILD == 1)
			static bool hasTriedCompile = false;
			if (hasTriedCompile)
			{
				return;
			}
			hasTriedCompile = true;

			// In development builds we auto-compile all local shader sources at startup.
			std::string scriptPath = std::string(RESOURCES_PATH) + "shaders/compile_all_shaders.bat";
			for (char &c : scriptPath)
			{
				if (c == '/')
				{
					c = '\\';
				}
			}
			std::ifstream scriptFile(scriptPath);
			if (!scriptFile.is_open())
			{
				errorFunc("Missing shader compile script at resources/shaders/compile_all_shaders.bat", userDefinedData);
				return;
			}

			const std::string command = std::string("cmd.exe /C call \"") + scriptPath + "\"";
			const int result = std::system(command.c_str());
			if (result != 0)
			{
				errorFunc("Failed to run resources/shaders/compile_all_shaders.bat", userDefinedData);
			}
		#endif
		}

		bool supportsSpirvShaders(SDL_GPUDevice *device)
		{
			if (!device)
			{
				return false;
			}
			return (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_SPIRV) != 0;
		}

		bool uploadRGBA8Texture(SDL_GPUDevice *device, SDL_GPUTexture *texture,
			const char *imageData, int width, int height)
		{
			if (!device || !texture || !imageData || width <= 0 || height <= 0)
			{
				return false;
			}

			const uint32_t uploadBytes = static_cast<uint32_t>(width * height * 4);
			SDL_GPUTransferBufferCreateInfo transferInfo = {};
			transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			transferInfo.size = uploadBytes;
			SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
			if (!transfer)
			{
				errorFunc("Failed to create SDL GPU transfer buffer for texture upload", userDefinedData);
				return false;
			}

			void *mapped = SDL_MapGPUTransferBuffer(device, transfer, true);
			if (!mapped)
			{
				errorFunc("Failed to map SDL GPU transfer buffer for texture upload", userDefinedData);
				SDL_ReleaseGPUTransferBuffer(device, transfer);
				return false;
			}

			std::memcpy(mapped, imageData, uploadBytes);
			SDL_UnmapGPUTransferBuffer(device, transfer);

			SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
			if (!cmd)
			{
				errorFunc("Failed to acquire SDL GPU command buffer for texture upload", userDefinedData);
				SDL_ReleaseGPUTransferBuffer(device, transfer);
				return false;
			}

			SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
			if (!copyPass)
			{
				errorFunc("Failed to begin SDL GPU copy pass for texture upload", userDefinedData);
				SDL_CancelGPUCommandBuffer(cmd);
				SDL_ReleaseGPUTransferBuffer(device, transfer);
				return false;
			}

			SDL_GPUTextureTransferInfo src = {};
			src.transfer_buffer = transfer;
			src.offset = 0;
			src.pixels_per_row = static_cast<uint32_t>(width);
			src.rows_per_layer = static_cast<uint32_t>(height);

			SDL_GPUTextureRegion dst = {};
			dst.texture = texture;
			dst.mip_level = 0;
			dst.layer = 0;
			dst.x = 0;
			dst.y = 0;
			dst.z = 0;
			dst.w = static_cast<uint32_t>(width);
			dst.h = static_cast<uint32_t>(height);
			dst.d = 1;

			SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
			SDL_EndGPUCopyPass(copyPass);

			const bool submitted = SDL_SubmitGPUCommandBuffer(cmd);
			SDL_ReleaseGPUTransferBuffer(device, transfer);
			if (!submitted)
			{
				errorFunc("Failed to submit SDL GPU texture upload command buffer", userDefinedData);
				return false;
			}

			return true;
		}

		bool clearGpuTextureTarget(SDL_GPUDevice *device, SDL_GPUTexture *target, const Color4f &color)
		{
			if (!device || !target)
			{
				return false;
			}

			SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
			if (!cmd)
			{
				return false;
			}

			SDL_GPUColorTargetInfo colorTarget = {};
			colorTarget.texture = target;
			colorTarget.clear_color = SDL_FColor{color.r, color.g, color.b, color.a};
			colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTarget.store_op = SDL_GPU_STOREOP_STORE;

			SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &colorTarget, 1, nullptr);
			if (!pass)
			{
				SDL_CancelGPUCommandBuffer(cmd);
				return false;
			}

			SDL_EndGPURenderPass(pass);
			if (!SDL_SubmitGPUCommandBuffer(cmd))
			{
				return false;
			}

			return true;
		}

		bool clearGpuSwapchainTarget(SDL_GPUDevice *device, SDL_Window *window, const Color4f &color)
		{
			if (!device || !window)
			{
				return false;
			}

			SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
			if (!cmd)
			{
				return false;
			}

			SDL_GPUTexture *swapchainTexture = nullptr;
			Uint32 textureW = 0;
			Uint32 textureH = 0;
			if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchainTexture, &textureW, &textureH))
			{
				SDL_CancelGPUCommandBuffer(cmd);
				return false;
			}

			if (!swapchainTexture)
			{
				SDL_CancelGPUCommandBuffer(cmd);
				return true;
			}

			SDL_GPUColorTargetInfo colorTarget = {};
			colorTarget.texture = swapchainTexture;
			colorTarget.clear_color = SDL_FColor{color.r, color.g, color.b, color.a};
			colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTarget.store_op = SDL_GPU_STOREOP_STORE;

			SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &colorTarget, 1, nullptr);
			if (!pass)
			{
				SDL_CancelGPUCommandBuffer(cmd);
				return false;
			}

			SDL_EndGPURenderPass(pass);
			if (!SDL_SubmitGPUCommandBuffer(cmd))
			{
				return false;
			}

			return true;
		}

		void releaseRendererGpuResources(Renderer2D &renderer)
		{
			if (!renderer.gpuDevice) { return; }

			if (renderer.vertexBuffer)
			{
				SDL_ReleaseGPUBuffer(renderer.gpuDevice, renderer.vertexBuffer);
				renderer.vertexBuffer = nullptr;
			}
			if (renderer.vertexTransferBuffer)
			{
				SDL_ReleaseGPUTransferBuffer(renderer.gpuDevice, renderer.vertexTransferBuffer);
				renderer.vertexTransferBuffer = nullptr;
			}
			renderer.vertexBufferSize = 0;

			if (renderer.pipelineSwapchain)
			{
				SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineSwapchain);
				renderer.pipelineSwapchain = nullptr;
			}
			if (renderer.pipelineOffscreen)
			{
				SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineOffscreen);
				renderer.pipelineOffscreen = nullptr;
			}
			if (renderer.pipelineSwapchainAdditive)
			{
				SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineSwapchainAdditive);
				renderer.pipelineSwapchainAdditive = nullptr;
			}
			if (renderer.pipelineOffscreenAdditive)
			{
				SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineOffscreenAdditive);
				renderer.pipelineOffscreenAdditive = nullptr;
			}
			renderer.pipelineSwapchainFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
			renderer.pipelineOffscreenFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
			renderer.pipelineSwapchainAdditiveFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
			renderer.pipelineOffscreenAdditiveFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

			if (renderer.defaultVertexShader)
			{
				SDL_ReleaseGPUShader(renderer.gpuDevice, renderer.defaultVertexShader);
				renderer.defaultVertexShader = nullptr;
			}
			if (renderer.defaultFragmentShader)
			{
				SDL_ReleaseGPUShader(renderer.gpuDevice, renderer.defaultFragmentShader);
				renderer.defaultFragmentShader = nullptr;
			}

			if (renderer.samplerLinear)
			{
				SDL_ReleaseGPUSampler(renderer.gpuDevice, renderer.samplerLinear);
				renderer.samplerLinear = nullptr;
			}
			if (renderer.samplerNearest)
			{
				SDL_ReleaseGPUSampler(renderer.gpuDevice, renderer.samplerNearest);
				renderer.samplerNearest = nullptr;
			}
		}

		bool ensureDefaultSamplers(Renderer2D &renderer)
		{
			if (!renderer.gpuDevice) { return false; }

			if (!renderer.samplerLinear)
			{
				SDL_GPUSamplerCreateInfo sampler = {};
				sampler.min_filter = SDL_GPU_FILTER_LINEAR;
				sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
				sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
				sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
				sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
				sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
				renderer.samplerLinear = SDL_CreateGPUSampler(renderer.gpuDevice, &sampler);
				if (!renderer.samplerLinear)
				{
					errorFunc("Failed to create SDL GPU linear sampler", userDefinedData);
					return false;
				}
			}

			if (!renderer.samplerNearest)
			{
				SDL_GPUSamplerCreateInfo sampler = {};
				sampler.min_filter = SDL_GPU_FILTER_NEAREST;
				sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
				sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
				sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
				sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
				sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
				renderer.samplerNearest = SDL_CreateGPUSampler(renderer.gpuDevice, &sampler);
				if (!renderer.samplerNearest)
				{
					errorFunc("Failed to create SDL GPU nearest sampler", userDefinedData);
					return false;
				}
			}

			return true;
		}

		bool ensureDefaultShaders(Renderer2D &renderer)
		{
			if (!renderer.gpuDevice) { return false; }
			if (renderer.defaultVertexShader && renderer.defaultFragmentShader) { return true; }

			if (!supportsSpirvShaders(renderer.gpuDevice))
			{
				errorFunc("gl2d SDL GPU backend requires SPIR-V shader support", userDefinedData);
				return false;
			}

			const char *vertexPath = RESOURCES_PATH "shaders/gl2d_batch.vert.spv";
			const char *fragmentPath = RESOURCES_PATH "shaders/gl2d_batch.frag.spv";
			const SDL_GPUShaderFormat shaderFormat = SDL_GPU_SHADERFORMAT_SPIRV;

			std::vector<Uint8> vertexCode;
			std::vector<Uint8> fragmentCode;
			if (!readBinaryFile(vertexPath, vertexCode) || !readBinaryFile(fragmentPath, fragmentCode))
			{
				errorFunc("Missing gl2d SDL GPU shader binaries in resources/shaders (compile .vert/.frag with compile_all_shaders.bat)", userDefinedData);
				return false;
			}

			SDL_GPUShaderCreateInfo vertexInfo = {};
			vertexInfo.code_size = vertexCode.size();
			vertexInfo.code = vertexCode.data();
			vertexInfo.entrypoint = "main";
			vertexInfo.format = shaderFormat;
			vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
			vertexInfo.num_samplers = 0;
			vertexInfo.num_storage_textures = 0;
			vertexInfo.num_storage_buffers = 0;
			vertexInfo.num_uniform_buffers = 0;

			SDL_GPUShaderCreateInfo fragmentInfo = {};
			fragmentInfo.code_size = fragmentCode.size();
			fragmentInfo.code = fragmentCode.data();
			fragmentInfo.entrypoint = "main";
			fragmentInfo.format = shaderFormat;
			fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
			fragmentInfo.num_samplers = 1;
			fragmentInfo.num_storage_textures = 0;
			fragmentInfo.num_storage_buffers = 0;
			fragmentInfo.num_uniform_buffers = 0;

			renderer.defaultVertexShader = SDL_CreateGPUShader(renderer.gpuDevice, &vertexInfo);
			renderer.defaultFragmentShader = SDL_CreateGPUShader(renderer.gpuDevice, &fragmentInfo);

			if (!renderer.defaultVertexShader || !renderer.defaultFragmentShader)
			{
				errorFunc("Failed to create SDL GPU shaders for gl2d", userDefinedData);
				if (renderer.defaultVertexShader)
				{
					SDL_ReleaseGPUShader(renderer.gpuDevice, renderer.defaultVertexShader);
					renderer.defaultVertexShader = nullptr;
				}
				if (renderer.defaultFragmentShader)
				{
					SDL_ReleaseGPUShader(renderer.gpuDevice, renderer.defaultFragmentShader);
					renderer.defaultFragmentShader = nullptr;
				}
				return false;
			}

			renderer.shaderFormat = shaderFormat;
			return true;
		}

		SDL_GPUGraphicsPipeline *createPipelineForFormat(Renderer2D &renderer,
			SDL_GPUTextureFormat targetFormat,
			bool additiveBlend)
		{
			if (!renderer.gpuDevice || !renderer.defaultVertexShader || !renderer.defaultFragmentShader)
			{
				return nullptr;
			}

			SDL_GPUVertexBufferDescription vertexBufferDesc[1] = {};
			vertexBufferDesc[0].slot = 0;
			vertexBufferDesc[0].pitch = sizeof(BatchVertex);
			vertexBufferDesc[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
			vertexBufferDesc[0].instance_step_rate = 0;

			SDL_GPUVertexAttribute vertexAttributes[3] = {};
			vertexAttributes[0].location = 0;
			vertexAttributes[0].buffer_slot = 0;
			vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
			vertexAttributes[0].offset = offsetof(BatchVertex, pos);

			vertexAttributes[1].location = 1;
			vertexAttributes[1].buffer_slot = 0;
			vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
			vertexAttributes[1].offset = offsetof(BatchVertex, uv);

			vertexAttributes[2].location = 2;
			vertexAttributes[2].buffer_slot = 0;
			vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
			vertexAttributes[2].offset = offsetof(BatchVertex, color);

			SDL_GPUVertexInputState vertexInput = {};
			vertexInput.vertex_buffer_descriptions = vertexBufferDesc;
			vertexInput.num_vertex_buffers = 1;
			vertexInput.vertex_attributes = vertexAttributes;
			vertexInput.num_vertex_attributes = 3;

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
			blend.enable_blend = true;
			if (additiveBlend)
			{
				blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
				// Preserve destination alpha while adding color.
				blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
				blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
			}
			else
			{
				blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
				blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
			}
			blend.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
				SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

			SDL_GPUColorTargetDescription colorTargetDesc[1] = {};
			colorTargetDesc[0].format = targetFormat;
			colorTargetDesc[0].blend_state = blend;

			SDL_GPUGraphicsPipelineTargetInfo targetInfo = {};
			targetInfo.num_color_targets = 1;
			targetInfo.color_target_descriptions = colorTargetDesc;
			targetInfo.has_depth_stencil_target = false;

			SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
			pipelineInfo.vertex_shader = renderer.defaultVertexShader;
			pipelineInfo.fragment_shader = renderer.defaultFragmentShader;
			pipelineInfo.vertex_input_state = vertexInput;
			pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
			pipelineInfo.rasterizer_state = rasterizer;
			pipelineInfo.multisample_state = multisample;
			pipelineInfo.depth_stencil_state = depthStencil;
			pipelineInfo.target_info = targetInfo;

			return SDL_CreateGPUGraphicsPipeline(renderer.gpuDevice, &pipelineInfo);
		}

		SDL_GPUGraphicsPipeline *ensurePipelineForTarget(Renderer2D &renderer,
			SDL_GPUTextureFormat targetFormat,
			bool swapchainTarget,
			bool additiveBlend)
		{
			if (!ensureDefaultShaders(renderer)) { return nullptr; }

			if (swapchainTarget)
			{
				if (additiveBlend)
				{
					if (renderer.pipelineSwapchainAdditive && renderer.pipelineSwapchainAdditiveFormat == targetFormat)
					{
						return renderer.pipelineSwapchainAdditive;
					}

					if (renderer.pipelineSwapchainAdditive)
					{
						SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineSwapchainAdditive);
						renderer.pipelineSwapchainAdditive = nullptr;
					}

					renderer.pipelineSwapchainAdditive = createPipelineForFormat(renderer, targetFormat, true);
					renderer.pipelineSwapchainAdditiveFormat = targetFormat;
					return renderer.pipelineSwapchainAdditive;
				}

				if (renderer.pipelineSwapchain && renderer.pipelineSwapchainFormat == targetFormat)
				{
					return renderer.pipelineSwapchain;
				}

				if (renderer.pipelineSwapchain)
				{
					SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineSwapchain);
					renderer.pipelineSwapchain = nullptr;
				}

				renderer.pipelineSwapchain = createPipelineForFormat(renderer, targetFormat, false);
				renderer.pipelineSwapchainFormat = targetFormat;
				return renderer.pipelineSwapchain;
			}

			if (additiveBlend)
			{
				if (renderer.pipelineOffscreenAdditive && renderer.pipelineOffscreenAdditiveFormat == targetFormat)
				{
					return renderer.pipelineOffscreenAdditive;
				}

				if (renderer.pipelineOffscreenAdditive)
				{
					SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineOffscreenAdditive);
					renderer.pipelineOffscreenAdditive = nullptr;
				}

				renderer.pipelineOffscreenAdditive = createPipelineForFormat(renderer, targetFormat, true);
				renderer.pipelineOffscreenAdditiveFormat = targetFormat;
				return renderer.pipelineOffscreenAdditive;
			}

			if (renderer.pipelineOffscreen && renderer.pipelineOffscreenFormat == targetFormat)
			{
				return renderer.pipelineOffscreen;
			}

			if (renderer.pipelineOffscreen)
			{
				SDL_ReleaseGPUGraphicsPipeline(renderer.gpuDevice, renderer.pipelineOffscreen);
				renderer.pipelineOffscreen = nullptr;
			}

			renderer.pipelineOffscreen = createPipelineForFormat(renderer, targetFormat, false);
			renderer.pipelineOffscreenFormat = targetFormat;
			return renderer.pipelineOffscreen;
		}

		bool ensureVertexBufferCapacity(Renderer2D &renderer, uint32_t requiredBytes)
		{
			if (!renderer.gpuDevice) { return false; }
			if (requiredBytes == 0) { return true; }

			if (renderer.vertexBuffer && renderer.vertexTransferBuffer && renderer.vertexBufferSize >= requiredBytes)
			{
				return true;
			}

			if (renderer.vertexBuffer)
			{
				SDL_ReleaseGPUBuffer(renderer.gpuDevice, renderer.vertexBuffer);
				renderer.vertexBuffer = nullptr;
			}
			if (renderer.vertexTransferBuffer)
			{
				SDL_ReleaseGPUTransferBuffer(renderer.gpuDevice, renderer.vertexTransferBuffer);
				renderer.vertexTransferBuffer = nullptr;
			}

			renderer.vertexBufferSize = std::max(requiredBytes, static_cast<uint32_t>(64 * 1024));

			SDL_GPUBufferCreateInfo bufferInfo = {};
			bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
			bufferInfo.size = renderer.vertexBufferSize;
			renderer.vertexBuffer = SDL_CreateGPUBuffer(renderer.gpuDevice, &bufferInfo);
			if (!renderer.vertexBuffer)
			{
				errorFunc("Failed to create SDL GPU vertex buffer", userDefinedData);
				return false;
			}

			SDL_GPUTransferBufferCreateInfo transferInfo = {};
			transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			transferInfo.size = renderer.vertexBufferSize;
			renderer.vertexTransferBuffer = SDL_CreateGPUTransferBuffer(renderer.gpuDevice, &transferInfo);
			if (!renderer.vertexTransferBuffer)
			{
				errorFunc("Failed to create SDL GPU transfer buffer", userDefinedData);
				SDL_ReleaseGPUBuffer(renderer.gpuDevice, renderer.vertexBuffer);
				renderer.vertexBuffer = nullptr;
				return false;
			}

			return true;
		}

		bool tryInitGpuBackend(Renderer2D &renderer)
		{
			if (!renderer.sdlRenderer) { return false; }
			renderer.gpuDevice = nullptr;

			SDL_PropertiesID rendererProps = SDL_GetRendererProperties(renderer.sdlRenderer);
			if (rendererProps)
			{
				SDL_GPUDevice *rendererDevice = static_cast<SDL_GPUDevice *>(
					SDL_GetPointerProperty(rendererProps, SDL_PROP_RENDERER_GPU_DEVICE_POINTER, nullptr));
				if (supportsSpirvShaders(rendererDevice))
				{
					renderer.gpuDevice = rendererDevice;
				}
			}

			renderer.ownsGpuDevice = false;
			if (!renderer.gpuDevice && supportsSpirvShaders(globalGpuDevice))
			{
				renderer.gpuDevice = globalGpuDevice;
			}
			if (!renderer.gpuDevice)
			{
				// Backend uses SPIR-V shader binaries and lets SDL pick the platform driver.
				renderer.gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
				renderer.ownsGpuDevice = renderer.gpuDevice != nullptr;
			}

			if (!renderer.gpuDevice)
			{
				#ifdef __EMSCRIPTEN__
				errorFunc("Failed to initialize SDL GPU device with SPIR-V support. This SDL version does not ship a web SDL_gpu backend, so browser builds fall back to SDL_Renderer.", userDefinedData);
				#else
				errorFunc("Failed to initialize SDL GPU device with SPIR-V support, falling back to SDL_Renderer path", userDefinedData);
				#endif
				return false;
			}

			renderer.gpuWindow = SDL_GetRenderWindow(renderer.sdlRenderer);
			if (!renderer.gpuWindow)
			{
				errorFunc("Failed to get SDL window from renderer for SDL GPU", userDefinedData);
				if (renderer.ownsGpuDevice)
				{
					SDL_DestroyGPUDevice(renderer.gpuDevice);
				}
				renderer.gpuDevice = nullptr;
				renderer.ownsGpuDevice = false;
				return false;
			}

			if (SDL_ClaimWindowForGPUDevice(renderer.gpuDevice, renderer.gpuWindow))
			{
				renderer.claimedWindow = true;
				SDL_SetGPUSwapchainParameters(renderer.gpuDevice, renderer.gpuWindow,
					SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
			}
			else
			{
				// If the device came from SDL_Renderer the window might already be claimed.
				renderer.claimedWindow = false;
				if (renderer.ownsGpuDevice)
				{
					errorFunc("Failed to claim window for Vulkan SDL GPU device", userDefinedData);
					SDL_DestroyGPUDevice(renderer.gpuDevice);
					renderer.gpuDevice = nullptr;
					renderer.ownsGpuDevice = false;
					return false;
				}
			}

			globalGpuDevice = renderer.gpuDevice;
			return true;
		}
	}

	namespace internal
	{
		float positionToScreenCoordsX(const float position, float w)
		{
			return (position / w) * 2 - 1;
		}

		float positionToScreenCoordsY(const float position, float h)
		{
			return -((-position / h) * 2 - 1);
		}

		stbtt_aligned_quad fontGetGlyphQuad(const Font font, const char c)
		{
			stbtt_aligned_quad quad = {0};

			float x = 0;
			float y = 0;

			stbtt_GetPackedQuad(font.packedCharsBuffer,
				font.size.x, font.size.y, c - ' ', &x, &y, &quad, 1);


			return quad;
		}

		glm::vec4 fontGetGlyphTextureCoords(const Font font, const char c)
		{
			float xoffset = 0;
			float yoffset = 0;

			const stbtt_aligned_quad quad = fontGetGlyphQuad(font, c);

			return glm::vec4{quad.s0, quad.t0, quad.s1, quad.t1};
		}

		

	}

#ifdef _WIN32
	typedef BOOL(WINAPI *PFNWGLSWAPINTERVALEXTPROC) (int interval);
#else
	typedef bool(*PFNWGLSWAPINTERVALEXTPROC) (int interval);
#endif

	struct
	{
		PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
	}extensions = {};

	bool hasInitialized = 0;
	void init()
	{
		if (hasInitialized) { return; }
		hasInitialized = true;


		//int last = 0;
		//glGetIntegerv(GL_NUM_EXTENSIONS, &last);
		//for(int i=0; i<last; i++)
		//{
		//	const char *c = (const char*)glGetStringi(GL_EXTENSIONS, i);
		//	if(strcmp(c, "WGL_EXT_swap_control") == 0)
		//	{
		//		extensions.WGL_EXT_swap_control_ext = true;
		//		break;
		//	}
		//}

		//defaultShader = createShaderProgram(defaultVertexShader, defaultFragmentShader);

		white1pxSquareTexture.create1PxSquare();

	}

	void cleanup()
	{
		white1pxSquareTexture.cleanup();
		if (deferredOwnedGpuDevice)
		{
			SDL_DestroyGPUDevice(deferredOwnedGpuDevice);
			deferredOwnedGpuDevice = nullptr;
		}
		globalGpuDevice = nullptr;
		hasInitialized = false;
	}

	bool setVsync(bool b)
	{
		//add linux suport
		if (extensions.wglSwapIntervalEXT != nullptr)
		{
			bool rezult = extensions.wglSwapIntervalEXT(b);
			return rezult;
		}
		else
		{
			return false;
		}
	}

	glm::vec2 rotateAroundPoint(glm::vec2 vec, glm::vec2 point, const float degrees)
	{
		point.y = -point.y;
		float a = glm::radians(degrees);
		float s = sinf(a);
		float c = cosf(a);
		vec.x -= point.x;
		vec.y -= point.y;
		float newx = vec.x * c - vec.y * s;
		float newy = vec.x * s + vec.y * c;
		// translate point back:
		vec.x = newx + point.x;
		vec.y = newy + point.y;
		return vec;
	}

	glm::vec2 scaleAroundPoint(glm::vec2 vec, glm::vec2 point, float scale)
	{
		vec = (vec - point) * scale + point;

		return vec;
	}


	///////////////////// Font /////////////////////
#pragma	region Font

	void Font::createFromTTF(const unsigned char *ttf_data, const size_t ttf_data_size,
		bool monospaced)
	{
		this->monospaced = monospaced;

		size.x = 2000;
		size.y = 2000;
		max_height = 0;
		packedCharsBufferSize = ('~' - ' ');

		// Initialize stbtt_fontinfo to get font metrics
		stbtt_fontinfo fontInfo;
		stbtt_InitFont(&fontInfo, ttf_data, stbtt_GetFontOffsetForIndex(ttf_data, 0));

		int ascent, descent, lineGap;
		stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

		float scale = stbtt_ScaleForPixelHeight(&fontInfo, 64); // Match font size used in PackFontRange
		max_height = (ascent - descent + lineGap) * scale;



		// STB TrueType will give us a one channel buffer of the font that we then convert to RGBA for OpenGL
		const size_t fontMonochromeBufferSize = size.x * size.y;
		const size_t fontRgbaBufferSize = size.x * size.y * 4;

		unsigned char *fontMonochromeBuffer = new unsigned char[fontMonochromeBufferSize];
		unsigned char *fontRgbaBuffer = new unsigned char[fontRgbaBufferSize];

		packedCharsBuffer = new stbtt_packedchar[packedCharsBufferSize]{};

		stbtt_pack_context stbtt_context;
		stbtt_PackBegin(&stbtt_context, fontMonochromeBuffer, size.x, size.y, 0, 2, NULL);
		stbtt_PackSetOversampling(&stbtt_context, 2, 2);
		stbtt_PackFontRange(&stbtt_context, ttf_data, 0, 64, ' ', '~' - ' ', packedCharsBuffer);
		stbtt_PackEnd(&stbtt_context);

		// Convert monochrome buffer to RGBA
		for (int i = 0; i < fontMonochromeBufferSize; i++)
		{
			fontRgbaBuffer[(i * 4)] = fontMonochromeBuffer[i];
			fontRgbaBuffer[(i * 4) + 1] = fontMonochromeBuffer[i];
			fontRgbaBuffer[(i * 4) + 2] = fontMonochromeBuffer[i];
			fontRgbaBuffer[(i * 4) + 3] = fontMonochromeBuffer[i] > 1 ? 255 : 0;
		}

		texture.createFromBuffer((char*)fontRgbaBuffer, size.x, size.y, false, false);

		// Init texture
		//glGenTextures(1, &texture.id);
		//glBindTexture(GL_TEXTURE_2D, texture.id);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, fontRgbaBuffer);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


		//if (monospaced)
		//{
		//	for (char c = ' '; c <= '~'; c++)
		//	{
		//		stbtt_packedchar &glyph = packedCharsBuffer[c - ' '];
		//		float charWidth = glyph.x1 - glyph.x0;
		//		if (charWidth > max_height)
		//			max_height = charWidth;
		//	}
		//}

		if (monospaced)
		{
			spaceSize = max_height;
		}
		else
		{
			spaceSize = packedCharsBuffer[' ' - ' '].xadvance;
		}


		delete[] fontMonochromeBuffer;
		delete[] fontRgbaBuffer;
	}

	void Font::createFromFile(const char *file, bool monospaced)
	{
		std::ifstream fileFont(file, std::ios::binary);

		if (!fileFont.is_open())
		{
			char c[300] = {0};
			strcat(c, "error openning: ");
			strcat(c + strlen(c), file);
			errorFunc(c, userDefinedData);
			return;
		}

		int fileSize = 0;
		fileFont.seekg(0, std::ios::end);
		fileSize = (int)fileFont.tellg();
		fileFont.seekg(0, std::ios::beg);
		unsigned char *fileData = new unsigned char[fileSize];
		fileFont.read((char *)fileData, fileSize);
		fileFont.close();

		createFromTTF(fileData, fileSize, monospaced);

		delete[] fileData;
	}

	void Font::cleanup()
	{
		texture.cleanup();
		*this = {};
	}


#pragma endregion

	///////////////////// Camera /////////////////////
#pragma region Camera


	glm::mat4x4 gl2d::Camera3D::getProjectionMatrix()
	{
		if (std::isinf(this->aspectRatio) || std::isnan(this->aspectRatio) || this->aspectRatio == 0)
		{
			return glm::mat4x4(1.f);
		}

		auto mat = glm::perspective(this->fovRadians, this->aspectRatio, this->closePlane,
			this->farPlane);

		return mat;
	}

	glm::mat4x4 gl2d::Camera3D::getViewMatrix()
	{

		glm::vec3 dir = glm::vec3(position) + glm::vec3(viewDirection);
		return  glm::lookAt(glm::vec3(position), dir, up);
	}

	glm::mat4x4 gl2d::Camera3D::getViewProjectionMatrix()
	{
		return getProjectionMatrix() * getViewMatrix();
	}

	void gl2d::Camera3D::rotateCamera(const glm::vec2 delta)
	{

		constexpr float PI = 3.1415926;

		yaw += delta.x;
		pitch += delta.y;

		if (yaw >= 2 * PI)
		{
			yaw -= 2 * PI;
		}
		else if (yaw < 0)
		{
			yaw = 2 * PI - yaw;
		}

		if (pitch > PI / 2.f - 0.01) { pitch = PI / 2.f - 0.01; }
		if (pitch < -PI / 2.f + 0.01) { pitch = -PI / 2.f + 0.01; }

		viewDirection = glm::vec3(0, 0, -1);

		viewDirection = glm::mat3(glm::rotate(yaw, up)) * viewDirection;

		glm::vec3 rotatePitchAxe = glm::cross(viewDirection, up);
		viewDirection = glm::mat3(glm::rotate(pitch, rotatePitchAxe)) * viewDirection;
	}

	void gl2d::Camera3D::moveFPS(glm::vec3 direction)
	{
		viewDirection = glm::normalize(viewDirection);

		//forward
		float forward = -direction.z;
		float leftRight = direction.x;
		float upDown = direction.y;

		glm::vec3 move = {};

		move += up * upDown;
		move += glm::normalize(glm::cross(viewDirection, up)) * leftRight;
		move += viewDirection * forward;

		this->position += move;
	}

	void gl2d::Camera3D::rotateFPS(glm::ivec2 mousePos, float speed)
	{
		glm::vec2 delta = lastMousePos - mousePos;
		delta *= speed;

		rotateCamera(delta);

		lastMousePos = mousePos;
	}



#pragma endregion

	///////////////////// Renderer2D /////////////////////
#pragma region Renderer2D

	//won't bind any fbo
	void internalFlush(gl2d::Renderer2D &renderer, bool clearDrawData)
	{
		if (!renderer.gpuDevice)
		{
			if (clearDrawData) { renderer.clearDrawData(); }
			return;
		}

		const bool hasBatchedQuads = !renderer.spritePositions.empty() && !renderer.spriteTextures.empty();
		const bool hasSwapchainCallback = renderer.gpuPassCallback != nullptr && renderer.boundFrameBuffer == nullptr;

		if (!hasBatchedQuads && !hasSwapchainCallback)
		{
			if (!renderer.boundFrameBuffer && renderer.pendingScreenClear)
			{
				clearGpuSwapchainTarget(renderer.gpuDevice, renderer.gpuWindow, renderer.pendingScreenClearColor);
				renderer.pendingScreenClear = false;
			}
			if (clearDrawData) { renderer.clearDrawData(); }
			return;
		}

		uint32_t quadCount = 0;
		uint32_t vertexCount = 0;
		uint32_t uploadBytes = 0;
		if (hasBatchedQuads)
		{
			quadCount = static_cast<uint32_t>(renderer.spriteTextures.size());
			if (renderer.spritePositions.size() != renderer.spriteColors.size() ||
				renderer.spritePositions.size() != renderer.texturePositions.size() ||
				renderer.spriteBlendModes.size() != quadCount ||
				renderer.spriteScissorRects.size() != quadCount ||
				renderer.spriteScissorEnabled.size() != quadCount ||
				renderer.spritePositions.size() != static_cast<size_t>(quadCount) * 6)
			{
				errorFunc("Corrupted gl2d SDL GPU batch data", userDefinedData);
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			if (!ensureDefaultSamplers(renderer))
			{
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			vertexCount = static_cast<uint32_t>(renderer.spritePositions.size());
			uploadBytes = vertexCount * static_cast<uint32_t>(sizeof(BatchVertex));
			if (!ensureVertexBufferCapacity(renderer, uploadBytes))
			{
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			BatchVertex *mappedVertices = static_cast<BatchVertex *>(
				SDL_MapGPUTransferBuffer(renderer.gpuDevice, renderer.vertexTransferBuffer, true));
			if (!mappedVertices)
			{
				errorFunc("Failed to map SDL GPU transfer buffer", userDefinedData);
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			for (uint32_t i = 0; i < vertexCount; ++i)
			{
				auto &dst = mappedVertices[i];
				const auto &pos = renderer.spritePositions[i];
				const auto &uv = renderer.texturePositions[i];
				const auto &col = renderer.spriteColors[i];

				dst.pos[0] = pos.x;
				dst.pos[1] = pos.y;
				dst.uv[0] = uv.x;
				dst.uv[1] = uv.y;
				dst.color[0] = col.r;
				dst.color[1] = col.g;
				dst.color[2] = col.b;
				dst.color[3] = col.a;
			}

			SDL_UnmapGPUTransferBuffer(renderer.gpuDevice, renderer.vertexTransferBuffer);
		}

		SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(renderer.gpuDevice);
		if (!commandBuffer)
		{
			errorFunc("Failed to acquire SDL GPU command buffer", userDefinedData);
			if (clearDrawData) { renderer.clearDrawData(); }
			return;
		}

		if (hasBatchedQuads)
		{
			SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
			if (!copyPass)
			{
				errorFunc("Failed to begin SDL GPU copy pass", userDefinedData);
				SDL_CancelGPUCommandBuffer(commandBuffer);
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			SDL_GPUTransferBufferLocation srcLocation = {};
			srcLocation.transfer_buffer = renderer.vertexTransferBuffer;
			srcLocation.offset = 0;

			SDL_GPUBufferRegion dstRegion = {};
			dstRegion.buffer = renderer.vertexBuffer;
			dstRegion.offset = 0;
			dstRegion.size = uploadBytes;

			SDL_UploadToGPUBuffer(copyPass, &srcLocation, &dstRegion, true);
			SDL_EndGPUCopyPass(copyPass);
		}

		SDL_GPUTexture *targetTexture = nullptr;
		Uint32 targetW = 0;
		Uint32 targetH = 0;
		SDL_GPUTextureFormat targetFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
		bool swapchainTarget = false;

		if (renderer.boundFrameBuffer && renderer.boundFrameBuffer->texture.gpuTexture)
		{
			targetTexture = renderer.boundFrameBuffer->texture.gpuTexture;
			targetW = std::max(renderer.boundFrameBuffer->w, 0);
			targetH = std::max(renderer.boundFrameBuffer->h, 0);
			targetFormat = renderer.boundFrameBuffer->texture.gpuFormat;
			if (targetFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
			{
				targetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			}
		}
		else
		{
			swapchainTarget = true;
			if (!renderer.gpuWindow)
			{
				errorFunc("SDL GPU window is not available", userDefinedData);
				SDL_CancelGPUCommandBuffer(commandBuffer);
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, renderer.gpuWindow,
				&targetTexture, &targetW, &targetH))
			{
				errorFunc("Failed to acquire SDL GPU swapchain texture", userDefinedData);
				SDL_CancelGPUCommandBuffer(commandBuffer);
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			if (!targetTexture)
			{
				// Window can be minimized; not an error.
				SDL_CancelGPUCommandBuffer(commandBuffer);
				if (clearDrawData) { renderer.clearDrawData(); }
				return;
			}

			targetFormat = SDL_GetGPUSwapchainTextureFormat(renderer.gpuDevice, renderer.gpuWindow);
		}

		if (targetW == 0 || targetH == 0)
		{
			SDL_CancelGPUCommandBuffer(commandBuffer);
			if (clearDrawData) { renderer.clearDrawData(); }
			return;
		}

		if (swapchainTarget && renderer.gpuPassCallback)
		{
			// Prepare uploads required by extra pass callback (e.g. ImGui buffers).
			renderer.gpuPassCallback(commandBuffer, nullptr, renderer.gpuPassCallbackUserData);
		}

		SDL_GPUColorTargetInfo colorTarget = {};
		colorTarget.texture = targetTexture;
		colorTarget.mip_level = 0;
		colorTarget.layer_or_depth_plane = 0;
		colorTarget.store_op = SDL_GPU_STOREOP_STORE;
		if (swapchainTarget && renderer.pendingScreenClear)
		{
			colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTarget.clear_color = SDL_FColor{
				renderer.pendingScreenClearColor.r,
				renderer.pendingScreenClearColor.g,
				renderer.pendingScreenClearColor.b,
				renderer.pendingScreenClearColor.a};
		}
		else
		{
			colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
		}

		SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
		if (!renderPass)
		{
			errorFunc("Failed to begin SDL GPU render pass", userDefinedData);
			SDL_CancelGPUCommandBuffer(commandBuffer);
			if (clearDrawData) { renderer.clearDrawData(); }
			return;
		}

		if (hasBatchedQuads)
		{
			SDL_GPUViewport viewport = {};
			viewport.x = 0;
			viewport.y = 0;
			viewport.w = static_cast<float>(targetW);
			viewport.h = static_cast<float>(targetH);
			viewport.min_depth = 0.f;
			viewport.max_depth = 1.f;
			SDL_SetGPUViewport(renderPass, &viewport);

			SDL_Rect fullTargetScissor = {};
			fullTargetScissor.x = 0;
			fullTargetScissor.y = 0;
			fullTargetScissor.w = static_cast<int>(targetW);
			fullTargetScissor.h = static_cast<int>(targetH);

			SDL_GPUBufferBinding vertexBinding = {};
			vertexBinding.buffer = renderer.vertexBuffer;
			vertexBinding.offset = 0;
			SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

			uint32_t firstVertex = 0;
			uint32_t i = 0;
			SDL_GPUGraphicsPipeline *boundPipeline = nullptr;
			while (i < quadCount)
			{
				Texture t = renderer.spriteTextures[i];
				SDL_GPUTexture *runTexture = t.gpuTexture ? t.gpuTexture : white1pxSquareTexture.gpuTexture;
				if (!runTexture)
				{
					i++;
					firstVertex += 6;
					continue;
				}

				const bool runNearest = t.pixelated;
				const Uint8 runBlendMode = renderer.spriteBlendModes[i];
				const bool runAdditive = runBlendMode == Renderer2D::BlendMode_Additive;
				const bool runScissorEnabled = renderer.spriteScissorEnabled[i] != 0;
				const SDL_Rect runScissorRect = runScissorEnabled
					? renderer.spriteScissorRects[i]
					: fullTargetScissor;
				uint32_t runQuads = 1;
				while (i + runQuads < quadCount)
				{
					Texture next = renderer.spriteTextures[i + runQuads];
					SDL_GPUTexture *nextTexture = next.gpuTexture ? next.gpuTexture : white1pxSquareTexture.gpuTexture;
					const Uint8 nextBlendMode = renderer.spriteBlendModes[i + runQuads];
					const bool nextScissorEnabled = renderer.spriteScissorEnabled[i + runQuads] != 0;
					if (nextTexture != runTexture || next.pixelated != runNearest ||
						nextScissorEnabled != runScissorEnabled ||
						nextBlendMode != runBlendMode)
					{
						break;
					}

					if (runScissorEnabled &&
						!rectEquals(renderer.spriteScissorRects[i + runQuads], runScissorRect))
					{
						break;
					}
					
					runQuads++;
				}

				SDL_GPUGraphicsPipeline *runPipeline = ensurePipelineForTarget(
					renderer,
					targetFormat,
					swapchainTarget,
					runAdditive);
				if (!runPipeline)
				{
					errorFunc("Failed to create SDL GPU graphics pipeline", userDefinedData);
					SDL_EndGPURenderPass(renderPass);
					SDL_CancelGPUCommandBuffer(commandBuffer);
					if (clearDrawData) { renderer.clearDrawData(); }
					return;
				}

				if (runPipeline != boundPipeline)
				{
					SDL_BindGPUGraphicsPipeline(renderPass, runPipeline);
					boundPipeline = runPipeline;
				}

				SDL_SetGPUScissor(renderPass, &runScissorRect);

				SDL_GPUTextureSamplerBinding textureBinding = {};
				textureBinding.texture = runTexture;
				textureBinding.sampler = runNearest ? renderer.samplerNearest : renderer.samplerLinear;
				SDL_BindGPUFragmentSamplers(renderPass, 0, &textureBinding, 1);

				SDL_DrawGPUPrimitives(renderPass, runQuads * 6, 1, firstVertex, 0);

				firstVertex += runQuads * 6;
				i += runQuads;
			}
		}

		if (swapchainTarget && renderer.gpuPassCallback)
		{
			renderer.gpuPassCallback(commandBuffer, renderPass, renderer.gpuPassCallbackUserData);
		}

		SDL_EndGPURenderPass(renderPass);

		if (!SDL_SubmitGPUCommandBuffer(commandBuffer))
		{
			errorFunc("Failed to submit SDL GPU command buffer", userDefinedData);
		}

		if (swapchainTarget)
		{
			renderer.pendingScreenClear = false;
		}

		if (clearDrawData)
		{
			renderer.clearDrawData();
		}
	}

	void gl2d::Renderer2D::flush(bool clearDrawData)
	{
		internalFlush(*this, clearDrawData);
	}

	void Renderer2D::reloadGpuShaders()
	{
		if (!gpuDevice)
		{
			return;
		}

		if (!spritePositions.empty() && !spriteTextures.empty())
		{
			flush(true);
		}

		if (pipelineSwapchain)
		{
			SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipelineSwapchain);
			pipelineSwapchain = nullptr;
		}

		if (pipelineOffscreen)
		{
			SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipelineOffscreen);
			pipelineOffscreen = nullptr;
		}

		if (pipelineSwapchainAdditive)
		{
			SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipelineSwapchainAdditive);
			pipelineSwapchainAdditive = nullptr;
		}

		if (pipelineOffscreenAdditive)
		{
			SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipelineOffscreenAdditive);
			pipelineOffscreenAdditive = nullptr;
		}

		pipelineSwapchainFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
		pipelineOffscreenFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
		pipelineSwapchainAdditiveFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
		pipelineOffscreenAdditiveFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

		if (defaultVertexShader)
		{
			SDL_ReleaseGPUShader(gpuDevice, defaultVertexShader);
			defaultVertexShader = nullptr;
		}

		if (defaultFragmentShader)
		{
			SDL_ReleaseGPUShader(gpuDevice, defaultFragmentShader);
			defaultFragmentShader = nullptr;
		}
	}

	void Renderer2D::flushFBO(FrameBuffer frameBuffer, bool clearDrawData)
	{
		if (!gpuDevice || !frameBuffer.texture.gpuTexture)
		{
			internalFlush(*this, clearDrawData);
			return;
		}

		FrameBuffer *oldTarget = boundFrameBuffer;
		boundFrameBuffer = &frameBuffer;
		internalFlush(*this, clearDrawData);
		boundFrameBuffer = oldTarget;
	}

	void Renderer2D::renderFrameBufferToTheEntireScreen(gl2d::FrameBuffer fbo, gl2d::FrameBuffer screen)
	{
		renderTextureToTheEntireScreen(fbo.texture, screen);
	}

	//doesn't bind or unbind stuff, except the vertex array,
	//doesn't set the viewport
	void renderQuadToScreenInternal(gl2d::Renderer2D &renderer)
	{
		static float positions[26] = {
		-1, 1,0,1,
		-1, -1,0,1,
		1, 1,0,1,

		1, 1,0,1,
		-1, -1,0,1,
		1, -1,0,1};

		//not used
		static float colors[6 * 4] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,};
		static float texCoords[12] = {
			0, 1,
			0, 0,
			1, 1,

			1, 1,
			0, 0,
			1, 0,
		};

	}

	void Renderer2D::renderTextureToTheEntireScreen(gl2d::Texture t, gl2d::FrameBuffer screen)
	{
		float drawW = static_cast<float>(windowW);
		float drawH = static_cast<float>(windowH);
		if (drawW <= 0 || drawH <= 0)
		{
			auto s = t.GetSize();
			drawW = static_cast<float>(std::max(s.x, 0));
			drawH = static_cast<float>(std::max(s.y, 0));
		}

		auto oldCamera = currentCamera;
		currentCamera = {};
		renderRectangle({0, 0, drawW, drawH}, t, Colors_White, {}, 0, {0, 0, 1, 1});
		currentCamera = oldCamera;

		flushFBO(screen, true);
	}

	void Renderer2D::flushPostProcess(const std::vector<ShaderProgram> &postProcesses,
		FrameBuffer frameBuffer, bool clearDrawData)
	{

		//todo SDL3

		//if (frameBuffer.fbo == 0)
		//{
		//	frameBuffer.fbo = defaultFBO;
		//}

		//if (postProcesses.empty())
		//{
		//	if (clearDrawData)
		//	{
		//		this->clearDrawData();
		//		return;
		//	}
		//}
		//
		//if (!postProcessFbo1.fbo) { postProcessFbo1.create(windowW, windowH); }
		//
		//postProcessFbo1.resize(windowW, windowH);
		//postProcessFbo1.clear();
		//
		//flushFBO(postProcessFbo1, clearDrawData);
		//
		//internalPostProcessFlip = 1;
		//postProcessOverATexture(postProcesses, postProcessFbo1.texture, frameBuffer);

	}

	void Renderer2D::postProcessOverATexture(const std::vector<ShaderProgram> &postProcesses,
		gl2d::Texture in,
		FrameBuffer frameBuffer)
	{
		//todo SDL3

		//if (postProcesses.empty())
		//{
		//	return;
		//}
		//
		//
		//if (!postProcessFbo1.fbo) { postProcessFbo1.create(0, 0); }
		//if (!postProcessFbo2.fbo && postProcesses.size() > 1)
		//{
		//	postProcessFbo2.create(0, 0);
		//}
		//
		//if (internalPostProcessFlip == 0)
		//{
		//	postProcessFbo1.resize(windowW, windowH);
		//	postProcessFbo1.clear();
		//	postProcessFbo2.resize(windowW, windowH);
		//	postProcessFbo2.clear();
		//}
		//else if (postProcessFbo2.fbo)
		//{
		//	//postProcessFbo1 has already been resized
		//	postProcessFbo2.resize(windowW, windowH);
		//	postProcessFbo2.clear();
		//}
		//
		//for (int i = 0; i < postProcesses.size(); i++)
		//{
		//	gl2d::FrameBuffer output;
		//	gl2d::Texture input;
		//
		//	if (internalPostProcessFlip == 0)
		//	{
		//		input = postProcessFbo2.texture;
		//		output = postProcessFbo1;
		//	}
		//	else
		//	{
		//		input = postProcessFbo1.texture;
		//		output = postProcessFbo2;
		//	}
		//
		//	if (i == 0)
		//	{
		//		input = in;
		//	}
		//
		//	if (i == postProcesses.size() - 1)
		//	{
		//		output = frameBuffer;
		//	}
		//	output.clear();
		//
		//	renderPostProcess(postProcesses[i], input, output);
		//	internalPostProcessFlip = !internalPostProcessFlip;
		//}
		//
		//
		//internalPostProcessFlip = 0;
	}


	///////////////////// Renderer2D - render ///////////////////// 

	void Renderer2D::renderRectangle(const Rect transforms, const Texture texture,
		const Color4f colors[4], const glm::vec2 origin, const float rotation, const glm::vec4 textureCoords, float positionZ)
	{
		glm::vec2 newOrigin;
		newOrigin.x = origin.x + transforms.x + (transforms.z / 2);
		newOrigin.y = origin.y + transforms.y + (transforms.w / 2);
		renderRectangleAbsRotation(transforms, texture, colors, newOrigin, rotation, textureCoords, positionZ);
	}

	static inline SDL_FPoint ToSDLPoint(glm::vec2 v)
	{
		return SDL_FPoint{v.x, -v.y};
	}

	static inline SDL_FColor ToSDLColor(const Color4f &c)
	{
		return SDL_FColor{c.r, c.g, c.b, c.a};
	}

	void RenderPreparedQuad(SDL_Renderer *sdlRenderer,
		glm::vec2 v1, glm::vec2 v2,
		glm::vec2 v3, glm::vec2 v4,
		const Color4f colors[4], gl2d::Texture texture, const glm::vec4 textureCoords)
	{
		SDL_Vertex verts[4];

		verts[0].position = ToSDLPoint(v1);
		verts[1].position = ToSDLPoint(v2);
		verts[2].position = ToSDLPoint(v3);
		verts[3].position = ToSDLPoint(v4);

		// per-corner colors (matches your OpenGL path)
		verts[0].color = ToSDLColor(colors[0]);
		verts[1].color = ToSDLColor(colors[1]);
		verts[2].color = ToSDLColor(colors[2]);
		verts[3].color = ToSDLColor(colors[3]);

		const int indices[6] = {0, 1, 2, 0, 2, 3};

		if (texture.isValid() && texture.tex != white1pxSquareTexture.tex)
		{
			const float u0 = textureCoords.x;
			const float v0 = textureCoords.y;
			const float u1 = textureCoords.z;
			const float v1 = textureCoords.w;

			// 1: (u0,v0) top-left
			// 2: (u0,v1) bottom-left
			// 3: (u1,v1) bottom-right
			// 4: (u1,v0) top-right
			verts[0].tex_coord = SDL_FPoint{u0, v0};
			verts[1].tex_coord = SDL_FPoint{u0, v1};
			verts[2].tex_coord = SDL_FPoint{u1, v1};
			verts[3].tex_coord = SDL_FPoint{u1, v0};

			SDL_RenderGeometry(sdlRenderer, texture.tex, verts, 4, indices, 6);

		}
		else
		{
			// texture ignored for now
			for (int i = 0; i < 4; ++i)
				verts[i].tex_coord = SDL_FPoint{0.f, 0.f};

			SDL_RenderGeometry(sdlRenderer, nullptr, verts, 4, indices, 6);
		}

	}

	void gl2d::Renderer2D::renderRectangleAbsRotation(const Rect transforms,
		const Texture texture, const Color4f colors[4], const glm::vec2 origin, const float rotation, const glm::vec4 textureCoords, float positionZ)
	{
		Texture textureCopy = texture;

		if (!textureCopy.isValid())
		{
			errorFunc("Invalid texture", userDefinedData);
			textureCopy = white1pxSquareTexture;
		}

		const float transformsY = transforms.y * -1;

		glm::vec2 v1 = {transforms.x,				  transformsY};
		glm::vec2 v2 = {transforms.x,				  transformsY - transforms.w};
		glm::vec2 v3 = {transforms.x + transforms.z, transformsY - transforms.w};
		glm::vec2 v4 = {transforms.x + transforms.z, transformsY};

		//Apply rotations
		if (rotation != 0)
		{
			v1 = rotateAroundPoint(v1, origin, rotation);
			v2 = rotateAroundPoint(v2, origin, rotation);
			v3 = rotateAroundPoint(v3, origin, rotation);
			v4 = rotateAroundPoint(v4, origin, rotation);
		}

		//Apply camera transformations
		v1.x -= currentCamera.position.x;
		v1.y += currentCamera.position.y;
		v2.x -= currentCamera.position.x;
		v2.y += currentCamera.position.y;
		v3.x -= currentCamera.position.x;
		v3.y += currentCamera.position.y;
		v4.x -= currentCamera.position.x;
		v4.y += currentCamera.position.y;

		//Apply camera rotation
		if (currentCamera.rotation != 0)
		{
			glm::vec2 cameraCenter;

			cameraCenter.x = windowW / 2.0f;
			cameraCenter.y = windowH / 2.0f;

			v1 = rotateAroundPoint(v1, cameraCenter, currentCamera.rotation);
			v2 = rotateAroundPoint(v2, cameraCenter, currentCamera.rotation);
			v3 = rotateAroundPoint(v3, cameraCenter, currentCamera.rotation);
			v4 = rotateAroundPoint(v4, cameraCenter, currentCamera.rotation);
		}

		//Apply camera zoom
		//if(renderer->currentCamera.zoom != 1)
		{

			glm::vec2 cameraCenter;
			cameraCenter.x = windowW / 2.0f;
			cameraCenter.y = -windowH / 2.0f;

			v1 = scaleAroundPoint(v1, cameraCenter, currentCamera.zoom);
			v2 = scaleAroundPoint(v2, cameraCenter, currentCamera.zoom);
			v3 = scaleAroundPoint(v3, cameraCenter, currentCamera.zoom);
			v4 = scaleAroundPoint(v4, cameraCenter, currentCamera.zoom);
		}

		if (!gpuDevice)
		{
			SDL_BlendMode blendMode = currentBlendMode == BlendMode_Additive
				? SDL_BLENDMODE_ADD
				: SDL_BLENDMODE_BLEND;
			SDL_SetRenderDrawBlendMode(sdlRenderer, blendMode);
			if (textureCopy.tex)
			{
				SDL_SetTextureBlendMode(textureCopy.tex, blendMode);
			}

			RenderPreparedQuad(sdlRenderer, v1, v2, v3, v4, colors, textureCopy, textureCoords);
			return;
		}

		if (windowW <= 0 || windowH <= 0)
		{
			return;
		}

		v1.x = internal::positionToScreenCoordsX(v1.x, (float)windowW);
		v2.x = internal::positionToScreenCoordsX(v2.x, (float)windowW);
		v3.x = internal::positionToScreenCoordsX(v3.x, (float)windowW);
		v4.x = internal::positionToScreenCoordsX(v4.x, (float)windowW);
		v1.y = internal::positionToScreenCoordsY(v1.y, (float)windowH);
		v2.y = internal::positionToScreenCoordsY(v2.y, (float)windowH);
		v3.y = internal::positionToScreenCoordsY(v3.y, (float)windowH);
		v4.y = internal::positionToScreenCoordsY(v4.y, (float)windowH);

		spritePositions.push_back(glm::vec4{v1.x, v1.y, positionZ, 1});
		spritePositions.push_back(glm::vec4{v2.x, v2.y, positionZ, 1});
		spritePositions.push_back(glm::vec4{v4.x, v4.y, positionZ, 1});

		spritePositions.push_back(glm::vec4{v2.x, v2.y, positionZ, 1});
		spritePositions.push_back(glm::vec4{v3.x, v3.y, positionZ, 1});
		spritePositions.push_back(glm::vec4{v4.x, v4.y, positionZ, 1});

		spriteColors.push_back(colors[0]);
		spriteColors.push_back(colors[1]);
		spriteColors.push_back(colors[3]);
		spriteColors.push_back(colors[1]);
		spriteColors.push_back(colors[2]);
		spriteColors.push_back(colors[3]);

		texturePositions.push_back(glm::vec2{textureCoords.x, textureCoords.y}); //1
		texturePositions.push_back(glm::vec2{textureCoords.x, textureCoords.w}); //2
		texturePositions.push_back(glm::vec2{textureCoords.z, textureCoords.y}); //4
		texturePositions.push_back(glm::vec2{textureCoords.x, textureCoords.w}); //2
		texturePositions.push_back(glm::vec2{textureCoords.z, textureCoords.w}); //3
		texturePositions.push_back(glm::vec2{textureCoords.z, textureCoords.y}); //4

		spriteTextures.push_back(textureCopy);
		spriteBlendModes.push_back(currentBlendMode);
		spriteScissorRects.push_back(gpuScissorRect);
		spriteScissorEnabled.push_back(gpuScissorEnabled ? 1 : 0);
	}

	void Renderer2D::renderRectangle(const Rect transforms, const Color4f colors[4], const glm::vec2 origin, const float rotation, float positionZ)
	{
		renderRectangle(transforms, white1pxSquareTexture, colors, origin, rotation, GL2D_DefaultTextureCoords, positionZ);
	}

	void Renderer2D::renderRectangleAbsRotation(const Rect transforms, const Color4f colors[4], const glm::vec2 origin, const float rotation, float positionZ)
	{
		renderRectangleAbsRotation(transforms, white1pxSquareTexture, colors, origin, rotation, GL2D_DefaultTextureCoords, positionZ);
	}

	void Renderer2D::renderLine(const glm::vec2 position, const float angleDegrees, const float length, const Color4f color, const float width, float positionZ)
	{
		renderRectangle({position - glm::vec2(0,width / 2.f), length, width},
			color, {-length / 2, 0}, angleDegrees, positionZ);
	}

	void Renderer2D::renderLine(const glm::vec2 start, const glm::vec2 end, const Color4f color, const float width, float positionZ)
	{
		glm::vec2 vector = end - start;
		float length = glm::length(vector);
		float angle = std::atan2(vector.y, vector.x);
		renderLine(start, -glm::degrees(angle), length, color, width, positionZ);
	}

	void Renderer2D::renderRectangleOutline(const glm::vec4 position, const Color4f color, const float width,
		const glm::vec2 origin, const float rotationDegrees, float positionZ)
	{

		glm::vec2 topLeft = position;
		glm::vec2 topRight = glm::vec2(position) + glm::vec2(position.z, 0);
		glm::vec2 bottomLeft = glm::vec2(position) + glm::vec2(0, position.w);
		glm::vec2 bottomRight = glm::vec2(position) + glm::vec2(position.z, position.w);

		glm::vec2 p1 = topLeft + glm::vec2(-width / 2.f, 0);
		glm::vec2 p2 = topRight + glm::vec2(+width / 2.f, 0);
		glm::vec2 p3 = topRight + glm::vec2(0, +width / 2.f);
		glm::vec2 p4 = bottomRight + glm::vec2(0, -width / 2.f);
		glm::vec2 p5 = bottomRight + glm::vec2(width / 2.f, 0);
		glm::vec2 p6 = bottomLeft + glm::vec2(-width / 2.f, 0);
		glm::vec2 p7 = bottomLeft + glm::vec2(0, -width / 2.f);
		glm::vec2 p8 = topLeft + glm::vec2(0, +width / 2.f);

		if (rotationDegrees != 0)
		{
			glm::vec2 o = origin + glm::vec2(position.x, -position.y) + glm::vec2(position.z, -position.w) / 2.f;

			p1 = rotateAroundPoint(p1, o, -rotationDegrees);
			p2 = rotateAroundPoint(p2, o, -rotationDegrees);
			p3 = rotateAroundPoint(p3, o, -rotationDegrees);
			p4 = rotateAroundPoint(p4, o, -rotationDegrees);
			p5 = rotateAroundPoint(p5, o, -rotationDegrees);
			p6 = rotateAroundPoint(p6, o, -rotationDegrees);
			p7 = rotateAroundPoint(p7, o, -rotationDegrees);
			p8 = rotateAroundPoint(p8, o, -rotationDegrees);
		}

		auto renderPoint = [&](glm::vec2 pos)
		{
			renderRectangle({pos - glm::vec2(width / 2.f),width,width}, Colors_Black, {}, 0, positionZ);
		};

		renderPoint(p1);
		renderPoint(p2);
		renderPoint(p3);
		renderPoint(p4);
		renderPoint(p5);
		renderPoint(p6);
		renderPoint(p7);
		renderPoint(p8);

		//add a padding so the lines align properly.
		renderLine(p1, p2, color, width, positionZ); //top line
		renderLine(p3, p4, color, width, positionZ);
		renderLine(p5, p6, color, width, positionZ); //bottom line
		renderLine(p7, p8, color, width, positionZ);

	}

	void  Renderer2D::renderCircleOutline(const glm::vec2 position,
		const float size, const Color4f color,
		const float width, const unsigned int segments, float positionZ)
	{

		auto calcPos = [&](int p)
		{
			glm::vec2 circle = {size,0};

			float a = 3.1415926 * 2 * ((float)p / segments);

			float c = std::cos(a);
			float s = std::sin(a);

			circle = {c * circle.x - s * circle.y, s * circle.x + c * circle.y};

			return circle + position;
		};


		glm::vec2 lastPos = calcPos(1);
		renderLine(calcPos(0), lastPos, color, width, positionZ);
		for (int i = 1; i < segments; i++)
		{

			glm::vec2 pos1 = lastPos;
			glm::vec2 pos2 = calcPos(i + 1);

			renderLine(pos1, pos2, color, width, positionZ);

			lastPos = pos2;
		}

	}



	void Renderer2D::render9Patch(const Rect position, const int borderSize,
		const Color4f color, const glm::vec2 origin, const float rotation, const Texture texture,
		const Texture_Coords textureCoords, const Texture_Coords inner_texture_coords, float positionZ)
	{
		glm::vec4 colorData[4] = {color, color, color, color};

		//inner
		Rect innerPos = position;
		innerPos.x += borderSize;
		innerPos.y += borderSize;
		innerPos.z -= borderSize * 2;
		innerPos.w -= borderSize * 2;
		renderRectangle(innerPos, texture, colorData, Position2D{0, 0}, 0, inner_texture_coords, positionZ);

		//top
		Rect topPos = position;
		topPos.x += borderSize;
		topPos.z -= (float)borderSize * 2;
		topPos.w = (float)borderSize;
		glm::vec4 upperTexPos;
		upperTexPos.x = inner_texture_coords.x;
		upperTexPos.y = textureCoords.y;
		upperTexPos.z = inner_texture_coords.z;
		upperTexPos.w = inner_texture_coords.y;
		renderRectangle(topPos, texture, colorData, Position2D{0, 0}, 0, upperTexPos, positionZ);

		//bottom
		Rect bottom = position;
		bottom.x += (float)borderSize;
		bottom.y += (float)position.w - borderSize;
		bottom.z -= (float)borderSize * 2;
		bottom.w = (float)borderSize;
		glm::vec4 bottomTexPos;
		bottomTexPos.x = inner_texture_coords.x;
		bottomTexPos.y = inner_texture_coords.w;
		bottomTexPos.z = inner_texture_coords.z;
		bottomTexPos.w = textureCoords.w;
		renderRectangle(bottom, texture, colorData, Position2D{0, 0}, 0, bottomTexPos, positionZ);

		//left
		Rect left = position;
		left.y += borderSize;
		left.z = (float)borderSize;
		left.w -= (float)borderSize * 2;
		glm::vec4 leftTexPos;
		leftTexPos.x = textureCoords.x;
		leftTexPos.y = inner_texture_coords.y;
		leftTexPos.z = inner_texture_coords.x;
		leftTexPos.w = inner_texture_coords.w;
		renderRectangle(left, texture, colorData, Position2D{0, 0}, 0, leftTexPos, positionZ);

		//right
		Rect right = position;
		right.x += position.z - borderSize;
		right.y += borderSize;
		right.z = (float)borderSize;
		right.w -= (float)borderSize * 2;
		glm::vec4 rightTexPos;
		rightTexPos.x = inner_texture_coords.z;
		rightTexPos.y = inner_texture_coords.y;
		rightTexPos.z = textureCoords.z;
		rightTexPos.w = inner_texture_coords.w;
		renderRectangle(right, texture, colorData, Position2D{0, 0}, 0, rightTexPos, positionZ);

		//topleft
		Rect topleft = position;
		topleft.z = (float)borderSize;
		topleft.w = (float)borderSize;
		glm::vec4 topleftTexPos;
		topleftTexPos.x = textureCoords.x;
		topleftTexPos.y = textureCoords.y;
		topleftTexPos.z = inner_texture_coords.x;
		topleftTexPos.w = inner_texture_coords.y;
		renderRectangle(topleft, texture, colorData, Position2D{0, 0}, 0, topleftTexPos, positionZ);

		//topright
		Rect topright = position;
		topright.x += position.z - borderSize;
		topright.z = (float)borderSize;
		topright.w = (float)borderSize;
		glm::vec4 toprightTexPos;
		toprightTexPos.x = inner_texture_coords.z;
		toprightTexPos.y = textureCoords.y;
		toprightTexPos.z = textureCoords.z;
		toprightTexPos.w = inner_texture_coords.y;
		renderRectangle(topright, texture, colorData, Position2D{0, 0}, 0, toprightTexPos, positionZ);

		//bottomleft
		Rect bottomleft = position;
		bottomleft.y += position.w - borderSize;
		bottomleft.z = (float)borderSize;
		bottomleft.w = (float)borderSize;
		glm::vec4 bottomleftTexPos;
		bottomleftTexPos.x = textureCoords.x;
		bottomleftTexPos.y = inner_texture_coords.w;
		bottomleftTexPos.z = inner_texture_coords.x;
		bottomleftTexPos.w = textureCoords.w;
		renderRectangle(bottomleft, texture, colorData, Position2D{0, 0}, 0, bottomleftTexPos, positionZ);

		//bottomright
		Rect bottomright = position;
		bottomright.y += position.w - borderSize;
		bottomright.x += position.z - borderSize;
		bottomright.z = (float)borderSize;
		bottomright.w = (float)borderSize;
		glm::vec4 bottomrightTexPos;
		bottomrightTexPos.x = inner_texture_coords.z;
		bottomrightTexPos.y = inner_texture_coords.w;
		bottomrightTexPos.z = textureCoords.z;
		bottomrightTexPos.w = textureCoords.w;
		renderRectangle(bottomright, texture, colorData, Position2D{0, 0}, 0, bottomrightTexPos, positionZ);

	}

	void Renderer2D::render9Patch2(const Rect position, const Color4f color, const glm::vec2 origin,
		const float rotation, const Texture texture, const Texture_Coords textureCoords,
		const Texture_Coords inner_texture_coords, float positionZ)
	{
		glm::vec4 colorData[4] = {color, color, color, color};


		auto textureSize = texture.GetSize();
		int w = textureSize.x;
		int h = textureSize.y;

		float textureSpaceW = textureCoords.z - textureCoords.x;
		float textureSpaceH = textureCoords.y - textureCoords.w;

		float topBorder = (textureCoords.y - inner_texture_coords.y) / textureSpaceH * position.w;
		float bottomBorder = (inner_texture_coords.w - textureCoords.w) / textureSpaceH * position.w;
		float leftBorder = (inner_texture_coords.x - textureCoords.x) / textureSpaceW * position.z;
		float rightBorder = (textureCoords.z - inner_texture_coords.z) / textureSpaceW * position.z;

		float newAspectRatio = position.z / position.w;

		if (newAspectRatio < 1.f)
		{
			topBorder *= newAspectRatio;
			bottomBorder *= newAspectRatio;
		}
		else
		{
			leftBorder /= newAspectRatio;
			rightBorder /= newAspectRatio;
		}



		//topBorder = 50;
		//bottomBorder = -50;
		//leftBorder = 0;
		//rightBorder = 0;


		//inner
		Rect innerPos = position;
		innerPos.x += leftBorder;
		innerPos.y += topBorder;
		innerPos.z -= leftBorder + rightBorder;
		innerPos.w -= topBorder + bottomBorder;
		renderRectangle(innerPos, texture, colorData, Position2D{0, 0}, 0, inner_texture_coords, positionZ);

		//top
		Rect topPos = position;
		topPos.x += leftBorder;
		topPos.z -= leftBorder + rightBorder;
		topPos.w = topBorder;
		glm::vec4 upperTexPos;
		upperTexPos.x = inner_texture_coords.x;
		upperTexPos.y = textureCoords.y;
		upperTexPos.z = inner_texture_coords.z;
		upperTexPos.w = inner_texture_coords.y;
		renderRectangle(topPos, texture, colorData, Position2D{0, 0}, 0, upperTexPos, positionZ);

		//Rect topPos = position;
		//topPos.x += leftBorder;
		//topPos.w = topBorder;
		//topPos.z = topBorder;
		//float end = rightBorder;
		//float size = topBorder;
		//
		//while(1)
		//{
		//	if(topPos.x + size <= end)
		//	{
		//
		//		//draw
		//		renderRectangle(topPos, colorData, Position2D{ 0, 0 }, 0, texture, upperTexPos);
		//
		//		topPos += size;
		//	}else
		//	{
		//		float newW = end - topPos.x;
		//		if(newW>0)
		//		{
		//			topPos.z = newW;
		//			renderRectangle(topPos, colorData, Position2D{ 0, 0 }, 0, texture, upperTexPos);
		//		}
		//		break;
		//	}
		//
		//}


		//bottom
		Rect bottom = position;
		bottom.x += leftBorder;
		bottom.y += (float)position.w - bottomBorder;
		bottom.z -= leftBorder + rightBorder;
		bottom.w = bottomBorder;
		glm::vec4 bottomTexPos;
		bottomTexPos.x = inner_texture_coords.x;
		bottomTexPos.y = inner_texture_coords.w;
		bottomTexPos.z = inner_texture_coords.z;
		bottomTexPos.w = textureCoords.w;
		renderRectangle(bottom, texture, colorData, Position2D{0, 0}, 0, bottomTexPos, positionZ);

		//left
		Rect left = position;
		left.y += topBorder;
		left.z = leftBorder;
		left.w -= topBorder + bottomBorder;
		glm::vec4 leftTexPos;
		leftTexPos.x = textureCoords.x;
		leftTexPos.y = inner_texture_coords.y;
		leftTexPos.z = inner_texture_coords.x;
		leftTexPos.w = inner_texture_coords.w;
		renderRectangle(left, texture, colorData, Position2D{0, 0}, 0, leftTexPos, positionZ);

		//right
		Rect right = position;
		right.x += position.z - rightBorder;
		right.y += topBorder;
		right.z = rightBorder;
		right.w -= topBorder + bottomBorder;
		glm::vec4 rightTexPos;
		rightTexPos.x = inner_texture_coords.z;
		rightTexPos.y = inner_texture_coords.y;
		rightTexPos.z = textureCoords.z;
		rightTexPos.w = inner_texture_coords.w;
		renderRectangle(right, texture, colorData, Position2D{0, 0}, 0, rightTexPos, positionZ);

		//topleft
		Rect topleft = position;
		topleft.z = leftBorder;
		topleft.w = topBorder;
		glm::vec4 topleftTexPos;
		topleftTexPos.x = textureCoords.x;
		topleftTexPos.y = textureCoords.y;
		topleftTexPos.z = inner_texture_coords.x;
		topleftTexPos.w = inner_texture_coords.y;
		renderRectangle(topleft, texture, colorData, Position2D{0, 0}, 0, topleftTexPos, positionZ);
		//repair here?


		//topright
		Rect topright = position;
		topright.x += position.z - rightBorder;
		topright.z = rightBorder;
		topright.w = topBorder;
		glm::vec4 toprightTexPos;
		toprightTexPos.x = inner_texture_coords.z;
		toprightTexPos.y = textureCoords.y;
		toprightTexPos.z = textureCoords.z;
		toprightTexPos.w = inner_texture_coords.y;
		renderRectangle(topright, texture, colorData, Position2D{0, 0}, 0, toprightTexPos, positionZ);

		//bottomleft
		Rect bottomleft = position;
		bottomleft.y += position.w - bottomBorder;
		bottomleft.z = leftBorder;
		bottomleft.w = bottomBorder;
		glm::vec4 bottomleftTexPos;
		bottomleftTexPos.x = textureCoords.x;
		bottomleftTexPos.y = inner_texture_coords.w;
		bottomleftTexPos.z = inner_texture_coords.x;
		bottomleftTexPos.w = textureCoords.w;
		renderRectangle(bottomleft, texture, colorData, Position2D{0, 0}, 0, bottomleftTexPos, positionZ);

		//bottomright
		Rect bottomright = position;
		bottomright.y += position.w - bottomBorder;
		bottomright.x += position.z - rightBorder;
		bottomright.z = rightBorder;
		bottomright.w = bottomBorder;
		glm::vec4 bottomrightTexPos;
		bottomrightTexPos.x = inner_texture_coords.z;
		bottomrightTexPos.y = inner_texture_coords.w;
		bottomrightTexPos.z = textureCoords.z;
		bottomrightTexPos.w = textureCoords.w;
		renderRectangle(bottomright, texture, colorData, Position2D{0, 0}, 0, bottomrightTexPos, positionZ);

	}

	void Renderer2D::create(SDL_Renderer *sdlRenderer, size_t quadCount)
	{
		if (!hasInitialized)
		{
			errorFunc("Library not initialized. Have you forgotten to call gl2d::init() ?", userDefinedData);
		}

		this->sdlRenderer = sdlRenderer;
		activeRendererInstance = this;

		clearDrawData();

		spritePositions.reserve(quadCount * 6);
		spriteColors.reserve(quadCount * 6);
		texturePositions.reserve(quadCount * 6);
		spriteTextures.reserve(quadCount);
		spriteBlendModes.reserve(quadCount);
		spriteScissorRects.reserve(quadCount);
		spriteScissorEnabled.reserve(quadCount);

		gpuScissorEnabled = false;
		gpuScissorRect = {};
		currentBlendMode = BlendMode_Alpha;
		pendingScreenClear = false;
		pendingScreenClearColor = {};
		boundFrameBuffer = nullptr;
		gpuPassCallback = nullptr;
		gpuPassCallbackUserData = nullptr;

		if (tryInitGpuBackend(*this))
		{
			runDevelopmentShaderCompileScriptOnce();

			// Ensure the default texture also has a GPU copy once a device exists.
			if (!white1pxSquareTexture.gpuTexture)
			{
				white1pxSquareTexture.create1PxSquare();
			}
		}

		this->resetCameraAndShader();


	}

	void Renderer2D::cleanup()
	{
		if (gpuDevice)
		{
			if (!spritePositions.empty() && !spriteTextures.empty())
			{
				flush(true);
			}

			releaseRendererGpuResources(*this);

			if (claimedWindow && gpuWindow)
			{
				SDL_ReleaseWindowFromGPUDevice(gpuDevice, gpuWindow);
			}

			if (ownsGpuDevice)
			{
				deferredOwnedGpuDevice = gpuDevice;
			}
		}

		if (activeRendererInstance == this)
		{
			activeRendererInstance = nullptr;
		}

		if (globalGpuDevice == gpuDevice && !ownsGpuDevice)
		{
			globalGpuDevice = nullptr;
		}

		gpuDevice = nullptr;
		gpuWindow = nullptr;
		ownsGpuDevice = false;
		claimedWindow = false;
		shaderFormat = SDL_GPU_SHADERFORMAT_INVALID;
		boundFrameBuffer = nullptr;
		pendingScreenClear = false;
		gpuScissorEnabled = false;
		currentBlendMode = BlendMode_Alpha;
		gpuPassCallback = nullptr;
		gpuPassCallbackUserData = nullptr;

		clearDrawData();

		postProcessFbo1.cleanup();
		postProcessFbo2.cleanup();
		internalPostProcessFlip = 0;
	}

	void Renderer2D::pushShader(ShaderProgram s)
	{
		shaderPushPop.push_back(currentShader);
		currentShader = s;
	}

	void Renderer2D::popShader()
	{
		if (shaderPushPop.empty())
		{
			errorFunc("Pop on an empty stack on popShader", userDefinedData);
		}
		else
		{
			currentShader = shaderPushPop.back();
			shaderPushPop.pop_back();
		}
	}

	void Renderer2D::pushCamera(Camera c)
	{
		cameraPushPop.push_back(currentCamera);
		currentCamera = c;
	}

	void Renderer2D::popCamera()
	{
		if (cameraPushPop.empty())
		{
			errorFunc("Pop on an empty stack on popCamera", userDefinedData);
		}
		else
		{
			currentCamera = cameraPushPop.back();
			cameraPushPop.pop_back();
		}
	}

	glm::vec4 Renderer2D::getViewRect()
	{
		auto rect = glm::vec4{0, 0, windowW, windowH};

		glm::mat3 mat =
		{1.f, 0, currentCamera.position.x ,
		 0, 1.f, currentCamera.position.y,
		 0, 0, 1.f};
		mat = glm::transpose(mat);

		glm::vec3 pos1 = {rect.x, rect.y, 1.f};
		glm::vec3 pos2 = {rect.z + rect.x, rect.w + rect.y, 1.f};

		pos1 = mat * pos1;
		pos2 = mat * pos2;

		glm::vec2 point((pos1.x + pos2.x) / 2.f, (pos1.y + pos2.y) / 2.f);

		pos1 = glm::vec3(scaleAroundPoint(pos1, point, 1.f / currentCamera.zoom), 1.f);
		pos2 = glm::vec3(scaleAroundPoint(pos2, point, 1.f / currentCamera.zoom), 1.f);

		rect = {pos1.x, pos1.y, pos2.x - pos1.x, pos2.y - pos1.y};

		return rect;
	}

	glm::vec4 Renderer2D::toScreen(const glm::vec4 &transform)
	{
		//We need to flip texture_transforms.y
		const float transformsY = transform.y * -1;

		glm::vec2 v1 = {transform.x,				  transformsY};
		glm::vec2 v2 = {transform.x,				  transformsY - transform.w};
		glm::vec2 v3 = {transform.x + transform.z, transformsY - transform.w};
		glm::vec2 v4 = {transform.x + transform.z, transformsY};

		//Apply camera transformations
		v1.x -= currentCamera.position.x;
		v1.y += currentCamera.position.y;
		v2.x -= currentCamera.position.x;
		v2.y += currentCamera.position.y;
		v3.x -= currentCamera.position.x;
		v3.y += currentCamera.position.y;
		v4.x -= currentCamera.position.x;
		v4.y += currentCamera.position.y;

		//Apply camera zoom
		//if(renderer->currentCamera.zoom != 1)
		{

			glm::vec2 cameraCenter;
			cameraCenter.x = windowW / 2.0f;
			cameraCenter.y = -windowH / 2.0f;

			v1 = scaleAroundPoint(v1, cameraCenter, currentCamera.zoom);
			v3 = scaleAroundPoint(v3, cameraCenter, currentCamera.zoom);
		}

		v1.x = internal::positionToScreenCoordsX(v1.x, (float)windowW);
		v3.x = internal::positionToScreenCoordsX(v3.x, (float)windowW);
		v1.y = internal::positionToScreenCoordsY(v1.y, (float)windowH);
		v3.y = internal::positionToScreenCoordsY(v3.y, (float)windowH);

		return glm::vec4(v1.x, v1.y, v3.x, v3.y);
	}

	void Renderer2D::schisor(const glm::vec4 &rect)
	{
		if (gpuDevice)
		{
			SDL_Rect clip = {};
			clip.x = (int)std::round(rect.x);
			clip.y = (int)std::round(rect.y);
			clip.w = (int)std::round(rect.z);
			clip.h = (int)std::round(rect.w);
			if (clip.w < 0)
			{
				clip.x += clip.w;
				clip.w = -clip.w;
			}
			if (clip.h < 0)
			{
				clip.y += clip.h;
				clip.h = -clip.h;
			}
			clip.w = std::max(0, clip.w);
			clip.h = std::max(0, clip.h);

			// The scissor state is snapped per quad while batching.

			gpuScissorEnabled = true;
			gpuScissorRect = clip;
			return;
		}

		if (!sdlRenderer) { return; }
		SDL_Rect clip = {};
		clip.x = (int)std::round(rect.x);
		clip.y = (int)std::round(rect.y);
		clip.w = (int)std::round(rect.z);
		clip.h = (int)std::round(rect.w);
		if (clip.w < 0)
		{
			clip.x += clip.w;
			clip.w = -clip.w;
		}
		if (clip.h < 0)
		{
			clip.y += clip.h;
			clip.h = -clip.h;
		}
		clip.w = std::max(0, clip.w);
		clip.h = std::max(0, clip.h);
		SDL_SetRenderClipRect(sdlRenderer, &clip);
	}

	void Renderer2D::stopSchisor()
	{
		if (gpuDevice)
		{
			gpuScissorEnabled = false;
			gpuScissorRect = {};
			return;
		}

		if (!sdlRenderer) { return; }
		SDL_SetRenderClipRect(sdlRenderer, nullptr);
	}

	glm::vec2 Renderer2D::getTextSize(const char *text, const Font font,
		const float sizePixels, const float spacing, const float line_space)
	{

		if (!font.texture.isValid())
		{
			errorFunc("Missing font", userDefinedData);
			return {};
		}

		float size = sizePixels / 64.f;

		glm::vec2 position = {};

		const int text_length = (int)strlen(text);
		Rect rectangle = {};
		rectangle.x = position.x;
		float linePositionY = position.y;

		if (text_length == 0) { return {}; }

		//This is the y position we render at because it advances when we encounter newlines
		float maxPos = 0;
		float maxPosY = 0;
		float bonusY = 0;
		int lineCount = 1;
		bool firstLine = true;
		float firstLineSize = 0;

		for (int i = 0; i < text_length; i++)
		{
			if (text[i] == '\n')
			{
				rectangle.x = position.x;
				linePositionY += (font.max_height + line_space) * size;
				bonusY += (font.max_height + line_space) * size;
				maxPosY = 0;
				lineCount++;
				firstLine = false;
			}
			else if (text[i] == '\t')
			{
				float x = font.max_height;
				rectangle.x += (x + spacing) * size * 3;
			}
			else if (text[i] == ' ')
			{
				rectangle.x += (font.spaceSize + spacing) * size;

				//float x = font.max_height;
				//rectangle.x += x * size + spacing * size;
			}
			else if (text[i] >= ' ' && text[i] <= '~')
			{
				const stbtt_aligned_quad quad = internal::fontGetGlyphQuad
				(font, text[i]);

				rectangle.z = quad.x1 - quad.x0;
				rectangle.w = quad.y1 - quad.y0;

				if (firstLine && rectangle.w > firstLineSize)
				{
					firstLineSize = rectangle.w;
				}

				rectangle.z *= size;
				rectangle.w *= size;

				rectangle.y = linePositionY + quad.y0 * size; //not needed

				if (font.monospaced)
				{
					rectangle.x += font.max_height + spacing * size;
				}
				else
				{
					rectangle.x += rectangle.z + spacing * size;
				}


				maxPosY = std::max(maxPosY, rectangle.y);
				maxPos = std::max(maxPos, rectangle.x);
			}
		}

		maxPos = std::max(maxPos, rectangle.x);
		maxPosY = std::max(maxPosY, rectangle.y);

		float paddX = maxPos;

		float paddY = maxPosY;

		paddY += font.max_height * size + bonusY;

		//paddY = ((lineCount-1) * font.max_height + (lineCount - 1) * line_space + firstLineSize) * size;
		paddY = ((lineCount)*font.max_height + (lineCount - 1) * line_space) * size;

		return glm::vec2{paddX, paddY};

	}

	float Renderer2D::determineTextRescaleFitSmaller(const std::string &str,
		gl2d::Font &f, glm::vec4 transform, float maxSize)
	{
		auto s = getTextSize(str.c_str(), f, maxSize);

		float ratioX = transform.z / s.x;
		float ratioY = transform.w / s.y;


		if (ratioX > 1 && ratioY > 1)
		{
			return maxSize;
		}
		else
		{
			if (ratioX < ratioY)
			{
				return maxSize * ratioX;
			}
			else
			{
				return maxSize * ratioY;
			}
		}
	}


	float Renderer2D::determineTextRescaleFitBigger(const std::string &str,
		gl2d::Font &f, glm::vec4 transform, float minSize)
	{
		auto s = getTextSize(str.c_str(), f, minSize);

		float ratioX = transform.z / s.x;
		float ratioY = transform.w / s.y;


		if (ratioX > 1 && ratioY > 1)
		{
			if (ratioX > ratioY)
			{
				return minSize * ratioY;
			}
			else
			{
				return minSize * ratioX;
			}
		}
		else
		{

		}

		return minSize;

	}

	float Renderer2D::determineTextRescaleFit(const std::string &str,
		gl2d::Font &f, glm::vec4 transform)
	{
		float ret = 1;

		auto s = getTextSize(str.c_str(), f, ret);

		float ratioX = transform.z / s.x;
		float ratioY = transform.w / s.y;


		if (ratioX > 1 && ratioY > 1)
		{
			if (ratioX > ratioY)
			{
				return ret * ratioY;
			}
			else
			{
				return ret * ratioX;
			}
		}
		else
		{
			if (ratioX < ratioY)
			{
				return ret * ratioX;
			}
			else
			{
				return ret * ratioY;
			}
		}

		return ret;
	}

	int  Renderer2D::wrap(const std::string &in, gl2d::Font &f,
		float baseSize, float maxDimension, std::string *outRez)
	{
		if (outRez)
		{
			*outRez = "";
			outRez->reserve(in.size() + 10);
		}

		std::string word = "";
		std::string currentLine = "";
		currentLine.reserve(in.size() + 10);

		bool wrap = 0;
		bool newLine = 1;
		int newLineCounter = 0;

		for (int i = 0; i < in.size(); i++)
		{
			word.push_back(in[i]);
			currentLine.push_back(in[i]);

			if (in[i] == ' ')
			{
				if (wrap)
				{
					if (outRez)
					{
						outRez->push_back('\n'); currentLine = "";
					}
					newLineCounter++;

				}

				if (outRez)
				{
					*outRez += word;
				}
				word = "";
				wrap = 0;
				newLine = false;
			}
			else if (in[i] == '\n')
			{
				if (wrap)
				{
					if (outRez)
					{
						outRez->push_back('\n');
					}
					newLineCounter++;
				}

				currentLine = "";

				if (outRez)
				{
					*outRez += word;
				}
				word = "";
				wrap = 0;
				newLine = true;
			}
			else
			{
				//let's check, only if needed
				if (!wrap && !newLine)
				{
					float size = baseSize;
					auto textSize = getTextSize(currentLine.c_str(), f, size);

					if (textSize.x >= maxDimension && !newLine)
					{
						//wrap last word
						wrap = 1;
					}
				};
			}

		}

		{
			if (wrap) { if (outRez)outRez->push_back('\n'); newLineCounter++; }

			if (outRez)
			{
				*outRez += word;
			}
		}

		return newLineCounter + 1;
	}

	void Renderer2D::renderText(glm::vec2 position, const char *text, const Font font,
		const Color4f color, const float sizePixels, const float spacing, const float line_space, bool showInCenter,
		const Color4f ShadowColor
		, const Color4f LightColor, float positionZ, float rotationDegrees
	)
	{

		float size = sizePixels / 64.f;

		if (!font.texture.isValid())
		{
			errorFunc("Missing font", userDefinedData);
			return;
		}

		const int text_length = (int)strlen(text);
		Rect rectangle;
		rectangle.x = position.x;
		float linePositionY = position.y;

		glm::vec2 textSize = {};
		if (showInCenter || std::abs(rotationDegrees) > 0.0001f)
		{
			textSize = this->getTextSize(text, font, sizePixels, spacing, line_space);

			if (showInCenter)
			{
				position.x -= textSize.x / 2.f;
				position.y -= textSize.y / 2.f;
				position.y += (font.max_height * size / 2.f) * 1.5f;
			}
		}

		glm::vec2 textCenter = position + textSize * 0.5f;

		rectangle = {};
		rectangle.x = position.x;

		//This is the y position we render at because it advances when we encounter newlines
		linePositionY = position.y;

		for (int i = 0; i < text_length; i++)
		{
			if (text[i] == '\n')
			{
				rectangle.x = position.x;
				linePositionY += (font.max_height + line_space) * size;
			}
			else if (text[i] == '\t')
			{
				float x = font.max_height;
				rectangle.x += (x + spacing) * size * 3;
			}
			else if (text[i] == ' ')
			{
				rectangle.x += (font.spaceSize + spacing) * size;

				//float x = font.max_height;
				//rectangle.x += x * size + spacing * size;
			}
			else if (text[i] >= ' ' && text[i] <= '~')
			{

				const stbtt_aligned_quad quad = internal::fontGetGlyphQuad
				(font, text[i]);

				rectangle.z = quad.x1 - quad.x0;
				rectangle.w = quad.y1 - quad.y0;

				rectangle.z *= size;
				rectangle.w *= size;

				//rectangle.y = linePositionY - rectangle.w;
				rectangle.y = linePositionY + quad.y0 * size;

				glm::vec4 colorData[4] = {color, color, color, color};

				glm::vec2 rectCenter = {rectangle.x + rectangle.z * 0.5f, rectangle.y + rectangle.w * 0.5f};
				glm::vec2 origin = {0, 0};
				if (std::abs(rotationDegrees) > 0.0001f)
				{
					origin = textCenter - rectCenter;
				}

				if (ShadowColor.w)
				{
					glm::vec2 pos = {-5, 3};
					pos *= size;
					glm::vec4 shadowRect = {rectangle.x + pos.x, rectangle.y + pos.y, rectangle.z, rectangle.w};
					glm::vec2 shadowCenter = {shadowRect.x + shadowRect.z * 0.5f, shadowRect.y + shadowRect.w * 0.5f};
					glm::vec2 shadowOrigin = origin;
					if (std::abs(rotationDegrees) > 0.0001f)
					{
						shadowOrigin = textCenter - shadowCenter;
					}
					renderRectangle(shadowRect,
						font.texture, ShadowColor, shadowOrigin, rotationDegrees,
						glm::vec4{quad.s0, quad.t0, quad.s1, quad.t1}, positionZ);

				}

				renderRectangle(rectangle, font.texture, colorData, origin, rotationDegrees,
					glm::vec4{quad.s0, quad.t0, quad.s1, quad.t1}, positionZ);

				if (LightColor.w)
				{
					glm::vec2 pos = {-2, 1};
					pos *= size;
					glm::vec4 lightRect = {rectangle.x + pos.x, rectangle.y + pos.y, rectangle.z, rectangle.w};
					glm::vec2 lightCenter = {lightRect.x + lightRect.z * 0.5f, lightRect.y + lightRect.w * 0.5f};
					glm::vec2 lightOrigin = origin;
					if (std::abs(rotationDegrees) > 0.0001f)
					{
						lightOrigin = textCenter - lightCenter;
					}
					renderRectangle(lightRect,
						font.texture,
						LightColor, lightOrigin, rotationDegrees,
						glm::vec4{quad.s0, quad.t0, quad.s1, quad.t1}, positionZ);

				}

				if (font.monospaced)
				{
					rectangle.x += font.max_height + spacing * size;
				}
				else
				{
					rectangle.x += rectangle.z + spacing * size;
				}

			}
		}
	}

	void Renderer2D::renderTextWrapped(const std::string &text,
		gl2d::Font f, glm::vec4 textPos, glm::vec4 color, float baseSize,
		float spacing, float lineSpacing,
		bool showInCenter, glm::vec4 shadowColor, glm::vec4 lightColor)
	{
		std::string newText;
		wrap(text, f, baseSize, textPos.z, &newText);
		renderText(textPos,
			newText.c_str(), f, color, baseSize, spacing, lineSpacing, showInCenter,
			shadowColor, lightColor);
	}

	glm::vec2 Renderer2D::getTextSizeWrapped(const std::string &text,
		gl2d::Font f, float maxTextLenght, float baseSize, float spacing, float lineSpacing)
	{
		std::string newText;
		wrap(text, f, baseSize, maxTextLenght, &newText);
		auto rez = getTextSize(
			newText.c_str(), f, baseSize, spacing, lineSpacing);

		return rez;
	}

	static inline Uint8 f2u8(float v)
	{
		v = std::clamp(v, 0.0f, 1.0f);
		return static_cast<Uint8>(v * 255.0f + 0.5f);
	}


	void Renderer2D::clearScreen(const Color4f color)
	{
		lastFrameBufferClearColor = color;

		if (gpuDevice)
		{
			if (boundFrameBuffer && boundFrameBuffer->texture.gpuTexture)
			{
				if (!spritePositions.empty() && !spriteTextures.empty())
				{
					flush(true);
				}
				clearGpuTextureTarget(gpuDevice, boundFrameBuffer->texture.gpuTexture, color);
				return;
			}

			if (!spritePositions.empty() && !spriteTextures.empty())
			{
				flush(true);
			}

			pendingScreenClear = true;
			pendingScreenClearColor = color;
			return;
		}

		SDL_SetRenderDrawColor(
			sdlRenderer, f2u8(color.r), f2u8(color.g), f2u8(color.b), f2u8(color.a)
		);
		SDL_RenderClear(sdlRenderer);
	}

	void Renderer2D::setShaderProgram(const ShaderProgram shader)
	{
		currentShader = shader;
	}

	void Renderer2D::setCamera(const Camera camera)
	{
		currentCamera = camera;
	}

	void Renderer2D::resetCameraAndShader()
	{
		currentCamera = defaultCamera;
	}

	void Renderer2D::renderPostProcess(ShaderProgram shader,
		Texture input, FrameBuffer result)
	{
		
	}

#pragma endregion

	glm::ivec2 Texture::GetSize() const
	{
		if (cachedSize.x > 0 && cachedSize.y > 0)
		{
			return cachedSize;
		}

		if (!tex) return {0, 0};

		float w = 0, h = 0;
		SDL_GetTextureSize(tex, &w, &h);
		return {w, h};
	}

	void Texture::createFromBuffer(const char *image_data, const int width, const int height
		, bool pixelated, bool useMipMaps)
	{

		if (!image_data || width <= 0 || height <= 0) { return; }
		cleanup();
		this->pixelated = pixelated;
		cachedSize = {width, height};

		if (globalGpuDevice)
		{
			SDL_GPUTextureCreateInfo textureInfo = {};
			textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
			textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			textureInfo.width = static_cast<uint32_t>(width);
			textureInfo.height = static_cast<uint32_t>(height);
			textureInfo.layer_count_or_depth = 1;
			textureInfo.num_levels = 1;
			textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

			gpuTexture = SDL_CreateGPUTexture(globalGpuDevice, &textureInfo);
			if (!gpuTexture)
			{
				errorFunc("Failed to create SDL GPU texture", userDefinedData);
				cachedSize = {};
				return;
			}

			gpuDevice = globalGpuDevice;
			gpuFormat = textureInfo.format;
			if (!uploadRGBA8Texture(globalGpuDevice, gpuTexture, image_data, width, height))
			{
				SDL_ReleaseGPUTexture(globalGpuDevice, gpuTexture);
				gpuTexture = nullptr;
				gpuDevice = nullptr;
				gpuFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
				cachedSize = {};
				return;
			}

			(void)useMipMaps;
			return;
		}

		tex = nullptr;

		// stb typically gives you 8-bit RGBA in memory.
		// In SDL, ABGR8888 is commonly the �matches RGBA bytes in memory� choice on little-endian.
		// If your channels look swapped, try SDL_PIXELFORMAT_RGBA8888 instead.
		tex = SDL_CreateTexture(platform::getSdlRenderer(), SDL_PIXELFORMAT_ABGR8888,
			SDL_TEXTUREACCESS_STATIC, width, height);
		if (!tex)
		{
			cachedSize = {};
			return;
		} // SDL_GetError()

		// Upload pixels
		const int pitch = width * 4;
		SDL_UpdateTexture(tex, nullptr, image_data, pitch);

		// Enable alpha blending (usually what you want for sprites/UI)
		SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

		// Filtering (zoom)
		SDL_SetTextureScaleMode(tex, pixelated ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

		// Mipmaps: not available via SDL_Renderer textures
		(void)useMipMaps; // (you'd need SDL_gpu or a backend-specific workaround)
	}

	void Texture::create1PxSquare(const char *b)
	{
		if (b == nullptr)
		{
			const unsigned char buff[] =
			{
				0xff,
				0xff,
				0xff,
				0xff
			};

			createFromBuffer((char *)buff, 1, 1);
		}
		else
		{
			createFromBuffer(b, 1, 1);
		}

	}

	void Texture::createFromFileData(const unsigned char *image_file_data, const size_t image_file_size
		, bool pixelated, bool useMipMaps)
	{
		stbi_set_flip_vertically_on_load(true);

		int width = 0;
		int height = 0;
		int channels = 0;

		const unsigned char *decodedImage = stbi_load_from_memory(image_file_data, (int)image_file_size, &width, &height, &channels, 4);

		createFromBuffer((const char *)decodedImage, width, height, pixelated, useMipMaps);

		STBI_FREE(decodedImage);
	}

	void Texture::createFromFileDataWithPixelPadding(const unsigned char *image_file_data, const size_t image_file_size, int blockSize,
		bool pixelated, bool useMipMaps)
	{
		stbi_set_flip_vertically_on_load(true);

		int width = 0;
		int height = 0;
		int channels = 0;

		const unsigned char *decodedImage = stbi_load_from_memory(image_file_data, (int)image_file_size, &width, &height, &channels, 4);

		int newW = width + ((width * 2) / blockSize);
		int newH = height + ((height * 2) / blockSize);

		auto getOld = [decodedImage, width](int x, int y, int c)->const unsigned char
		{
			return decodedImage[4 * (x + (y * width)) + c];
		};


		unsigned char *newData = new unsigned char[newW * newH * 4] {};

		auto getNew = [newData, newW](int x, int y, int c)
		{
			return &newData[4 * (x + (y * newW)) + c];
		};

		int newDataCursor = 0;
		int dataCursor = 0;

		//first copy data
		for (int y = 0; y < newH; y++)
		{
			int yNo = 0;
			if ((y == 0 || y == newH - 1
				|| ((y) % (blockSize + 2)) == 0 ||
				((y + 1) % (blockSize + 2)) == 0
				))
			{
				yNo = 1;
			}

			for (int x = 0; x < newW; x++)
			{
				if (
					yNo ||

					((
					x == 0 || x == newW - 1
					|| (x % (blockSize + 2)) == 0 ||
					((x + 1) % (blockSize + 2)) == 0
					)
					)

					)
				{
					newData[newDataCursor++] = 0;
					newData[newDataCursor++] = 0;
					newData[newDataCursor++] = 0;
					newData[newDataCursor++] = 0;
				}
				else
				{
					newData[newDataCursor++] = decodedImage[dataCursor++];
					newData[newDataCursor++] = decodedImage[dataCursor++];
					newData[newDataCursor++] = decodedImage[dataCursor++];
					newData[newDataCursor++] = decodedImage[dataCursor++];
				}

			}

		}

		//then add margins


		for (int x = 1; x < newW - 1; x++)
		{
			//copy on left
			if (x == 1 ||
				(x % (blockSize + 2)) == 1
				)
			{
				for (int y = 0; y < newH; y++)
				{
					*getNew(x - 1, y, 0) = *getNew(x, y, 0);
					*getNew(x - 1, y, 1) = *getNew(x, y, 1);
					*getNew(x - 1, y, 2) = *getNew(x, y, 2);
					*getNew(x - 1, y, 3) = *getNew(x, y, 3);
				}

			}
			else //copy on rigght
				if (x == newW - 2 ||
					(x % (blockSize + 2)) == blockSize
					)
				{
					for (int y = 0; y < newH; y++)
					{
						*getNew(x + 1, y, 0) = *getNew(x, y, 0);
						*getNew(x + 1, y, 1) = *getNew(x, y, 1);
						*getNew(x + 1, y, 2) = *getNew(x, y, 2);
						*getNew(x + 1, y, 3) = *getNew(x, y, 3);
					}
				}
		}

		for (int y = 1; y < newH - 1; y++)
		{
			if (y == 1 ||
				(y % (blockSize + 2)) == 1
				)
			{
				for (int x = 0; x < newW; x++)
				{
					*getNew(x, y - 1, 0) = *getNew(x, y, 0);
					*getNew(x, y - 1, 1) = *getNew(x, y, 1);
					*getNew(x, y - 1, 2) = *getNew(x, y, 2);
					*getNew(x, y - 1, 3) = *getNew(x, y, 3);
				}
			}
			else
				if (y == newH - 2 ||
					(y % (blockSize + 2)) == blockSize
					)
				{
					for (int x = 0; x < newW; x++)
					{
						*getNew(x, y + 1, 0) = *getNew(x, y, 0);
						*getNew(x, y + 1, 1) = *getNew(x, y, 1);
						*getNew(x, y + 1, 2) = *getNew(x, y, 2);
						*getNew(x, y + 1, 3) = *getNew(x, y, 3);
					}
				}

		}

		createFromBuffer((const char *)newData, newW, newH, pixelated, useMipMaps);

		STBI_FREE(decodedImage);
		delete[] newData;
	}

	void Texture::loadFromFile(const char *fileName, bool pixelated, bool useMipMaps)
	{
		std::ifstream file(fileName, std::ios::binary);

		if (!file.is_open())
		{
			char c[300] = {0};
			strcat(c, "error openning: ");
			strcat(c + strlen(c), fileName);
			errorFunc(c, userDefinedData);
			return;
		}

		int fileSize = 0;
		file.seekg(0, std::ios::end);
		fileSize = (int)file.tellg();
		file.seekg(0, std::ios::beg);
		unsigned char *fileData = new unsigned char[fileSize];
		file.read((char *)fileData, fileSize);
		file.close();

		createFromFileData(fileData, fileSize, pixelated, useMipMaps);

		delete[] fileData;

	}

	void Texture::loadFromFileWithPixelPadding(const char *fileName, int blockSize,
		bool pixelated, bool useMipMaps)
	{
		std::ifstream file(fileName, std::ios::binary);

		if (!file.is_open())
		{
			char c[300] = {0};
			strcat(c, "error openning: ");
			strcat(c + strlen(c), fileName);
			errorFunc(c, userDefinedData);
			return;
		}

		int fileSize = 0;
		file.seekg(0, std::ios::end);
		fileSize = (int)file.tellg();
		file.seekg(0, std::ios::beg);
		unsigned char *fileData = new unsigned char[fileSize];
		file.read((char *)fileData, fileSize);
		file.close();

		createFromFileDataWithPixelPadding(fileData, fileSize, blockSize, pixelated, useMipMaps);

		delete[] fileData;

	}

	//TODO SDL3, Probably not possible without SDL_GPU
	size_t Texture::getMemorySize(int mipLevel, glm::ivec2 *outSize)
	{
		assert(0);
		return 0;
	}

	void Texture::readTextureData(void *buffer, int mipLevel)
	{
		assert(0);
		//
	}

	std::vector<unsigned char> Texture::readTextureData(int mipLevel, glm::ivec2 *outSize)
	{
		assert(0);
		return {};
	}

	void Texture::bind(const unsigned int sample)
	{
		
	}

	void Texture::unbind()
	{
	}

	void Texture::cleanup()
	{
		if (gpuTexture && gpuDevice)
		{
			SDL_ReleaseGPUTexture(gpuDevice, gpuTexture);
		}
		if (tex) SDL_DestroyTexture(tex);
		*this = {};
	}

	//glm::mat3 Camera::getMatrix()
	//{
	//	glm::mat3 m;
	//	m = { zoom, 0, position.x ,
	//		 0, zoom, position.y,
	//		0, 0, 1,
	//	};
	//	m = glm::transpose(m);
	//	return m; //todo not tested, add rotation
	//}

	void Camera::follow(glm::vec2 pos, float speed, float min, float max, float w, float h)
	{
		pos.x -= w / 2.f;
		pos.y -= h / 2.f;

		glm::vec2 delta = pos - position;
		bool signX = delta.x >= 0;
		bool signY = delta.y >= 0;

		float len = glm::length(delta);

		delta = glm::normalize(delta);

		if (len < min * 2)
		{
			speed /= 4.f;
		}
		else if (len < min * 4)
		{
			speed /= 2.f;
		}

		if (len > min)
		{
			if (len > max)
			{
				len = max;
				position = pos - (max * delta);
				//osition += delta * speed;
			}
			else
			{
				position += delta * speed;


			}

			glm::vec2 delta2 = pos - position;
			bool signX2 = delta.x >= 0;
			bool signY2 = delta.y >= 0;
			if (signX2 != signX || signY2 != signY || glm::length(delta2) > len)
			{
				//position = pos;
			}
		}
	}

	glm::vec2 internal::convertPoint(const Camera &camera, const glm::vec2 &p, float windowW, float windowH)
	{
		glm::vec2 r = p;


		//Apply camera transformations
		r.x += camera.position.x;
		r.y += camera.position.y;

		{
			glm::vec2 cameraCenter = {camera.position.x + windowW / 2, -camera.position.y - windowH / 2};

			r = rotateAroundPoint(r,
				cameraCenter,
				camera.rotation);
		}

		{
			glm::vec2 cameraCenter = {camera.position.x + windowW / 2, camera.position.y + windowH / 2};

			r = scaleAroundPoint(r,
				cameraCenter,
				1.f / camera.zoom);
		}

		//if (this->rotation != 0)
		//{
		//	glm::vec2 cameraCenter;
		//
		//	cameraCenter.x = windowW / 2.0f;
		//	cameraCenter.y = windowH / 2.0f;
		//
		//	r = rotateAroundPoint(r, cameraCenter, this->rotation);
		//
		//}

		//{
		//	glm::vec2 cameraCenter;
		//	cameraCenter.x = windowW / 2.0f;
		//	cameraCenter.y = -windowH / 2.0f;
		//
		//	r = scaleAroundPoint(r, cameraCenter, this->zoom);
		//
		//}

		return r;
	}


	void FrameBuffer::create(int w, int h, bool nearestFilter)
	{
		cleanup();

		this->w = w;
		this->h = h;

		if (w <= 0 || h <= 0)
		{
			this->w = 0;
			this->h = 0;
			return;
		}

		if (globalGpuDevice)
		{
			texture.pixelated = nearestFilter;
			texture.cachedSize = {w, h};
			texture.gpuDevice = globalGpuDevice;
			texture.gpuFormat = gpuTextureFormat;

			SDL_GPUTextureFormat selectedFormat = gpuTextureFormat;
			if (selectedFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
			{
				selectedFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			}

			const SDL_GPUTextureUsageFlags usage =
				SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			if (!SDL_GPUTextureSupportsFormat(globalGpuDevice, selectedFormat,
				SDL_GPU_TEXTURETYPE_2D, usage))
			{
				selectedFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			}
			texture.gpuFormat = selectedFormat;

			SDL_GPUTextureCreateInfo textureInfo = {};
			textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
			textureInfo.format = selectedFormat;
			textureInfo.usage = usage;
			textureInfo.width = static_cast<uint32_t>(w);
			textureInfo.height = static_cast<uint32_t>(h);
			textureInfo.layer_count_or_depth = 1;
			textureInfo.num_levels = 1;
			textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

			texture.gpuTexture = SDL_CreateGPUTexture(globalGpuDevice, &textureInfo);
			if (!texture.gpuTexture)
			{
				errorFunc("Failed to create SDL GPU framebuffer texture", userDefinedData);
				this->w = 0;
				this->h = 0;
				texture = {};
			}
			return;
		}

		texture.tex = SDL_CreateTexture(
			platform::getSdlRenderer(),
			SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET,
			w, h
		);

		if (!texture.tex)
		{
			this->w = 0;
			this->h = 0;
			texture.cachedSize = {};
			return;
		}

		SDL_SetTextureBlendMode(texture.tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(
			texture.tex,
			nearestFilter ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR
		);
	}

	void FrameBuffer::resize(int w, int h)
	{
		if (w < 0) { w = 0; }
		if (h < 0) { h = 0; }

		if (this->w == w && this->h == h) return;

		cleanup();
		create(w, h);

	}

	void FrameBuffer::cleanup()
	{
		texture.cleanup();
		w = h = 0;
		previousBoundFrameBuffer = nullptr;
	}

	void FrameBuffer::clear()
	{
		if (texture.gpuTexture && texture.gpuDevice)
		{
			clearGpuTextureTarget(texture.gpuDevice, texture.gpuTexture, lastFrameBufferClearColor);
			return;
		}

		// Bind this framebuffer
		SDL_SetRenderTarget(platform::getSdlRenderer(), texture.tex);

		// Clear using current draw color
		SDL_RenderClear(platform::getSdlRenderer());

		// Unbind back to main framebuffer
		SDL_SetRenderTarget(platform::getSdlRenderer(), nullptr);

	}

	void FrameBuffer::bind()
	{
		if (activeRendererInstance && activeRendererInstance->gpuDevice && texture.gpuTexture)
		{
			if (!activeRendererInstance->spritePositions.empty() && !activeRendererInstance->spriteTextures.empty())
			{
				activeRendererInstance->flush(true);
			}

			if (activeRendererInstance->boundFrameBuffer == this)
			{
				return;
			}

			// Preserve current target so unbind can return to it (nested offscreen passes).
			previousBoundFrameBuffer = activeRendererInstance->boundFrameBuffer;
			activeRendererInstance->boundFrameBuffer = this;
			return;
		}

		SDL_SetRenderTarget(platform::getSdlRenderer(), texture.tex);
	}

	void FrameBuffer::unbind()
	{
		if (activeRendererInstance && activeRendererInstance->gpuDevice)
		{
			if (!activeRendererInstance->spritePositions.empty() && !activeRendererInstance->spriteTextures.empty())
			{
				activeRendererInstance->flush(true);
			}

			if (activeRendererInstance->boundFrameBuffer == this)
			{
				activeRendererInstance->boundFrameBuffer = previousBoundFrameBuffer;
				previousBoundFrameBuffer = nullptr;
			}
			return;
		}

		SDL_SetRenderTarget(platform::getSdlRenderer(), nullptr);
	}

	glm::vec4 computeTextureAtlas(int xCount, int yCount, int x, int y, bool flip)
	{
		float xSize = 1.f / xCount;
		float ySize = 1.f / yCount;

		if (flip)
		{
			return {(x + 1) * xSize, 1 - (y * ySize), (x)*xSize, 1.f - ((y + 1) * ySize)};
		}
		else
		{
			return {x * xSize, 1 - (y * ySize), (x + 1) * xSize, 1.f - ((y + 1) * ySize)};
		}

	}

	glm::vec4 computeTextureAtlasWithPadding(int mapXsize, int mapYsize,
		int xCount, int yCount, int x, int y, bool flip)
	{
		float xSize = 1.f / xCount;
		float ySize = 1.f / yCount;

		float Xpadding = 1.f / mapXsize;
		float Ypadding = 1.f / mapYsize;

		glm::vec4 noFlip = {x * xSize + Xpadding, 1 - (y * ySize) - Ypadding, (x + 1) * xSize - Xpadding, 1.f - ((y + 1) * ySize) + Ypadding};

		if (flip)
		{
			glm::vec4 flip = {noFlip.z, noFlip.y, noFlip.x, noFlip.w};

			return flip;
		}
		else
		{
			return noFlip;
		}
	}




}

#ifdef _MSC_VER
#pragma warning( pop )
#endif


#endif
