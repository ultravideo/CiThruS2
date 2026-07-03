#include "PubSubCommunicator.h"
#include "MqttPublisher.h"
#include "FilePublisher.h"
#include "Traffic/Entities/Car.h"
#include "Traffic/Entities/Pedestrian.h"
#include "Traffic/Entities/Bicycle.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Traffic/Parking/ParkingSpace.h"
#include "Traffic/Parking/ParkingController.h"
#include "Misc/CommandLine.h"
#include "Misc/GeoUtility.h"
#include "Misc/Parse.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GeoReferencingSystem.h"
#include "Misc/Debug.h"
#include "JsonObjectConverter.h"
#include <format>


void UPubSubCommunicator::PublishBool(FString topic, bool value)
{
	std::string valueAsString = value ? "true" : "false";

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::PublishInt(FString topic, int value)
{
	std::string valueAsString = std::to_string(value);

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::PublishFloat(FString topic, float value)
{
	std::string valueAsString = std::to_string(value);

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::PublishString(FString topic, FString value)
{
	std::string valueAsString = TCHAR_TO_UTF8(*value);

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::Publish2DVector(FString topic, FVector2D value)
{
	std::string valueAsString = "[" + std::to_string(value.X) + ", " + std::to_string(value.Y) + "]";

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::Publish3DVector(FString topic, FVector value)
{
	std::string valueAsString = "[" + std::to_string(value.X) + ", " + std::to_string(value.Y) + ", " + std::to_string(value.Z) + "]";

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::Publish4DVector(FString topic, FVector4 value)
{
	std::string valueAsString = "[" + std::to_string(value.X) + ", " + std::to_string(value.Y) + ", " + std::to_string(value.Z) + ", " + std::to_string(value.W) + "]";

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::PublishTrafficEntity(FString topic, AActor* actor)
{
	if (publisher_ == nullptr)
	{
		return;
	}

	if (!IsValid(actor))
	{
		return;
	}

	APawn* pawn = Cast<APawn>(actor);

	if (pawn == nullptr || !pawn->IsPlayerControlled() || !publishEgoVehicleData_)
	{
		return;
	}

	const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	const FVector worldLocation = actor->GetActorLocation();
	const FVector unrealLinearVelocity = actor->GetVelocity();

	//TODO: Bind the gear to EgoCar data
	int selectedGear = 1;

	GeoData geoData;

	if (!GetGeoData(actor, worldLocation, unrealLinearVelocity, actor->GetActorQuat(), now, geoData))
	{
		return;
	}

	FEgoMessage Message;

	Message.Timestamp = FString(std::format("{:%FT%TZ}", now).c_str());

	Message.Vehicle.CurrentLocation = {
		geoData.geographicCoordinates.Latitude,
		geoData.geographicCoordinates.Longitude,
		geoData.geographicCoordinates.Altitude
	};

	Message.Vehicle.CurrentRotation = {
		geoData.tangentRotation.Roll,
		geoData.tangentRotation.Pitch,
		geoData.tangentRotation.Yaw
	};

	Message.Vehicle.LinearVelocity = {
		geoData.linearVelocityEnuMps.X,
		geoData.linearVelocityEnuMps.Y,
		geoData.linearVelocityEnuMps.Z
	};

	Message.Vehicle.AngularVelocity = {
		geoData.tangentAngularVelocity.Roll,
		geoData.tangentAngularVelocity.Pitch,
		geoData.tangentAngularVelocity.Yaw
	};

	lastPublishedEgoActor_ = actor;

	FString JsonString;
	FJsonObjectConverter::UStructToJsonObjectString(Message, JsonString);

	std::string utf8 = TCHAR_TO_UTF8(*JsonString);
	PublishInternal(topic, reinterpret_cast<uint8_t*>(utf8.data()), utf8.size());
}

void UPubSubCommunicator::PublishTrafficArray(FString topic, const TArray<AActor*>& trafficEntities)
{
	if (publisher_ == nullptr)
	{
		return;
	}

	const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	FTrafficArrayMessage Message;
	Message.Timestamp = FString(std::format("{:%FT%TZ}", now).c_str());

	TSet<AActor*> sentTrafficActors;

	bool firstTrafficItem = true;

	FVector EgoPos = FVector::ZeroVector;
	FVector EgoVel = FVector::ZeroVector;
	float EgoRadius = 120.0f;

	if (lastPublishedEgoActor_ != nullptr)
	{
		EgoPos = lastPublishedEgoActor_->GetActorLocation();
		EgoVel = lastPublishedEgoActor_->GetVelocity();
	}

	for (AActor* actor : trafficEntities)
	{
		if (!IsValid(actor))
		{
			continue;
		}

		FString type = "Unknown";
		const ITrafficEntity* trafficEntity = nullptr;

		auto existingId = trafficEntityIds_.find(actor);

		if (existingId == trafficEntityIds_.end())
		{
			std::string newId = std::to_string(lastUsedTrafficEntityId_);

			lastUsedTrafficEntityId_++;

			existingId = trafficEntityIds_.insert({ actor, newId }).first;
		}

		const std::string& uniqueId = existingId->second;

		bool isParkedCar = false;

		float entityRadius = 120.0f;

		FVector worldLocation = FVector::ZeroVector;
		FQuat worldQuat = FQuat::Identity;
		FVector unrealLinearVelocity = FVector::ZeroVector;
		int32 vehicleType = -1;

		if (AParkingSpace* parkingSpace = Cast<AParkingSpace>(actor))
		{
			if (!publishParkedCarData_ || !parkingSpace->HasParkedCar())
			{
				continue;
			}

			type = "Car";
			isParkedCar = true;

			const FTransform parkedTransform = parkingSpace->GetPublishTransform();

			worldLocation = parkedTransform.GetLocation();
			worldQuat = parkedTransform.GetRotation();
			unrealLinearVelocity = FVector::ZeroVector;
			vehicleType = -1; // TODO Type from parked car
		}
		else if (ACar* car = Cast<ACar>(actor))
		{
			if (!publishCarData_)
			{
				continue;
			}

			type = "Car";
			trafficEntity = car;
			vehicleType = car->GetVariantId();
		}
		else if (APedestrian* pedestrian = Cast<APedestrian>(actor))
		{
			if (!publishPedestrianData_)
			{
				continue;
			}

			type = "Pedestrian";
			trafficEntity = pedestrian;
			entityRadius = 60.0f;
		}
		else if (ABicycle* bicycle = Cast<ABicycle>(actor))
		{
			if (!publishCyclistData_)
			{
				continue;
			}

			type = "Bicycle";
			trafficEntity = bicycle;
			entityRadius = 60.0f;
		}
		else
		{
			continue;
		}

		if (!isParkedCar)
		{
			worldLocation = actor->GetActorLocation();
			if (type == "Pedestrian")
			{
				// Pedestrian actor Z location is in the middle of the actor, so move it down to the ground
				worldLocation.Z -= APedestrian::HEIGHT_CM * 0.5f;
			}
			worldQuat = actor->GetActorQuat();

			const bool useInterfaceVelocity = (type == "Car" || type == "Bicycle");

			if (useInterfaceVelocity && trafficEntity != nullptr)
			{
				if (trafficEntity->Stopped() || trafficEntity->Blocked())
				{
					unrealLinearVelocity = FVector::ZeroVector;
				}
				else
				{
					const FVector direction = trafficEntity->GetMoveDirection().GetSafeNormal();
					const float speed = trafficEntity->GetMoveSpeed();
					unrealLinearVelocity = direction * speed;
				}
			}
			else
			{
				unrealLinearVelocity = actor->GetVelocity();
			}
		}

		GeoData geoData;

		if (!GetGeoData(actor, worldLocation, unrealLinearVelocity, worldQuat, now, geoData))
		{
			continue;
		}

		bool isVisible = IsActorVisible(actor);

		FTrafficCollisionResult collisionResult;
		bool bCollisionRisk = false;

		if (lastPublishedEgoActor_ != nullptr && actor != lastPublishedEgoActor_)
		{
			bCollisionRisk = CheckCollisionRisk(
				EgoPos,
				EgoVel,
				EgoRadius,
				worldLocation,
				unrealLinearVelocity,
				entityRadius,
				3.0f,
				collisionResult
			);
		}

		FTrafficItemMessage Item;

		Item.Id = FString(uniqueId.c_str());
		Item.Type = type;
		Item.VehicleType = vehicleType;
		Item.Parked = isParkedCar;

		// Blind spot and collision are independent: an out-of-sight object can also be on a
		// collision course, so both flags are reported rather than collapsed into one warning.
		Item.Warnings.BlindSpotAlert = !isVisible;
		Item.Warnings.CollisionAlert = bCollisionRisk;
		Item.Warnings.TimeToImpact = collisionResult.TimeToImpact;

		Item.CurrentLocation = {
			geoData.geographicCoordinates.Latitude,
			geoData.geographicCoordinates.Longitude,
			geoData.geographicCoordinates.Altitude
		};

		Item.CurrentRotation = {
			geoData.tangentRotation.Roll,
			geoData.tangentRotation.Pitch,
			geoData.tangentRotation.Yaw
		};

		Item.LinearVelocity = {
			geoData.linearVelocityEnuMps.X,
			geoData.linearVelocityEnuMps.Y,
			geoData.linearVelocityEnuMps.Z
		};

		Item.AngularVelocity = {
			geoData.tangentAngularVelocity.Roll,
			geoData.tangentAngularVelocity.Pitch,
			geoData.tangentAngularVelocity.Yaw
		};

		Message.Traffic.Add(Item);
		sentTrafficActors.Add(actor);
	}

	// Drop stale traffic entries so actors that disappear/reappear don't accumulate stale angular-rate state.
	for (auto it = lastPublishedTrafficEntityData_.begin(); it != lastPublishedTrafficEntityData_.end();)
	{
		AActor* cachedActor = it->first;
		const bool isCurrentTrafficActor = sentTrafficActors.Contains(cachedActor);
		const bool isTrackedEgoActor = cachedActor != nullptr && cachedActor == lastPublishedEgoActor_;

		if (!isCurrentTrafficActor && !isTrackedEgoActor)
		{
			it = lastPublishedTrafficEntityData_.erase(it);
			trafficEntityIds_.erase(cachedActor);
			continue;
		}

		++it;
	}

	FString JsonString;
	FJsonObjectConverter::UStructToJsonObjectString(Message, JsonString);

	std::string utf8 = TCHAR_TO_UTF8(*JsonString);
	PublishInternal(topic,
		reinterpret_cast<uint8_t*>(utf8.data()),
		utf8.size());
}

void UPubSubCommunicator::StartMqttClient(FString serverUri, FString username, FString password, int maxMsgsPerSecond)
{
	ApplyCommandLineParameters();

	publisher_ = TSharedPtr<IPublisher>(
		new MqttPublisher(
			TCHAR_TO_UTF8(*serverUri),
			TCHAR_TO_UTF8(*username),
			TCHAR_TO_UTF8(*password),
			maxMsgsPerSecond));
}

void UPubSubCommunicator::StartFileSaveClient(FString directoryPath, int maxMsgsPerSecond)
{
	ApplyCommandLineParameters();

	publisher_ = TSharedPtr<IPublisher>(
		new FilePublisher(
			TCHAR_TO_UTF8(*directoryPath),
			maxMsgsPerSecond));
}

void UPubSubCommunicator::StopAllClients()
{
	publisher_.Reset();
}

void UPubSubCommunicator::SetTopicPrefix(FString prefix)
{
	topicPrefix_ = std::string(TCHAR_TO_UTF8(*prefix));
}

void UPubSubCommunicator::SetPublishEgoVehicleData(bool value)
{
	publishEgoVehicleData_ = value;
}

void UPubSubCommunicator::SetPublishCarData(bool value)
{
	publishCarData_ = value;
}

void UPubSubCommunicator::SetPublishParkedCarData(bool value)
{
	publishParkedCarData_ = value;
}

void UPubSubCommunicator::SetPublishPedestrianData(bool value)
{
	publishPedestrianData_ = value;
}

void UPubSubCommunicator::SetPublishCyclistData(bool value)
{
	publishCyclistData_ = value;
}

void UPubSubCommunicator::SetTrackedCameraForLineOfSightChecks(USceneCaptureComponent2D* camera, float aspectRatio)
{
	losTrackedCamera_ = camera;
	losTrackedCameraAspectRatio_ = aspectRatio;
}

bool UPubSubCommunicator::GetGeoData(
	AActor* actor,
	const FVector& ueLocation,
	const FVector& ueVelocity,
	const FQuat& ueRotation,
	const std::chrono::system_clock::time_point& now,
	GeoData& geoData)
{
	AGeoReferencingSystem* geoRef = nullptr;

	if (!GeoUtility::TryGetGeoReferencingSystem(actor, geoRef))
	{
		return false;
	}

	if (!GeoUtility::TryWorldToGeographic(geoRef, ueLocation, geoData.geographicCoordinates))
	{
		return false;
	}

	FVector east = FVector::ZeroVector;
	FVector north = FVector::ZeroVector;
	FVector up = FVector::ZeroVector;

	if (!GeoUtility::TryGetEnuBasisAtWorldLocation(geoRef, ueLocation, east, north, up))
	{
		return false;
	}

	geoData.linearVelocityEnuMps = GeoUtility::WorldVelocityCmpsToEnuMps(ueVelocity, east, north, up);
	geoData.tangentRotation = GeoUtility::WorldQuatToTangentRotatorYawNorth(ueRotation, east, north, up);

	geoData.tangentAngularVelocity = FRotator::ZeroRotator;
	auto lastPublishedData = lastPublishedTrafficEntityData_.find(actor);
	if (lastPublishedData != lastPublishedTrafficEntityData_.end())
	{
		const double deltaSeconds = std::chrono::duration<double, std::ratio<1, 1>>(now - lastPublishedData->second.timestamp).count();
		geoData.tangentAngularVelocity = GeoUtility::ComputeAngularVelocityDegPerSec(lastPublishedData->second.geoData.tangentRotation, geoData.tangentRotation, deltaSeconds);
	}

	lastPublishedTrafficEntityData_[actor] = { now, geoData };

	return true;
}

bool UPubSubCommunicator::IsActorVisible(AActor* actor)
{
	// If we don't have a camera to track, assume everything is visible
	if (losTrackedCamera_ == nullptr)
	{
		return true;
	}

	// Raise the position up by half a meter because most traffic entities have their origin at ground level
	FVector actorLocation = actor->GetActorLocation() + FVector::UpVector * 50.0f;

	// First check if point is inside the view frustrum of the tracked camera
	FMinimalViewInfo viewInfo{};

	// I don't know why there is a deltaTime parameter here. Why would the length of a frame affect the view?
	// Looking at the source code of this function, the parameter isn't even used, so I just put 0.1f there
	losTrackedCamera_->GetCameraView(0.1f, viewInfo);

	FMatrix cameraViewMatrix = FMatrix(
		viewInfo.Rotation.UnrotateVector(FVector::UnitX()),
		viewInfo.Rotation.UnrotateVector(FVector::UnitY()),
		viewInfo.Rotation.UnrotateVector(FVector::UnitZ()),
		viewInfo.Rotation.UnrotateVector(-viewInfo.Location));
	FVector4 posInCameraSpace = cameraViewMatrix.TransformPosition(actorLocation);

	// If the X coordinate is negative, the point is behind the camera and thus not visible
	if (posInCameraSpace.X < 0)
	{
		return false;
	}

	// Point is in front of camera, but we still have to check whether it's outside the edges of the camera view
	float cameraTanInverse = 1.0f / tan(FMath::DegreesToRadians(viewInfo.FOV / 2.0f));

	// Calculate the projected position of the object in the camera view
	FVector2D posProjected = FVector2D(
		posInCameraSpace.Y * cameraTanInverse / posInCameraSpace.X,
		posInCameraSpace.Z * cameraTanInverse * losTrackedCameraAspectRatio_ / posInCameraSpace.X);

	// Margin to prevent actors being "not visible" when they are still partly on screen. 
	// TODO: Proper view projection to actor collider intersect
	float margin = 0.07f;

	// The projected space is normalized so the edges are always at -1.0 and 1.0 regardless of resolution or aspect ratio
	if (posProjected.X < -1.0f - margin || posProjected.X > 1.0f + margin || posProjected.Y < -1.0f - margin || posProjected.Y > 1.0f + margin)
	{
		return false;
	}

	// The point is inside the view frustrum, but it may still be hidden behind something
	FHitResult hitResult{};

	// Line of sight check. The point isn't visible if the line trace finds something between the camera and the point (that isn't the actor itself)
	if (actor->GetWorld()->LineTraceSingleByChannel(hitResult, losTrackedCamera_->GetComponentLocation(), actorLocation, ECollisionChannel::ECC_Visibility)
		&& hitResult.GetActor() != actor)
	{
		// Parking spaces use HISMs so checking which one was hit is more complicated
		if (!hitResult.GetActor()->IsA(AParkingController::StaticClass())
			|| !hitResult.GetComponent()->IsA(UHierarchicalInstancedStaticMeshComponent::StaticClass())
			|| !actor->IsA(AParkingSpace::StaticClass()))
		{
			//Debug::DrawTemporaryLine(actor->GetWorld(), losTrackedCamera_->GetComponentLocation(), actorLocation, FColor::Red, 0.1f);
			//Debug::DrawTemporaryLine(actor->GetWorld(), hitResult.ImpactPoint, hitResult.ImpactPoint + FVector::UpVector * 100.0f, FColor::Purple, 0.1f);

			return false;
		}

		AParkingController* parkingController = Cast<AParkingController>(hitResult.GetActor());
		UHierarchicalInstancedStaticMeshComponent* hism = Cast<UHierarchicalInstancedStaticMeshComponent>(hitResult.GetComponent());
		AParkingSpace* parkingSpace = Cast<AParkingSpace>(actor);

		if (parkingSpace->GetParkingController() != parkingController)
		{
			return false;
		}

		int hismInstance = hitResult.Item;

		if (!parkingController->HismInstanceBelongsToParkingSpace(hism, hismInstance, parkingSpace))
		{
			//Debug::DrawTemporaryLine(actor->GetWorld(), losTrackedCamera_->GetComponentLocation(), actorLocation, FColor::Red, 0.1f);
			//Debug::DrawTemporaryLine(actor->GetWorld(), hitResult.ImpactPoint, hitResult.ImpactPoint + FVector::UpVector * 100.0f, FColor::Purple, 0.1f);

			return false;
		}
	}

	//Debug::DrawTemporaryLine(actor->GetWorld(), losTrackedCamera_->GetComponentLocation(), actorLocation, FColor::Green, 0.1f);

	return true;
}

bool UPubSubCommunicator::CheckCollisionRisk(const FVector& EgoPos, const FVector& EgoVel, float EgoRadius, const FVector& OtherPos, const FVector& OtherVel, float OtherRadius, float MaxPredictionTime, FTrafficCollisionResult& OutResult)
{
	const double EgoSpeedSq = EgoVel.SizeSquared();

	// no ego motion -> no warning
	if (EgoSpeedSq < FMath::Square(5.0f))
	{
		return false;
	}

	FVector RelPos = OtherPos - EgoPos;
	FVector RelVel = OtherVel - EgoVel;

	double VelSq = RelVel.SizeSquared();

	if (VelSq < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// Not approaching -> ignore
   /* if (FVector::DotProduct(RelPos, RelVel) >= 0.0f)
	{
		return false;
	}*/

	double t = -FVector::DotProduct(RelPos, RelVel) / VelSq;
	t = FMath::Clamp(t, 0.0f, MaxPredictionTime);

	FVector ClosestRelPos = RelPos + RelVel * t;
	double DistSq = ClosestRelPos.SizeSquared();

	double CombinedRadius = EgoRadius + OtherRadius;

	if (DistSq <= CombinedRadius * CombinedRadius)
	{
		OutResult.bWillCollide = true;
		OutResult.TimeToImpact = t;
		OutResult.ClosestDistance = FMath::Sqrt(DistSq);
		return true;
	}

	return false;
}

void UPubSubCommunicator::PublishInternal(FString topic, uint8_t* data, const size_t& size)
{
	if (publisher_ == nullptr)
	{
		return;
	}

	publisher_->Publish(topicPrefix_ + std::string(TCHAR_TO_UTF8(*topic)), data, size);
}

void UPubSubCommunicator::ApplyCommandLineParameters()
{
	FString parsedEgoValue;

	if (FParse::Value(FCommandLine::Get(), TEXT("PublishEgoVehicle="), parsedEgoValue))
	{
		publishEgoVehicleData_ = parsedEgoValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	FString parsedCarsValue;

	if (FParse::Value(FCommandLine::Get(), TEXT("PublishCars="), parsedCarsValue))
	{
		publishCarData_ = parsedCarsValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	FString parsedParkedValue;

	if (FParse::Value(FCommandLine::Get(), TEXT("PublishParkedCars="), parsedParkedValue))
	{
		publishParkedCarData_ = parsedParkedValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	FString parsedPedestriansValue;

	if (FParse::Value(FCommandLine::Get(), TEXT("PublishPedestrians="), parsedPedestriansValue))
	{
		publishPedestrianData_ = parsedPedestriansValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	FString parsedCyclistsValue;

	if (FParse::Value(FCommandLine::Get(), TEXT("PublishCyclists="), parsedCyclistsValue))
	{
		publishCyclistData_ = parsedCyclistsValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}
}
