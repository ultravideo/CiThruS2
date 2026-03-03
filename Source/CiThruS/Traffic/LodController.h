#pragma once

#include <vector>
#include <mutex>
#include <limits>
#include <unordered_map>

class ITrafficEntity;
class ATrafficController;

class LodController
{
public:
	LodController(ATrafficController* trafficController, const float& farDistance);

	// This is used by ATrafficSnapViewSynthesizer
	ITrafficEntity* GetClosestEntityInFrontOfPlayer() const { return entityInFront_; }

	void AddEntity(ITrafficEntity* entity);
	void RemoveEntity(ITrafficEntity* entity);

	void UpdateAllLods();

	inline bool GetVisualizeViewFrustrum() const { return visualizeViewFrustrum_; }
	inline void SetVisualizeViewFrustrum(const bool& value) { visualizeViewFrustrum_ = value; }

private:
	struct EntityLodInfo
	{
		ITrafficEntity* entity = nullptr;
		bool isNear = false;
		bool previousIsNear = false;
		float distanceFromCamera = std::numeric_limits<float>::max();
	};

	std::unordered_map<ITrafficEntity*, EntityLodInfo> entityLodInfo_;

	ATrafficController* trafficController_;

	float farDistance_;

	ITrafficEntity* entityInFront_;
	float entityInFrontDistance_;
	std::mutex entityInFrontMutex_;

	bool visualizeViewFrustrum_;

	// Get properties of current camera or, optionally, the editor viewport. Used to determine if an entity is in view.
	//// FVector cameraLocation -- the camera location
	//// FMatrix cameraViewMatrix -- the view matrix
	//// float cameraTanInverse -- one over the tangent of half the camera FOV
	//// float aspectRatioInverse -- one over the aspect ratio of the camera
	void GetCameraViewProperties(FVector& cameraLocation, FMatrix& cameraViewMatrix, float& cameraTanInverse, float& aspectRatioInverse);
	// Returns true if the given entity is in view
	bool EntityInCameraView(ITrafficEntity* entity, FVector& cameraLocation, FMatrix& cameraViewMatrix, float& cameraTanInverse, float& aspectRatioInverse);
};
