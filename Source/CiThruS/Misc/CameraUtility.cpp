#include "CameraUtility.h"
#include "Debug.h"


bool CameraUtility::GetViewportDimensions(UWorld* world, FVector2D& viewportSize)
{
	if (world == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraUtility::GetViewportDimensions: world is null!"));
		return false;
	}
	
	APlayerController* playerController = world->GetFirstPlayerController();
	//viewportSize = FVector2D::ZeroVector;
	if (playerController == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraUtility::GetViewportDimensions: no player controller found!"));
		return false; 
	}

	ULocalPlayer* localPlayer = playerController->GetLocalPlayer();
	if (localPlayer == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraUtility::GetViewportDimensions: no local player found!"));
		return false;
	}

	UGameViewportClient* viewportClient = localPlayer->ViewportClient;
	if (viewportClient)
	{
		viewportClient->GetViewportSize(viewportSize);
		return true;		
	} else	{
		UE_LOG(LogTemp, Warning, TEXT("CameraUtility::GetViewportDimensions: no viewport client found!"));
		return false;
	}
	
}
