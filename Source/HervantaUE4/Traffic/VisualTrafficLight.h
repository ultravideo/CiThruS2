#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisualTrafficLight.generated.h"

// One bit for each light, allows for bitwise operations if needed
UENUM(BlueprintType)
enum class ETrafficLightState : uint8
{
	Off = 0,
	Red = 0b0001,
	Yellow = 0b0010,
	RedYellow = Red | Yellow,
	Green = 0b0100
};

// This would be better as an interface but Unreal Engine has an issue where it thinks
// blueprints don't implement C++ interfaces even though they do and so the interface
// doesn't work correctly in the editor
UCLASS()
class HERVANTAUE4_API AVisualTrafficLight : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	void SetLightState(const ETrafficLightState& lightState);
	void SetLightState_Implementation(const ETrafficLightState& lightState) { };

protected:
	AVisualTrafficLight() { };
};
