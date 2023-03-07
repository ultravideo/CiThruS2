#include "KeypointGraph.h"
#include "../Debug.h"

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

void KeypointGraph::ClearKeypoints()
{
	keypoints_.clear();
	links_.clear();
	entryPoints_.clear();
	spawnPoints_.clear();
}

void KeypointGraph::SaveToFile(const std::string& filePath)
{
	std::ofstream outfile(filePath);

	if (!outfile.is_open())
	{
		std::cout << "couldn't open file " << filePath << std::endl;
		return;
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

	outfile.close();
}

void KeypointGraph::LoadFromFile(const std::string& filePath)
{
	std::ifstream inFile(filePath);

	if (!inFile.is_open())
	{
		std::cout << "couldn't open file " << filePath << std::endl;
		return;
	}

	ClearKeypoints();

	std::string line;

	// 0, 1, 2
	int readKpsLinksSpawnpoints = 0;

	while (std::getline(inFile, line))
	{
		if (line[0] == 'k')
		{
			readKpsLinksSpawnpoints = 0;
			continue;
		}
		else if (line[0] == 'l')
		{
			readKpsLinksSpawnpoints = 1;
			continue;
		}
		if (line[0] == 's')
		{
			readKpsLinksSpawnpoints = 2;
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
	}

	inFile.close();

	UpdateEntryPoints();
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
		collisionShape.SetBox(FVector(20.0f));

		if (world->SweepSingleByChannel(outHit,
			keypoint.position + FVector::UpVector * 100.0f, keypoint.position - FVector::UpVector * 300.0f,
			FQuat::Identity, ECollisionChannel::ECC_WorldStatic, collisionShape))
		{
			keypoint.position.Z = outHit.ImpactPoint.Z;
		}
	}
}

KeypointPath KeypointGraph::GetRandomPathFrom(const int& startKeypointIndex) const
{
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
		const std::vector<int>& nexts = keypoints_[last].outboundKeypoints;

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

std::vector<int> KeypointGraph::GetKeypointsWithMargin(const float& margin) const
{
	float marginSquared = margin * margin;
	std::vector<int> result;

	result.reserve(keypoints_.size());

	// Add only keypoints that are not within the margin of previously added keypoints
	for (int i = 0; i < keypoints_.size(); i++)
	{
		bool otherKeypointsWithinMargin = false;

		for (int j = 0; j < result.size(); j++)
		{
			// Using squared vector lengths is slightly faster and doesn't change the results
			if ((keypoints_[i].position - keypoints_[result[j]].position).SizeSquared() < marginSquared)
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
