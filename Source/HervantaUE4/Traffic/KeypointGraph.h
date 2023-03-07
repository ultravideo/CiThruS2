#pragma once

#include <random>
#include <vector>
#include <string>

#include "Math/Vector.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"

class KeypointGraph;

struct KeypointPath
{
	const KeypointGraph* graph;
	TArray<int> keypoints;
};

// Represents a generic graph made of keypoints connected by links. Can be saved to and loaded from files
class KeypointGraph
{
public:
	KeypointGraph() { };

	void AddKeypoint(const FVector& position);
	void RemoveKeypoint(const int& kpIndex);
	void LinkKeypoints(const int& firstKeypointIndex, const int& secondKeypointIndex);
	void ClearKeypoints();

	void SaveToFile(const std::string& filePath);
	void LoadFromFile(const std::string& filePath);

	void AlignWithWorldGround(UWorld* world);

	KeypointPath GetRandomPathFrom(const int& startKeypointIndex) const;

	// Gets a list of keypoint indices where each keypoint is guaranteed to
	// be at least margin units away from every other keypoint in the list.
	std::vector<int> GetKeypointsWithMargin(const float& margin) const;

	inline int KeypointCount() const { return keypoints_.size(); };
	inline FVector GetKeypointPosition(const int& keypointIndex) const { return keypoints_[keypointIndex].position; };
	int GetClosestKeypoint(const FVector& position) const;

	inline int LinkCount() const { return links_.size(); };
	inline std::pair<int, int> GetLinkKeypoints(const int& linkIndex) const { return links_[linkIndex]; };

	inline int SpawnPointCount() const { return spawnPoints_.size(); };
	inline int GetSpawnPoint(const int& spawnPointIndex) const { return spawnPoints_[spawnPointIndex]; };
	inline int GetRandomSpawnPoint() const { return spawnPoints_[FMath::RandRange(0, SpawnPointCount() - 1)]; };

protected:
	struct Keypoint
	{
		FVector position;

		// Contains keypoints that this keypoint leads into
		std::vector<int> outboundKeypoints;

		// Contains keypoints that lead into this keypoint
		std::vector<int> inboundKeypoints;
	};

	using Link = std::pair<int, int>;

	std::vector<Keypoint> keypoints_;
	std::vector<Link> links_;

	std::vector<int> entryPoints_;
	std::vector<int> spawnPoints_;

	void UpdateEntryPoints();
};
