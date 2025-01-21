#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CWeatherPreset.generated.h"

UENUM(BlueprintType)
enum class ESeason : uint8
{
	Spring,
	Summer,
	Autumn,
	Winter
};

class UNiagaraSystem;

// A data asset storing preset of common variables needed to initialize a certain type of weather
UCLASS(BlueprintType)
class CITHRUS_API UCWeatherPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Temperature")
	float AverageTemperature = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Temperature")
	float MinTemperature = 20;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Temperature")
	float MaxTemperature = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Temperature")
	TObjectPtr<UMaterialInterface> TemperaturePostProcessMaterial = nullptr;
	
	//A scalar multiplier for wind, should be static
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind")
	float WindStrength = 1000;

	//Wind direction in world space.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta=(UIMin=-1, UIMax=1))
	FVector WindDirection = FVector(1,0,0);

	//The general weight, contribution of the wind towards WPO.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind", meta=(UIMin=-1, UIMax=1))
	float WindWeight = 0.5;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Season")
	ESeason Season = ESeason::Spring;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Season")
	TArray<UNiagaraSystem*> GlobalParticleSystems;	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky")
	float SunIntensity = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky")
	float CloudHeight = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky")
	float NoisePower1 = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky")
	float NoisePower2 = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky")
	float Phase = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sky")
	float MieScattering = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fog")
	float FogIntensity = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fog")
	float HeightFogFalloff = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fog")
	float VolumetricScattering = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fog")
	float HeavyFogFalloff = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fog")
	float HeavyFogIntensity = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fog")
	FColor FogColor = FColor::White;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Time of day", meta=(UIMin=0, UIMax=24))
	float DefaultTimeOfDay = 14;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Time of day", meta=(UIMin=0, UIMax=24))
	float Sunrise = 6;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Time of day", meta=(UIMin=0, UIMax=24))
	float Sunset = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Time of day", meta=(UIMin=0, UIMax=360))
	float SunDirection = 180;

	UFUNCTION(BlueprintPure)
	FVector GetScaledWindSpeed() const;
};
