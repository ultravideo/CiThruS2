#pragma once

#include "PlayerScenarioData.generated.h"

// Contains the scenario data for the player
USTRUCT(BlueprintType)
struct FPlayerScenarioData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FVector location = FVector(0, 0, 0);
	UPROPERTY(BlueprintReadWrite) 
	FRotator rotation = FRotator(0, 0, 0);

	friend FArchive& operator<<(FArchive& Ar, FPlayerScenarioData& data)
	{
		Ar << data.location;
		Ar << data.rotation;

		return Ar;
	}
};
