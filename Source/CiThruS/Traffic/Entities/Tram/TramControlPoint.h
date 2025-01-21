#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TramControlPoint.generated.h"

// Used to trigger tram speed up / slow down
UCLASS()
class CITHRUS_API ATramControlPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATramControlPoint();

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex, bool bFromSweep, const FHitResult& hit);
	void OnDelayComplete(AActor* otherActor);

protected:
	// NEED TO BE SET IN EDITOR
	UPROPERTY(BluePrintReadWrite, EditAnywhere);
	float _targetSpeed = 500; // speed to set to
	UPROPERTY(BluePrintReadWrite, EditAnywhere);
	float _delay = 0; // possible delay after which speed/slow effect takes place (0 = no delay)
};
