#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UdpLidarVisualizer.generated.h"

class FSocket;

// Listens for Velodyne-protocol LIDAR data packets on a UDP port and draws the decoded point cloud with DrawDebugPoint.

UCLASS()
class CITHRUS_API AUdpLidarVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AUdpLidarVisualizer();

	virtual void Tick(float deltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "UDP Lidar Visualizer")
	bool OpenSocket();

	UFUNCTION(BlueprintCallable, Category = "UDP Lidar Visualizer")
	void CloseSocket();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer")
	bool enabled_ = true;

	// UDP port to listen on. 2368 is the Velodyne (and MetaLidar) data port.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer")
	int32 listenPort_ = 2368;

	// Optional override for the actor whose transform anchors the point cloud. Usually leave this
	// null and instead attach this actor to the sensor as a Child Actor Component - it has a scene
	// root, so its own transform then follows the mount. Set this only when the visualizer can't be
	// parented to the sensor (e.g. a standalone actor).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer")
	TObjectPtr<AActor> mountActor_ = nullptr;

	// Per-laser elevation angles in degrees, in packet channel order. Defaults to the VLP-16
	// factory table (which MetaLidar also uses). Replace with a 32-entry table for 32-laser models.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer")
	TArray<float> elevationAnglesDeg_;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer")
	FColor pointColor_ = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer|Draw")
	float pointSize_ = 5.0f;

	// How long each decoded point stays visible, in seconds. Roughly one sensor rotation gives a
	// continuous full-sweep image (0.1 s at 10 Hz; a little over hides rotation seams).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer|Draw")
	float pointLifetime_ = 0.15f;

	// Draw every Nth decoded point. 1 = all points. Raise this if debug drawing starts to cost
	// frame time at full sensor rate (VLP-16 @ 10 Hz is ~300k points/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer|Draw", meta = (ClampMin = "1"))
	int32 drawEveryNthPoint_ = 1;

	// Safety cap on packets processed per Tick so a flooded socket can't stall the game thread.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Lidar Visualizer|Advanced", meta = (ClampMin = "1"))
	int32 maxPacketsPerTick_ = 512;

private:
	FSocket* socket_ = nullptr;

	TArray<uint8> packetBuffer_;

	bool loggedFirstPacket_ = false;

	// Decodes one 1206-byte Velodyne data packet and draws its points.
	void DecodeAndDrawPacket(const uint8* packet, const FTransform& sensorTransform);
};
