#pragma once

#include "KeypointGraph.h"

#include "CoreMinimal.h"

// Guides a traffic entity along a KeypointPath, keeping track of which keypoints the entity has passed so far.
// Only tells the entity where to go but not the exact shape of the path it has to take. Useful for pedestrians
class CITHRUS_API FreePathFollower
{
public:
	FreePathFollower() { }

	void Initialize(const KeypointGraph* graph, AActor* trafficEntity, const FVector& locationOffset);

	void AdvanceTarget();

	// Get current location on path
	inline FVector GetLocation() { return GetLocationAt(currentPoint_); }

	void NewPath(const int& startKeypoint, const int& targetKeypoint); // Get path from point A to B
	void NewPath(const bool& fromNearestKeypoint); // Get a random path from current location or a random spawnpoint

	// Is the path ending on next target?
	inline bool IsLastTarget() const { return (PointCount() - 1) == currentPoint_; }

protected:
	KeypointPath path_;
	AActor* trafficEntity_; // Entity controlled by this PathFollower

	int currentPoint_;
	FVector locationOffset_;

	FVector GetLocationAt(const int& target) const;

	inline FVector GetPosition(const int& index) const { return path_.GetPointPosition(index); }
	inline int PointCount() const { return path_.keypoints.Num(); }
};
