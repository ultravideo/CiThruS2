#include "CurvePathFollower.h"
#include "Misc/MathUtility.h"
#include "Misc/Debug.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Traffic/Entities/Car.h"
#include "Traffic/Parking/ParkingController.h"
#include "CurveFactory.h"
#include "Traffic/Parking/ParkingSpace.h"
#include <iostream>

void CurvePathFollower::Initialize(const KeypointGraph* graph, AActor* trafficEntity, bool makeSCurve)
{
	makeSCurve_ = makeSCurve;
	trafficEntity_ = trafficEntity;
	path_ = KeypointPath();
	path_.graph = graph;



	if (trafficEntity->GetActorLocation().IsZero()) // Position not set, spawn to any random keypoint
	{
		FirstTimeSpawn();
	}
	else // Position already set, create path from nearest keypoint
	{
		NewPath(true);	
	}
}

void CurvePathFollower::Advance(const float& step)
{
	if (path_.graph == nullptr)
	{
		return;
	}

	AdvancePointAndProgress(currentPoint_, progressToNextPoint_, currentCurve_, step);
	GetTurnAmount(progressToNextPoint_);

	if (currentPoint_ >= PointCount() - 1)
	{
		// End car parking sequence, if entity is car & is currently in park sequence
		if (ACar* car = Cast<ACar>(trafficEntity_))
		{
			if (car->spaceBeingParkedTo_ != nullptr)
			{
				car->spaceBeingParkedTo_->FinishParking();
			}

			car->spaceBeingParkedTo_ = nullptr;
			car->reverseUntilPoint = -1;
			car->reverseFromPoint = -1;
		}

		// Start a new path from the current keypoint if there are outbound keypoints, otherwise
		// teleport to a new spawnpoint (teleport to a spawnpoint or get deleted & spawn new car from a parking space)
		NewPath(path_.graph->GetOutBoundKeypoints(path_.keypoints.Last()).size() != 0);	
	}
}

FVector CurvePathFollower::GetLocationAt(const int& point, const float& progress, FVector& tangent, std::shared_ptr<ICurve> curve) const
{
	//Invalid cases
	if (curve == nullptr || PointCount() < 2)
	{
		tangent = FVector::UnitX();
		return FVector::ZeroVector;
	}

	if (point >= PointCount() - 1)
	{
		tangent = GetTangent(PointCount() - 1);
		return GetPosition(PointCount() - 1);
	}

	if (point < 0)
	{
		tangent = GetTangent(0);
		return GetPosition(0);
	}

	FVector position;

	curve->GetPositionAndTangent(progress * curve->GetLength(), position, tangent);

	return position;
}

FVector CurvePathFollower::GetLocationAt(const int& target, const float& progress, FVector& tangent, const float& step) const
{
	int dummyTarget = target;
	float dummyProgress = progress;

	if (currentCurve_ != nullptr)
	{
		std::shared_ptr<ICurve> dummyCurve = currentCurve_->Clone();
		AdvancePointAndProgress(dummyTarget, dummyProgress, dummyCurve, step);
		return GetLocationAt(dummyTarget, dummyProgress, tangent, dummyCurve);
	}
	else
	{
		return GetLocationAt(dummyTarget, dummyProgress, tangent, nullptr);
	}
}

void CurvePathFollower::AdvancePointAndProgress(int& point, float& progress, std::shared_ptr<ICurve>& curve, float distance) const
{	
	const KeypointGraph* graph = path_.graph;

	if (graph == nullptr)
	{
		return;
	}
	
	int pointNum = PointCount();

	if (pointNum < 2 || point >= pointNum - 1 || point < 0)
	{
		return;
	}

	if (distance > 0)
	{
		while (distance > 0 && point < pointNum - 1)
		{
			float fullDistance = curve->GetLength();
			float distanceToTarget = (1 - progress) * fullDistance;

			if (distance >= distanceToTarget)
			{
				distance -= distanceToTarget;
				point++;
				progress = 0;

				curve = CreateCurveFromPoint(point);
			}
			else
			{
				progress += distance / fullDistance;
				return;
			}
		}
	}
	else if (distance < 0) // moving backwards?
	{
		while (distance < 0 && point > 0)
		{
			float fullDistance = curve->GetLength();
			float distanceToTarget = progress * fullDistance;

			if (-distance >= distanceToTarget)
			{
				distance += distanceToTarget;
				point--;
				progress = 1;

				curve = CreateCurveFromPoint(point);
			}
			else
			{
				progress += distance / fullDistance;
				return;
			}
		}
	}
}

// Set a new path from one point to another known point
// Note: startKeypoint & targetKeypoint are indexes of the global graph(i.e from roadGraph.data), not the local path indexes
// Serves the same purpose as KeypointGraph.FindPath, but locally, for this entity
void CurvePathFollower::NewPath(const int& startKeypoint, const int& targetKeypoint)
{
	const KeypointGraph* graph = path_.graph;
	path_ = graph->FindPath(startKeypoint, targetKeypoint);
	trafficEntity_->SetActorLocation(GetLocation());
	progressToNextPoint_ = 0.0f;
	currentPoint_ = 0;
}

