#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeypointGraph.h"
#include "KeypointToolComponent.generated.h"

UENUM(BlueprintType)
enum class EKeypointToolGraph : uint8 {
	E_Car       UMETA(DisplayName = "Car"),
	E_Pedestrian        UMETA(DisplayName = "Pedestrian"),
	E_Tram        UMETA(DisplayName = "Tram"),
};

USTRUCT(BlueprintType)
struct FKeypointToolKP
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
		int id;
	UPROPERTY(BlueprintReadWrite)
		FVector location = FVector(0, 0, 0);
	UPROPERTY(BlueprintReadWrite)
		TArray<int> outbound;
	UPROPERTY(BlueprintReadWrite)
		TArray<int> inbound;
	UPROPERTY(BlueprintReadWrite)
		int32 rules;

	FKeypointToolKP(int newId = 0, FVector newLocation = FVector(0,0,0), std::vector<int> newOutbound = std::vector<int>(), std::vector<int> newInbound = std::vector<int>(), int32 newRules = 0)
	{
		id = newId;
		location = newLocation;
		outbound = TArray<int>();
		for (int i : newOutbound)
		{
			outbound.Add(i);
		}
		inbound = TArray<int>();
		for (int i : newInbound)
		{
			inbound.Add(i);
		}
		rules = newRules;
	}
};

// Tool for editing keypoint graphs
UCLASS()
class CITHRUS_API AKeypointToolComponent : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	TArray<FKeypointToolKP> GetKeypoints(EKeypointToolGraph graph);

	UFUNCTION(BlueprintCallable)
	bool SaveToFile(TArray<FKeypointToolKP> keypointsData, EKeypointToolGraph graph);

	UFUNCTION(BlueprintCallable)
	void InitZoneController();
	UFUNCTION(BlueprintCallable)
	int32 GetZoneRulesForPoint(FVector position);
private:
	void WriteLine(FArchive* fileWriter, FString line);
	KeypointGraph GetCorrectGraph(EKeypointToolGraph eg);
};
