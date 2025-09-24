#include "TramControlPoint.h"
#include "Tram.h"

ATramControlPoint::ATramControlPoint()
{
	UStaticMeshComponent* visualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	RootComponent = visualMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> visualAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (visualAsset.Succeeded())
	{
		visualMesh->SetStaticMesh(visualAsset.Object);
	}

	visualMesh->bHiddenInGame = false;
	visualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	visualMesh->OnComponentBeginOverlap.AddDynamic(this, &ATramControlPoint::OnBeginOverlap);
}

void ATramControlPoint::OnBeginOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex, bool bFromSweep, const FHitResult& hit)
{
	if (!otherActor->IsA(ATram::StaticClass()))
	{
		return;
	}

	if (_delay >= 1) // Timers sometimes break if less than 1 second - ??
	{
		FTimerHandle UnusedHandle;
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ATramControlPoint::OnDelayComplete, otherActor);
		GetWorldTimerManager().SetTimer(UnusedHandle, Delegate, _delay, false);
	}
	else
	{
		OnDelayComplete(otherActor);
	}
}

void ATramControlPoint::OnDelayComplete(AActor* otherActor)
{
	Cast<ATram>(otherActor)->SetMoveSpeed(_targetSpeed);
}
