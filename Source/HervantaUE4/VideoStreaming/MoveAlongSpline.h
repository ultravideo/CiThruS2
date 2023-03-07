#pragma once

#include "Engine.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h" 
#include "MoveAlongSpline.generated.h"

UCLASS()
class HERVANTAUE4_API AMoveAlongSpline : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMoveAlongSpline();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animate Settings")
		USplineComponent* moveSpline;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animate Settings")
		float speed = 200.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animate Settings")
		AActor* moveActor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animate Settings")
		bool lockRollPitch = true;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Animate Settings")
		void ResetDistance();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Animate Settings")
		void StartAnimation();

	UPROPERTY(VisibleAnywhere, Category = "Animate Settings")
		float distance = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Animate Settings")
		bool alwaysIgnoreTick = true;

	FTransform GetMovedTransform(float timeDelta = 0.0) { if (alwaysIgnoreTick) TickInternal(timeDelta); return movedTransform; }

	bool SplineMoveIsComplete() { return distance >= moveSpline->GetSplineLength(); };

	bool doTick = false;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual bool ShouldTickIfViewportsOnly() const override;

protected:
	void TickInternal(float timeInterval);

	FTransform movedTransform;

};
