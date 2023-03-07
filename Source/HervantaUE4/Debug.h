#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

// This namespace is for cleaner usage of UE4 debug functions with shorter syntax and without
// having to check whether the engine or world pointers are null.
namespace Debug
{
	void Log(const FString& message);

	inline void Log(const int& message) { Log(FString::FromInt(message)); };

	inline void Log(const float& message) { Log(FString::SanitizeFloat(message)); };

	inline void Log(const bool& message) { Log(message ? FString("True") : FString("False")); };

	inline void Log(const FVector& message) { Log(message.ToString()); };

	inline void Log(const FVector2D& message) { Log(message.ToString()); };

	inline void Log(const FRotator& message) { Log(message.ToString()); };

	inline void Log(const UObject* message) { Log(message ? message->GetName() : FString("Null")); };

	inline void Log(const char* message) { Log(FString(message)); };

	void DrawTemporaryLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& duration, const float& width = 20.0f);
	void DrawPersistentLine(UWorld* world, const FVector& start, const FVector& end, const FColor& color, const float& width = 20.0f);
	void DeletePersistentLines(UWorld* world);
}
