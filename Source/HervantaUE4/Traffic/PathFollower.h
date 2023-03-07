#pragma once

#include "KeypointGraph.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <vector>
#include <tuple>

// Guides an entity along a KeypointPath, keeping track of which keypoints the entity has passed so far
class HERVANTAUE4_API PathFollower
{
public:
	PathFollower() { };

	void SetPath(const KeypointPath& path);

	// Calculates which way an entity at the given position should go when following the path and when it's expected to move step units during its next move.
	// Also internally tracks which keypoints the path is being followed at
	FVector GetDirectionOnPathAndAdvance(const FVector& currentPosition, const float& step);

	// Calculates the furthest ahead point on the path that is step units away from currentPosition or closest to the path if the path is further away than step
	FVector GetPointOnPath(const FVector& currentPosition, const float& step) const;

	void AdvanceTarget();

	FVector GetCurrentOrigin() const;
	FVector GetCurrentTarget() const;
	inline bool FullyTraversed() const { return traversed_; };

protected:
	KeypointPath path_;

	int currentKeypoint_;
	bool traversed_;

	FVector GetPointOnPathInternal(const FVector& currentPosition, const float& step, int& currentKeypoint, bool& traversed) const;

	// Increments or decrements currentKeypoint based on step and returns true if there are no more keypoints left
	void UpdateKeypoint(const float& step, int& currentKeypoint, FVector& currentOrigin, FVector& currentTarget, bool& traversed) const;
};
