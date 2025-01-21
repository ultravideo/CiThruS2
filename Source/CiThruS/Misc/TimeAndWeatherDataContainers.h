/* 
// Contains all data structures that are used for
// controlling weather and time through TimeAndWeatherController
// @see TimeAndWeatherController.h
*/
#pragma once

#include "TimeAndWeatherDataContainers.generated.h"

UENUM(BlueprintType)
enum EWeatherTypes : uint8
{
	Unspecified UMETA(DisplayName = "Unspecified"),
	Clear UMETA(DisplayName = "Clear"),
	Rain UMETA(DisplayName = "Rainy"),
	Snow UMETA(DisplayName = "Snowy")
};

USTRUCT(BlueprintType)
struct FRainSetup
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere) float fogDensity = 0.01;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) float sunIntensity = 8.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "1")) float fallIntensity = 0.0f;
};

USTRUCT(BlueprintType)
struct FSnowSetup
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere) float fogDensity = 0.1f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) float sunIntensity = 8.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (UIMin = "0", UIMax = "1")) float treeSnowCoverage = 1.0f;
};

USTRUCT(BlueprintType)
struct FWeatherSetup
{
	GENERATED_BODY()
public:
	/* Base weather type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere) TEnumAsByte<EWeatherTypes> weather = EWeatherTypes::Unspecified;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) bool enableRainOrSnow = true;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) FRainSetup rainSetup;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) FSnowSetup snowSetup;
};

USTRUCT(BlueprintType)
struct FTimeSetup
{
	GENERATED_BODY()
public:
	/* Enable time of day progression ? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere) bool enableProgression = true;
	/* Leave at -1, to remain unaltered */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "-1", ClampMax = "24", UIMin = "-1", UIMax = "24")) int timeOfDay = -1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) float dayLengthInSeconds = 2000;
};

USTRUCT(BlueprintType)
struct FTimeAndWeatherDataPreset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) FString description = "Short description";
	UPROPERTY(EditAnywhere) FWeatherSetup Weather;
	UPROPERTY(EditAnywhere) FTimeSetup Time;
};
