#include "RoadsideParkingSpace.h"
#include "Traffic/Entities/Car.h"
#include "ParkingController.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Debug.h"

bool ARoadsideParkingSpace::ParkCar(ACar* car)
{
	if (occupied_ || occupant_ != nullptr)
	{
		return false;
	}

	if (parkingController_ == nullptr)
	{
		Debug::Log("No ParkingController found. Check that the simulation is running first");

		return false;
	}

	if (parkingController_->GetTrafficController() == nullptr)
	{
		Debug::Log("No TrafficController found. Check that the simulation is running first");

		return false;
	}

	FTransform vehiclePoint = car->GetActorTransform();
	FTransform endPoint = FTransform(GetActorRotation(), GetActorLocation(), FVector::One());

	FVector vehicleLocal = endPoint.InverseTransformPosition(vehiclePoint.GetLocation());

	FTransform parkingSpace = GetParkedTransform();

	if (FVector::DotProduct(parkingSpace.GetRotation().GetForwardVector(), GetActorForwardVector()) < 0.0f)
	{
		parkingSpace.SetRotation(parkingSpace.GetRotation() * FQuat(FVector::UpVector, PI));
	}

	// Prevent car from parking if at beginning of path (for ~15 keypoints?)
	if (car->GetPathFollower().GetCurrentLocalPoint() <= MIN_TRAVERSED_DISTANCE_BEFORE_PARKING)
	{
		return false;
	}

	if (vehiclePoint.GetLocation() == FVector::Zero())
	{
		// Lane starting keypoint is invalid
		return false;
	}

	// Starting point
	TArray<FVector> customPoints;
	customPoints.Add(car->GetActorLocation() - car->GetActorForwardVector() * 0.5f * car->GetWheelbase());

	// Lane correction
	KeypointGraph graph = parkingController_->GetTrafficController()->GetRoadGraph();
	int laneStartKeypoint = graph.GetClosestKeypoint(arrivingPoint_);

	if (graph.GetOutBoundKeypoints(laneStartKeypoint).size() != 1)
	{
		return false;
	}
	
	arrivingPoint_ = graph.GetKeypointPosition(laneStartKeypoint);
	// Add a point through the first lane point, if it is in front of the car
	if (FVector::DotProduct(car->GetTransform().GetUnitAxis(EAxis::X), vehiclePoint.GetLocation() - car->GetTransform().GetTranslation()) > 0.0f)
	{
		// If theres space in between start point & lane point, add a guiding point to reduce snapping movement
		if (car->GetWheelbase() * 0.55f < FVector::Dist2D(car->GetActorLocation(), arrivingPoint_))
		{
			customPoints.Add(car->GetActorLocation());
		}

		customPoints.Add(arrivingPoint_);
	}
	else
	{
		// Add a guiding point to replace lane point
		customPoints.Add(car->GetActorLocation());
	}

	int laneEndKeypoint = graph.GetOutBoundKeypoints(laneStartKeypoint)[0];
	// Get the closest point to this on the lane
	FVector pointOnLane = parkingSpace.GetLocation() + parkingSpace.GetRotation().GetForwardVector() * car->GetWheelbase() * 2.5f;
	pointOnLane = FMath::ClosestPointOnSegment(pointOnLane, arrivingPoint_, graph.GetKeypointPosition(laneEndKeypoint));
	customPoints.Add(pointOnLane);

	// Ending points
	customPoints.Add(parkingSpace.GetLocation() + parkingSpace.GetRotation().GetForwardVector() * car->GetWheelbase() * 1.25f);
	customPoints.Add(parkingSpace.GetLocation() + parkingSpace.GetRotation().GetForwardVector() * car->GetWheelbase() * 0.5f);

	// Add custompoints' indexes to path
	TArray<int> path;

	for (int i = 0; i < customPoints.Num(); i++)
	{
		path.Insert((-8 - i), i);
	}

	//Apply new path & other car params
	car->ApplyCustomPath(path, customPoints, 0, 0.0f);
	car->SetMoveSpeed(350.0f);

	car->spaceBeingParkedTo_ = this; // Lets the car know it is currently on a parking path

	occupant_ = car;

	return true;
}

