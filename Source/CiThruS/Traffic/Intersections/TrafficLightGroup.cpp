#include "TrafficLightGroup.h"
#include "Traffic/Areas/TrafficStopArea.h"
#include "IntersectionController.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MathUtility.h"
#include "TrafficLightGroup.h"

ATrafficLightGroup::ATrafficLightGroup()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATrafficLightGroup::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (timeRemainingInCurrentState_ <= 0 || intersection_ != nullptr && lightState_ == ETrafficLightState::Red)
	{
		return;
	}

	timeRemainingInCurrentState_ -= deltaTime;

	// Cycle state when timer expires
	if (timeRemainingInCurrentState_ <= 0)
	{
		switch (lightState_)
		{
		case ETrafficLightState::Red:
			SetLightState(ETrafficLightState::RedYellow);
			break;

		case ETrafficLightState::Yellow:
			SetLightState(ETrafficLightState::Red);

			if (intersection_ != nullptr)
			{
				intersection_->CycleFinished();
			}

			break;

		case ETrafficLightState::RedYellow:
			SetLightState(ETrafficLightState::Green);
			break;

		case ETrafficLightState::Green:
			SetLightState(ETrafficLightState::Yellow);
			break;
		}
	}
}

void ATrafficLightGroup::BeginPlay()
{
	Super::BeginPlay();

	if (intersection_ == nullptr)
	{
		SetLightState(ETrafficLightState::Red);
	}
}

bool ATrafficLightGroup::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ATrafficLightGroup::ConnectToIntersectionController(AIntersectionController* intersection)
{
	intersection_ = intersection;

	SetLightState(ETrafficLightState::Red);
}

void ATrafficLightGroup::Cycle()
{
	if (intersection_ == nullptr)
	{
		return;
	}

	SetLightState(ETrafficLightState::RedYellow);
}

void ATrafficLightGroup::SetLightState(const ETrafficLightState& newState)
{
	if (newState == lightState_)
	{
		return;
	}

	lightState_ = newState;

	switch (lightState_)
	{
	case ETrafficLightState::Red:
		timeRemainingInCurrentState_ = greenLightDuration_;

		for (ATrafficStopArea* stopArea : stopAreas_)
		{
			if (stopArea == nullptr)
			{
				continue;
			}

			stopArea->Activate();
		}

		break;

	case ETrafficLightState::Yellow:
	case ETrafficLightState::RedYellow:
		timeRemainingInCurrentState_ = yellowLightDuration_;

		for (ATrafficStopArea* stopArea : stopAreas_)
		{
			if (stopArea == nullptr)
			{
				continue;
			}

			stopArea->Activate();
		}

		break;

	case ETrafficLightState::Green:
		timeRemainingInCurrentState_ = greenLightDuration_;

		for (ATrafficStopArea* stopArea : stopAreas_)
		{
			if (stopArea == nullptr)
			{
				continue;
			}

			stopArea->Deactivate();
		}

		break;
	}

	for (AVisualTrafficLight* visualLight : visualLights_)
	{
		if (visualLight == nullptr)
		{
			continue;
		}

		visualLight->SetLightState(lightState_);
	}
}
