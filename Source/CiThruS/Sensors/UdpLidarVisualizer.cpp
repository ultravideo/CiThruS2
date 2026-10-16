#include "UdpLidarVisualizer.h"

#include "Common/UdpSocketBuilder.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
	// Velodyne data packet layout (matches both real sensors and MetaLidar's GenerateDataPacket):
	// 12 blocks of [0xFF 0xEE][azimuth uint16 LE, 0.01 deg][32 x (distance uint16 LE, 2 mm units;
	// intensity uint8)] followed by [timestamp uint32 LE][return mode][model] = 1206 bytes.
	constexpr int32 PacketSize = 1206;
	constexpr int32 BlockCount = 12;
	constexpr int32 BlockStride = 100;
	constexpr int32 ChannelsPerBlock = 32;

	uint16 ReadU16(const uint8* data)
	{
		return static_cast<uint16>(data[0] | (data[1] << 8));
	}
}

AUdpLidarVisualizer::AUdpLidarVisualizer()
{
	PrimaryActorTick.bCanEverTick = true;

	// A scene root is required for this actor to have a usable world transform.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// VLP-16 factory elevation table, in packet channel order (also what MetaLidar uses)
	elevationAnglesDeg_ = { -15.f, 1.f, -13.f, 3.f, -11.f, 5.f, -9.f, 7.f,
		-7.f, 9.f, -5.f, 11.f, -3.f, 13.f, -1.f, 15.f };
}

void AUdpLidarVisualizer::BeginPlay()
{
	Super::BeginPlay();

	if (enabled_) 
		OpenSocket();

	packetBuffer_.SetNumUninitialized(PacketSize);
}

void AUdpLidarVisualizer::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	CloseSocket();

	Super::EndPlay(endPlayReason);
}

bool AUdpLidarVisualizer::OpenSocket()
{
	// Non-blocking receive socket; Tick polls it on the game thread so no extra threads or
	// cross-thread marshaling are needed. The buffer absorbs a few frames' worth of packets
	// (VLP-16 @ 10 Hz is ~754 packets/s, ~16 kB per 60 fps frame).
	socket_ = FUdpSocketBuilder(TEXT("UdpLidarVisualizer"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToPort(listenPort_)
		.WithReceiveBufferSize(2 * 1024 * 1024);

	if (socket_ == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UdpLidarVisualizer: failed to bind UDP port %d, is someone already listening?"), listenPort_);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UdpLidarVisualizer: listening on UDP port %d"), listenPort_);
	return true;
}

void AUdpLidarVisualizer::CloseSocket()
{
	if (socket_ != nullptr)
	{
		ISocketSubsystem::Get()->DestroySocket(socket_);
		socket_ = nullptr;
	}
}

void AUdpLidarVisualizer::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	if (socket_ == nullptr)
	{
		return;
	}

	// mountActor_ overrides; otherwise anchor to this actor's own transform, which now (with a root
	// component) correctly follows the vehicle when attached as a Child Actor Component.
	const AActor* mount = mountActor_ != nullptr ? mountActor_.Get() : this;
	const FTransform sensorTransform = mount->GetActorTransform();

	TSharedRef<FInternetAddr> sender = ISocketSubsystem::Get()->CreateInternetAddr();

	uint32 pendingSize = 0;

	for (int32 packetIndex = 0; packetIndex < maxPacketsPerTick_ && socket_->HasPendingData(pendingSize); packetIndex++)
	{
		int32 bytesRead = 0;

		if (pendingSize > static_cast<uint32>(packetBuffer_.Num()))
		{
			packetBuffer_.SetNumUninitialized(pendingSize);
		}

		if (!socket_->RecvFrom(packetBuffer_.GetData(), packetBuffer_.Num(), bytesRead, *sender))
		{
			break;
		}

		// Drain but don't draw while disabled, and ignore anything that isn't a data packet
		// (e.g. the 512-byte Velodyne position packets)
		if (!enabled_ || bytesRead != PacketSize)
		{
			continue;
		}

		if (!loggedFirstPacket_)
		{
			loggedFirstPacket_ = true;
			UE_LOG(LogTemp, Log, TEXT("UdpLidarVisualizer: first lidar packet received from %s"),
				*sender->ToString(true));
		}

		DecodeAndDrawPacket(packetBuffer_.GetData(), sensorTransform);
	}
}

void AUdpLidarVisualizer::DecodeAndDrawPacket(const uint8* packet, const FTransform& sensorTransform)
{
	const int32 lasersPerFiring = elevationAnglesDeg_.Num();

	if (lasersPerFiring <= 0 || ChannelsPerBlock % lasersPerFiring != 0)
	{
		return;
	}

	UWorld* world = GetWorld();
	const int32 drawEveryNth = FMath::Max(1, drawEveryNthPoint_);
	int32 pointCounter = 0;

	// Block azimuths, for interpolating the second firing sequence of two-firing models (VLP-16
	// packs two 16-laser firings per block; the second one is half an inter-block step ahead)
	float blockAzimuths[BlockCount];

	for (int32 block = 0; block < BlockCount; block++)
	{
		blockAzimuths[block] = ReadU16(packet + block * BlockStride + 2) * 0.01f;
	}

	for (int32 block = 0; block < BlockCount; block++)
	{
		const uint8* blockData = packet + block * BlockStride;

		// 0xFFEE marks a valid data block
		if (blockData[0] != 0xFF || blockData[1] != 0xEE)
		{
			continue;
		}

		float azimuthHalfStep = 0.0f;

		if (block + 1 < BlockCount)
		{
			const float diff = FMath::Fmod(blockAzimuths[block + 1] - blockAzimuths[block] + 360.0f, 360.0f);

			// Sanity bound: consecutive blocks should only ever be a fraction of a degree apart
			if (diff < 10.0f)
			{
				azimuthHalfStep = diff * 0.5f;
			}
		}

		for (int32 channel = 0; channel < ChannelsPerBlock; channel++)
		{
			const uint8* channelData = blockData + 4 + channel * 3;
			const uint16 rawDistance = ReadU16(channelData);

			// Distance 0 means no return for this laser
			if (rawDistance == 0)
			{
				continue;
			}

			if (++pointCounter % drawEveryNth != 0)
			{
				continue;
			}

			const float rangeCm = rawDistance * 0.2f; // 2 mm units -> cm

			const int32 firing = channel / lasersPerFiring;
			const float elevationDeg = elevationAnglesDeg_[channel % lasersPerFiring];
			const float azimuthDeg = blockAzimuths[block] + firing * azimuthHalfStep;

			// Same convention the sensor sim traces with: pitch = elevation, yaw = azimuth,
			// composed onto the sensor's world rotation
			const FVector localOffset = FRotator(elevationDeg, azimuthDeg, 0.0f).Vector() * rangeCm;
			const FVector worldPos = sensorTransform.TransformPositionNoScale(localOffset);

			DrawDebugPoint(world, worldPos, pointSize_, pointColor_, false, pointLifetime_);
		}
	}
}
