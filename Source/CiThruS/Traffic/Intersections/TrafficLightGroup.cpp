#include "TrafficLightGroup.h"
#include "Traffic/Areas/TrafficStopArea.h"
#include "IntersectionController.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MathUtility.h"

ATrafficLightGroup::ATrafficLightGroup()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATrafficLightGroup::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (timeRemainingInCurrentState_ <= 0 || (intersection_ != nullptr && lightState_ == ETrafficLightState::Red))
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

			// Stay deactivated if the next group continues to be green to
			// avoid activating for a split second and then immediately
			// deactivating again
			if (intersection_ != nullptr
				&& intersection_->WillDeactivateNext(stopArea))
			{
				continue;
			}

			stopArea->Activate();
		}

		break;

	case ETrafficLightState::Yellow:
		timeRemainingInCurrentState_ = yellowLightDuration_;

		for (ATrafficStopArea* stopArea : stopAreas_)
		{
			if (stopArea == nullptr)
			{
				continue;
			}

			// Stay deactivated if the next group continues to be green to
			// avoid activating for a split second and then immediately
			// deactivating again
			if (intersection_ != nullptr
				&& intersection_->WillDeactivateNext(stopArea))
			{
				continue;
			}

			stopArea->Activate();
		}

		break;

	case ETrafficLightState::RedYellow:
		timeRemainingInCurrentState_ = yellowLightDuration_;

		for (ATrafficStopArea* stopArea : stopAreas_)
		{
			if (stopArea == nullptr)
			{
				continue;
			}

			// Stay deactivated if the previous group was green to avoid
			// activating for a split second and then immediately
			// deactivating again
			if (intersection_ != nullptr
				&& intersection_->WasDeactivatedPreviously(stopArea))
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

			// Stay deactivated if the previous group was green to avoid
			// activating for a split second and then immediately
			// deactivating again
			if (intersection_ != nullptr
				&& intersection_->WasDeactivatedPreviously(stopArea))
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

		if (intersection_ != nullptr)
		{
			// Don't set traffic lights back to red if they will also go green in the next group.
			// It looks weird because they don't have any time to be red between groups
			if ((lightState_ == ETrafficLightState::Yellow
				|| lightState_ == ETrafficLightState::Red)
				&& intersection_->WillBeGreenNext(visualLight))
			{
				continue;
			}

			// If the above code kept the light green through the previous group, don't cycle
			// through RedYellow again, just stay green from the start of this group
			if ((lightState_ == ETrafficLightState::RedYellow
				|| lightState_ == ETrafficLightState::Green)
				&& intersection_->WasGreenPreviously(visualLight))
			{
				continue;
			}
		}

		visualLight->SetLightState(lightState_);
	}
}

bool ATrafficLightGroup::Contains(AVisualTrafficLight* trafficLight)
{
	return visualLights_.Contains(trafficLight);
}

bool ATrafficLightGroup::Contains(ATrafficStopArea* stopArea)
{
	return stopAreas_.Contains(stopArea);
}
