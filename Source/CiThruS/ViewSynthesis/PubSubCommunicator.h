#pragma once

#include "CoreMinimal.h"

#include <string>
#include <chrono>
#include <unordered_map>

#include "PubSubCommunicator.generated.h"

class IPublisher;

UCLASS()
class CITHRUS_API UPubSubCommunicator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static void PublishBool(FString topic, bool value);

	UFUNCTION(BlueprintCallable)
	static void PublishInt(FString topic, int value);

	UFUNCTION(BlueprintCallable)
	static void PublishFloat(FString topic, float value);

	UFUNCTION(BlueprintCallable)
	static void PublishString(FString topic, FString value);

	UFUNCTION(BlueprintCallable)
	static void Publish2DVector(FString topic, FVector2D value);

	UFUNCTION(BlueprintCallable)
	static void Publish3DVector(FString topic, FVector value);

	UFUNCTION(BlueprintCallable)
	static void Publish4DVector(FString topic, FVector4 value);

	UFUNCTION(BlueprintCallable)
	static void PublishTrafficEntity(FString topic, AActor* trafficEntity);

	UFUNCTION(BlueprintCallable)
	static void StartMqttClient(FString serverUri, FString username, FString password, int maxMsgsPerSecond);

	UFUNCTION(BlueprintCallable)
	static void StopAllClients();

	UFUNCTION(BlueprintCallable)
	static void SetTopicPrefix(FString prefix);

	UFUNCTION(BlueprintCallable)
	static void SetPublishEgoVehicleData(bool value);

	UFUNCTION(BlueprintCallable)
	static void SetPublishCarData(bool value);

	UFUNCTION(BlueprintCallable)
	static void SetPublishPedestrianData(bool value);

	UFUNCTION(BlueprintCallable)
	static void SetPublishCyclistData(bool value);

private:
	struct TrafficEntityData
	{
		std::chrono::system_clock::time_point timestamp;
		FVector velocity;
		FRotator rotation;
	};

	UPubSubCommunicator() { }

	inline static TSharedPtr<IPublisher> publisher_ = nullptr;

	inline static std::string topicPrefix_ = "CiThruS2/";

	inline static bool publishEgoVehicleData_ = true;
	inline static bool publishCarData_ = true;
	inline static bool publishPedestrianData_ = true;
	inline static bool publishCyclistData_ = true;

	inline static std::unordered_map<AActor*, TrafficEntityData> lastPublishedTrafficEntityData_;

	static void PublishInternal(FString topic, uint8_t* data, const size_t& size);
};
