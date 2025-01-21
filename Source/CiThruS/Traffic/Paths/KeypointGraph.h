#pragma once

#include "Math/Vector.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"

#include <random>
#include <vector>
#include <string>

class KeypointGraph;
class ATrafficController;

struct Keypoint;

struct KeypointPath
{
	const KeypointGraph* graph;
	TArray<int> keypoints;
	TArray<FVector> customPoints;

	void SetNewCustomPath(TArray<int> newIndexes, TArray<FVector> newCustomPoints)
	{
		keypoints = newIndexes;
		customPoints = newCustomPoints;
	}	

	FVector GetPointPosition(const int index) const;

};

// Enum to hold keypoint rules
UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EKeypointRules : uint8
{
	None = 0 UMETA(Hidden),
	ParkingAccessOnly = 1 << 0 UMETA(DisplayName = "Parking Access Only"),
	HeavyWeightLimited = 1 << 1 UMETA(DisplayName = "Limited Access for Heavy Vehicles"),
	NoAccess = 1 << 2 UMETA(DisplayName = "NO ACCESS ROAD")
	// Implement more rules here as needed.. RuleName = 1 << 3 UMETA(DisplayName = "Editor Name")
};

ENUM_CLASS_FLAGS(EKeypointRules)

struct Keypoint
{
	FVector position;

	// Contains keypoints that this keypoint leads into
	std::vector<int> outboundKeypoints;

	// Contains keypoints that lead into this keypoint
	std::vector<int> inboundKeypoints;

	// Contains the specific rules for this keypoint
	int32 rules;
};

// Represents a generic graph made of keypoints connected by links. Can be saved to and loaded from files. Used for traffic entity paths
class KeypointGraph
{
public:
	KeypointGraph() { };

	void AddKeypoint(const FVector& position);
	void RemoveKeypoint(const int& kpIndex);
	void LinkKeypoints(const int& firstKeypointIndex, const int& secondKeypointIndex);
	void MarkSpawnPoint(const int& keypointIndex);
	void OvertakeSetup(const int& lane1StartIndex, const int& lane1EndIndex, const int& lane2StartIndex, const int& lane2EndIndex);
	void SetKeypointRules(const int& kpIndex, const int32& rules);
	void ClearKeypoints();

	bool SaveToFile(const std::string& filePath);
	bool LoadFromFile(const std::string& filePath);

	void AlignWithWorldGround(UWorld* world);
	void ApplyZoneRules(ATrafficController* controller);

	// Get random paths. Only allow paths that comply with given rule exceptions. All rules bypassed by default.
	KeypointPath GetRandomPathFrom(const int& startKeypointIndex, const int32 ruleExceptions = MAX_int32) const;
	KeypointPath GetRandomPath(const int32 ruleExceptions = MAX_int32) const { return GetRandomPathFrom(FMath::RandRange(0, KeypointCount() - 1), ruleExceptions); };
	/* Pathing algorithm to find route from point A to B.Only allow paths that comply with given rule exceptions.All rules bypassed by default. */
	KeypointPath FindPath(const int& startKeypointIndex, const int& destination, const int32 ruleExceptions = MAX_int32) const;

	// Gets a list of keypoint indices where each keypoint is guaranteed to
	// be at least margin units away from every other keypoint in the list.
	std::vector<int> GetKeypointsWithMargin(const float& margin) const;
	// Remove keypoints from a list if they are too close to a position
	void RemoveKeypointsByRange(std::vector<int>& list, const FVector2D position, float range);

	inline int KeypointCount() const { return keypoints_.size(); };

	/**
	*	Note, that if relevant, you should use KeypointPath::GetPointPosition instead
	*   i.e. if an entity has custom (key)points, you need to use that
	*	@see KeypointPath, KeypointPath::GetPointPosition
	*/
	inline FVector GetKeypointPosition(const int& keypointIndex) const { return keypointIndex > KeypointCount() - 1 ? FVector() : keypoints_[keypointIndex].position; };
	FRotator GetKeypointRotation(const int& keypointIndex) const;
	int GetClosestKeypoint(const FVector& position) const;

	inline int LinkCount() const { return links_.size(); };
	inline std::pair<int, int> GetLinkKeypoints(const int& linkIndex) const { return links_[linkIndex]; };

	inline int SpawnPointCount() const { return spawnPoints_.size(); };
	inline int GetSpawnPoint(const int& spawnPointIndex) const { return spawnPoints_[spawnPointIndex]; };
	inline int GetRandomSpawnPoint() const { return spawnPoints_[FMath::RandRange(0, SpawnPointCount() - 1)]; };

	std::vector<int> GetInboundKeypoints(const int keypointIndex) const { return (keypointIndex >= 0 && keypointIndex < keypoints_.size()) ? keypoints_[keypointIndex].inboundKeypoints : std::vector<int>(); }
	std::vector<int> GetOutBoundKeypoints(const int keypointIndex) const { return (keypointIndex >= 0 && keypointIndex < keypoints_.size()) ? keypoints_[keypointIndex].outboundKeypoints : std::vector<int>(); }

	// Methods for getting & comparing keypoint rules
	int32 GetKeypointRules(const int keypointIndex) const { return keypoints_[keypointIndex].rules; }
	bool CompareRules(const Keypoint& keypoint, const int32 exceptions) const;
	bool CompareRules(const int keypointIndex, const int32 exceptions) const;
	// Find keypoints containing rules
	std::vector<Keypoint*> GetKeypointsByRules(std::vector<Keypoint>& from, const int32 exceptions);
	std::vector<Keypoint*> GetKeypointsByRules(const int32 exceptions) { return GetKeypointsByRules(keypoints_, exceptions); };

	/* If the given lane allows it, return the lane that is paired with the given lane for overtaking.
	if allowBothWays is true, overtaking from left lane to right is allowed. Returns (-1, -1) if overtaking not allowed in given lane */
	std::pair<int, int> GetOvertakePair(std::pair<int, int> currentLane, bool allowBothWays) const;
	std::vector< std::pair<std::pair<int, int>, std::pair<int, int>> > GetOvertakes() const { return overtakes_; };

protected:

	using Link = std::pair<int, int>;
	using OvertakeLaneLink = std::pair<std::pair<int, int>, std::pair<int, int>>;

	std::vector<Keypoint> keypoints_;
	std::vector<Link> links_;
	std::vector<OvertakeLaneLink> overtakes_;

	std::vector<int> entryPoints_;
	std::vector<int> spawnPoints_;

	void UpdateEntryPoints();

	// Remove any illegal keypoints from given list. Use this when comparing rules with a list of keypoints.
	void VerifyKeypoints(std::vector<int>& keypoints, const int32 ruleExceptions);
	// Same except returns a new list of keypoints
	std::vector<int> GetVerifiedKeypoints(std::vector<int> keypoints, const int32 ruleExceptions) const;
};
