#include "SegmentationController.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Misc/Debug.h"

#include <stdexcept>
#include <algorithm>
#include <forward_list>

void USegmentationController::RegisterObject(UPrimitiveComponent* component, ESegmentationClass segmentationClass)
{
	if (objects_.contains(component))
	{
		return;
		//throw std::runtime_error("Object is already registered");
	}

	/*while (nextTrackingId_ < trackingIdUsed_.size() && trackingIdUsed_[nextTrackingId_])
	{
		nextTrackingId_++;
	}

	if (nextTrackingId_ > 255)
	{
		throw std::runtime_error("Can't register any more objects, maximum limit reached for 8-bit integers");
	}*/

	SegmentedObject data{};

	data.trackingId = nextTrackingId_;
	data.classId = segmentationClass;
	data.ueComponent = component;
	data.parentUeComponent = nullptr;

	// Leave space in mask colors for generic classes
	data.maskColor = GetMaskColor(static_cast<int>(ESegmentationClass::Max) + data.trackingId);

	//trackingIdUsed_.set(nextTrackingId_);
	nextTrackingId_++;

	objects_.insert({ component, data });

	/*component->SetRenderCustomDepth(true);
	component->SetCustomDepthStencilValue(data.trackingId);*/
}

void USegmentationController::RegisterSubobject(UPrimitiveComponent* component, UPrimitiveComponent* parent)
{
	auto parentObject = objects_.find(parent);

	if (parentObject == objects_.end())
	{
		return;
		//throw std::runtime_error("Object is not registered");
	}

	if (objects_.contains(component))
	{
		return;
		//throw std::runtime_error("Object is already registered");
	}

	SegmentedObject data{};

	data.trackingId = parentObject->second.trackingId;
	data.classId = parentObject->second.classId;
	data.ueComponent = component;
	data.parentUeComponent = parent;

	// Leave space in mask colors for generic classes
	data.maskColor = GetMaskColor(static_cast<int>(ESegmentationClass::Max) + data.trackingId);

	objects_.insert({ component, data });

	/*component->SetRenderCustomDepth(true);
	component->SetCustomDepthStencilValue(data.trackingId);*/
}

void USegmentationController::UnregisterObject(UPrimitiveComponent* component)
{
	auto existingObject = objects_.find(component);

	if (existingObject == objects_.end())
	{
		return;
		//throw std::runtime_error("Object is not registered");
	}

	/*trackingIdUsed_.reset(existingObject->second.trackingId);

	if (nextTrackingId_ > existingObject->second.trackingId)
	{
		nextTrackingId_ = existingObject->second.trackingId;
	}

	existingObject->first->SetRenderCustomDepth(false);
	existingObject->first->SetCustomDepthStencilValue(0);*/

	objects_.erase(existingObject);
}

void USegmentationController::SetSegmentationType(ESegmentationType segmentationType)
{
	segmentationType_ = segmentationType;
}

