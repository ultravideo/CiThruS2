#include "IntersectionController.h"
#include "TrafficLightGroup.h"

void AIntersectionController::CycleFinished()
{
	cyclingGroup_++;

	if (cyclingGroup_ == trafficLightGroups_.Num())
	{
		cyclingGroup_ = 0;
	}

	trafficLightGroups_[cyclingGroup_]->Cycle();
}

void AIntersectionController::ResetLights()
{
	// Set all lights to red & start cycle again from current cycle group
	for (ATrafficLightGroup* g : trafficLightGroups_)
	{
		g->SetLightState(ETrafficLightState::Red);
		
	}
	trafficLightGroups_[cyclingGroup_]->SetLightState(ETrafficLightState::Green);
	trafficLightGroups_[cyclingGroup_]->Cycle();
}

bool AIntersectionController::WillBeGreenNext(AVisualTrafficLight* trafficLight) const
{
	int nextCyclingGroup = cyclingGroup_ + 1;

	if (nextCyclingGroup == trafficLightGroups_.Num())
	{
		nextCyclingGroup = 0;
	}

	return trafficLightGroups_[nextCyclingGroup]->Contains(trafficLight);
}

bool AIntersectionController::WasGreenPreviously(AVisualTrafficLight* trafficLight) const
{
	int prevCyclingGroup = cyclingGroup_ - 1;

	if (prevCyclingGroup == -1)
	{
		prevCyclingGroup = trafficLightGroups_.Num() - 1;
	}

	return trafficLightGroups_[prevCyclingGroup]->Contains(trafficLight);
}

bool AIntersectionController::WillDeactivateNext(ATrafficStopArea* stopArea) const
{
	int nextCyclingGroup = cyclingGroup_ + 1;

	if (nextCyclingGroup == trafficLightGroups_.Num())
	{
		nextCyclingGroup = 0;
	}

	return trafficLightGroups_[nextCyclingGroup]->Contains(stopArea);
}

bool AIntersectionController::WasDeactivatedPreviously(ATrafficStopArea* stopArea) const
{
	int prevCyclingGroup = cyclingGroup_ - 1;

	if (prevCyclingGroup == -1)
	{
		prevCyclingGroup = trafficLightGroups_.Num() - 1;
	}

	return trafficLightGroups_[prevCyclingGroup]->Contains(stopArea);
}

void AIntersectionController::BeginPlay()
{
	Super::BeginPlay();
	
	if (trafficLightGroups_.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("IntersectionController has no trafficLightGroups set!"));
		Destroy();
		return;
	}

	for (ATrafficLightGroup* lightGroup : trafficLightGroups_)
	{
		lightGroup->ConnectToIntersectionController(this);
	}

	trafficLightGroups_[cyclingGroup_]->Cycle();
}
