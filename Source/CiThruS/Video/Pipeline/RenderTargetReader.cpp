#include "RenderTargetReader.h"
#include "Misc/Debug.h"
#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/TextureRenderTarget2D.h"

RenderTargetReader::RenderTargetReader(std::vector<UTextureRenderTarget2D*> textures, const bool& depth, const float& depthRange)
	: depth_(depth), depthRange_(depthRange), frameDirty_(false), flushNeeded_(false), bufferIndex_(0), initialized_(false), destroyed_(false)
{
	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;

	queuedUserData_ = nullptr;
	queuedUserDataSize_ = 0;

	userData_[0] = nullptr;
	userData_[1] = nullptr;
	userDataSize_ = 0;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);

	GetOutputPin<1>().SetData(nullptr);
	GetOutputPin<1>().SetSize(0);
	GetOutputPin<1>().SetFormat("binary");

	if (textures.empty())
	{
		GetOutputPin<0>().SetFormat("error");
		return;
	}
	
	ETextureRenderTargetFormat format = textures[0]->RenderTargetFormat;

	switch (format)
	{
	case ETextureRenderTargetFormat::RTF_R32f:
		GetOutputPin<0>().SetFormat("gray32f");
		bytesPerPixel_ = 4 * 1;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA32f:
		GetOutputPin<0>().SetFormat("rgba32f");
		bytesPerPixel_ = 4 * 4;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA8:
	case ETextureRenderTargetFormat::RTF_RGBA8_SRGB:
		// For some reason UE actually outputs BGRA, not RGBA...
		GetOutputPin<0>().SetFormat("bgra");
		bytesPerPixel_ = 1 * 4;
		break;
	default:
		return;
	}

	if (depth_)
	{
		GetOutputPin<0>().SetFormat("rgba");
		bytesPerPixel_ = 1 * 4;
	}

	textures_.resize(textures.size(), nullptr);

	// Note that this is executed on the render thread later and not yet
	ENQUEUE_RENDER_COMMAND(InitializeReader)(
		[this, textures](FRHICommandListImmediate& RHICmdList)
		{
			std::unique_lock<std::mutex> lock(resourceMutex_);

			if (destroyed_)
			{
				return;
			}

			for (int i = 0; i < textures.size(); i++)
			{
				textures_[i] = textures[i]->GetResource()->GetTexture2DRHI();
			}

			EPixelFormat pixelFormat = EPixelFormat();

			if (!textures_.empty())
			{
				// Assumes every texture is the same size, otherwise they would be hard to concatenate
				frameWidth_ = textures_[0]->GetDesc().Extent.X;
				frameHeight_ = textures_[0]->GetDesc().Extent.Y;

				pixelFormat = textures[0]->GetFormat();
			}

			if (depth_)
			{
				FRHITextureDesc depthTextureDesc(ETextureDimension::Texture2D, ETextureCreateFlags::RenderTargetable, PF_R8G8B8A8, FClearValueBinding::Black, FIntPoint(textures_.size() * frameWidth_, frameHeight_), 1, 1, 1, 1, 0);
				FRHITextureCreateDesc depthCreateDesc(depthTextureDesc, ERHIAccess::RTV, TEXT("Render Target Reader Depth Conversion Texture"));

				depthBuffer_ = RHICmdList.CreateTexture(depthCreateDesc);
			}

			// Vulkan doesn't support copying texture data to a specific offset in a CPU-accessible texture so a separate
			// buffer is needed for concatenating the 360 cubemap sides on the GPU. In DX11/12 it is supported, so this can be skipped
			if (textures.size() != 1 && FString(GDynamicRHI->GetName()) == TEXT("Vulkan"))
			{
				FRHITextureDesc concatTextureDesc(ETextureDimension::Texture2D, ETextureCreateFlags::None, pixelFormat, FClearValueBinding::Black, FIntPoint(textures_.size() * frameWidth_, frameHeight_), 1, 1, 1, 1, 0);
				FRHITextureCreateDesc concatCreateDesc(concatTextureDesc, ERHIAccess::DSVRead | ERHIAccess::DSVWrite, TEXT("Render Target Reader Concatenation Texture"));

				concatBuffer_ = RHICmdList.CreateTexture(concatCreateDesc);
			}

			FRHITextureDesc stagingTextureDesc(ETextureDimension::Texture2D, ETextureCreateFlags::CPUReadback, depth_ ? PF_R8G8B8A8 : pixelFormat, FClearValueBinding::Black, FIntPoint(textures_.size() * frameWidth_, frameHeight_), 1, 1, 1, 1, 0);
			FRHITextureCreateDesc stagingCreateDesc(stagingTextureDesc, ERHIAccess::CPURead | ERHIAccess::DSVWrite, TEXT("Render Target Reader Staging Texture"));

			stagingBuffer_ = RHICmdList.CreateTexture(stagingCreateDesc);

			uint32_t outputSize = textures_.size() * frameWidth_ * frameHeight_ * bytesPerPixel_;

			frameBuffers_[0] = new uint8_t[outputSize]();
			frameBuffers_[1] = new uint8_t[outputSize]();

			initialized_ = true;
		});
}

