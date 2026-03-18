#include "PubSubCommunicator.h"
#include "MqttPublisher.h"
#include "Traffic/Entities/Car.h"
#include "Traffic/Entities/Pedestrian.h"
#include "Traffic/Entities/Bicycle.h"
#include "Traffic/Parking/ParkingSpace.h"
#include "Misc/GeoUtility.h"
#include "GeoReferencingSystem.h"
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

	std::string valueAsString
		= "{\n"
		"\t\"Timestamp\": \"" + std::format("{:%FT%TZ}", now) + "\",\n"
		"\t\"Vehicle\": {\n"
		"\t\t\"Powertrain\": {\n"
		"\t\t\t\"Transmission\": {\n"
		"\t\t\t\t\"SelectedGear\": " + std::to_string(selectedGear) + "\n"
		"\t\t\t}\n"
		"\t\t},\n"
		"\t\t\"CurrentLocation\": {\n"
		"\t\t\t\"Latitude\": " + std::format("{:.8f}", geoData.geographicCoordinates.Latitude) + ",\n"
		"\t\t\t\"Longitude\": " + std::format("{:.8f}", geoData.geographicCoordinates.Longitude) + ",\n"
		"\t\t\t\"Altitude\": " + std::format("{:.8f}", geoData.geographicCoordinates.Altitude) + "\n"
		"\t\t},\n"
		"\t\t\"CurrentRotation\": {\n"
		"\t\t\t\"Roll\": " + std::format("{:.8f}", geoData.tangentRotation.Roll) + ",\n"
		"\t\t\t\"Pitch\": " + std::format("{:.8f}", geoData.tangentRotation.Pitch) + ",\n"
		"\t\t\t\"Yaw\": " + std::format("{:.8f}", geoData.tangentRotation.Yaw) + "\n"
		"\t\t},\n"
		"\t\t\"LinearVelocity\": {\n"
		"\t\t\t\"Lateral\": " + std::format("{:.8f}", geoData.linearVelocityEnuMps.X) + ",\n"
		"\t\t\t\"Longitudinal\": " + std::format("{:.8f}", geoData.linearVelocityEnuMps.Y) + ",\n"
		"\t\t\t\"Vertical\": " + std::format("{:.8f}", geoData.linearVelocityEnuMps.Z) + "\n"
		"\t\t},\n"
		"\t\t\"AngularVelocity\": {\n"
		"\t\t\t\"Roll\": " + std::format("{:.8f}", geoData.tangentAngularVelocity.Roll) + ",\n"
		"\t\t\t\"Pitch\": " + std::format("{:.8f}", geoData.tangentAngularVelocity.Pitch) + ",\n"
		"\t\t\t\"Yaw\": " + std::format("{:.8f}", geoData.tangentAngularVelocity.Yaw) + "\n"
		"\t\t}\n"
		"\t}\n"
		"}";

	lastPublishedEgoActor_ = actor;

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::PublishTrafficArray(FString topic, const TArray<AActor*>& trafficEntities)
{
	const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	TSet<AActor*> sentTrafficActors;

	std::string trafficItems;
	bool firstTrafficItem = true;

	for (AActor* actor : trafficEntities)
	{
		if (!IsValid(actor))
		{
			continue;
		}

		std::string type = "Unknown";

		auto existingId = trafficEntityIds_.find(actor);

		if (existingId == trafficEntityIds_.end())
		{
			std::string newId = std::to_string(lastUsedTrafficEntityId_);

			lastUsedTrafficEntityId_++;

			existingId = trafficEntityIds_.insert({ actor, newId }).first;
		}

		const std::string& uniqueId = existingId->second;

		bool isParkedCar = false;

		FVector worldLocation = FVector::ZeroVector;
		FQuat worldQuat = FQuat::Identity;
		FVector unrealLinearVelocity = FVector::ZeroVector;

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
		}
		else if (actor->IsA(ACar::StaticClass()))
		{
			if (!publishCarData_)
			{
				continue;
			}

			type = "Car";
		}
		else if (actor->IsA(APedestrian::StaticClass()))
		{
			if (!publishPedestrianData_)
			{
				continue;
			}

			type = "Pedestrian";
		}
		else if (actor->IsA(ABicycle::StaticClass()))
		{
			if (!publishCyclistData_)
			{
				continue;
			}

			type = "Bicycle";
		}
		else
		{
			continue;
		}

		if (!isParkedCar)
		{
			worldLocation = actor->GetActorLocation();
			worldQuat = actor->GetActorQuat();
			unrealLinearVelocity = actor->GetVelocity();
		}

		GeoData geoData;

		if (!GetGeoData(actor, worldLocation, unrealLinearVelocity, worldQuat, now, geoData))
		{
			continue;
		}

		std::string trafficItem
			= "\t\t{\n"
			"\t\t\t\"Id\": " + uniqueId + ",\n"
			"\t\t\t\"Type\": \"" + type + "\",\n"
			"\t\t\t\"CurrentLocation\": {\n"
			"\t\t\t\t\"Latitude\": " + std::format("{:.8f}", geoData.geographicCoordinates.Latitude) + ",\n"
			"\t\t\t\t\"Longitude\": " + std::format("{:.8f}", geoData.geographicCoordinates.Longitude) + ",\n"
			"\t\t\t\t\"Altitude\": " + std::format("{:.8f}", geoData.geographicCoordinates.Altitude) + "\n"
			"\t\t\t},\n"
			"\t\t\t\"CurrentRotation\": {\n"
			"\t\t\t\t\"Roll\": " + std::format("{:.8f}", geoData.tangentRotation.Roll) + ",\n"
			"\t\t\t\t\"Pitch\": " + std::format("{:.8f}", geoData.tangentRotation.Pitch) + ",\n"
			"\t\t\t\t\"Yaw\": " + std::format("{:.8f}", geoData.tangentRotation.Yaw) + "\n"
			"\t\t\t},\n"
			"\t\t\t\"LinearVelocity\": {\n"
			"\t\t\t\t\"Lateral\": " + std::format("{:.8f}", geoData.linearVelocityEnuMps.X) + ",\n"
			"\t\t\t\t\"Longitudinal\": " + std::format("{:.8f}", geoData.linearVelocityEnuMps.Y) + ",\n"
			"\t\t\t\t\"Vertical\": " + std::format("{:.8f}", geoData.linearVelocityEnuMps.Z) + "\n"
			"\t\t\t},\n"
			"\t\t\t\"AngularVelocity\": {\n"
			"\t\t\t\t\"Roll\": " + std::format("{:.8f}", geoData.tangentAngularVelocity.Roll) + ",\n"
			"\t\t\t\t\"Pitch\": " + std::format("{:.8f}", geoData.tangentAngularVelocity.Pitch) + ",\n"
			"\t\t\t\t\"Yaw\": " + std::format("{:.8f}", geoData.tangentAngularVelocity.Yaw) + "\n"
			"\t\t\t}\n"
			"\t\t}";

		if (!firstTrafficItem)
		{
			trafficItems += ",\n";
		}

		firstTrafficItem = false;
		trafficItems += trafficItem;
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

	std::string valueAsString
		= "{\n"
		"\t\"Timestamp\": \"" + std::format("{:%FT%TZ}", now) + "\",\n"
		"\t\"Traffic\": [";

	if (!trafficItems.empty())
	{
		valueAsString += "\n";
		valueAsString += trafficItems;
		valueAsString += "\n\t";
	}

	valueAsString += "]\n}";

	PublishInternal(topic, reinterpret_cast<uint8_t*>(valueAsString.data()), valueAsString.size());
}

void UPubSubCommunicator::StartMqttClient(FString serverUri, FString username, FString password, int maxMsgsPerSecond)
{
	publisher_ = TSharedPtr<IPublisher>(
		new MqttPublisher(
			TCHAR_TO_UTF8(*serverUri),
			TCHAR_TO_UTF8(*username),
			TCHAR_TO_UTF8(*password),
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

void UPubSubCommunicator::PublishInternal(FString topic, uint8_t* data, const size_t& size)
{
	if (publisher_ == nullptr)
	{
		return;
	}

	publisher_->Publish(topicPrefix_ + std::string(TCHAR_TO_UTF8(*topic)), data, size);
}
