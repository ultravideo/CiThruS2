#pragma once

#include "CoreMinimal.h"
#include "ViewSynthesizer.h"

#include "TrafficSnapViewSynthesizer.generated.h"

class ATrafficController;

// A view synthesizer that automatically snaps to traffic entities near the player
UCLASS()
class ATrafficSnapViewSynthesizer : public AViewSynthesizer
{
	GENERATED_BODY()

public:
	ATrafficSnapViewSynthesizer() { }

	virtual void Tick(float deltaTime) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FVector cameraOffset_ = FVector(2.0f, 0.0f, 1.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FRotator cameraRotation_ = FRotator(0.0f, 0.0f, 0.0f);

protected:
	ATrafficController* trafficController_;

	virtual void BeginPlay() override;
};