bool ARoadsideParkingSpace::DepartCar()
{
	if (!occupied_)
	{
		return false;
	}

	if (parkingController_->GetTrafficController() == nullptr)
	{
		Debug::Log("No TrafficController found. Check that the simulation is running first");
		return false;
	}

	FTransform vehiclePoint = GetParkedTransform();
	
	FVector departingPoint;

	if (FVector::DotProduct(GetActorForwardVector(), vehiclePoint.GetRotation().GetForwardVector()) < 0.0f)
	{
		departingPoint = departingPoints_[1];
	}
	else
	{
		departingPoint = departingPoints_[0];
	}

	FVector vehicleLocal = vehiclePoint.InverseTransformPosition(departingPoint);

	TArray<FVector> customPoints = { };
	int reverseUntilPoint;

	customPoints =
	{
		vehiclePoint.GetLocation(),
		vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X, 0.0f, 400.0f), vehicleLocal.Y * 0.7f, 0.0f)),
		vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X, 0.0f, 600.0f), vehicleLocal.Y, 0.0f)),
		departingPoint
	};

	reverseUntilPoint = -1;

	if (customPoints.Num() <= 1 || customPoints[0].IsZero())
	{
		Debug::Log("Something wrong with route from car location");
		return false;
	}

	ACar* car = parkingController_->GetTrafficController()->SpawnCar(customPoints[0], FRotator(vehiclePoint.GetRotation()), true, carClass_, carVariant_);

	parkingController_->DestroyParkedInstance(visualInstanceId_);

	// Create a custom path from parking space to closest keypoint, create a random path starting from that keypoint
	KeypointGraph graph = parkingController_->GetTrafficController()->GetRoadGraph();
	TArray<int> path = graph.GetRandomPathFrom(graph.GetClosestKeypoint(customPoints.Last()), car->GetKeypointRuleExceptions()).keypoints;
	// Last custom point is in the same location as the keypoint, replace it
	customPoints.RemoveAt(customPoints.Num() - 1);

	// Check that the created path is valid
	if (graph.GetInboundKeypoints(path[0]).size() == 0)
	{
		Debug::Log("Departing keypoint is invalid");
		return false;
	}

	// Add a point correcting lane position, if necessary. This forces the car to follow the lane & not block traffic going the other way
	// i.e. the car will go directly onto its lane after turning, instead of cruising in the middle or wrong side of the road for a longer time
	FVector dirlast = (graph.GetKeypointPosition(path[0]) - customPoints.Last());
	if (FVector::Dist(FVector::Zero(), dirlast) > 1100.0f)
	{
		dirlast.Normalize();
		FVector closeToPoint = customPoints.Last() + dirlast * 1000.0f;

		int inboundKeypoint = graph.GetInboundKeypoints(path[0])[0];
		FVector lanePoint;
		lanePoint = FMath::ClosestPointOnSegment(closeToPoint, graph.GetKeypointPosition(inboundKeypoint), graph.GetKeypointPosition(path[0]));

		customPoints.Add(lanePoint);
	}

	// Add a point just behind car starting position, otherwise it will instantly rotate towards next point. This also corrects the spawning position - otherwise the car would spawn in the wrong location.
	if (reverseUntilPoint == -1)
	{
		customPoints.Insert(customPoints[0] - vehiclePoint.GetRotation().Vector() * car->GetWheelbase(), 0);
	}
	else
	{
		customPoints.Insert(customPoints[0] - vehiclePoint.GetRotation().Vector() * car->GetWheelbase(), 1);
		reverseUntilPoint++;
	}

	// Additional point for when the car is reversing. To prevent snappy movement.
	if (reverseUntilPoint != -1)
	{
		// We can use one of the earlier points. This will create a nicer curve towards the next point. (Lane point)
		customPoints.Insert(customPoints[2] + FVector(1, 1, 0), 4);
	}

	// Add all custom points' indexes into the path
	for (int i = 0; i < customPoints.Num(); i++)
	{
		path.Insert((-8 - i), i);
	}

	// Apply path & other settings
	car->ApplyCustomPath(path, customPoints, 1, 0.0f);
	car->ReverseUntilPoint(reverseUntilPoint);
	car->SetInstantSpeed(0.0f);

	// Set to wait until road is clear
	car->WaitForUnobstructedDepart(FVector(2600.0f, 1300.0f, 500.0f), FVector(-600.0f, 0, 0));

	occupied_ = false;
	carClass_ = nullptr;
	carVariant_ = -1;

	return true;
}
