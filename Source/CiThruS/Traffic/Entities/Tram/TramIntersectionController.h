#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TramLightArea.h"
#include "TramIntersectionController.generated.h"

// Controls a tram intersection
UCLASS()
class CITHRUS_API ATramIntersectionController : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATramIntersectionController();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(BluePrintReadWrite, EditAnywhere);
	TArray<ATramLightArea*> lightGroups_;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
