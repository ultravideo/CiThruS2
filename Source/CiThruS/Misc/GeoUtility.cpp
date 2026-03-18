#include "GeoUtility.h"

#include "GeoReferencingSystem.h"
#include "GeographicCoordinates.h"
#include "Math/RotationMatrix.h"

bool GeoUtility::TryGetGeoReferencingSystem(const UObject* WorldContext, AGeoReferencingSystem*& OutGeoRef)
{
	OutGeoRef = nullptr;

	if (!IsValid(WorldContext))
	{
		return false;
	}

	AGeoReferencingSystem* GeoRef = AGeoReferencingSystem::GetGeoReferencingSystem(const_cast<UObject*>(WorldContext));

	if (!IsValid(GeoRef))
	{
		return false;
	}

	OutGeoRef = GeoRef;
	return true;
}

bool GeoUtility::TryWorldToGeographic(AGeoReferencingSystem* GeoRef, const FVector& WorldLocation, FGeographicCoordinates& OutGeo)
{
	if (!IsValid(GeoRef))
	{
		return false;
	}

	GeoRef->EngineToGeographic(WorldLocation, OutGeo);
	return true;
}

bool GeoUtility::TryGetEnuBasisAtWorldLocation(
	AGeoReferencingSystem* GeoRef,
	const FVector& WorldLocation,
	FVector& OutEast,
	FVector& OutNorth,
	FVector& OutUp)
{
	if (!IsValid(GeoRef))
	{
		return false;
	}

	FGeographicCoordinates Geo;

	if (!TryWorldToGeographic(GeoRef, WorldLocation, Geo))
	{
		return false;
	}

	GeoRef->GetENUVectorsAtGeographicLocation(Geo, OutEast, OutNorth, OutUp);

	OutEast = OutEast.GetSafeNormal();
	OutNorth = OutNorth.GetSafeNormal();
	OutUp = OutUp.GetSafeNormal();

	return !OutEast.IsNearlyZero() && !OutNorth.IsNearlyZero() && !OutUp.IsNearlyZero();
}

FVector3d GeoUtility::WorldVelocityCmpsToEnuMps(
	const FVector& WorldVelocityCmps,
	const FVector& East,
	const FVector& North,
	const FVector& Up)
{
	const FVector3d EastNorm = FVector3d(East.GetSafeNormal());
	const FVector3d NorthNorm = FVector3d(North.GetSafeNormal());
	const FVector3d UpNorm = FVector3d(Up.GetSafeNormal());
	const FVector3d WorldVelocityMps = FVector3d(WorldVelocityCmps) / 100.0;

	return FVector3d(
		WorldVelocityMps | EastNorm,
		WorldVelocityMps | NorthNorm,
		WorldVelocityMps | UpNorm);
}

FRotator GeoUtility::WorldQuatToTangentRotatorYawNorth(
	const FQuat& WorldQuat,
	const FVector& East,
	const FVector& North,
	const FVector& Up)
{
	const FVector EastNorm = East.GetSafeNormal();
	const FVector NorthNorm = North.GetSafeNormal();
	const FVector UpNorm = Up.GetSafeNormal();

	if (EastNorm.IsNearlyZero() || NorthNorm.IsNearlyZero() || UpNorm.IsNearlyZero())
	{
		return FRotator::ZeroRotator;
	}

	const FQuat NormalizedWorldQuat = WorldQuat.GetNormalized();
	const FVector ForwardWorld = NormalizedWorldQuat.RotateVector(FVector::ForwardVector);
	const FVector UpWorld = NormalizedWorldQuat.RotateVector(FVector::UpVector);

	// Tangent frame is defined as X=North, Y=East, Z=Up to enforce yaw zero at North.
	FVector ForwardTangent(
		FVector::DotProduct(ForwardWorld, NorthNorm),
		FVector::DotProduct(ForwardWorld, EastNorm),
		FVector::DotProduct(ForwardWorld, UpNorm));

	FVector UpTangent(
		FVector::DotProduct(UpWorld, NorthNorm),
		FVector::DotProduct(UpWorld, EastNorm),
		FVector::DotProduct(UpWorld, UpNorm));

	ForwardTangent = ForwardTangent.GetSafeNormal();
	UpTangent = UpTangent.GetSafeNormal();

	if (ForwardTangent.IsNearlyZero() || UpTangent.IsNearlyZero())
	{
		return FRotator::ZeroRotator;
	}

	FRotator Result = FRotationMatrix::MakeFromXZ(ForwardTangent, UpTangent).Rotator();
	Result.Normalize();
	return Result;
}

FRotator GeoUtility::ComputeAngularVelocityDegPerSec(const FRotator& Prev, const FRotator& Curr, double DeltaSeconds)
{
	if (DeltaSeconds <= UE_DOUBLE_SMALL_NUMBER)
	{
		return FRotator::ZeroRotator;
	}

	FRotator AngularVelocity = FRotator::ZeroRotator;
	AngularVelocity.Roll = FMath::FindDeltaAngleDegrees(Prev.Roll, Curr.Roll) / DeltaSeconds;
	AngularVelocity.Pitch = FMath::FindDeltaAngleDegrees(Prev.Pitch, Curr.Pitch) / DeltaSeconds;
	AngularVelocity.Yaw = FMath::FindDeltaAngleDegrees(Prev.Yaw, Curr.Yaw) / DeltaSeconds;

	return AngularVelocity;
}
