#include "KeypointGraph.h"
#include "Misc/Debug.h"
#include "Traffic/TrafficController.h"

#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"

#include <utility>
#include <iostream>
#include <fstream>

#define RANDOM_PATH_MAX_LENGTH 300

void KeypointGraph::AddKeypoint(const FVector& position)
{
	keypoints_.push_back({ position });
}

void KeypointGraph::RemoveKeypoint(const int& kpIndex)
{
	// Remove links that reference this keypoint
	for (auto it = links_.begin(); it != links_.end();)
	{
		// Remove the keypoint index from the inbound keypoint lists of other keypoints
		if (it->first == kpIndex)
		{
			std::vector<int>& otherKpList = keypoints_[it->second].inboundKeypoints;

			for (auto it2 = otherKpList.begin(); it2 != otherKpList.end(); it2++)
			{
				if (*it2 == kpIndex)
				{
					otherKpList.erase(it2);
					break;
				}
			}

			it = links_.erase(it);
			continue;
		}

		// Remove the keypoint index from the outbound keypoint lists of other keypoints
		if (it->second == kpIndex)
		{
			std::vector<int>& otherKpList = keypoints_[it->first].outboundKeypoints;

			for (auto it2 = otherKpList.begin(); it2 != otherKpList.end(); it2++)
			{
				if (*it2 == kpIndex)
				{
					otherKpList.erase(it2);
					break;
				}
			}

			it = links_.erase(it);
			continue;
		}

		it++;
	}

	keypoints_.erase(keypoints_.begin() + kpIndex);
}

void KeypointGraph::LinkKeypoints(const int& firstKeypointIndex, const int& secondKeypointIndex)
{
	links_.push_back(Link{ firstKeypointIndex, secondKeypointIndex });

	keypoints_[firstKeypointIndex].outboundKeypoints.push_back(secondKeypointIndex);
	keypoints_[secondKeypointIndex].inboundKeypoints.push_back(firstKeypointIndex);
}

void KeypointGraph::MarkSpawnPoint(const int& keypointIndex)
{
	spawnPoints_.push_back(keypointIndex);
}

void KeypointGraph::OvertakeSetup(const int& lane1StartIndex, const int& lane1EndIndex, const int& lane2StartIndex, const int& lane2EndIndex)
{
	overtakes_.push_back(OvertakeLaneLink{ std::make_pair(lane1StartIndex, lane1EndIndex), std::make_pair(lane2StartIndex, lane2EndIndex)});
}

void KeypointGraph::SetKeypointRules(const int& kpIndex, const int32& rules)
{
	if (keypoints_.size() <= kpIndex)
	{
		return;
	}

	keypoints_[kpIndex].rules = rules;
}

void KeypointGraph::ClearKeypoints()
{
	keypoints_.clear();
	links_.clear();
	overtakes_.clear();
	entryPoints_.clear();
	spawnPoints_.clear();
}

bool KeypointGraph::SaveToFile(const std::string& filePath)
{
	std::ofstream outfile(filePath);

	if (!outfile.is_open())
	{
		std::cout << "couldn't open file " << filePath << std::endl;
		return false;
	}

	outfile << "keypoints\n";

	for (int i = 0; i < keypoints_.size(); i++)
	{
		outfile
			<< i << ":"
			<< keypoints_[i].position.X << ","
			<< keypoints_[i].position.Y << ","
			<< keypoints_[i].position.Z << "\n";
	}

	outfile << "links\n";

	for (int i = 0; i < links_.size(); i++)
	{
		outfile << links_[i].first << "," << links_[i].second << "\n";
	}

	outfile << "spawnpoints\n";

	for (int i = 0; i < spawnPoints_.size(); i++)
	{
		outfile << spawnPoints_[i] << "\n";
	}

	outfile << "overtakelanes ### format is \"rightLaneStart,rightLaneEnd;leftLaneStart,leftLaneEnd\" in keypoint indexes ###\n";

	for (int i = 0; i < overtakes_.size(); i++)
	{
		outfile << overtakes_[i].first.first << ',' << overtakes_[i].first.second << ';' << overtakes_[i].second.first << ',' << overtakes_[i].second.second << "\n";
	}

	outfile << "rules\n";

	for (int i = 0; i < keypoints_.size(); i++)
	{
		Keypoint kp = keypoints_[i];
		if (kp.rules != 0)
		{
			outfile << i << ":" << kp.rules << "\n";
		}
	}

	outfile.close();

	return true;
}

