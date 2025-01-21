#include "TramIntersectionController.h"

ATramIntersectionController::ATramIntersectionController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAllowTickBeforeBeginPlay = false;
}

// Called when the game starts or when spawned
void ATramIntersectionController::BeginPlay()
{
	Super::BeginPlay();

	// Check validity
	for (int i = 0; i < lightGroups_.Num(); i++)
	{
		if (lightGroups_[i] == nullptr)
		{
			FString myName = GetName();
			UE_LOG(LogTemp, Warning, TEXT("Tramlightgroups not assigned in %s"), *myName);
			PrimaryActorTick.bCanEverTick = false;
			break;
		}
	}
}

void ATramIntersectionController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (ATramLightArea* g : lightGroups_)
	{
		TArray<ATrafficLightGroup*> groupA = g->GetLights();
		TArray<ATrafficLightGroup*> groupB = g->GetClearLights();

		for (ATrafficLightGroup* tlg : groupA)
		{
			tlg->SetLightState(ETrafficLightState::Green);
		}

		for (ATrafficLightGroup* tlg : groupB)
		{
			tlg->SetLightState(ETrafficLightState::Green);
		}
	}

	int activeAreas = 0;

	for (ATramLightArea* g : lightGroups_)
	{
		if (g->GetState() != TramLightAreaState::GREEN)
		{
			activeAreas++;
		}
	}

	if (activeAreas == 0)
	{
		return;
	}

	for (ATramLightArea* g : lightGroups_)
	{
		if (g->GetState() == TramLightAreaState::GREEN)
		{
			continue;
		}

		ETrafficLightState tls = g->GetState() == TramLightAreaState::YELLOW ?
								 ETrafficLightState::Yellow : ETrafficLightState::Red;

		TArray<ATrafficLightGroup*> groupA = g->GetLights();
		TArray<ATrafficLightGroup*> groupB = g->GetClearLights();

		for (ATrafficLightGroup* tlg : groupA)
		{
			tlg->SetLightState(tls);
		}

		if (activeAreas == 1)
		{
			for (ATrafficLightGroup* tlg : groupB)
			{
				tlg->SetLightState(tls);
			}
		}
	}
}
