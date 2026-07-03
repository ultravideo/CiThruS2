#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "CithrusGameViewportClient.generated.h"

// Broadcast on the game thread immediately after UGameViewportClient::Draw
// returns. At that moment the scene has been rendered (commands enqueued for the
// render thread) but Slate has not yet composited UMG / debug widgets on top —
// so a subscriber that copies the viewport's render target here gets a UMG-free image.

DECLARE_TS_MULTICAST_DELEGATE_OneParam(FOnGameViewportPostDraw, FViewport*);

UCLASS()
class CITHRUS_API UCithrusGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;

	// Subscribe / unsubscribe with .AddRaw / .Remove. Thread-safe in both directions.
	static FOnGameViewportPostDraw& OnPostDraw() { return OnPostDraw_; }

private:
	static FOnGameViewportPostDraw OnPostDraw_;
};
