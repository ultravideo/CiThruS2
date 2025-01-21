#include "TramStationArea.h"
#include "Components/BoxComponent.h"

ATramStationArea::ATramStationArea()
{
	UBoxComponent* visualMesh = CreateDefaultSubobject<UBoxComponent>(TEXT("Visuals"));
	RootComponent = visualMesh;

	visualMesh->bHiddenInGame = false;
	visualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	visualMesh->OnComponentBeginOverlap.AddDynamic(this, &ATramStationArea::OnBeginOverlap);
	visualMesh->OnComponentEndOverlap.AddDynamic(this, &ATramStationArea::OnEndOverlap);
}

void ATramStationArea::OnBeginOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex, bool bFromSweep, const FHitResult& hit)
{
	if (!otherActor->IsA(ATram::StaticClass()))
	{
		return;
	}

	ATram* tram = Cast<ATram>(otherActor);

	// Slow tram down inside area
	tram->SetMoveSpeed(500);
}

void ATramStationArea::OnEndOverlap(UPrimitiveComponent* overlappedComponent, AActor* otherActor, UPrimitiveComponent* otherComponent, int32 otherBodyIndex)
{
	if (!otherActor->IsA(ATram::StaticClass()))
	{
		return;
	}

	ATram* tram = Cast<ATram>(otherActor);
	tram->SetMoveSpeed(0);

	// Delay before leaving station
	FTimerHandle UnusedHandle;
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ATramStationArea::ReleaseTram, tram);
	GetWorldTimerManager().SetTimer(UnusedHandle, Delegate, stopTime_, false);
}

void ATramStationArea::ReleaseTram(ATram* tram)
{
	tram->SetMoveSpeed(leaveSpeed_);
}
