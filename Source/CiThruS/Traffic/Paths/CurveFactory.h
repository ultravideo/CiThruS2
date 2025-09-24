#pragma once

#include "ICurve.h"

// Creates appropriate curves for connecting the given positions and tangents without discontinuities
namespace CurveFactory
{
	std::shared_ptr<ICurve> GetCurveFor(/* const UWorld *world, */ const FVector& startPosition, const FVector& endPosition, const FVector& startTangent, const FVector& endTangent, const bool allowSCurve);
}
