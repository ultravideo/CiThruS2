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

void AIntersectionController::BeginPlay()
{
	Super::BeginPlay();
	
	for (ATrafficLightGroup* lightGroup : trafficLightGroups_)
	{
		lightGroup->ConnectToIntersectionController(this);
	}

	trafficLightGroups_[cyclingGroup_]->Cycle();
}
