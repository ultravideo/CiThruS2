#pragma once

#include "KeypointGraph.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficController.generated.h"

class ANPCar;
class APedestrian;
class ITrafficArea;
class ITrafficEntity;

UCLASS()
class HERVANTAUE4_API ATrafficController : public AActor
{
	GENERATED_BODY()
	
public:	
	ATrafficController();

	virtual void Tick(float deltaTime) override;

	// creates a new keypoint and links it to the previous one
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Keypoint Editing")
	void AddKeypointAtTemplate();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Keypoint Editing")
	void LinkToNearest();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Keypoint Editing")
	void LinkFromNearest();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Keypoint Editing")
	void ClearLinkLines();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Keypoint Editing")
	void RedrawLinkLines();

	ANPCar* SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate);
	void DeleteAllCars();

	APedestrian* SpawnPedestrian(const FVector& position, const FRotator& rotation, const bool& simulate);
	void DeleteAllPedestrians();

	inline const KeypointGraph& GetRoadGraph() const { return roadGraph_; };
	inline const KeypointGraph& GetSharedUseGraph() const { return sharedUseGraph_; };

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC Debug")
	bool disableAllNpcs_ = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keypoint Editing")
	AActor* templateKeypoint_;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Simulate")
	TArray<TSubclassOf<ANPCar>> templateCars_;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Simulate")
	TArray<TSubclassOf<APedestrian>> templatePedestrians_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC Simulate")
	int amountOfCarsToSpawn_ = 500;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC Simulate")
	int amountOfPedestriansToSpawn_ = 25;

	TArray<ANPCar*> simulatedVehicles_;
	TArray<ANPCar*> staticVehicles_;

	TArray<APedestrian*> pedestrians_;

	KeypointGraph roadGraph_;
	KeypointGraph sharedUseGraph_;

	TArray<ITrafficArea*> trafficAreas_;

	bool visualizeCollisions_;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NPC Simulate")
	void BeginSimulateTraffic();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NPC Debug")
	void ToggleCollisionVisualization() { visualizeCollisions_ = !visualizeCollisions_; };

	void VisualizeCollisionForEntity(ITrafficEntity* entity, const float& deltaTime) const;

	virtual void BeginPlay() override;

	void CheckCarCollisions();
	void CleanupInvalidTrafficEntities();

	int GetClosestKeypointToTemplateKeypoint();
};