// Set a new randomized path from a random startPoint or nearest keypoint
void CurvePathFollower::NewPath(const bool& fromNearestKeypoint)
{
	const KeypointGraph* graph = path_.graph;
	if (graph == nullptr)
	{
		return;
	}

	if (fromNearestKeypoint) // the entity probably shouldn't move if we want to create a path from nearest keypoint (it's already close to it, so just create a custom path to that keypoint)
	{
		int closestIndex = graph->GetClosestKeypoint(trafficEntity_->GetActorLocation());
		std::vector<int> outbounds = graph->GetOutBoundKeypoints(closestIndex);

		// We need to start from a random outbound keypoint, to prevent car going backwards
		path_ = graph->GetRandomPathFrom(graph->GetClosestKeypoint(trafficEntity_->GetActorLocation()), GetKeypointRuleExceptions());

		if (outbounds.size() != 0 && path_.keypoints.Num() > 1) // The new path should not end immediately
		{
			// Add a custom starting point, unless car already on a keypoint
			if (!trafficEntity_->GetActorLocation().Equals(path_.GetPointPosition(0), 10))
			{
				path_.customPoints.Add(trafficEntity_->GetActorLocation());
				path_.keypoints.Insert(-8, 0);
			}

			progressToNextPoint_ = 0.0f;
			currentPoint_ = 0;

			currentCurve_ = CreateCurveFromPoint(currentPoint_);

			return;
		}
	}
	
	if (ACar* car = Cast<ACar>(trafficEntity_))
	{
		// (for cars) Since we're not continuing from nearest point,
		// either spawn to a spawnpoint or a parking space 
		car->GetController()->RespawnCar(car);
	}
	else
	{	
		// Failed to find,
		//  or don't want to find a path continuing from nearest keypoint
		//		=> So get a new path from a random spawnpoint
		path_ = graph->GetRandomPathFrom(graph->GetRandomSpawnPoint(), GetKeypointRuleExceptions());	

		progressToNextPoint_ = 0.0f;
		currentPoint_ = 0;

		currentCurve_ = CreateCurveFromPoint(currentPoint_);
		trafficEntity_->SetActorLocation(GetLocation());		
	}
}

void CurvePathFollower::SetPointAndProgress(int point, float progress)
{
	currentPoint_ = point;
	progressToNextPoint_ = progress;

	currentCurve_ = CreateCurveFromPoint(currentPoint_);
}

void CurvePathFollower::FirstTimeSpawn()
{
	const KeypointGraph* graph = path_.graph;

	if (graph == nullptr)
	{
		return;
	}

	path_ = graph->GetRandomPath(GetKeypointRuleExceptions());

	currentPoint_ = 0;
	// Randomize spawn point between the first 2 points
	progressToNextPoint_ = FMath::RandRange(0.0f, 1.0f);

	currentCurve_ = CreateCurveFromPoint(currentPoint_);
	trafficEntity_->SetActorLocation(GetLocation());
}

FVector CurvePathFollower::GetPosition(const int& index) const
{
	// Calculate the position in the middle of the two keypoints around the index
	if (index <= 0)
	{
		return path_.GetPointPosition(0);
	}

	if (index >= path_.keypoints.Num())
	{
		return path_.GetPointPosition(path_.keypoints.Num() - 1);
	}

	return (path_.GetPointPosition(index) + path_.GetPointPosition(index - 1)) * 0.5f;
}

// Gets the tangent (vector) between KeypointPath[index] and KeypointPath[index - 1]
FVector CurvePathFollower::GetTangent(const int& index) const
{
	// Calculate the tangent between the two keypoints around the index
	if (path_.keypoints.Num() < 2)
	{
		return FVector::UnitX();
	}

	if (index <= 0)
	{
		return (path_.GetPointPosition(1) - path_.GetPointPosition(0)).GetSafeNormal();
	}

	if (index >= path_.keypoints.Num())
	{
		return (path_.GetPointPosition(path_.keypoints.Num() - 1) - path_.GetPointPosition(path_.keypoints.Num() - 2)).GetSafeNormal();
	}

	return (path_.GetPointPosition(index) - path_.GetPointPosition(index - 1)).GetSafeNormal();
}

std::shared_ptr<ICurve> CurvePathFollower::CreateCurveFromPoint(const int& point) const
{
	if (PointCount() < 2)
	{
		return nullptr;
	}

	int currentPoint = point;
	int nextPoint = point + 1;

	if (currentPoint < 0)
	{
		currentPoint = 0;
		nextPoint = 1;
	}

	if (nextPoint > PointCount() - 1)
	{
		currentPoint = PointCount() - 2;
		nextPoint =  PointCount() - 1;
	}

	FVector startPosition = GetPosition(currentPoint);
	FVector endPosition = GetPosition(nextPoint);
	FVector startPointTangent = GetTangent(currentPoint);
	FVector endPointTangent = GetTangent(nextPoint);

/* 	
	UWorld *world;
	if (trafficEntity_) world = trafficEntity_->GetWorld();
	else UE_LOG(LogTemp, Warning, TEXT("ALERT! trafficEntity pointer is null!"));
 */

	return CurveFactory::GetCurveFor(/* trafficEntity_->GetWorld() , */startPosition, endPosition, startPointTangent, endPointTangent, makeSCurve_);
}

int32 CurvePathFollower::GetKeypointRuleExceptions() const
{
	//TODO: Probably a better way to do this. Only works for cars that use CurvePathFollower, not implemented elsewhere
	// ITrafficEntity contains a method GetKeypointRuleExceptions() that is used
	//if (ITrafficEntity* i = Cast<ITrafficEntity>(trafficEntity_)) { return i->GetKeypointRuleExceptions(); } <-- doesn't work
	if (ACar* car = Cast<ACar>(trafficEntity_))
	{
		return car->GetKeypointRuleExceptions();
	}

	return 0;
}

float CurvePathFollower::GetTurnAmount(const float progress) const
{
	//float asdf = currentCurve_->GetCurvatureAt(step);
	//UE_LOG(LogTemp, Warning, TEXT("HEP! %f"), asdf);
	if (currentCurve_) return currentCurve_->GetCurvatureAt(currentCurve_->GetLength() * progress);
	else return 0.0f;
}