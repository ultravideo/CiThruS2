#include "TrafficController.h"
#include "Misc/Debug.h"
#include "Entities/ITrafficEntity.h"
#include "EngineUtils.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"

#if WITH_EDITOR
//#include "GameFramework/SpectatorPawn.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

#include <list>
#include <vector>

void ATrafficController::EntityChangeZone(std::pair<int32, int32> currentZone, std::pair<int32, int32> previousZone, ITrafficEntity* entity)
{
	// combine keys for hashing
	int64 currentZoneKey = currentZone.second + (static_cast<int64>(currentZone.first) << 32);
	int64 previousZoneKey = previousZone.second + (static_cast<int64>(previousZone.first) << 32);
	
	entitiesByZone_[previousZoneKey].erase(entity);
	entitiesByZone_[currentZoneKey].insert(entity);
}

void ATrafficController::EntityZoneDelete(std::pair<int32, int32> currentZone, ITrafficEntity* entity)
{
	int64 currentZoneKey = currentZone.second + (static_cast<int64>(currentZone.first) << 32);
	entitiesByZone_[currentZoneKey].erase(entity);
}

void ATrafficController::CheckEntityCollisions()
{
	APawn* playerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
	CollisionRectangle playerCollision = CollisionRectangle();

	if (!playerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("No player pawn found!"));
	}
	else
	{
		// Calculate player pawn collision bounds
		FBox box = playerPawn->CalculateComponentsBoundingBoxInLocalSpace(false, false);
		playerCollision = CollisionRectangle(box.GetSize(), playerPawn->GetActorLocation(), playerPawn->GetActorQuat());
	}

	// TODO probably don't do this in a separate loop. Needs synchronization to be combined with ParallelFor
	for (int i = 0; i < simulatedEntities_.size(); i++)
	{
		for (ITrafficArea* trafficArea : trafficAreas_)
		{
			trafficArea->UpdateCollisionStatusWithEntity(simulatedEntities_[i]);
		}
	}

	// Naively loop through all other entities for every entity
	auto ProcessAllEntities = [&](int32 i)
	{
		ITrafficEntity* entity = simulatedEntities_[i];

		// All collisions should be cleared first to avoid the issue that a traffic entity is deleted but
		// other traffic entities are still referencing it
		entity->ClearBlockingCollisions();
	
		// Check collisions
		for (int32 j = 0; j < simulatedEntities_.size(); j++)
		{
			ITrafficEntity* otherEntity = simulatedEntities_[j];

			// Don't check against self
			if (i == j)
			{
				continue;
			}

			entity->UpdateBlockingCollisionWith(otherEntity);
		}
		
		for (ITrafficEntity* otherEntity : staticEntities_)
		{
			entity->UpdateBlockingCollisionWith(otherEntity);
		}

		if (playerPawn)
		{
			entity->UpdatePawnCollision(playerCollision);
		}
	};

	// For every entity, check collisions against entities in adjacent zones
	auto ProcessNearbyEntities = [&](int32 i) 
	{
		ITrafficEntity* entity = simulatedEntities_[i];
		entity->ClearBlockingCollisions();

		// Check collisions against entities in adjacent zones
		std::pair<int32, int32> currentZone = entity->GetCurrentZone();
		for (int32 x = -1; x <= 1; x++)
		{
			for (int32 z = -1; z <= 1; z++)
			{
				int64 zoneKey = currentZone.second + z + (static_cast<int64>(currentZone.first + x) << 32);
				std::unordered_map<int64, std::unordered_set<ITrafficEntity*>>::iterator itr = entitiesByZone_.find(zoneKey);

				if (itr == entitiesByZone_.end())
				{
					continue;
				}

				std::unordered_set<ITrafficEntity*> entitiesInZone = itr->second;

				for (auto& zoneEntity : entitiesInZone)
				{
					if (zoneEntity == entity)
					{
						continue;
					}

					entity->UpdateBlockingCollisionWith(zoneEntity);
				}
			}
		}
		
		for (ITrafficEntity* otherEntity : staticEntities_)
		{
			otherEntity->UpdateBlockingCollisionWith(entity);
		}
		
		if (playerPawn)
		{
			entity->UpdatePawnCollision(playerCollision);
		}
	};
	
	switch (collisionCheckingType_)
	{
	case CITHRUS_COLLISIONS_NAIVE_PAR:
		//UE_LOG(LogTemp, Warning, TEXT("Doing parallel collision checks for all entities"));
		ParallelFor(simulatedEntities_.size(), ProcessAllEntities);
		break;

	case CITHRUS_COLLISION_ZONES_PAR:
		//UE_LOG(LogTemp, Warning, TEXT("Doing parallel collision checks with collision zones"));
		ParallelFor(simulatedEntities_.size(), ProcessNearbyEntities);
		break;

	case CITHRUS_COLLISIONS_NAIVE:
		//UE_LOG(LogTemp, Warning, TEXT("Doing sequential collision checks for all entities"));
		for (int32 i = 0; i < simulatedEntities_.size(); i++)
		{
			ProcessAllEntities(i);
		}
		break;

	case CITHRUS_COLLISION_ZONES:
		//UE_LOG(LogTemp, Warning, TEXT("Doing sequential collision checks for all entities"));
		for (int32 i = 0; i < simulatedEntities_.size(); i++)
		{
			ProcessAllEntities(i);
		}
		break;
	}
}
