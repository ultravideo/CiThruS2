#include "LodController.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "TrafficController.h"
#include "ViewSynthesis/PubSubCommunicator.h"
#include "Parking/ParkingController.h"
#include "Parking/ParkingSpace.h"
#include "Misc/CameraUtility.h"
#include "Misc/Debug.h"

#include <algorithm>

LodController::LodController(ATrafficController* trafficController, const float& farDistance)
	: trafficController_(trafficController),
	farDistance_(farDistance),
	entityInFront_(nullptr),
	entityInFrontDistance_(0.0f),
	visualizeViewFrustrum_(false)
{

}

void LodController::AddEntity(ITrafficEntity* entity)
{
	// Should always be inserted to avoid sync issues/dangling pointers
	if (!entityLodInfo_.insert({ entity, EntityLodInfo{ entity, false, false, std::numeric_limits<float>::max() } }).second)
	{
		throw std::runtime_error("LOD controller state is invalid!");
	}
}

void LodController::RemoveEntity(ITrafficEntity* entity)
{
	// Should always be erased to avoid sync issues/dangling pointers
	if (entityLodInfo_.erase(entity) != 1)
	{
		throw std::runtime_error("LOD controller state is invalid!");
	}
}

void LodController::UpdateAllLods()
{
	TArray<AActor*> publishableActors;

	FVector cameraLocation = FVector::ZeroVector;
	FMatrix cameraViewMatrix = FMatrix::Identity;
	float cameraTanInverse = 0.0f;
	float aspectRatioInverse = 0.0f;

	GetCameraViewProperties(cameraLocation, cameraViewMatrix, cameraTanInverse, aspectRatioInverse);

	APawn* playerPawn = trafficController_->GetWorld()->GetFirstPlayerController()->GetPawn();
	const float farDistanceSquared = farDistance_ * farDistance_;

	entityInFront_ = nullptr;
	entityInFrontDistance_ = std::numeric_limits<float>::max();

	for (auto it = entityLodInfo_.begin(); it != entityLodInfo_.end(); it++)
	{
		EntityLodInfo& entityWrapper = it->second;

		// Comparing squared lengths is a tiny bit faster, same result as with normal lengths
		if ((entityWrapper.entity->GetCollisionRectangle().GetPosition() - cameraLocation).SquaredLength() > farDistanceSquared)
		{
			// Entity is too far away
			entityWrapper.isNear = false;

			continue;
		}

		// Entity data should be published for nearby entities even if they're not currently visible
		AActor* entityAsActor = Cast<AActor>(entityWrapper.entity);

		if (entityAsActor != nullptr)
		{
			publishableActors.Add(entityAsActor);
		}

		if (!EntityInCameraView(entityWrapper.entity, cameraLocation, cameraViewMatrix, cameraTanInverse, aspectRatioInverse))
		{
			// Entity is not in view
			entityWrapper.isNear = false;

			continue;
		}

		// Entity in camera view and nearby
		entityWrapper.isNear = true;

		if (playerPawn)
		{
			FVector pawnForward = playerPawn->GetActorForwardVector();
			FVector entityRelativeToPawn = entityWrapper.entity->GetCollisionRectangle().GetPosition() - playerPawn->GetActorLocation();

			if (FVector::DotProduct(pawnForward, entityRelativeToPawn.GetSafeNormal()) > 0.8f && playerPawn->GetActorRotation().Quaternion().AngularDistance(entityWrapper.entity->GetCollisionRectangle().GetRotation()) < UE_PI / 3.0f)
			{
				float entityDistance = entityRelativeToPawn.SquaredLength();

				entityInFrontMutex_.lock();

				if (entityDistance < entityInFrontDistance_)
				{
					entityInFront_ = entityWrapper.entity;
					entityInFrontDistance_ = entityDistance;
				}

				entityInFrontMutex_.unlock();
			}
		}
	}

	AParkingController* parkingController = trafficController_ != nullptr ? trafficController_->GetParkingController() : nullptr;

	if (parkingController != nullptr)
	{
		const TArray<AParkingSpace*>& parkingSpaces = parkingController->GetParkingSpaces();

		for (AParkingSpace* parkingSpace : parkingSpaces)
		{
			if (!IsValid(parkingSpace)
				|| !parkingSpace->HasParkedCar()
				|| (parkingSpace->GetPublishTransform().GetLocation() - cameraLocation).SquaredLength() > farDistanceSquared)
			{
				continue;
			}

			publishableActors.Add(parkingSpace);
		}
	}

	for (auto it = entityLodInfo_.begin(); it != entityLodInfo_.end(); it++)
	{
		EntityLodInfo& entityWrapper = it->second;

		// Call Near/Far events on entities if they've changed state
		if (entityWrapper.isNear && !entityWrapper.previousIsNear)
		{
			entityWrapper.entity->OnNear();
		}
		else if (!entityWrapper.isNear && entityWrapper.previousIsNear)
		{
			entityWrapper.entity->OnFar();
		}

		entityWrapper.previousIsNear = entityWrapper.isNear;
	}

	UPubSubCommunicator::PublishTrafficArray("Traffic", publishableActors);
}

