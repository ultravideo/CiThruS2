#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeAndWeatherController.generated.h"

struct FTimeAndWeatherDataPreset;
struct FWeatherSetup;
struct FTimeSetup;
enum EWeatherTypes : uint8;

// Parent of LightFXManager blueprint, Used to access & control it through C++ 
// Contains modifiable presets to quickly chance weather and time conditions.
UCLASS(Abstract)
class CITHRUS_API ATimeAndWeatherController : public AActor
{
	GENERATED_BODY()

public:
	ATimeAndWeatherController() { PrimaryActorTick.bCanEverTick = false; }

	// Time
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetTimeOfDay(FTimeSetup timeSetup); // Link to blueprint, FTimeSetup contains all time values
	void SetTimeOfDay(float timeOfDay, bool enableProgression = false, float dayLengthInSeconds = 2000); // Change some common time values
	UFUNCTION(BlueprintImplementableEvent)
	void GetTimeOfDay(float& timeOfDay);

	// Weather
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetWeather(FWeatherSetup weatherSetup); // Link to blueprint, FWeatherSetup contains all weather values
	void SetWeather(EWeatherTypes weatherType); // Change weather type only (clear, rain, snow)

	// Presets
	void ApplyPreset(FTimeAndWeatherDataPreset preset); // Apply custom preset
	UFUNCTION(BlueprintCallable)
	void ApplyPreset(int index); // Apply preset by index
	// More presets
	UFUNCTION(BlueprintCallable)
	TArray<FString> GetPresetDescriptions(); // Get the description for each preset, in order.
	inline TArray<FTimeAndWeatherDataPreset> GetPresets() const { return presets_; }

protected:
	// List of presets (modify inside editor (LightFXManager))
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weather Presets")
	TArray<FTimeAndWeatherDataPreset> presets_;
};
