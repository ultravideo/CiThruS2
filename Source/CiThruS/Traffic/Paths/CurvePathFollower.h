#pragma once

#include "KeypointGraph.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <memory>

class ICurve;

// Guides a traffic entity along a KeypointPath, keeping track of which keypoints the entity has passed so far.
// Generates a smooth, continuous curve that the entity can stick to. Useful for wheeled vehicles
class CITHRUS_API CurvePathFollower
{
public:
	CurvePathFollower() { }
	~CurvePathFollower() { }

	void Initialize(const KeypointGraph* graph, AActor* trafficEntity, bool makeSCurve = true);

	// Advance on path by <step>
	void Advance(const float& step);

	// Get current location and tangent on path, or a location offset by <step>
	inline FVector GetLocation(FVector& tangent) { return GetLocationAt(currentPoint_, progressToNextPoint_, tangent, currentCurve_); }
	inline FVector GetLocation(const float& step, FVector& tangent) { return GetLocationAt(currentPoint_, progressToNextPoint_, tangent, step); }
	inline FVector GetLocation() { FVector dummyTangent; return GetLocation(dummyTangent); }
	inline FVector GetLocation(const float& step) { FVector dummyTangent; return GetLocation(step, dummyTangent); }

	float GetTurnAmount(const float progress) const;
	float GetTurnAmount() const { return GetTurnAmount(progressToNextPoint_); };

	// Set a new path from one point to another known point
	// Note: startKeypoint & targetKeypoint are indexes of the global graph(i.e from roadGraph.data), not the local path indexes
	// Serves the same purpose as KeypointGraph.FindPath, but locally, for this entity
	void NewPath(const int& startKeypoint, const int& targetKeypoint); // Get path from point A to B
	void NewPath(const bool& fromNearestKeypoint); // Get a random path from current location or a random spawnpoint

	// Getters
	inline KeypointPath& GetPath() { return path_; }
	inline int GetCurrentLocalPoint() { return currentPoint_; }
	inline float GetProgress() { return progressToNextPoint_; }

	/* Set new path with custom point positions. "customPoints" are positions that do not exist as keypoints. They must be indexed starting from -8 and going down(-8, -9...).
	"indexes" should contain all keypoint indexes & custom point indexes of the path. Note: use Car::ApplyCustomPath if trying to set custom path for cars.*/
	inline void SetNewCustomPath(TArray<int> indexes, TArray<FVector> customPoints = TArray<FVector>()) { path_.SetNewCustomPath(indexes, customPoints); }
	void SetPointAndProgress(int point, float progress);

	// Is the path ending on next target?
	inline bool IsLastTarget() const { return (PointCount() - 1) == currentPoint_; }

protected:
	KeypointPath path_;
	AActor* trafficEntity_; // Entity controlled by this PathFollower
	std::shared_ptr<ICurve> currentCurve_; // The curve representing the current path segment

	int currentPoint_ = 0;
	float progressToNextPoint_ = 0.0f;

	bool makeSCurve_ = true;

	void AdvancePointAndProgress(int& startPoint, float& startProgress, std::shared_ptr<ICurve>& curve, float distance) const;

	FVector GetLocationAt(const int& target, const float& progress, FVector& tangent, std::shared_ptr<ICurve> curve) const;
	FVector GetLocationAt(const int& target, const float& progress, FVector& tangent, const float& step) const;

	// Note: These are NOT the actual keypoints of the path, but instead points in the middle of each connected pair of keypoints.
	// This is needed to ensure good tangents for curves
	FVector GetPosition(const int& index) const;
	FVector GetTangent(const int& index) const;
	inline int PointCount() const { return path_.keypoints.Num() + 1; }

	std::shared_ptr<ICurve> CreateCurveFromPoint(const int& point) const;

	// First time spawn should be random so the cars spread as much as possible, after that start spawning to
	// allocated spawn points when dead end has been reached
	void FirstTimeSpawn();

	int32 GetKeypointRuleExceptions() const;
};
