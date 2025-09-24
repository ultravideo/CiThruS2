#include "TrafficSnapViewSynthesizer.h"
#include "Traffic/TrafficController.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Components/SceneCaptureComponent2D.h"
#include <Kismet/GameplayStatics.h>
#include "EngineUtils.h"

void ATrafficSnapViewSynthesizer::BeginPlay()
{
	Super::BeginPlay();

	// Get ref to traffic controller 
	TArray<AActor*> find;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATrafficController::StaticClass(), find);

	if (find.Num() <= 0)
	{
		return;
	}

	trafficController_ = Cast<ATrafficController>(find[0]);
}

void ATrafficSnapViewSynthesizer::Tick(float deltaTime)
{
	if (trafficController_)
	{
		ITrafficEntity* entityInFrontOfPlayer = trafficController_->GetEntityInFrontOfPlayer();

		if (entityInFrontOfPlayer)
		{
			const CollisionRectangle& frontCollisionRectangle = entityInFrontOfPlayer->GetCollisionRectangle();
			frontCamera_->SetWorldLocationAndRotation(frontCollisionRectangle.GetPosition() + frontCollisionRectangle.GetRotation() * FVector(frontCollisionRectangle.GetDimensions().X * 0.5f, 0.0f, 0.0f), frontCollisionRectangle.GetRotation());
		}

		for (TActorIterator<APawn> it = TActorIterator<APawn>(GetWorld()); it; it.operator++())
		{
			if (*it == nullptr)
			{
				continue;
			}

			rearCamera_->SetWorldLocationAndRotation(it->GetActorLocation() + it->GetActorRotation().Quaternion() * (cameraOffset_ * 100.0f), it->GetActorRotation().Quaternion() * cameraRotation_.Quaternion());

			break;
		}
	}
	
	Super::Tick(deltaTime);
}
