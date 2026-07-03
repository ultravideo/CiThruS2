#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#include <vector>
#include <unordered_map>
#include <bitset>

#include "SegmentationController.generated.h"

UENUM(BlueprintType)
enum class ESegmentationClass : uint8
{
	None	     UMETA(Hidden),
	Person       UMETA(DisplayName = "Person"),
	Car          UMETA(DisplayName = "Car"),
	Bicycle      UMETA(DisplayName = "Bicycle"),
	TrafficLight UMETA(DisplayName = "Traffic Light"),
	Max	         UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ESegmentationType : uint8
{
	Semantic UMETA(DisplayName = "Semantic Segmentation"),
	Instance UMETA(DisplayName = "Instance Segmentation")
};

struct SegmentedObject
{
	uint32_t trackingId;
	ESegmentationClass classId;
	TWeakObjectPtr<UPrimitiveComponent> ueComponent;
	TWeakObjectPtr<UPrimitiveComponent> parentUeComponent;
	FColor maskColor;
};

class USceneCaptureComponent2D;

UCLASS()
class CITHRUS_API USegmentationController : public UObject
{
	GENERATED_BODY()
	
public:
	// Registers a segmentable object for tracking
	UFUNCTION(BlueprintCallable)
	static void RegisterObject(UPrimitiveComponent* component, ESegmentationClass segmentationClass);

	// Registers an object with the same tracking ID and class as the parent object (parent object must already be registered)
	UFUNCTION(BlueprintCallable)
	static void RegisterSubobject(UPrimitiveComponent* component, UPrimitiveComponent* parent);

	// Unregisters a segmentable object from tracking
	UFUNCTION(BlueprintCallable)
	static void UnregisterObject(UPrimitiveComponent* component);

	// Sets the type of segmentation OptimizeIndicesForCamera will use
	UFUNCTION(BlueprintCallable)
	static void SetSegmentationType(ESegmentationType segmentationType);

	// Assigns custom stencil indices to tracked objects that may be visible from the given camera and leaves other objects' indices unset.
	// Returns a mapping from the custom stencil indices to the corresponding tracked objects.
	// The purpose of this function is to reuse stencil indices to allow tracking more than 256 objects at the same time
	static std::vector<SegmentedObject>* OptimizeIndicesForCamera(USceneCaptureComponent2D* sceneCapture, const uint16_t& resolutionWidth, const uint16_t& resolutionHeight);

private:
	inline static std::unordered_map<UPrimitiveComponent*, SegmentedObject> objects_;

	//inline static std::bitset<256> trackingIdUsed_;

	inline static uint32_t nextTrackingId_ = 1;

	inline static ESegmentationType segmentationType_ = ESegmentationType::Semantic;

	USegmentationController() {}

	static bool ComponentMayBeVisibleFromCamera(UPrimitiveComponent* component, USceneCaptureComponent2D* sceneCapture, const uint16_t& resolutionWidth, const uint16_t& resolutionHeight);
	static FColor GetMaskColor(const uint32_t& index);
};
