#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VideoTransmitter.h"
#include "SceneCapture360.h"
#include "MoveAlongSpline.h"
#include "RgbMaskCaptureHandler.generated.h"

UCLASS()
class HERVANTAUE4_API ARgbMaskCaptureHandler : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARgbMaskCaptureHandler();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings")
		AVideoTransmitter* rgbTransmitter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings")
		ASceneCapture360* rgbCapturer;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings")
		AVideoTransmitter* maskTransmitter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings")
		ASceneCapture360* maskCapturer;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings")
		AMoveAlongSpline* splineMover;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capture Settings")
		bool lockPitchRoll = true;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Capture Settings")
		void StopCaptureAnimation();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Capture Settings")
		void StartCaptureAnimation();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual bool ShouldTickIfViewportsOnly() const override;

protected:
	bool doTick = false;
};
