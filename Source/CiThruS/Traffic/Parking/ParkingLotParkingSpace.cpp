#include "ParkingLotParkingSpace.h"
#include "Traffic/Entities/Car.h"
#include "ParkingController.h"
#include "Misc/Debug.h"
#include <Kismet/KismetMathLibrary.h>

bool AParkingLotParkingSpace::ParkCar(ACar* car)
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
	FTransform endPoint = GetParkedTransform();

	FVector vehicleLocal = endPoint.InverseTransformPosition(vehiclePoint.GetLocation());

	TArray<FVector> customPoints = { };
	int startReverseAt;

	if (vehicleLocal.X < 0.0f)
	{
		if (vehicleLocal.Y < 0.0f)
		{
			// RL
			customPoints = { vehiclePoint.GetLocation(), endPoint.TransformPosition(FVector(vehicleLocal.X, -100.0f, 0.0f)), endPoint.TransformPosition(FVector(vehicleLocal.X * 0.75f, 0.0f, 0.0f)), endPoint.GetLocation() };
			startReverseAt = -1;
		}
		else
		{
			// RR
			customPoints = { vehiclePoint.GetLocation(), endPoint.TransformPosition(FVector(vehicleLocal.X, 100.0f, 0.0f)), endPoint.TransformPosition(FVector(vehicleLocal.X * 0.75f, 0.0f, 0.0f)), endPoint.GetLocation() };
			startReverseAt = -1;
		}
	}
	else
	{
		if (vehicleLocal.Y < 0.0f)
		{
			// FL
			customPoints = { vehiclePoint.GetLocation(), endPoint.TransformPosition(FVector(vehicleLocal.X, 500.0f, 0.0f)), endPoint.TransformPosition(FVector(vehicleLocal.X * 0.8f, 100.0f, 0.0f)), endPoint.TransformPosition(FVector(vehicleLocal.X * 0.4f, 0.0f, 0.0f)), endPoint.GetLocation() };
			startReverseAt = 1;
		}
		else
		{
			// FR
			customPoints = { vehiclePoint.GetLocation(), endPoint.TransformPosition(FVector(vehicleLocal.X, -500.0f, 0.0f)), endPoint.TransformPosition(FVector(vehicleLocal.X * 0.8f, -100.0f, 0.0f)), endPoint.TransformPosition(FVector(vehicleLocal.X * 0.4f, 0.0f, 0.0f)), endPoint.GetLocation() };
			startReverseAt = 1;
		}
	}

	if (customPoints.Num() <= 1 || customPoints[0].IsZero())
	{
		Debug::Log("Something wrong with route to parking space / car location");

		return false;
	}

	// Prevent car from parking if at beginning of path (for ~15 keypoints?)
	if (car->GetPathFollower().GetCurrentLocalPoint() <= MIN_TRAVERSED_DISTANCE_BEFORE_PARKING)
	{
		return false;
	}

	// Lane & parking correction
	if (arrivingPoint_ != FVector::Zero())
	{
		KeypointGraph graph = parkingController_->GetTrafficController()->GetRoadGraph();
		int laneStartKeypoint = graph.GetClosestKeypoint(arrivingPoint_);

		// Correct points to be on lane
		FVector oldPoint = customPoints[1];
		int laneEndKeypoint;

		if (graph.GetOutBoundKeypoints(laneStartKeypoint).size() == 1)
		{
			laneEndKeypoint = graph.GetOutBoundKeypoints(laneStartKeypoint)[0];

			FVector pointOnLane;
			pointOnLane = FMath::ClosestPointOnSegment(oldPoint, graph.GetKeypointPosition(laneStartKeypoint), graph.GetKeypointPosition(laneEndKeypoint));

			FVector pointsDiff = pointOnLane - oldPoint;
			customPoints[1] = pointOnLane;
			customPoints[2] = customPoints[2] + pointsDiff;

			// Verify that custompoints 1 & 2 don't go over point 4 on the local y-axis of the lane & same for x-axis
			// Those are the points that create the curve that connects the lane and the parking space
			// First, create a transform from lane start & end keypoints to then compare locations on its local x & y axis
			FVector kpPosition = pointOnLane;
			FVector kpForward = graph.GetKeypointPosition(laneEndKeypoint) - graph.GetKeypointPosition(laneStartKeypoint);
			kpForward.Normalize();
			FQuat kpRotation = kpForward.ToOrientationQuat();
			FVector kpScale = FVector(1, 1, 1);
			FTransform kpTransform = FTransform(kpRotation, kpPosition, kpScale);

			// Custompoints to local space
			FVector vp1 = UKismetMathLibrary::InverseTransformLocation(kpTransform, customPoints[1]);
			FVector vp2 = UKismetMathLibrary::InverseTransformLocation(kpTransform, customPoints[2]);
			FVector vp3 = UKismetMathLibrary::InverseTransformLocation(kpTransform, customPoints[3]);

			// Y-Axis (Horizontal compared to lane), if the lane is on the left side of parking space
			if (vp3.Y > 0 && vp2.Y > vp3.Y)
			{
				FVector diff = FVector(0, vp2.Y - vp3.Y, 0) + FVector(0, 100.0f, 0); //TODO: Turn this from local to global :D
				diff = kpTransform.TransformPosition(diff) - kpPosition;
				customPoints[1] = customPoints[1] - diff;
				customPoints[2] = customPoints[2] - diff;
			}
			// Else if the lane is on the right side of parking space 
			else if (vp3.Y < 0 && vp2.Y < vp3.Y)
			{
				FVector diff = FVector(0, vp3.Y - vp2.Y, 0) + FVector(0, 100.0f, 0);
				diff = kpTransform.TransformPosition(diff) - kpPosition;
				customPoints[1] = customPoints[1] + diff;
				customPoints[2] = customPoints[2] + diff;
			}

			// X-Axis (Vertical compared to lane), if reversing to space
			if (startReverseAt != -1 && vp2.X < vp3.X)
			{
				FVector diff = FVector(vp3.X - vp2.X, 0, 0) + FVector(0, 100.0f, 0);
				diff = kpTransform.TransformPosition(diff) - kpPosition;
				customPoints[1] = customPoints[1] + diff;
				customPoints[2] = customPoints[2] + diff;
			}
			// Else if going forwards
			else if (startReverseAt == -1 && vp2.X > vp3.X)
			{
				FVector diff = FVector(vp2.X - vp3.X, 0, 0) + FVector(0, 100.0f, 0);
				diff = kpTransform.TransformPosition(diff) - kpPosition;
				customPoints[1] = customPoints[1] - diff;
				customPoints[2] = customPoints[2] - diff;
			}

			// Add a point just before turning starts, otherwise the car may drift from the lane on longer distances
			//   & Add a point to start of lane to force car to always stay on that
			//      & If car is meant to reverse to space, remember we add 2 points in between
			customPoints.Insert(FMath::Lerp(arrivingPoint_, customPoints[1], 0.75f), 1);
			customPoints.Insert(arrivingPoint_, 1);

			if (startReverseAt != -1)
			{
				startReverseAt += 2;
			}

		}
	}

	// Correct starting point
	customPoints.Insert(customPoints[0] - car->GetActorForwardVector() * 0.5f * car->GetWheelbase(), 0);

	// Correct ending point
	if (startReverseAt == -1)
	{
		FVector forward = (customPoints.Last() - customPoints[customPoints.Num() - 2]);
		forward.Normalize();
		customPoints.Last() = customPoints.Last() - forward * car->GetWheelbase() * 0.5f;
	}
	else
	{
		FVector forward = (customPoints.Last() - customPoints[customPoints.Num() - 2]);
		forward.Normalize();
		customPoints.Last() = customPoints.Last() + forward * car->GetWheelbase() * 0.5f;
		startReverseAt += 2;
	}

	// Smoothen the curve for reversing cars
	if (startReverseAt != -1)
	{
		FVector forward = (customPoints[4] - customPoints[3]);
		forward.Normalize();
		customPoints.Insert(customPoints[4] - forward * car->GetWheelbase() * 0.75f, 5);
	}

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
	car->StartReverseFromPoint(startReverseAt);

	occupant_ = car;

	return true;
}