bool KeypointGraph::LoadFromFile(const std::string& filePath)
{
	std::ifstream inFile(filePath);

	if (!inFile.is_open())
	{
		std::cout << "couldn't open file " << filePath << std::endl;
		return false;
	}

	ClearKeypoints();

	std::string line;

	// 0, 1, 2, 3
	int readKpsLinksSpawnpoints = 0;

	while (std::getline(inFile, line))
	{
		if (line.starts_with("keypoints"))
		{
			readKpsLinksSpawnpoints = 0;
			continue;
		}
		if (line.starts_with("links"))
		{
			readKpsLinksSpawnpoints = 1;
			continue;
		}
		if (line.starts_with("spawnpoints"))
		{
			readKpsLinksSpawnpoints = 2;
			continue;
		}
		if (line.starts_with("overtakelanes"))
		{
			readKpsLinksSpawnpoints = 3;
			continue;
		}
		if (line.starts_with("rules"))
		{
			readKpsLinksSpawnpoints = 4;
			continue;
		}

		const FString fline(line.c_str());

		// keypoints
		if (readKpsLinksSpawnpoints == 0)
		{
			// read x, y, z position
			FString index, values;
			fline.Split(":", &index, &values);

			FString x, y, z, temp;
			values.Split(",", &x, &y);

			FString yremainder = y;
			yremainder.Split(",", &y, &z);

			FString zremainder = z;
			zremainder.Split(",", &z, &temp);

			const float xf = FCString::Atof(*x);
			const float yf = FCString::Atof(*y);
			const float zf = FCString::Atof(*z);

			AddKeypoint(FVector(xf, yf, zf));
		}
		// links
		else if (readKpsLinksSpawnpoints == 1)
		{
			FString begin, end;
			fline.Split(",", &begin, &end);

			const int beginI = FCString::Atoi(*begin);
			const int endI = FCString::Atoi(*end);

			LinkKeypoints(beginI, endI);
		}
		// spawnpoints
		else if (readKpsLinksSpawnpoints == 2)
		{
			const int sPointId = FCString::Atoi(*fline);
			spawnPoints_.push_back(sPointId);
		}
		//overtake lanes
		else if (readKpsLinksSpawnpoints == 3)
		{
			// read laness start & end ids
			FString a1, a2, b1, b2, temp;
			fline.Split(",", &a1, &temp);
			temp.Split(";", &a2, &temp);
			temp.Split(",", &b1, &b2);

			const int i1 = FCString::Atof(*a1);
			const int i2 = FCString::Atof(*a2);
			const int j1 = FCString::Atof(*b1);
			const int j2 = FCString::Atof(*b2);

			OvertakeSetup(i1, i2, j1, j2);
		}
		//keypoint rules
		else if (readKpsLinksSpawnpoints == 4)
		{
			FString index, rules;
			fline.Split(":", &index, &rules);

			const int i = FCString::Atof(*index);
			const int32 r = FCString::Atof(*rules);

			SetKeypointRules(i, r);
		}
	}

	inFile.close();

	UpdateEntryPoints();

	return true;
}

void KeypointGraph::AlignWithWorldGround(UWorld* world)
{
	for (Keypoint& keypoint : keypoints_)
	{
		FHitResult outHit;
		FCollisionShape collisionShape;

		// Sweep with a small box instead of a point to avoid the trace going into
		// small crevices in the ground e.g. between tram rails
		collisionShape.ShapeType = ECollisionShape::Box;
		collisionShape.SetBox(FVector3f(20.0f));

		if (world->SweepSingleByChannel(outHit,
			keypoint.position + FVector::UpVector * 100.0f, keypoint.position - FVector::UpVector * 300.0f,
			FQuat::Identity, ECollisionChannel::ECC_WorldStatic, collisionShape))
		{
			keypoint.position.Z = outHit.ImpactPoint.Z;
		}
	}
}

void KeypointGraph::ApplyZoneRules(ATrafficController* controller)
{
	if (controller == nullptr)
	{
		return;
	}
	for (Keypoint& keypoint : keypoints_)
	{
		int32 zoneRules = controller->GetKeypointRegulationRulesAtPoint(keypoint.position);
		keypoint.rules = keypoint.rules | zoneRules;
	}

	return;
}

KeypointPath KeypointGraph::GetRandomPathFrom(const int& startKeypointIndex, const int32 ruleExceptions) const
{
	// Can't start path from current start index? Get a random path instead.
	if (!CompareRules(startKeypointIndex, ruleExceptions))
	{
		return GetRandomPath(); 
	}

	TArray<int> kps;

	kps.Add(startKeypointIndex);

	int last = startKeypointIndex;

	// Randomize a path until a dead end or until the path is getting too long.
	// There should be a maximum length because otherwise this algorithm could
	// theoretically get stuck on a circular path forever.
	// 
	// This can cause the problem that traffic entities teleport onto a new path
	// from the middle of a road because the maximum length is reached there.
	// A better solution would be to somehow guarantee they always reach a dead
	// end, ideally by somehow making them search for the nearest dead end. A
	// pathfinding algorithm like A* is probably needed for that. Note that the
	// graph might not always contain dead ends.

	while (kps.Num() < RANDOM_PATH_MAX_LENGTH)
	{
		std::vector<int> nexts = GetVerifiedKeypoints(keypoints_[last].outboundKeypoints, ruleExceptions);

		if (nexts.size() == 0)
		{
			break;
		}

		const int next = nexts[FMath::RandRange(0, nexts.size() - 1)];

		kps.Add(next);
		last = next;
	}

	return { this, kps };
}

