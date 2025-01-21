#pragma once

#include "PlayerScenarioData.h"
#include "CarScenarioData.h"
#include "PedestrianScenarioData.h"
#include "IntersectionScenarioData.h"

#include "ScenarioData.generated.h"

// Contains all the data for a single scenario
USTRUCT(BlueprintType)
struct FScenarioData
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite)		
	FString scenarioName;

	UPROPERTY(BlueprintReadWrite)		
	FPlayerScenarioData playerData;

	UPROPERTY(BlueprintReadWrite)		
	TArray<FCarScenarioData> carData;
	
	UPROPERTY(BlueprintReadWrite)		
	TArray<FPedestrianScenarioData> pedestrianData;

	UPROPERTY(BlueprintReadWrite)
	TArray<FIntersectionScenarioData> intersectionData;

	UPROPERTY(BlueprintReadWrite)		
	int32 randomCarAmount = 0;
	UPROPERTY(BlueprintReadWrite)		
	float randomCarMinDistance = 100.0f;

	friend FArchive& operator<<(FArchive& Ar, FScenarioData& data)
	{
		Ar << data.playerData;

		Ar << data.carData;

		Ar << data.pedestrianData;

		Ar << data.intersectionData;

		Ar << data.randomCarAmount;
		Ar << data.randomCarMinDistance;

		return Ar;
	}
};
