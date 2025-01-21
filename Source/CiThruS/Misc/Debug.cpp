#include "Debug.h"
#include "DrawDebugHelpers.h"

CITHRUS_API void Debug::Log(const FString& message)
{
	if (GEngine == nullptr)
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, message);
}

CITHRUS_API void Debug::LogBinary(const int32& value)
{
	FString BinaryString;
	for (int32 i = 31; i >= 0; --i)
	{
		int32 Mask = 1 << i;
		bool BitSet = (value & Mask) != 0;
		BinaryString += (BitSet ? TEXT("1") : TEXT("0"));
		if (i % 4 == 0)
		{
			BinaryString += TEXT(" ");
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Binary representation of %d is %s"), value, *BinaryString);
}

CITHRUS_API void Debug::DrawTemporaryLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& duration, const float& width)
{
	if (world == nullptr)
	{
		return;
	}

	DrawDebugLine(world, start, end, color, false, duration, -1, width);
}

CITHRUS_API void Debug::DrawPersistentLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& width)
{
	if (world == nullptr)
	{
		return;
	}

	DrawDebugLine(world, start, end, color, true, 1.0f, -1, width);
}

CITHRUS_API void Debug::DeletePersistentLines(UWorld* world)
{
	if (world == nullptr)
	{
		return;
	}

	FlushPersistentDebugLines(world);
}

CITHRUS_API void Debug::DrawTemporarySphere(UWorld* world, const FVector& center, const FColor& color, const float& duration, const float& radius)
{
	if (world == nullptr)
	{
		return;
	}

	DrawDebugSphere(world, center, radius, 12, color, false, duration, -1, 0.0f);
}

CITHRUS_API void Debug::DrawTemporaryRect(UWorld* world, const FVector& a, const FVector& b, const FVector& c, const FVector& d, const FVector& up, const float& height, const float& lineWidth, const FColor& color, const float& duration)
{
	if (world != nullptr)
	{
		FVector au = a + up * 0.5 * height;
		FVector ad = a - up * 0.5 * height;
		FVector bu = b + up * 0.5 * height;
		FVector bd = b - up * 0.5 * height;
		FVector cu = c + up * 0.5 * height;
		FVector cd = c - up * 0.5 * height;
		FVector du = d + up * 0.5 * height;
		FVector dd = d - up * 0.5 * height;
		DrawTemporaryLine(world, au, bu, color, duration, lineWidth);
		DrawTemporaryLine(world, au, du, color, duration, lineWidth);
		DrawTemporaryLine(world, au, ad, color, duration, lineWidth);
		DrawTemporaryLine(world, ad, bd, color, duration, lineWidth);
		DrawTemporaryLine(world, ad, dd, color, duration, lineWidth);
		DrawTemporaryLine(world, cu, bu, color, duration, lineWidth);
		DrawTemporaryLine(world, cu, du, color, duration, lineWidth);
		DrawTemporaryLine(world, cu, cd, color, duration, lineWidth);
		DrawTemporaryLine(world, cd, bd, color, duration, lineWidth);
		DrawTemporaryLine(world, cd, dd, color, duration, lineWidth);
		DrawTemporaryLine(world, bd, bu, color, duration, lineWidth);
		DrawTemporaryLine(world, dd, du, color, duration, lineWidth);
	}
}

CITHRUS_API void Debug::DrawTemporaryRect(UWorld* world, const FVector& center, const FVector& forward, const float& length, const float& width, const float& height, const float& lineWidth, const FColor& color, const float& duration)
{
	FVector right = forward.ToOrientationQuat().GetRightVector();

	FVector a = center + forward * length * 0.5f - right * width * 0.5f;
	FVector b = center + forward * length * 0.5f + right * width * 0.5f;
	FVector c = center - forward * length * 0.5f + right * width * 0.5f;
	FVector d = center - forward * length * 0.5f - right * width * 0.5f;

	DrawTemporaryRect(world, a, b, c, d, forward.UpVector, height, lineWidth, color, duration);
}

CITHRUS_API void Debug::DrawTemporaryRect(UWorld* world, const FVector& center, const FVector& forward, const FVector& dimensions, const float& lineWidth, const FColor& color, const float& duration)
{
	DrawTemporaryRect(world, center, forward, dimensions.X, dimensions.Y, dimensions.Z, lineWidth, color, duration);
}

CITHRUS_API void Debug::DrawTemporaryArrow(UWorld* world, const FVector& location, const FVector& forward, const float& duration, const FColor& color, const float& length, const float thickness)
{
	FVector startLocation = location - forward * length * 0.5f;
	FVector endLocation = location + forward * length * 0.5f;

	DrawDebugDirectionalArrow(world, startLocation, endLocation, 4.0f, color, false, duration, -1, thickness);
}
