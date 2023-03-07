#include "Debug.h"
#include "DrawDebugHelpers.h"

void Debug::Log(const FString& message)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, message);
	}
}

void Debug::DrawTemporaryLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& duration, const float& width)
{
	if (world != nullptr)
	{
		DrawDebugLine(world, start, end, color, false, duration, -1, width);
	}
}

void Debug::DrawPersistentLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& width)
{
	if (world != nullptr)
	{
		DrawDebugLine(world, start, end, color, true, 1.0f, -1, width);
	}
}

void Debug::DeletePersistentLines(UWorld* world)
{
	if (world != nullptr)
	{
		FlushPersistentDebugLines(world);
	}
}
