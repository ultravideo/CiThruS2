#pragma once

#include "CoreMinimal.h"

class AGeoReferencingSystem;
struct FGeographicCoordinates;

namespace GeoUtility
{
	bool TryGetGeoReferencingSystem(const UObject* WorldContext, AGeoReferencingSystem*& OutGeoRef);
	bool TryWorldToGeographic(AGeoReferencingSystem* GeoRef, const FVector& WorldLocation, FGeographicCoordinates& OutGeo);
	bool TryGetEnuBasisAtWorldLocation(
		AGeoReferencingSystem* GeoRef,
		const FVector& WorldLocation,
		FVector& OutEast,
		FVector& OutNorth,
		FVector& OutUp);

	FVector3d WorldVelocityCmpsToEnuMps(
		const FVector& WorldVelocityCmps,
		const FVector& East,
		const FVector& North,
		const FVector& Up);

	FRotator WorldQuatToTangentRotatorYawNorth(
		const FQuat& WorldQuat,
		const FVector& East,
		const FVector& North,
		const FVector& Up);

	FRotator ComputeAngularVelocityDegPerSec(const FRotator& Prev, const FRotator& Curr, double DeltaSeconds);
}
