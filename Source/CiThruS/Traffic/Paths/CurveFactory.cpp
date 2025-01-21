#include "CurveFactory.h"
#include "SingleCurve.h"
#include "SCurve.h"
#include "StraightLineCurve.h"

std::shared_ptr<ICurve> CurveFactory::GetCurveFor(const FVector& startPosition, const FVector& endPosition, const FVector& startTangent, const FVector& endTangent)
{
	FVector startToEnd = endPosition - startPosition;

	// Calculate the normal of a plane that contains the curve. This doesn't work right if there is no such plane, but the way CurvePathFollower
	// uses this class the positions and tangents it passes should always have one
	FVector curvePlaneNormal;

	if (abs(startTangent.Dot(endTangent)) > 0.9999f)
	{
		if (abs(startTangent.Dot(startToEnd.GetSafeNormal())) > 0.9999f)
		{
			// The tangents and positions are all in a straight line: no fancy curve needed
			return std::make_shared<StraightLineCurve>(startPosition, endPosition);
		}
		else
		{
			// The tangents are parallel: any vector perpendicular to startTangent is also perpendicular to endTangent
			curvePlaneNormal = startTangent.Cross(startToEnd).GetSafeNormal();
		}
	}
	else
	{
		curvePlaneNormal = startTangent.Cross(endTangent).GetSafeNormal();
	}

	// startCurveDirection and endCurveDirection are vectors that are perpendicular to the start and end tangents respectively and
	// "facing" the opposite position (positive dot product with a vector pointing towards the opposite position)
	FVector startCurveDirection = startTangent.Cross(curvePlaneNormal).GetSafeNormal();
	FVector endCurveDirection = -endTangent.Cross(curvePlaneNormal).GetSafeNormal();

	if (startCurveDirection.Dot(startToEnd) < 0.0)
	{
		startCurveDirection = -startCurveDirection;
	}

	if (endCurveDirection.Dot(-startToEnd) < 0.0)
	{
		endCurveDirection = -endCurveDirection;
	}

	// If startCurveDirection and endCurveDirection are on the same side of the line from start to end, a SingleCurve can be used
	// to connect them. If they're on the opposite sides, an SCurve is needed instead
	if (startCurveDirection.Dot(endCurveDirection) > 0.0)
	{
		return std::make_shared<SingleCurve>(startPosition, endPosition, startTangent, endTangent, startCurveDirection, endCurveDirection);
	}
	else
	{
		return std::make_shared<SCurve>(startPosition, endPosition, startTangent, endTangent, startCurveDirection, endCurveDirection);
	}
}
