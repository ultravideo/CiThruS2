#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

#include <string>

// This namespace is for cleaner usage of Unreal Engine debug functions with shorter syntax and without
// having to check whether the engine or world pointers are null.
namespace Debug
{
	CITHRUS_API void Log(const FString& message);

	CITHRUS_API inline void Log(const int& message) { Log(FString::FromInt(message)); }

	CITHRUS_API inline void Log(const float& message) { Log(FString::SanitizeFloat(message)); }

	CITHRUS_API inline void Log(const bool& message) { Log(message ? FString("True") : FString("False")); }

	CITHRUS_API inline void Log(const FVector& message) { Log(message.ToString()); }

	CITHRUS_API inline void Log(const FVector2D& message) { Log(message.ToString()); }

	CITHRUS_API inline void Log(const FRotator& message) { Log(message.ToString()); }

	CITHRUS_API inline void Log(const UObject* message) { Log(message ? message->GetName() : FString("Null")); }

	CITHRUS_API inline void Log(const char* message) { Log(FString(message)); }

	CITHRUS_API inline void Log(const std::string& message) { Log(FString(message.data())); }

	CITHRUS_API void LogBinary(const int32& value);

	CITHRUS_API void DrawTemporaryLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& duration, const float& width = 20.0f);
	CITHRUS_API void DrawPersistentLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& width = 20.0f);
	CITHRUS_API void DeletePersistentLines(UWorld* world);

	CITHRUS_API void DrawTemporarySphere(UWorld* world, const FVector& center, const FColor& color, const float& duration, const float& radius = 30.0f);

	CITHRUS_API void DrawTemporaryRect(UWorld* world, const FVector& a, const FVector& b, const FVector& c, const FVector& d, const FVector& up, const float& height, const float& lineWidth, const FColor& color, const float& duration);
	CITHRUS_API void DrawTemporaryRect(UWorld* world, const FVector& center, const FVector& forward, const float& length = 100.0f, const float& width = 100.0f, const float& height = 100.0f, const float& lineWidth = 5.0f, const FColor& color = FColor::Red, const float& duration = 5.0f);
	CITHRUS_API void DrawTemporaryRect(UWorld* world, const FVector& center, const FVector& forward = FVector::ForwardVector, const FVector& dimensions = FVector(100.0f, 100.0f, 100.0f), const float& lineWidth = 5.0f, const FColor& color = FColor::Red, const float& duration = 5.0f);

	CITHRUS_API void DrawTemporaryArrow(UWorld* world, const FVector& location, const FVector& forward, const float& duration = 5.0f, const FColor& color = FColor::Red, const float& length = 100.0f, const float thickness = 5.0f);
}
