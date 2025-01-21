#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Traffic/Intersections/TrafficLightGroup.h"
#include "TramLightArea.generated.h"

enum TramLightAreaState { GREEN, YELLOW, RED };

// Controls a tram traffic light area
UCLASS()
class CITHRUS_API ATramLightArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATramLightArea();
	TramLightAreaState GetState() { return state_; }
	TArray<ATrafficLightGroup*> GetLights() const { return lightGroupsRed_; }
	TArray<ATrafficLightGroup*> GetClearLights() const { return lightGroupsRedWhenClear; }

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Hit);
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

protected:
	int tramsColliding_ = 0; //counter / trams inside
	TramLightAreaState state_ = TramLightAreaState::GREEN;

	/* Lights that are always red when a tram is coming*/
	UPROPERTY(BluePrintReadWrite, EditAnywhere, Category="Traffic Light Groups");
	TArray<ATrafficLightGroup*> lightGroupsRed_;
	/* Lights that are red only when there are no trams coming from other directions*/
	UPROPERTY(BluePrintReadWrite, EditAnywhere, Category = "Traffic Light Groups");
	TArray<ATrafficLightGroup*> lightGroupsRedWhenClear;

	UPROPERTY(BluePrintReadWrite, EditAnywhere, Category = "Traffic Light Groups");
	float yellowTime_ = 2.0f;

	void SetState(TramLightAreaState state) { state_ = state; }
};
