#include "PathFollower.h"
#include "../MathUtility.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

// General notes about this implementation:

// The reason there's comparisons between squared vector lengths
// is because it's slightly faster to calculate a squared vector
// length (omits the square root) and the order of squared lengths
// is still the same as regular lengths.

// The reason functions don't modify currentKeypoint_ and traversed_
// directly is so that GetPointOnPath can "scout" ahead on the path
// without telling the Pather to actually advance on the path.

void PathFollower::SetPath(const KeypointPath& path)
{
	path_ = path;
	currentKeypoint_ = 0;
	traversed_ = false;

	if (path_.keypoints.Num() == 0)
	{
		traversed_ = true;
	}
}

FVector PathFollower::GetDirectionOnPathAndAdvance(const FVector& currentPosition, const float& step)
{
	FVector targetPosition = GetPointOnPathInternal(currentPosition, step, currentKeypoint_, traversed_);

	return (targetPosition - currentPosition).GetSafeNormal();
}

FVector PathFollower::GetPointOnPath(const FVector& currentPosition, const float& step) const
{
	// Pass dummy variables to GetPointOnPathInternal to avoid changing currentKeypoint_ and traversed_
	int dummyKeypoint = currentKeypoint_;
	bool dummyTraversed = traversed_;

	return GetPointOnPathInternal(currentPosition, step, dummyKeypoint, dummyTraversed);
}

void PathFollower::AdvanceTarget()
{
	if (traversed_)
	{
		return;
	}

	if (path_.keypoints.Num() == 1)
	{
		traversed_ = true;

		return;
	}

	// Pass a dummy variable to UpdateKeypoint as the origin and target positions are not used
	FVector dummyVector;

	UpdateKeypoint(1.0f, currentKeypoint_, dummyVector, dummyVector, traversed_);
}

FVector PathFollower::GetCurrentOrigin() const
{
	if (currentKeypoint_ >= path_.keypoints.Num())
	{
		return FVector::ZeroVector;
	}

	return path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint_]);
}

FVector PathFollower::GetCurrentTarget() const
{
	if (currentKeypoint_ + 1 >= path_.keypoints.Num())
	{
		if (currentKeypoint_ >= path_.keypoints.Num())
		{
			return FVector::ZeroVector;
		}

		return path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint_]);
	}

	return path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint_ + 1]);
}

FVector PathFollower::GetPointOnPathInternal(const FVector& currentPosition, const float& step,
	int& currentKeypoint, bool& traversed) const
{
	// Check trivial cases
	if (path_.keypoints.Num() == 0 || step == 0)
	{
		return currentPosition;
	}

	float stepSquared = step * step;

	if (path_.keypoints.Num() == 1)
	{
		FVector currentTarget = path_.graph->GetKeypointPosition(path_.keypoints[0]);

		if ((currentTarget - currentPosition).SizeSquared() < stepSquared)
		{
			traversed = true;

			return currentPosition;
		}

		return currentTarget;
	}

	FVector currentOrigin;
	FVector currentTarget;

	// If step is negative, we're moving backwards so swap the origin and target positions
	if (step < 0)
	{
		currentOrigin = path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint + 1]);
		currentTarget = path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint]);
	}
	else
	{
		currentOrigin = path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint]);
		currentTarget = path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint + 1]);
	}

	FVector positionToTarget = currentTarget - currentPosition;

	// Increment current keypoint while step is large enough to reach the next keypoint
	while (positionToTarget.SizeSquared() <= stepSquared)
	{
		UpdateKeypoint(step, currentKeypoint, currentOrigin, currentTarget, traversed);

		if (traversed)
		{
			return currentPosition;
		}

		positionToTarget = currentTarget - currentPosition;
	}

	// Project position onto a line that is parallel to the current path segment
	FVector currentPathSegment = currentTarget - currentOrigin;
	FVector currentPathDirection = currentPathSegment.GetSafeNormal();
	FVector positionToPath = positionToTarget - positionToTarget.FVector::ProjectOnTo(currentPathDirection);
	FVector positionOnPath = currentPosition + positionToPath;

	// Clamp positionOnPath to current path segment's end points
	if ((currentTarget - positionOnPath).SizeSquared() > currentPathSegment.SizeSquared())
	{
		// currentPosition is behind currentOrigin, set the path point to currentOrigin
		positionToPath = currentOrigin - currentPosition;
		positionOnPath = currentOrigin;
	}
	else if ((currentOrigin - positionOnPath).SizeSquared() > currentPathSegment.SizeSquared())
	{
		// currentPosition is in front of currentTarget, set the path point to currentTarget
		positionToPath = currentTarget - currentPosition;
		positionOnPath = currentTarget;
	}

	// If step isn't large enough to reach the path, just move directly towards the path
	if (positionToPath.SizeSquared() >= stepSquared)
	{
		return currentPosition + positionToPath.GetSafeNormal() * FMath::Abs(step);
	}
	
	// If step reaches the path, move onto the path and advance on it so that the total distance moved is equal to step
	float pathStepSquared = stepSquared - positionToPath.SizeSquared();

	// This math was derived from the pythagorean theorem a^2 = c^2 - b^2
	while (stepSquared - positionToPath.SizeSquared() > (currentTarget - positionOnPath).SizeSquared())
	{
		UpdateKeypoint(step, currentKeypoint, currentOrigin, currentTarget, traversed);

		if (traversed)
		{
			return currentTarget;
		}

		currentPathDirection = (currentTarget - currentOrigin).GetSafeNormal();

		positionToTarget = currentTarget - currentPosition;
		positionToPath = positionToTarget - positionToTarget.FVector::ProjectOnTo(currentPathDirection);
		positionOnPath = currentPosition + positionToPath;
	}

	return positionOnPath + currentPathDirection * FMath::Sqrt(pathStepSquared);
}

void PathFollower::UpdateKeypoint(const float& step, int& currentKeypoint, FVector& currentOrigin, FVector& currentTarget, bool& traversed) const
{
	if (step < 0)
	{
		// Moving backward

		if (currentKeypoint != 0)
		{
			currentKeypoint--;
		}
		else
		{
			// No more keypoints on path
			traversed = true;

			return;
		}

		currentOrigin = currentTarget;
		currentTarget = path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint]);
	}
	else
	{
		// Moving forward

		if (currentKeypoint != path_.keypoints.Num() - 2)
		{
			currentKeypoint++;
		}
		else
		{
			// No more keypoints on path
			traversed = true;

			return;
		}

		currentOrigin = currentTarget;
		currentTarget = path_.graph->GetKeypointPosition(path_.keypoints[currentKeypoint + 1]);
	}
}
