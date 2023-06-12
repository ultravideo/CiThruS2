#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine.h"
#include "RHI.h"
#include "RenderResource.h"
#include "SceneCapture360.h"
#include "Cube2Equirectangular.h"
#include "rgb2yuv.h"
#include "Timer.h"

#if __cplusplus >= 201703L || _MSC_VER > 1911
#if !__has_include("../ThirdParty/uvgRTP/Include/lib.hh") || \
	!__has_include("../ThirdParty/Kvazaar/Include/kvazaar.h")
#define CITHRUS_NO_KVAZAAR_OR_UVGRTP
#else
#include "../ThirdParty/uvgRTP/Include/lib.hh"
#include "../ThirdParty/Kvazaar/Include/kvazaar.h"
#endif // !__has_include (...)
#else
#include "../ThirdParty/uvgRTP/Include/lib.hh"
#include "../ThirdParty/Kvazaar/Include/kvazaar.h"
#endif // __cplusplus (...)

#include <iostream>
#include <memory>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "VideoTransmitter.generated.h"


UCLASS()
class HERVANTAUE4_API AVideoTransmitter : public AActor
{
	GENERATED_BODY()
	
public:	
	AVideoTransmitter();

	// Called every frame
	virtual void Tick(float deltaTime) override;

	virtual bool ShouldTickIfViewportsOnly() const override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Transmit")
	void StartTransmit();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Transmit")
	void StopTransmit();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FString remote_stream_ip = "127.0.0.1";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remote_src_port = 8888;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remote_dst_port = 8888;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remote_stream_width = 1280;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remote_stream_height = 720;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	ASceneCapture360* scene_capture;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int processing_thread_count = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	bool transmit_asynchronously = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int overlapped_wavefront = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int wavefront_parallel_processing = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int quantization_parameter = 27;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	bool enable_360_capture = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	int width_and_height_per_capture_side = 512;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	bool bilinear_filtering = true;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Debug")
	void TransmitDebugMessage();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Debug")
	void ResetStreams();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stream Debug")
	bool debug_write_to_hevc = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Stream Debug")
	FString file_write_hevc_name = "video_out.hevc";

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Debug")
	void CloseFileWrite() { if (debug_outfile) { fclose(debug_outfile); debug_outfile = nullptr; } }

private:
	enum FilterDirection : uint8_t { Down, Right, Up, Left };

#ifndef CITHRUS_NO_KVAZAAR_OR_UVGRTP
	uvgrtp::context stream_context;
	uvgrtp::session* stream_session;
	uvgrtp::media_stream* stream;

	const kvz_api* kvazaar_api = kvz_api_get(8);
	kvz_config* kvazaar_config;
	kvz_encoder* kvazaar_encoder;
	kvz_picture* kvazaar_transmit_picture;
#endif

	std::vector<FRHITexture2D*> render_targets;

	FTexture2DRHIRef concat_buffer;
	FTexture2DRHIRef staging_buffer;

	Cube2Equirectangular cube_2_equirect;

	std::mutex rgba_frame_mutex;
	std::condition_variable rgba_frame_cv;

	std::mutex vram_copy_mutex;
	std::condition_variable vram_copy_cv;
	bool vram_extraction_in_progress;
	
	uint16_t capture_frame_width;
	uint16_t capture_frame_height;

	uint16_t transmit_frame_width;
	uint16_t transmit_frame_height;

	uint8_t* rgba_frame;
	uint8_t* warped_frame;
	uint8_t* yuv_frame;
	uint8_t* encoded_frame;

	std::thread encoding_thread;
	std::mutex transmit_mutex;

	int thread_count;

	bool terminate_encoding_thread;

	bool transmit_360_video;
	bool transmit_async;
	bool transmit_enabled;

	bool use_editor_tick;

	std::ofstream debug_hevc_stream;
	FILE* debug_outfile = nullptr;
	bool debug_prev_write_to_hevc;

	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	void DeleteStreams();

	void StartStreams();

	void CaptureAndTransmit();

	void EncodeAndTransmitFrame();

	void TransmitAsync();

	void ExtractFrameFromGPU();

	void WarpFrameEquirectangular();

	int EdgePixelIndexFromDirs(const FilterDirection& out_dir, const FilterDirection& in_dir,
		const CUBE_FACES& face, const int& face_x, const int& face_y, const int& num_frames);

	void OpenDebugFile();
};
