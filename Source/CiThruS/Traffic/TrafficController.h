#pragma once

#include "Paths/KeypointGraph.h"
#include "Areas/RoadRegulationZone.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <mutex>
#include "TrafficController.generated.h"

class ACar;
class APedestrian;
class ATram;
class ABicycle;
class ITrafficArea;
class ITrafficEntity;
class AParkingController;

// Spawns all traffic entities and simulates them. Static parked cars are not traffic entities and are handled by ParkingController instead
// There should be only one of these in the environment at a time
UCLASS()
class CITHRUS_API ATrafficController : public AActor
{
	GENERATED_BODY()
	
public:	
	ATrafficController();

	virtual void Tick(float deltaTime) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Traffic System")
	void ClearLinkLines();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Traffic System")
	void RedrawLinkLines();

	ACar* SpawnCar();
	ACar* SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate);
	ACar* SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate, const TSubclassOf<ACar>& carClass, const int& carVariant);
	APedestrian* SpawnPedestrian(const FVector& position, const FRotator& rotation, const bool& simulate);
	ABicycle* SpawnBicycle(const FVector& position, const FRotator& rotation, const bool& simulate);
	ATram* SpawnTram(const FVector& position, const bool& simulate);

	void RespawnCar(ACar* car); // Used to re-spawn cars after start of simulation

	inline void DeleteAllCars() { DeleteAllEntitiesOfType<ACar>(); }
	inline void DeleteAllTrams() { DeleteAllEntitiesOfType<ATram>(); }
	inline void DeleteAllPedestrians() { DeleteAllEntitiesOfType<APedestrian>(); }
	inline void DeleteAllBicycles() { DeleteAllEntitiesOfType<ABicycle>(); }
	inline void DeleteAllEntities() { DeleteAllCars(); DeleteAllTrams(); DeleteAllPedestrians(); DeleteAllBicycles(); }

	void InvalidateTrafficEntity(ITrafficEntity* entity);

	inline const KeypointGraph& GetRoadGraph() const { return roadGraph_; }
	inline const KeypointGraph& GetSharedUseGraph() const { return sharedUseGraph_; }
	inline const KeypointGraph& GetTramwayGraph() const { return tramwayGraph_; }

	TArray<AActor*> GetEntitiesInArea(FVector center3d, FVector forward3d, float length, float width);

	TArray<TSubclassOf<ACar>> GetTemplateCars() const { return templateCars_; }

	ITrafficEntity* GetEntityInFrontOfPlayer() const { return entityInFront_; }

protected:
	struct TrafficEntityWrapper
	{
		ITrafficEntity* entity;
		bool isNear = false;
		bool previousIsNear = false;
		float distanceFromCamera;
	};

	/* Enable/disable parking */
	UPROPERTY(EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	bool simulateParking_ = true;

	/* TODO: Pawn collisions check traffic entities against the player vehicle, but also mean superfluous checks against e.g. pedestrians */
	UPROPERTY(EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	bool checkPawnCollisions_ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic System|Vehicle Simulation")
	TArray<TSubclassOf<ACar>> templateCars_;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic System|Vehicle Simulation")
	TArray<TSubclassOf<APedestrian>> templatePedestrians_;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic System|Vehicle Simulation")
	TArray<TSubclassOf<ABicycle>> templateBicycles_;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic System|Vehicle Simulation")
	TArray<TSubclassOf<ATram>> templateTrams_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	int amountOfCarsToSpawn_ = 200;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	int amountOfPedestriansToSpawn_ = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	int amountOfBicyclesToSpawn_ = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	int amountOfTramsToSpawn_ = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	float distanceFromCameraToEnableLowDetail_ = 5000.0f;

	/* Enable LOD system on traffic entitites.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	bool lowDetailEntitiesOutsideCamera_ = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	int maxHighDetailVehiclesZeroForUnlimited_ = 10;

	/* Use editor viewport camera rather than player vehicle for LOD calculation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	bool useEditorViewportCamera_ = false;

	/*
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	bool hideDistantBicycles_ = true;
	*/

	AParkingController* parkingController_;
	
	std::vector<TrafficEntityWrapper> simulatedEntities_;

	// Used for static entities that are not simulated (spawned with simulate = false)
	std::vector<TrafficEntityWrapper> staticEntities_;

	std::vector<TrafficEntityWrapper*> nearestEntities_;
	std::mutex nearestEntitiesMutex_;

	ITrafficEntity* entityInFront_;
	float entityInFrontDistance_;
	std::mutex entityInFrontMutex_;

	bool massDeletionInProgress_;

	KeypointGraph roadGraph_;
	KeypointGraph sharedUseGraph_;
	KeypointGraph tramwayGraph_;
	KeypointGraph bicycleGraph_;

	TArray<ITrafficArea*> trafficAreas_;

	AParkingController* parkingSystem_;

	bool visualizeCollisions_;
	bool visualizeViewFrustrum_;

	float farDistance_;
 
	UFUNCTION(BlueprintCallable)
	void BeginSimulateTraffic();
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Traffic System")
	void ToggleCollisionBoxes() { visualizeCollisions_ = !visualizeCollisions_; }

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Traffic System")
	void ToggleViewFrustrum() { visualizeViewFrustrum_ = !visualizeViewFrustrum_; }

	void VisualizeCollisionForEntity(ITrafficEntity* entity, const float& deltaTime) const;
	void VisualizeGraph(KeypointGraph* graph) const;

	virtual void BeginPlay() override;

	void CheckEntityCollisions();

	template <class T>
	void DeleteAllEntitiesOfType();

	// Add new road regulation zone
	UFUNCTION(CallInEditor, Category = "Traffic System")
	void AddNewRegulationZone();

	// Set visibility of VehicleRuleZoneComponent actors
	UFUNCTION(CallInEditor, Category = "Traffic System")
	void ApplyRegulationZoneVisibility();

	/* Get rules at point in zone, or default rules if no zones in point */
	FZoneRules GetApplyingRegulationRulesAtPoint(FVector point);
	TArray<ARoadRegulationZone*> GetAllRegulationZones() const;

	/* Hide RoadRegulationZone actors? */
	UPROPERTY(EditAnywhere, Category = "Traffic System|Road Regulation Zones")
	bool hideRegulationZones_ = true;

	/* Hide RoadRegulationZone actors? */
	UPROPERTY(EditAnywhere, Category = "Traffic System|Road Regulation Zones")
	FZoneRules defaultRegulationZone_;

public:

	inline int32 GetKeypointRegulationRulesAtPoint(FVector point) { return GetApplyingRegulationRulesAtPoint(point).rules; }
	inline float GetRegulatedSpeedAtPoint(FVector point) { return GetApplyingRegulationRulesAtPoint(point).speedLimit; }

	// Used when regulation zone collisions are needed inside editor (sim not running)
	void EDITOR_InitRegulationCollisions();	
};
