#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficController.h"
#include "NPCar.h"
#include "PlayerCarPawn.h"
#include <vector>
#include "TrafficScenarios.generated.h"

struct CarSpawnInfo
{
	FVector position;
	FRotator rotation;
	bool simulate;
};

struct TrafficScenario
{
	FString name;
	CarSpawnInfo playerCar;
	TArray<CarSpawnInfo> cars;
};

UCLASS()
class HERVANTAUE4_API ATrafficScenarios : public AActor
{
	GENERATED_BODY()
	
public:	
	ATrafficScenarios() { };

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic Scenarios")
	ATrafficController* trafficController_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic Scenarios")
	APlayerCarPawn* playerCar_;

	void ActivateScenario(const TrafficScenario& scenario);

protected:
	std::vector<TrafficScenario> scenarios_ =
	{
		{
			"Parking lot",
			{ FVector(-5700.0f, -48462.0f, 2200.0f), FRotator(0.0f, 54.0f, 0.0f), true },
			{ }
		},
		{
			"Parallel parking",
			{ FVector(7640.0f, -85050.0f, 2200.0f), FRotator(0.0f, 334.0f, 0.0f), true },
			{
				{ FVector(12260.0f, -87200.0f, 1950.0f), FRotator(0.0f, 333.0f, 0.0f), false },
				{ FVector(11059.0f, -86597.0f, 1950.0f), FRotator(0.0f, 333.0f, 0.0f), false }
			}
		},
		{
			"Roundabout with traffic",
			{ FVector(-18634.0f, -42332.0f, 2270.0f), FRotator(0.0f, 150.0f, 0.0f), true },
			{
				{ FVector(-23486.0f, -42491.0f, 2298.0f), FRotator(0.0f, 83.0f, 0.0f), true },
				{ FVector(-25755.0f, -38760.0f, 2487.0f), FRotator(0.0f, 333.0f, 0.0f), true },
				{ FVector(-29582.0f, -36980.0f, 2624.0f), FRotator(0.0f, 333.0f, 0.0f), true },
				{ FVector(-20773.0f, -38381.0f, 2386.0f), FRotator(0.0f, 240.0f, 0.0f), true }
			}
		},
		{
			"Intersection with traffic lights",
			{ FVector(-12720.0f, -47135.0f, 2200.0f), FRotator(0.0f, 82.0f, 0.0f), true },
			{
				{ FVector(-14964.0f, -43644.0f, 2223.0f), FRotator(0.0f, 333.0f, 0.0f), true },
				{ FVector(-14864.0f, -43320.0f, 2269.0f), FRotator(0.0f, 333.0f, 0.0f), true },
				{ FVector(-16256.0f, -42809.0f, 2223.0f), FRotator(0.0f, 333.0f, 0.0f), true },
				{ FVector(-10763.0f, -43209.0f, 2216.0f), FRotator(0.0f, 240.0f, 0.0f), true },
				{ FVector(-6826.0f, -49448.0f, 2152.0f), FRotator(0.0f, 150.0f, 0.0f), true },
				{ FVector(-13441.0f, -47253.0f, 2270.0f), FRotator(0.0f, 82.0f, 0.0f), true }
			}
		},
		{
			"Overtake with same sided traffic",
			{ FVector(-38538.0f, -109444.0f, 2700.0f), FRotator(0.0f, 240.0f, 0.0f), true },
			{
				{ FVector(-39640.0f, -111020.0f, 2440.0f), FRotator(0.0f, 240.0f, 0.0f), false },
				{ FVector(-39250.0f, -107679.0f, 2520.0f), FRotator(0.0f, 240.0f, 0.0f), true },
				{ FVector(-40053.0f, -107089.0f, 2520.0f), FRotator(0.0f, 240.0f, 0.0f), true }
			}
		},
		{
			"Overtake with oncoming traffic",
			{ FVector(-2833.0f, -26359.0f, 2600.0f), FRotator(0.0f, 60.0f, 0.0f), true },
			{
				{ FVector(2359.0f, -18052.0f, 2484.0f), FRotator(0.0f, 60.0f, 0.0f), true },
				{ FVector(13767.0f, 1552.0f, 2183.0f), FRotator(0.0f, 240.0f, 0.0f), true }
			}
		},
	};

	void ResetTrafficScenario();
};
