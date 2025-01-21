#pragma once

#include "CarScenarioData.generated.h"

// Contains the scenario data for a car
USTRUCT(BlueprintType)
struct FCarScenarioData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FVector location = FVector(0,0,0);
	UPROPERTY(BlueprintReadWrite)
	FRotator rotation = FRotator(0, 0, 0);
	UPROPERTY(BlueprintReadWrite)
	float speed = 500.0f;
	UPROPERTY(BlueprintReadWrite)
	bool simulate = true;
	UPROPERTY(BlueprintReadWrite)
	FVector routeStart = FVector(0, 0, 0);
	UPROPERTY(BlueprintReadWrite)
	FVector routeEnd = FVector(0, 0, 0);
	UPROPERTY(BlueprintReadWrite)
	float spawnRate = 1.0f;

	friend FArchive& operator<<(FArchive& Ar, FCarScenarioData& data)
	{
		Ar << data.location;
		Ar << data.rotation;
		Ar << data.speed;
		Ar << data.simulate;
		Ar << data.routeStart;
		Ar << data.routeEnd;
		Ar << data.spawnRate;

		return Ar;
	}
};