std::vector<SegmentedObject>* USegmentationController::OptimizeIndicesForCamera(USceneCaptureComponent2D* sceneCapture, const uint16_t& resolutionWidth, const uint16_t& resolutionHeight)
{
	if (sceneCapture == nullptr)
	{
		throw std::runtime_error("Scene capture cannot be null");
	}

	std::vector<SegmentedObject>* indexMap = new std::vector<SegmentedObject>();
	indexMap->resize(256);

	// Always add generic classes at the start in case there are not enough stencil indices to map every object individually
	for (int i = 0; i < static_cast<int>(ESegmentationClass::Max); i++)
	{
		SegmentedObject mapData{};

		mapData.classId = static_cast<ESegmentationClass>(i);
		mapData.trackingId = 0;
		mapData.ueComponent = nullptr;
		mapData.maskColor = GetMaskColor(i);

		(*indexMap)[i] = mapData;
	}

	std::forward_list<UPrimitiveComponent*> invalidObjects;

	if (segmentationType_ == ESegmentationType::Semantic)
	{
		// Assign generic class IDs to tracked objects
		for (std::pair<UPrimitiveComponent* const, SegmentedObject> const& objectData : objects_)
		{
			UPrimitiveComponent* const& component = objectData.first;

			if (!objectData.second.ueComponent.IsValid())
			{
				invalidObjects.push_front(component);

				continue;
			}

			component->SetRenderCustomDepth(true);
			component->SetCustomDepthStencilValue(static_cast<int>(objectData.second.classId));
		}
	}
	else if (segmentationType_ == ESegmentationType::Instance)
	{
		// Try to assign a unique ID to every object
		// Reserve space for generic class IDs at the start as a fallback in case there aren't enough stencil values
		uint32_t nextStencilValue = static_cast<int>(ESegmentationClass::Max);

		std::unordered_map<UPrimitiveComponent*, int> componentToStencilIndex;

		for (std::pair<UPrimitiveComponent* const, SegmentedObject> const& objectData : objects_)
		{
			UPrimitiveComponent* const& component = objectData.first;

			if (!objectData.second.ueComponent.IsValid())
			{
				invalidObjects.push_front(component);

				continue;
			}

			if (!ComponentMayBeVisibleFromCamera(component, sceneCapture, resolutionWidth, resolutionHeight))
			{
				component->SetRenderCustomDepth(false);

				continue;
			}

			UPrimitiveComponent* const& stencilComponent =
				objectData.second.parentUeComponent.IsValid() && objectData.second.parentUeComponent != nullptr
					? objectData.second.parentUeComponent.Get()
					: component;

			auto existingMapping = componentToStencilIndex.find(stencilComponent);

			if (existingMapping != componentToStencilIndex.end())
			{
				component->SetRenderCustomDepth(true);
				component->SetCustomDepthStencilValue(existingMapping->second);

				continue;
			}
			
			int stencilValue = nextStencilValue;

			if (stencilValue > 255)
			{
				Debug::Log("Cannot segment into more than 255 objects in one image! " + component->GetOwner()->GetName() + " will have a class id only and no tracking id");

				component->SetRenderCustomDepth(true);
				component->SetCustomDepthStencilValue(static_cast<int>(objectData.second.classId));

				continue;
			}

			nextStencilValue++;

			componentToStencilIndex[stencilComponent] = stencilValue;

			SegmentedObject mapData{};

			mapData.classId = objectData.second.classId;
			mapData.trackingId = objectData.second.trackingId;
			mapData.ueComponent = stencilComponent;
			mapData.maskColor = objectData.second.maskColor;

			(*indexMap)[stencilValue] = mapData;

			component->SetRenderCustomDepth(true);
			component->SetCustomDepthStencilValue(stencilValue);
		}
	}
	else
	{
		Debug::Log("Unknown segmentation type!");
	}

	// If UE deletes components, they need to be removed from objects_ too
	for (UPrimitiveComponent* component : invalidObjects)
	{
		objects_.erase(component);
	}

	return indexMap;
}

