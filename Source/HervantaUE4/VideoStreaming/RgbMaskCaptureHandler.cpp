#include "RgbMaskCaptureHandler.h"
#include "../Debug.h"

ARgbMaskCaptureHandler::ARgbMaskCaptureHandler()
{
 	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
}

void ARgbMaskCaptureHandler::StopCaptureAnimation()
{
	if (!rgbCapturer || !maskCapturer || !rgbTransmitter || !maskTransmitter)
	{
		Debug::Log("missing capturers or transmitters!");
		return;
	}

	doTick = false;

	splineMover->ResetDistance();

	const auto new_transform = splineMover->GetMovedTransform(0.0);
	const float yaw = new_transform.Rotator().Yaw;

	rgbCapturer->SetActorTransform(new_transform);
	maskCapturer->SetActorTransform(new_transform);

	if (lockPitchRoll)
	{
		rgbCapturer->SetActorRotation(FRotator(0.0, yaw, 0.0));
		maskCapturer->SetActorRotation(FRotator(0.0, yaw, 0.0));
	}

	rgbTransmitter->StopTransmit();
	maskTransmitter->StopTransmit();
}

void ARgbMaskCaptureHandler::StartCaptureAnimation()
{
	if (!rgbCapturer || !maskCapturer || !rgbTransmitter || !maskTransmitter)
	{
		Debug::Log("missing capturers or transmitters!");
		return;
	}

	splineMover->ResetDistance();

	const auto new_transform = splineMover->GetMovedTransform(0.0);
	const float yaw = new_transform.Rotator().Yaw;

	rgbCapturer->SetActorTransform(new_transform);
	maskCapturer->SetActorTransform(new_transform);

	if (lockPitchRoll)
	{
		rgbCapturer->SetActorRotation(FRotator(0.0, yaw, 0.0));
		maskCapturer->SetActorRotation(FRotator(0.0, yaw, 0.0));
	}

	rgbTransmitter->StartTransmit();
	maskTransmitter->StartTransmit();

	doTick = true;
}

// Called every frame
void ARgbMaskCaptureHandler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!doTick || !rgbCapturer || !maskCapturer)
	{
		return;
	}

	const auto new_transform = splineMover->GetMovedTransform(DeltaTime);
	const float yaw = new_transform.Rotator().Yaw;

	rgbCapturer->SetActorTransform(new_transform);
	maskCapturer->SetActorTransform(new_transform);

	if (lockPitchRoll)
	{
		rgbCapturer->SetActorRotation(FRotator(0.0, yaw, 0.0));
		maskCapturer->SetActorRotation(FRotator(0.0, yaw, 0.0));
	}

	if (splineMover->SplineMoveIsComplete())
	{
		StopCaptureAnimation();
	}
}

bool ARgbMaskCaptureHandler::ShouldTickIfViewportsOnly() const
{
	return doTick;
}

