#pragma once

#include "RHIResources.h"
#include "PixelFormat.h"
#include "Delegates/IDelegateInstance.h"
#include "Pipeline/Internal/PipelineSource.h"

#include <cstdint>
#include <mutex>

class FViewport;

// Reads the rendered game viewport's render target (scene + HUD, no UMG) once per
// frame. Subscribes to UCithrusGameViewportClient::OnPostDraw, which fires on the
// game thread right after the viewport draws its scene but before Slate composites
// UMG / debug widgets. A render-thread copy enqueued from that point is serialized
// between the scene render and the UMG composite, so the captured image is
// UMG-free in PIE, standalone, and packaged builds alike. Avoids the extra scene
// render incurred by a USceneCaptureComponent2D.
//
// Requires UCithrusGameViewportClient (or a subclass) to be the active game
// viewport client — set via Project Settings -> Engine -> General Settings ->
// Default Classes -> Game Viewport Client Class.
class CITHRUS_API BackBufferReader : public PipelineSource<1>
{
public:
	BackBufferReader(const uint16_t& frameWidth, const uint16_t& frameHeight);
	virtual ~BackBufferReader();

	virtual void Process() override;

private:
	uint16_t frameWidth_;
	uint16_t frameHeight_;
	uint8_t bytesPerPixel_;

	uint8_t* frameBuffers_[2];
	uint8_t bufferIndex_;

	bool frameDirty_;
	std::mutex readMutex_;

	FTextureRHIRef stagingBuffer_;
	bool stagingReady_;
	EPixelFormat sourceFormat_;

	// Handle for the thread-safe OnPostDraw subscription. Remove() is safe to call
	// from the AsyncPipelineRunner worker thread that destroys this reader.
	FDelegateHandle postDrawDelegateHandle_;

	// One-shot diagnostic flags so we only log once per event type, not every frame.
	bool loggedFirstEvent_;
	bool loggedFormatReject_;
	bool loggedSizeReject_;
	bool loggedFirstCapture_;
	bool loggedNullViewport_;
	bool loggedNullRT_;

	std::mutex resourceMutex_;
	bool destroyed_;

	// Called on the game thread by UCithrusGameViewportClient::Draw after the scene
	// has been rendered. Snapshots the viewport's RT pointer and enqueues a render-
	// thread copy command.
	void OnViewportPostDraw_GameThread(FViewport* viewport);

	// Render-thread half of the capture; runs the actual CopyTexture + map/readback.
	void Capture_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIRef sourceTexture);
};
