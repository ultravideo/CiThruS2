#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tram.h"
#include "TramStationArea.generated.h"

// Makes trams stop at stations
UCLASS()
class CITHRUS_API ATramStationArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATramStationArea();

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex, bool bFromSweep, const FHitResult& hit);
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex);

	void ReleaseTram(ATram* tram); // Set tram speed after leaving station

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Station parameters");
	float stopTime_ = 10; // Time it takes to leave station after stopping
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Station parameters");
	float leaveSpeed_ = 1000; // Speed after leaving station
};