KeypointPath KeypointGraph::FindPath(const int& startKeypointIndex, const int& destination, const int32 ruleExceptions) const
{
	// Can't start path from current start index? Get a random path instead.
	if (!CompareRules(startKeypointIndex, ruleExceptions))
	{
		return GetRandomPath();
	}

	// This is a basic A* algorithm, from here on out
	// Structure to store nodes
	struct KPNode
	{
		KPNode(KPNode* p, int k, float w)
		{
			parent = p;
			keypoint = k;
			weight = w;
		}
		float weight = -1;
		KPNode* parent = nullptr;
		int keypoint;
	};

	TArray< KPNode* > closed;
	TArray< KPNode* > opened;
	KPNode* start = new KPNode(nullptr, startKeypointIndex, 0);
	opened.Add(start);

	while (opened.Num() > 0)
	{
		KPNode* q = nullptr;
		float w = -1;

		for (KPNode* kpn : opened)
		{
			if (q == nullptr || kpn->weight < w)
			{
				w = kpn->weight;
				q = kpn;
			}
		}

		opened.Remove(q);

		// Check rules. Only open those keypoints that are accessible by current entity rules
		std::vector<int> validOutbound = GetVerifiedKeypoints(keypoints_[q->keypoint].outboundKeypoints, ruleExceptions);

		for (int kpi : validOutbound)
		{
			Keypoint kp = keypoints_[kpi];
			float childWeight = q->weight + FVector::Dist2D(keypoints_[q->keypoint].position, kp.position);

			// Additional checks
			bool cont = true;
			for (KPNode* check : opened)
			{
				if (check->keypoint == kpi && check->weight < childWeight)
				{
					cont = false;
					break;
				}
			}

			if (!cont)
			{
				break;
			}

			for (KPNode* check : closed)
			{
				if (check->keypoint == kpi && check->weight < childWeight)
				{
					cont = false;
					break;
				}
			}

			if (!cont)
			{
				break;
			}

			KPNode* child = new KPNode(q, kpi, childWeight);
			opened.Add(child);

			// Destination found?
			if (kpi == destination)
			{
				// Flip & return shortest keypoint path from destination to start
				TArray<int> kps;
				KPNode* r = child;

				// Get parent refs all the way to start node
				while (r->parent != nullptr)
				{
					kps.Add(r->keypoint);
					r = r->parent;
				}

				kps.Add(startKeypointIndex);
				TArray<int> kpsr;

				// Reverse keypoint array
				for (int i = kps.Num() - 1; i >= 0; i--)
				{
					kpsr.Add(kps[i]);
				}

				// Clear references
				opened.Empty();
				closed.Empty();

				return { this, kpsr };
			}
		}

		closed.Add(q);
	}
// Clear references
	opened.Empty();
	closed.Empty();


// A path could not be found, give path to a random outbound keypoint if there are any. Otherwise just spawn the vehicle again from a spawn point.
	std::vector<int> outbounds = GetVerifiedKeypoints(keypoints_[startKeypointIndex].outboundKeypoints, ruleExceptions);
	if (outbounds.size() == 0)
	{
		return GetRandomPathFrom(GetRandomSpawnPoint());
	}
	else
	{
		int rand = FMath::RandRange(0, outbounds.size() - 1);
		return { this, TArray<int>({startKeypointIndex, outbounds[rand]}) };
	}	
}

std::vector<int> KeypointGraph::GetKeypointsWithMargin(const float& margin) const
{
	float marginSquared = margin * margin;
	std::vector<int> result;

	result.reserve(keypoints_.size());

	//Randomize keypoint order, so we don't always end up with the same set of keypoints
	std::vector<Keypoint> randOrderKeypoints = std::vector<Keypoint>(keypoints_);
	auto rd = std::random_device{};
	auto rng = std::default_random_engine{ rd() };
	std::shuffle(std::begin(randOrderKeypoints), std::end(randOrderKeypoints), rng);

	// Add only keypoints that are not within the margin of previously added keypoints
	for (int i = 0; i < randOrderKeypoints.size(); i++)
	{
		bool otherKeypointsWithinMargin = false;

		for (int j = 0; j < result.size(); j++)
		{
			// Using squared vector lengths is slightly faster and doesn't change the results
			if ((randOrderKeypoints[i].position - randOrderKeypoints[result[j]].position).SizeSquared() < marginSquared)
			{
				otherKeypointsWithinMargin = true;
				break;
			}
		}

		if (otherKeypointsWithinMargin)
		{
			continue;
		}

		result.push_back(i);
	}

	return result;
}