bool AParkingLotParkingSpace::DepartCar()
{
	if (!occupied_ || occupant_ != nullptr)
	{
		return false;
	}

	if (parkingController_->GetTrafficController() == nullptr)
	{
		Debug::Log("No TrafficController found. Check that the simulation is running first");
		return false;
	}

	FTransform vehiclePoint = GetParkedTransform();
	FVector departingPoint = departingPoint_;

	FVector vehicleLocal = vehiclePoint.InverseTransformPosition(departingPoint);

	TArray<FVector> customPoints = { };
	int reverseUntilPoint;

	if (vehicleLocal.X < 0.0f)
	{
		if (vehicleLocal.Y < 0.0f)
		{
			// RL
			customPoints =
			{
				vehiclePoint.GetLocation(),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X * 0.75f, -400.0f, -300.0f), 50.0f, 0.0f)),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X, -500.0f, -400.0f), 550.0f, 0.0f)),
				departingPoint
			};

			reverseUntilPoint = 2;
		}
		else
		{
			// RR
			customPoints =
			{
				vehiclePoint.GetLocation(),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X * 0.75f, -400.0f, -300.0f), -50.0f, 0.0f)),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X, -500.0f, -400.0f), -550.0f, 0.0f)),
				departingPoint
			};

			reverseUntilPoint = 2;
		}
	}
	else
	{
		if (vehicleLocal.Y < 0.0f)
		{
			// FL
			customPoints =
			{
				vehiclePoint.GetLocation(),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X * 0.75f, 150.0f, 250.0f), 50.0f, 0.0f)),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X, 250.0f, 10000.0f), FMath::Clamp(vehicleLocal.Y, -400.0f, 0.0f), 0.0f)),
				departingPoint
			};

			reverseUntilPoint = -1;
		}
		else
		{
			// FR
			customPoints =
			{
				vehiclePoint.GetLocation(),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X * 0.75f, 150.0f, 250.0f), -50.0f, 0.0f)),
				vehiclePoint.TransformPosition(FVector(FMath::Clamp(vehicleLocal.X, 250.0f, 10000.0f), FMath::Clamp(vehicleLocal.Y, 0.0f, 400.0f), 0.0f)),
				departingPoint
			};

			reverseUntilPoint = -1;
		}
	}

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
	car->WaitForUnobstructedDepart(FVector(1300.0f, 2600.0f, 500.0f), FVector(reverseUntilPoint == -1 ? -600.0f : 600.0f, 0, 0));

	occupied_ = false;
	carClass_ = nullptr;
	carVariant_ = -1;

	return true;
}