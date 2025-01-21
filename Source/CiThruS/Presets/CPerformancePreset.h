#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CPerformancePreset.generated.h"

// This data asset stores a preset of rendering settings for performance.
UCLASS(BlueprintType)
class CITHRUS_API UCPerformancePreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bSSGI = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bVolumetricFog = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int NumSunCascades = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CascadedShadowDistance = 10000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bLightShaftBloom = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bLightShaftOcclusion = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float VolumetricFogDistance = 40000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bVolumetricClouds = false;
};
