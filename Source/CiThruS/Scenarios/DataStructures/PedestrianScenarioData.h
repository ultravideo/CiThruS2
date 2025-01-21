#pragma once

#include "PedestrianScenarioData.generated.h"

// Contains the scenario data for a pedestrian
USTRUCT(BlueprintType)
struct FPedestrianScenarioData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FVector location = FVector(0, 0, 0);

	friend FArchive& operator<<(FArchive& Ar, FPedestrianScenarioData& data)
	{
		Ar << data.location;

		return Ar;
	}
};
