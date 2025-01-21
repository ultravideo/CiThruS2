#pragma once

#include "IntersectionScenarioData.generated.h"

// Contains the scenario data for an intersection
USTRUCT(BlueprintType)
struct FIntersectionScenarioData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	TArray<float> greenTimes = TArray<float>({});

	UPROPERTY(BlueprintReadWrite)
	int firstGreen = 0;

	friend FArchive& operator<<(FArchive& Ar, FIntersectionScenarioData& data)
	{
		Ar << data.greenTimes;
		Ar << data.firstGreen;

		return Ar;
	}
};
