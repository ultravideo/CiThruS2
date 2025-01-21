#include "TramLightArea.h"
#include "Components/BoxComponent.h"
#include "Tram.h"

ATramLightArea::ATramLightArea()
{
	PrimaryActorTick.bCanEverTick = false;

	UBoxComponent* visualMesh = CreateDefaultSubobject<UBoxComponent>(TEXT("Visuals"));
	RootComponent = visualMesh;

	visualMesh->bHiddenInGame = true;
	visualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	visualMesh->OnComponentBeginOverlap.AddDynamic(this, &ATramLightArea::OnBeginOverlap);
	visualMesh->OnComponentEndOverlap.AddDynamic(this, &ATramLightArea::OnEndOverlap);
}

void ATramLightArea::OnBeginOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex, bool bFromSweep, const FHitResult& hit)
{
	if (!otherActor->IsA(ATram::StaticClass()))
	{
		return;
	}

	tramsColliding_++;

	if (tramsColliding_ == 1) // Set light to red only when 1st tram entering area
	{
		state_ = TramLightAreaState::YELLOW;
		FTimerHandle UnusedHandle;
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ATramLightArea::SetState, TramLightAreaState::RED);
		GetWorldTimerManager().SetTimer(UnusedHandle, Delegate, yellowTime_, false);
	}
}

void ATramLightArea::OnEndOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex)
{
	if (!otherActor->IsA(ATram::StaticClass()))
	{
		return;
	}

	tramsColliding_--;

	if (tramsColliding_ == 0) // Set light back to green when all trams left area
	{
		state_ = TramLightAreaState::YELLOW;
		FTimerHandle UnusedHandle;
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ATramLightArea::SetState, TramLightAreaState::GREEN);
		GetWorldTimerManager().SetTimer(UnusedHandle, Delegate, yellowTime_, false);
	}
}
