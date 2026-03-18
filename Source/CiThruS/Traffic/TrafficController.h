#pragma once

#include "Paths/KeypointGraph.h"
#include "Areas/RoadRegulationZone.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <mutex>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include "TrafficController.generated.h"


class ACar;
class APedestrian;
class ATram;
class ABicycle;
class ITrafficArea;
class ITrafficEntity;
class AParkingController;
class LodController;

UENUM()
enum CollisionCheckingEnum
{
	CITHRUS_COLLISIONS_NAIVE					UMETA(DisplayName = "Naive O(n^2) checks"),
	CITHRUS_COLLISIONS_NAIVE_PAR				UMETA(DisplayName = "Parallel O(n^2) checks"),
	CITHRUS_COLLISION_ZONES						UMETA(DisplayName = "Zone-based (check only nearby)"),
	CITHRUS_COLLISION_ZONES_PAR					UMETA(DisplayName = "Parallel zone-based"),
	CITHRUS_COLLISION_DISABLED					UMETA(DisplayName = "Collision checking disabled")
};


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

	void RespawnCar(ACar* car); // Used to respawn cars after start of simulation

	inline void DeleteAllCars() { DeleteAllEntitiesOfType<ACar>(); }
	inline void DeleteAllTrams() { DeleteAllEntitiesOfType<ATram>(); }
	inline void DeleteAllPedestrians() { DeleteAllEntitiesOfType<APedestrian>(); }
	inline void DeleteAllBicycles() { DeleteAllEntitiesOfType<ABicycle>(); }
	inline void DeleteAllEntities() { DeleteAllCars(); DeleteAllTrams(); DeleteAllPedestrians(); DeleteAllBicycles(); }

	void InvalidateTrafficEntity(ITrafficEntity* entity);

	inline const KeypointGraph& GetRoadGraph() const { return roadGraph_; }
	inline const KeypointGraph& GetSharedUseGraph() const { return sharedUseGraph_; }
	inline const KeypointGraph& GetBicycleGraph() const { return bicycleGraph_; }
	inline const KeypointGraph& GetTramwayGraph() const { return tramwayGraph_; }

	TArray<AActor*> GetEntitiesInArea(FVector center3d, FVector forward3d, float length, float width);

	TArray<TSubclassOf<ACar>> GetTemplateCars() const { return templateCars_; }

	void EntityChangeZone(std::pair<int, int> currentZone, std::pair<int, int> previousZone, ITrafficEntity* entity);

	inline int32 GetKeypointRegulationRulesAtPoint(FVector point) { return GetApplyingRegulationRulesAtPoint(point).rules; }
	inline float GetRegulatedSpeedAtPoint(FVector point) { return GetApplyingRegulationRulesAtPoint(point).speedLimit; }

	// Used when regulation zone collisions are needed inside editor (sim not running)
	void EDITOR_InitRegulationCollisions();

	inline LodController* GetLodController() const { return lodController_; }
	inline AParkingController* GetParkingController() const { return parkingController_; }

	inline bool UseEditorViewportCameraForLods() const { return useEditorViewportCamera_; }

protected:
	std::unordered_map<int64, std::unordered_set<ITrafficEntity*>> entitiesByZone_;

	void EntityZoneDelete(std::pair<int32, int32> currentZone, ITrafficEntity* entity);

	/* Enable disable traffic simulation. */
	UPROPERTY(EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	bool simulateTraffic_ = true;

	/* Enable/disable parking */
	UPROPERTY(EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	bool simulateParking_ = true;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Simulation")
	bool spawnEmergencyVehicle_ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic System|Vehicle Simulation")
	TSubclassOf<ACar> emergencyVehicleActor_;

	/* The type of collision checking to do between Traffic System entities*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic System|Vehicle Collision Checking")
	TEnumAsByte<CollisionCheckingEnum> collisionCheckingType_ = CollisionCheckingEnum::CITHRUS_COLLISIONS_NAIVE_PAR;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	float distanceFromCameraToEnableLowDetail_ = 5000.0f;

	/* Enable LOD system on traffic entities. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	bool lowDetailEntitiesOutsideCamera_ = true;

	/* Use editor viewport camera rather than player vehicle for LOD calculation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	bool useEditorViewportCamera_ = false;

	/*
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Vehicle Detail Control")
	bool hideDistantBicycles_ = true;
	*/

	AParkingController* parkingController_;

	LodController* lodController_;
	
	// Stores all simulated (moving) traffic entities
	std::vector<ITrafficEntity*> simulatedEntities_;

	// Used for static entities that are not simulated (spawned with simulate = false)
	std::vector<ITrafficEntity*> staticEntities_;

	bool massDeletionInProgress_;

	KeypointGraph roadGraph_;
	KeypointGraph sharedUseGraph_;
	KeypointGraph tramwayGraph_;
	KeypointGraph bicycleGraph_;

	TArray<ITrafficArea*> trafficAreas_;

	bool visualizeCollisions_;
 
	UFUNCTION(BlueprintCallable)
	void BeginSimulateTraffic();
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Traffic System")
	void ToggleCollisionBoxes() { visualizeCollisions_ = !visualizeCollisions_; }

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Traffic System")
	void ToggleViewFrustrum();

	void VisualizeGraph(KeypointGraph* graph) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

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
};
