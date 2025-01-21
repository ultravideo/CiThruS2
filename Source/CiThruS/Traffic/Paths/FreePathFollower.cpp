#include "FreePathFollower.h"
#include "Misc/MathUtility.h"
#include "Misc/Debug.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Traffic/Entities/Car.h"
#include "CurveFactory.h"

void FreePathFollower::Initialize(const KeypointGraph* graph, AActor* trafficEntity, const FVector& locationOffset)
{
	trafficEntity_ = trafficEntity;
	path_ = KeypointPath();
	path_.graph = graph;
	locationOffset_ = locationOffset;

	NewPath(true);
}

void FreePathFollower::AdvanceTarget()
{
	if (++currentPoint_ >= PointCount() - 1)
	{
		if (path_.graph != nullptr)
		{
			// Start a new path from the current keypoint if there are outbound keypoints, otherwise
			// teleport to a new spawnpoint
			NewPath(path_.graph->GetOutBoundKeypoints(path_.keypoints.Last()).size() != 0);
		}
	}
}

FVector FreePathFollower::GetLocationAt(const int& point) const
{
	//Invalid cases
	if (PointCount() < 2)
	{
		return FVector::ZeroVector;
	}

	if (point >= PointCount() - 1)
	{
		return GetPosition(PointCount() - 1) + locationOffset_;
	}

	if (point < 0)
	{
		return GetPosition(0) + locationOffset_;
	}

	return GetPosition(point) + locationOffset_;
}

// Set a new path from one point to another known point
// Note: startKeypoint & targetKeypoint are indexes of the global graph(i.e from roadGraph.data), not the local path indexes
// Serves the same purpose as KeypointGraph.FindPath, but locally, for this entity
void FreePathFollower::NewPath(const int& startKeypoint, const int& targetKeypoint)
{
	const KeypointGraph* graph = path_.graph;
	path_ = graph->FindPath(startKeypoint, targetKeypoint);
	trafficEntity_->SetActorLocation(GetLocation() + locationOffset_);
	currentPoint_ = 0;
}

// Set a new randomized path from a random startPoint or nearest keypoint
void FreePathFollower::NewPath(const bool& fromNearestKeypoint)
{
	const KeypointGraph* graph = path_.graph;
	if (graph == nullptr)
	{
		return;
	}

	if (fromNearestKeypoint)
	{
		path_ = graph->GetRandomPathFrom(graph->GetClosestKeypoint(trafficEntity_->GetActorLocation()));
	}
	else
	{	
		// Teleport to allocated spawn point
		path_ = graph->GetRandomPathFrom(graph->GetRandomSpawnPoint());	
	}

	currentPoint_ = 0;

	trafficEntity_->SetActorLocation(GetLocation() + locationOffset_);
}
