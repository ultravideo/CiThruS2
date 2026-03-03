#include "PubSubCommunicator.h"
#include "MqttPublisher.h"
#include "Traffic/Entities/Car.h"
#include "Traffic/Entities/Pedestrian.h"
#include "Traffic/Entities/Bicycle.h"
#include "Misc/MathUtility.h"
#include <format>
#include <cmath>

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
	if (actor == nullptr)
	{
		return;
	}

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	std::string timestamp = std::format("{:%FT%TZ}", now);
	std::string uniqueId = std::to_string(reinterpret_cast<std::uintptr_t>(actor));
	std::string type = "unknown";

	if (actor->IsA(ACar::StaticClass()))
	{
		type = "car";

		if (!publishCarData_)
		{
			return;
		}
	}
	else if (actor->IsA(APedestrian::StaticClass()))
	{
		type = "pedestrian";

		if (!publishPedestrianData_)
		{
			return;
		}
	}
	else if (actor->IsA(ABicycle::StaticClass()))
	{
		type = "bicycle";

		if (!publishCyclistData_)
		{
			return;
		}
	}
	else if (actor->IsA(APawn::StaticClass()) && Cast<APawn>(actor)->IsPlayerControlled())
	{
		type = "self";

		if (!publishEgoVehicleData_)
		{
			return;
		}
	}

	FVector position = MathUtility::UnrealEngineCoordsToWgs84AndAltitude(actor->GetActorLocation());
	FRotator rotation = MathUtility::UnrealEngineRotationToRealLifeRotation(actor->GetActorRotation().Quaternion()).Rotator();
	FVector velocity = FVector::ZeroVector;
	FVector direction = FVector::ZeroVector;
	float speed = 0.0f;
	FVector acceleration = FVector::ZeroVector;
	FRotator angularVelocity = FRotator::ZeroRotator;
	FVector heading = MathUtility::UnrealEngineDirectionToRealLifeDirection(actor->GetActorForwardVector());

	ITrafficEntity* trafficEntity = dynamic_cast<ITrafficEntity*>(actor);

	if (trafficEntity != nullptr)
	{
		direction = trafficEntity->GetMoveDirection();
		speed = trafficEntity->GetMoveSpeed() / 100;
		velocity = direction * speed;
	}
	else
	{
		velocity = MathUtility::UnrealEngineDirectionToRealLifeDirection(actor->GetVelocity()) / 100;
		direction = velocity.GetSafeNormal();
		speed = velocity.Length();
	}

	auto lastPublishedData = lastPublishedTrafficEntityData_.find(actor);

	// UE doesn't track acceleration or angular velocity so have to track them ourselves
	if (lastPublishedData != lastPublishedTrafficEntityData_.end())
	{
		double deltaTimeInverse = 1.0 / std::chrono::duration<double, std::ratio<1, 1>>(now - lastPublishedData->second.timestamp).count();

		acceleration = (velocity - lastPublishedData->second.velocity) * deltaTimeInverse;
		angularVelocity = (rotation - lastPublishedData->second.rotation) * deltaTimeInverse;
	}

	bool fixStatus = true;
	FVector antennaPosition = FVector::ZeroVector; // TODO not sure how this should be specified

	float horizontalAccuracy = 1.0e-6; // TODO not sure if this is how this was meant to work
	float verticalAccuracy = 1.0e-6; // TODO not sure if this is how this was meant to work

	float angularSpeed = std::sqrt(angularVelocity.Pitch * angularVelocity.Pitch + angularVelocity.Yaw * angularVelocity.Yaw + angularVelocity.Roll * angularVelocity.Roll);

	std::string valueAsString
		= "{\n"
		  "\t\"timestamp\": \"" + timestamp + "\",\n"
		  "\t\"id\": " + uniqueId + ",\n"
		  "\t\"type\": \"" + type + "\",\n"
		  "\t\"position\": [" + std::to_string(position.X) + ", " + std::to_string(position.Y) + ", " + std::to_string(position.Z) + "],\n"
		  "\t\"direction\": [" + std::to_string(direction.X) + ", " + std::to_string(direction.Y) + ", " + std::to_string(direction.Z) + ", " + std::to_string(speed) + "],\n"
		  "\t\"acceleration\": [" + std::to_string(acceleration.X) + ", " + std::to_string(acceleration.Y) + ", " + std::to_string(acceleration.Z) + "],\n"
		  "\t\"angularVelocity\": [" + std::to_string(angularVelocity.Roll) + ", " + std::to_string(angularVelocity.Pitch) + ", " + std::to_string(angularVelocity.Yaw) + "],\n"
		  "\t\"heading\": [" + std::to_string(heading.X) + ", " + std::to_string(heading.Y) + ", " + std::to_string(heading.Z) + "],\n"
		  "\t\"angularSpeed\": " + std::to_string(angularSpeed) + ",\n"
		  "\t\"fixStatus\": " + (fixStatus ? "true" : "false") + ",\n"
		  "\t\"antennaPosition\": [" + std::to_string(antennaPosition.X) + ", " + std::to_string(antennaPosition.Y) + ", " + std::to_string(antennaPosition.Z) + "],\n"
		  "\t\"altitude\": " + std::to_string(position.Z) + ",\n"
		  "\t\"horizontalAccuracy\": " + std::to_string(horizontalAccuracy) + ",\n"
		  "\t\"verticalAccuracy\": " + std::to_string(verticalAccuracy) + "\n"
		  "}";

	lastPublishedTrafficEntityData_[actor] = { now, velocity, rotation };

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

void UPubSubCommunicator::SetPublishPedestrianData(bool value)
{
	publishPedestrianData_ = value;
}

void UPubSubCommunicator::SetPublishCyclistData(bool value)
{
	publishCyclistData_ = value;
}

void UPubSubCommunicator::PublishInternal(FString topic, uint8_t* data, const size_t& size)
{
	if (publisher_ == nullptr)
	{
		return;
	}

	publisher_->Publish(topicPrefix_ + std::string(TCHAR_TO_UTF8(*topic)), data, size);
}