void KeypointGraph::RemoveKeypointsByRange(std::vector<int>& list, const FVector2D position, float range)
{
	std::vector<int> newList;
	newList.reserve(list.size());
	range = range * range;

	for (int i : list)
	{
		if (FVector2D::DistSquared(FVector2D(GetKeypointPosition(i)), position) > range)
			newList.push_back(i);
	}

	list = newList;
}

FRotator KeypointGraph::GetKeypointRotation(const int& keypointIndex) const
{
	if (keypointIndex > KeypointCount() - 1)
	{
		return FRotator();
	}

	FVector directionVector;

	for (const int& outboundKeypoint : keypoints_[keypointIndex].outboundKeypoints)
	{
		directionVector += keypoints_[outboundKeypoint].position / keypoints_[keypointIndex].outboundKeypoints.size();
	}

	return directionVector.ToOrientationRotator();
}

int KeypointGraph::GetClosestKeypoint(const FVector& position) const
{
	if (keypoints_.size() == 0)
	{
		return -1;
	}

	int keypointIndex = 0;
	float shortestDistance = FVector::DistSquared(position, keypoints_[0].position);

	for (int i = 1; i < keypoints_.size(); i++)
	{
		float currDist = FVector::DistSquared(position, keypoints_[i].position);

		if (currDist <= shortestDistance)
		{
			shortestDistance = currDist;
			keypointIndex = i;
		}
	}

	return keypointIndex;
}

bool KeypointGraph::CompareRules(const Keypoint& keypoint, const int32 exceptions) const
{
	// Compare bits in keypoint rules & given rule exceptions.
	return (keypoint.rules & exceptions) == keypoint.rules;
}

bool KeypointGraph::CompareRules(const int keypointIndex, const int32 exceptions) const
{
	if (keypointIndex >= keypoints_.size())
	{
		return false;
	}

	return CompareRules(keypoints_[keypointIndex], exceptions);
}

std::vector<Keypoint*> KeypointGraph::GetKeypointsByRules(std::vector<Keypoint>& from, const int32 exceptions)
{
	std::vector<Keypoint*> accessibleKeypoints;
	accessibleKeypoints.reserve(from.size());

	for (int i = 0; i < from.size(); i++)
	{
		if (CompareRules(i, exceptions))
		{
			accessibleKeypoints.push_back(&from[i]);
		}
	}

	return accessibleKeypoints;
}

std::pair<int, int> KeypointGraph::GetOvertakePair(std::pair<int, int> currentLane, bool allowBothWays) const
{
	for (OvertakeLaneLink link : overtakes_)
	{
		if (link.first.first == currentLane.first && link.first.second == currentLane.second)
		{
			return std::make_pair(link.second.first, link.second.second);
		}
		else if (allowBothWays && link.second.first == currentLane.first && link.second.second == currentLane.second)
		{
			return std::make_pair(link.first.first, link.first.second);
		}
	}

	return std::pair<int, int>(-1, -1);
}

void KeypointGraph::UpdateEntryPoints()
{
	entryPoints_.clear();

	for (int i = 0; i < keypoints_.size(); i++)
	{
		if (keypoints_[i].inboundKeypoints.size() == 0)
		{
			entryPoints_.push_back(i);
		}
	}
}

void KeypointGraph::VerifyKeypoints(std::vector<int>& keypoints, const int32 ruleExceptions)
{
	for (int i = keypoints.size() - 1; i >= 0; --i)
	{
		if (!CompareRules(keypoints[i], ruleExceptions))
		{
			keypoints.erase(keypoints.begin() + i);
		}
	}	
}

std::vector<int> KeypointGraph::GetVerifiedKeypoints(std::vector<int> keypoints, const int32 ruleExceptions) const
{
	std::vector<int> ret;
	ret.reserve(keypoints.size());

	for (int i : keypoints)
	{
		if (CompareRules(i, ruleExceptions))
		{
			ret.push_back(i);
		}
	}

	return ret;
}

// Note: KeypointPath class implementation starts here
FVector KeypointPath::GetPointPosition(const int index) const
{
	const int keypoint = keypoints[index];

	if (keypoint <= -8)
	{
		const int customIndex = FMath::Abs(keypoint) - 8;
		if (customPoints.Num() > customIndex)
		{
			return customPoints[customIndex];
		}
	}
	else if (keypoint >= 0)
	{
		return graph->GetKeypointPosition(keypoint);
	}

	UE_LOG(LogTemp, Error, TEXT("Bad index. Make sure to use indexes from a KeypointPath object, and not from the global graph."));
	return FVector();
}