RenderTargetReader::~RenderTargetReader()
{
	resourceMutex_.lock();

	destroyed_ = true;
	initialized_ = false;

	textures_.clear();

	if (concatBuffer_)
	{
		concatBuffer_ = nullptr;
	}

	if (stagingBuffer_)
	{
		stagingBuffer_ = nullptr;
	}

	delete[] frameBuffers_[0];
	delete[] frameBuffers_[1];

	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;

	delete[] userData_[0];
	delete[] userData_[1];

	userData_[0] = nullptr;
	userData_[1] = nullptr;

	delete[] queuedUserData_;

	queuedUserData_ = nullptr;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);

	GetOutputPin<1>().SetData(nullptr);
	GetOutputPin<1>().SetSize(0);

	resourceMutex_.unlock();
}

void RenderTargetReader::Process()
{
	{
		std::unique_lock<std::mutex> lock(readMutex_);

		// Should not swap buffers if there is no new data as that would lead to showing old frames again
		if (!frameDirty_)
		{
			GetOutputPin<0>().SetData(nullptr);
			GetOutputPin<0>().SetSize(0);

			GetOutputPin<1>().SetData(nullptr);
			GetOutputPin<1>().SetSize(0);

			return;
		}

		// Alternate between two buffers to make sure one buffer can be read safely while the other is being written
		GetOutputPin<0>().SetData(frameBuffers_[bufferIndex_]);
		GetOutputPin<0>().SetSize(textures_.size() * frameWidth_ * frameHeight_ * bytesPerPixel_);
		GetOutputPin<1>().SetData(userData_[bufferIndex_]);
		GetOutputPin<1>().SetSize(userDataSize_);
		bufferIndex_ = (bufferIndex_ + 1) % 2;

		frameDirty_ = false;
	}

	{
		std::lock_guard<std::mutex> lock(flushMutex_);

		flushNeeded_ = false;
	}

	flushCv_.notify_all();
}

void RenderTargetReader::Read(uint8_t* userData, const uint32_t& userDataSize)
{
	{
		std::unique_lock<std::mutex> lock(flushMutex_);

		// Wait until the previous frame has been passed to the pipeline before processing more frames
		if (flushNeeded_)
		{
			Flush();
		}

		flushCv_.wait(lock, [this] { return !flushNeeded_; });

		flushNeeded_ = true;
	}

	// Copy the user data so that it can be safely deleted by the caller of this function
	queuedUserData_ = new uint8_t[userDataSize];
	queuedUserDataSize_ = userDataSize;
	memcpy(queuedUserData_, userData, userDataSize);

	ENQUEUE_RENDER_COMMAND(CopyBuffers)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			std::lock_guard<std::mutex> resourceLock(resourceMutex_);

			if (!initialized_)
			{
				delete[] queuedUserData_;
				queuedUserData_ = nullptr;
				queuedUserDataSize_ = 0;

				{
					std::lock_guard<std::mutex> flushLock(flushMutex_);

					flushNeeded_ = false;
				}

				flushCv_.notify_all();

				return;
			}

			CopyToStagingBuffer(RHICmdList);
		});
}

void RenderTargetReader::Flush()
{
	// Unreal Engine delays the execution of some RHI functions inside
	// CopyToStagingBuffer until the next frame, so this function is needed to
	// ensure that the staging buffer is ready before ExtractStagingBuffer is
	// called

	ENQUEUE_RENDER_COMMAND(ExtractRenderTargets)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			{
				std::lock_guard<std::mutex> lock(resourceMutex_);

				if (initialized_)
				{
					ExtractStagingBuffer(RHICmdList);
				}
			}
		});
}

class FDepthConversionVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI(FRHICommandListBase& RHICmdList)
	{
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(6);

		Vertices[0].Position = FVector4f(-1, 1, 0, 1);
		Vertices[0].UV = FVector2f(0, 0);

		Vertices[1].Position = FVector4f(1, 1, 0, 1);
		Vertices[1].UV = FVector2f(1, 0);

		Vertices[2].Position = FVector4f(-1, -1, 0, 1);
		Vertices[2].UV = FVector2f(0, 1);

		Vertices[3].Position = FVector4f(1, -1, 0, 1);
		Vertices[3].UV = FVector2f(1, 1);

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(reinterpret_cast<uintptr_t>(Vertices.GetData()));
		VertexBufferRHI = RHICmdList.CreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};

TGlobalResource<FDepthConversionVertexBuffer> GDepthConversionVertexBuffer;

class DepthConversionVertexShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(DepthConversionVertexShader);

	static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	DepthConversionVertexShader() = default;
	DepthConversionVertexShader(ShaderMetaType::CompiledShaderInitializerType const& Initializer) :
		FGlobalShader(Initializer) { }
};

class DepthConversionPixelShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(DepthConversionPixelShader);
	SHADER_USE_PARAMETER_STRUCT(DepthConversionPixelShader, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, DepthRange)
		SHADER_PARAMETER(float, Gamma)
		SHADER_PARAMETER_TEXTURE(Texture2D, Tex0)
		SHADER_PARAMETER_SAMPLER(SamplerState, SamplerTex0)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(DepthConversionVertexShader, "/Shaders/DepthConversion.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(DepthConversionPixelShader, "/Shaders/DepthConversion.usf", "MainPS", SF_Pixel);

void RenderTargetReader::ConvertDepth(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_PixelShader);
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_Pixel);

	FRHIRenderPassInfo RenderPassInfo(depthBuffer_, ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("ShaderPlugin_OutputToRenderTarget"));

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<DepthConversionVertexShader> VertexShader(ShaderMap);
	TShaderMapRef<DepthConversionPixelShader> PixelShader(ShaderMap);

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	// Setup the pixel shader
	DepthConversionPixelShader::FParameters PassParameters;

	PassParameters.Tex0 = textures_[0];
	PassParameters.SamplerTex0 = TStaticSamplerState<>::GetRHI();
	PassParameters.Gamma = 2.2;
	PassParameters.DepthRange = depthRange_;

	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters);

	// Draw
	RHICmdList.SetStreamSource(0, GDepthConversionVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawPrimitive(0, 2, 1);

	RHICmdList.EndRenderPass();
}

void RenderTargetReader::ExtractStagingBuffer(FRHICommandListImmediate& RHICmdList)
{
	// Read the contents of the staging texture
	void* resource;
	int32_t stagingBufferWidth;
	int32_t stagingBufferHeight;

	RHICmdList.MapStagingSurface(stagingBuffer_, resource, stagingBufferWidth, stagingBufferHeight);

	const int32_t numFrames = textures_.size();

	{
		std::unique_lock<std::mutex> lock(readMutex_);

		if (stagingBufferWidth == numFrames * frameWidth_ && stagingBufferHeight == frameHeight_)
		{
			// Mapped texture data has no padding: multiple rows can be copied in a single memcpy
			memcpy(
				frameBuffers_[bufferIndex_],
				reinterpret_cast<uint8_t*>(resource),
				stagingBufferHeight * stagingBufferWidth * bytesPerPixel_);
		}
		else
		{
			// Mapped texture data has padding: must be copied row by row to discard the padding
			const int32_t rgbaPitch = numFrames * frameWidth_ * bytesPerPixel_;
			const int32_t stagingPitch = stagingBufferWidth * bytesPerPixel_;

			for (int j = 0; j < stagingBufferHeight; j++)
			{
				memcpy(
					frameBuffers_[bufferIndex_] + j * rgbaPitch,
					reinterpret_cast<uint8_t*>(resource) + j * stagingPitch,
					rgbaPitch);
			}
		}

		delete[] userData_[bufferIndex_];
		userData_[bufferIndex_] = queuedUserData_;
		userDataSize_ = queuedUserDataSize_;

		queuedUserData_ = nullptr;

		frameDirty_ = true;
	}

	RHICmdList.UnmapStagingSurface(stagingBuffer_);
}

void RenderTargetReader::CopyToStagingBuffer(FRHICommandListImmediate& RHICmdList)
{
	// Copy render targets to the staging texture, with an extra concatenation buffer or not
	if (concatBuffer_)
	{
		for (int i = 0; i < textures_.size(); i++)
		{
			FRHICopyTextureInfo copyTextureInfo = {};

			copyTextureInfo.DestPosition = FIntVector(i * frameWidth_, 0, 0);
			copyTextureInfo.Size = FIntVector(frameWidth_, frameHeight_, 1);

			RHICmdList.CopyTexture(textures_[i], concatBuffer_, copyTextureInfo);
		}

		FRHICopyTextureInfo copyTextureInfo = {};

		RHICmdList.CopyTexture(concatBuffer_, stagingBuffer_, copyTextureInfo);
	}
	else if (depth_)
	{
		ConvertDepth(RHICmdList);

		FRHICopyTextureInfo copyTextureInfo = {};

		copyTextureInfo.DestPosition = FIntVector(0, 0, 0);
		copyTextureInfo.Size = FIntVector(frameWidth_, frameHeight_, 1);

		RHICmdList.CopyTexture(depthBuffer_, stagingBuffer_, copyTextureInfo);
	}
	else
	{
		for (int i = 0; i < textures_.size(); i++)
		{
			FRHICopyTextureInfo copyTextureInfo = {};

			copyTextureInfo.DestPosition = FIntVector(i * frameWidth_, 0, 0);
			copyTextureInfo.Size = FIntVector(frameWidth_, frameHeight_, 1);

			RHICmdList.CopyTexture(textures_[i], stagingBuffer_, copyTextureInfo);
		}
	}
}