void LodController::GetCameraViewProperties(FVector& cameraLocation, FMatrix& cameraViewMatrix, float& cameraTanInverse, float& aspectRatioInverse)
{
	FMinimalViewInfo cameraViewInfo;
	UWorld* world = trafficController_->GetWorld();

	FMatrix cameraMatrix = FMatrix::Identity;

	// Get camera view infos
	if (world != nullptr)
	{
#if WITH_EDITOR
		if (GEditor && trafficController_->UseEditorViewportCameraForLods())
		{
			for (FEditorViewportClient* viewportClient : GEditor->GetAllViewportClients())
			{
				if (viewportClient && viewportClient->IsPerspective() && viewportClient->IsLevelEditorClient() && viewportClient->IsRealtime())
				{
					cameraViewMatrix = FTransform(viewportClient->GetViewRotation(), viewportClient->GetViewLocation()).Inverse().ToMatrixNoScale();
					//cameraMatrix = FInverseRotationMatrix(viewportClient->GetViewRotation()) * FTranslationMatrix(-viewportClient->GetViewLocation());
					cameraMatrix = FTransform(viewportClient->GetViewRotation(), viewportClient->GetViewLocation()).ToMatrixNoScale();
					aspectRatioInverse = 1.0f / viewportClient->AspectRatio;
					cameraTanInverse = 1.0f / tan(FMath::DegreesToRadians(viewportClient->FOVAngle / 2.0f));
					cameraLocation = viewportClient->GetViewLocation();
					break;
				}
			}
		}
		if (cameraLocation == FVector::ZeroVector)
#endif // WITH_EDITOR
		{
			FVector2D viewportSize = FVector2D::ZeroVector;
			float aspectRatio = 16.0f / 9.0f;

			if (CameraUtility::GetViewportDimensions(world, viewportSize))
			{
				aspectRatio = viewportSize.X / viewportSize.Y;
			}

			cameraViewInfo = world->GetFirstPlayerController()->PlayerCameraManager->GetCameraCacheView();
			cameraMatrix = FTransform(cameraViewInfo.Rotation, cameraViewInfo.Location).ToMatrixNoScale();
			cameraViewMatrix = FTransform(cameraViewInfo.Rotation, cameraViewInfo.Location).Inverse().ToMatrixNoScale();
			//aspectRatioInverse = 1.0f / cameraViewInfo.AspectRatio;
			aspectRatioInverse = 1.0f / aspectRatio;
			cameraTanInverse = 1.0f / tan(FMath::DegreesToRadians(cameraViewInfo.FOV / 2.0f));

			cameraLocation = cameraViewInfo.Location;
		}

		if (visualizeViewFrustrum_)
		{
			float nearPlane = 100.0f;
			float farPlane = farDistance_;

			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, nearPlane / cameraTanInverse, (nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, -nearPlane / cameraTanInverse, (nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, -nearPlane / cameraTanInverse, -(nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, nearPlane / cameraTanInverse, -(nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);

			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
		}
	}
}

bool LodController::EntityInCameraView(ITrafficEntity* entity, FVector& cameraLocation, FMatrix& cameraViewMatrix, float& cameraTanInverse, float& aspectRatioInverse)
{
	bool visibleInFront = false;
	bool visibleInBack = false;

	// Check if entity is in camera view
	std::vector<FVector> corners = entity->GetCollisionRectangle().GetCorners();

	// Calculate the bounding box of the object's collision rectangle in the camera view
	for (int j = 0; j < 8; j++)
	{
		FVector4 posInCameraSpace = cameraViewMatrix.TransformPosition(corners[j]);
		FVector posProjected;

		// If the X coordinate is negative, the object is behind the camera and not visible
		if (posInCameraSpace.X > 0)
		{
			// Calculate the projected position of the object in the camera view
			posProjected = FVector(
				posInCameraSpace.Y * cameraTanInverse / posInCameraSpace.X * aspectRatioInverse,
				posInCameraSpace.Z * cameraTanInverse / posInCameraSpace.X,
				posInCameraSpace.X / farDistance_);

			if (posProjected.X > -1.0f && posProjected.X < 1.0f && posProjected.Y > -1.0f && posProjected.Y < 1.0f && posProjected.Z > -1.0f && posProjected.Z < 1.0f)
			{
				visibleInFront = true;
			}
		}
		else
		{
			float x = -posInCameraSpace.X;
			posProjected = FVector(
				posInCameraSpace.Y * cameraTanInverse / x * aspectRatioInverse,
				posInCameraSpace.Z * cameraTanInverse / x,
				x / farDistance_);

			if (posProjected.X > -1.0f && posProjected.X < 1.0f && posProjected.Y > -1.0f && posProjected.Y < 1.0f && posProjected.Z > -1.0f && posProjected.Z < 1.0f)
			{
				visibleInBack = true;
			}
		}
	}

	return visibleInFront || visibleInBack;
}
