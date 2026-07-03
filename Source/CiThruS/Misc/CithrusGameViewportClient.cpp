#include "CithrusGameViewportClient.h"

FOnGameViewportPostDraw UCithrusGameViewportClient::OnPostDraw_;

void UCithrusGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	Super::Draw(InViewport, SceneCanvas);

	// Scene render commands have been enqueued for the render thread by Super::Draw.
	// Slate's UMG composite step hasn't run yet — that happens later in the main loop
	// when FSlateApplication ticks. Subscribers that enqueue a render-thread copy
	// here will have their copy serialized between the scene render and the UMG
	// composite, giving a UMG-free image.
	OnPostDraw_.Broadcast(InViewport);
}
