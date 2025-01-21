#pragma once

#include <CoreMinimal.h>
#include <utility>

namespace MathUtility
{
	bool PointInsideTriangle(const FVector2D& point, const FVector2D& a, const FVector2D& b, const FVector2D& c);
	bool PointInsideRectangle(const FVector2D& point, const FVector2D& a, const FVector2D& b, const FVector2D& c, const FVector2D& d);
	bool PointInsideCone(const FVector2D& point, const FVector2D& coneOrigin, const FVector2D& coneDir, const float& coneAngle, const float& coneLength);
	bool PointBeyondLine(const FVector2D& point, const FVector2D& lineNormal, const FVector2D& linePos);
	bool PointInsideBoundingBox(const FVector2D& point, const FVector2D& boundingBoxMin, const FVector2D& boundingBoxMax);

	bool AllPointsInFrontOfAnySinglePolygonSide(const FVector2D* points, const int& pointCount, const FVector2D* polygonCorners, const int& polygonCornerCount);
	// Only works with convex polygons!
	bool PolygonIntersectingPolygon(const FVector2D* aCorners, const int& aCornerCount, const FVector2D* bCorners, const int& bCornerCount);

	// angle of b in reference to a, positive angle is to the right (clock-wise), [-180, 180]
	float SignedAngleCW(const FVector2D& a, const FVector2D& b);

	// <point, tangent>
	std::pair<FVector, FVector> SamplePointBezier2DPlane3Point(const FVector& p0, const FVector& p1, const FVector& p2, const float& t);
	FVector SamplePointBezier2DPlane4Point(const FVector& p0, const FVector& p1, const FVector& p2, const FVector& p3, const float& t);

	float BezierCurveLength(const FVector& p0, const FVector& p1, const FVector& p2);
}
