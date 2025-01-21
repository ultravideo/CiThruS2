#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Landscape.h"
#include "LSAlign.generated.h"

// Performs landscape alignment
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CITHRUS_API ULSAlign : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULSAlign();
	void Init();

	// Called every frame
	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

	UPROPERTY()
	ALandscape* LS = nullptr;
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
};