bool USegmentationController::ComponentMayBeVisibleFromCamera(UPrimitiveComponent* component, USceneCaptureComponent2D* sceneCapture, const uint16_t& resolutionWidth, const uint16_t& resolutionHeight)
{
	float aspectRatio = static_cast<float>(resolutionWidth) / resolutionHeight;
	FBoxSphereBounds bounds = component->Bounds;

	// First check if bounding sphere collides with view frustrum
	FMinimalViewInfo viewInfo{};

	// I don't know why there is a deltaTime parameter here. Why would the length of a frame affect the view?
	// Looking at the source code of this function, the parameter isn't even used, so I just put 0.1f there
	sceneCapture->GetCameraView(0.1f, viewInfo);

	FMatrix cameraViewMatrix = FMatrix(
		viewInfo.Rotation.UnrotateVector(FVector::UnitX()),
		viewInfo.Rotation.UnrotateVector(FVector::UnitY()),
		viewInfo.Rotation.UnrotateVector(FVector::UnitZ()),
		viewInfo.Rotation.UnrotateVector(-viewInfo.Location));
	FVector4 posInCameraSpace = cameraViewMatrix.TransformPosition(bounds.Origin);

	// If the bounding sphere is fully behind the view, it's not visible
	if (posInCameraSpace.X < -bounds.SphereRadius)
	{
		return false;
	}

	// Bounding sphere is in front of the view, but we still have to check whether it's outside the edges of the camera view
	float horizontalHalfFovRadians = FMath::DegreesToRadians(viewInfo.FOV / 2.0f);
	float horizontalHalfFovSin = sin(horizontalHalfFovRadians);
	float horizontalHalfFovCos = cos(horizontalHalfFovRadians);
	float horizontalHalfFovTan = horizontalHalfFovSin / horizontalHalfFovCos;
	float verticalHalfFovTan = horizontalHalfFovTan / aspectRatio;
	float verticalHalfFovRadians = atan2(verticalHalfFovTan, 1.0f);
	float verticalHalfFovSin = sin(verticalHalfFovRadians);
	float verticalHalfFovCos = cos(verticalHalfFovRadians);

	// Find the closest points to the view frustrum on the bounding sphere in each direction
	FVector boundingSphereMaxX = FVector(
		posInCameraSpace.X + horizontalHalfFovSin * bounds.SphereRadius,
		posInCameraSpace.Y + horizontalHalfFovCos * bounds.SphereRadius,
		posInCameraSpace.Z);

	FVector boundingSphereMinX = FVector(
		posInCameraSpace.X + horizontalHalfFovSin * bounds.SphereRadius,
		posInCameraSpace.Y - horizontalHalfFovCos * bounds.SphereRadius,
		posInCameraSpace.Z);

	FVector boundingSphereMaxY = FVector(
		posInCameraSpace.X + verticalHalfFovSin * bounds.SphereRadius,
		posInCameraSpace.Y,
		posInCameraSpace.Z + verticalHalfFovCos * bounds.SphereRadius);

	FVector boundingSphereMinY = FVector(
		posInCameraSpace.X + verticalHalfFovSin * bounds.SphereRadius,
		posInCameraSpace.Y,
		posInCameraSpace.Z - verticalHalfFovCos * bounds.SphereRadius);

	FVector2D projectedMin = FVector2D(
		boundingSphereMinX.Y / (boundingSphereMinX.X * horizontalHalfFovTan),
		boundingSphereMinY.Z / (boundingSphereMinY.X * verticalHalfFovTan));

	FVector2D projectedMax = FVector2D(
		boundingSphereMaxX.Y / (boundingSphereMaxX.X * horizontalHalfFovTan),
		boundingSphereMaxY.Z / (boundingSphereMaxY.X * verticalHalfFovTan));

	// The projected space is normalized so the edges are always at -1.0 and 1.0 regardless of resolution or aspect ratio
	if (projectedMax.X < -1.0f || projectedMin.X > 1.0f || projectedMax.Y < -1.0f || projectedMin.Y > 1.0f)
	{
		return false;
	}

	// If the bounding sphere occupies less space than 100 pixels, it's probably too small to be recognized in the image.
	// Occlusion culling would be ideal for detecting if the objects are behind something but that would be too complex
	if ((std::min(projectedMax.X, 1.0) - std::max(projectedMin.X, -1.0)) * resolutionWidth * 0.5f
		* (std::min(projectedMax.Y, 1.0) - std::max(projectedMin.Y, -1.0)) * resolutionHeight * 0.5f
		< 100)
	{
		return false;
	}

	// If all these checks passed, then assume the object is visible even though it may not be
	return true;
}

FColor USegmentationController::GetMaskColor(const uint32_t& index)
{
	// Only handle 24 bits because that's what we can fit in the RGB components
	uint32_t hash = index & 0xFFFFFF;

	// Based on the hash pattern discussed here https://nullprogram.com/blog/2018/07/31/
	hash = (hash ^ (hash >> 11)) & 0xFFFFFF;
	hash = (hash * 0x1a3c6dU) & 0xFFFFFF;
	hash = (hash ^ (hash >> 10)) & 0xFFFFFF;
	hash = (hash * 0x7a2e39U) & 0xFFFFFF;
	hash = (hash ^ (hash >> 11)) & 0xFFFFFF;

	return FColor((hash) & 0xFF, (hash >> 8) & 0xFF, (hash >> 16) & 0xFF);
}
