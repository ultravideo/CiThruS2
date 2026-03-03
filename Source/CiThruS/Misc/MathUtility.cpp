#include "MathUtility.h"
#include "Debug.h"

namespace MathUtility
{
	const static double RAD_2_DEG = 57.2957795131;

	// This was calculated using OpenCV's getPerspectiveTransform with 4 matching points on a map and inside CiThruS
	// The altitude is not accurate inside CiThruS, so here it's just a rough estimate calculated separately
	const static FMatrix44d UE_TO_WGS_CONVERSION_MATRIX = FMatrix44d(
		FPlane(-5.58315305e-07, -2.91959870e-08,        0.0, -9.08600256e-09),
		FPlane(-6.32518473e-07, -2.10895676e-07,        0.0, -8.83018673e-09),
		FPlane(            0.0,             0.0,       0.01,             0.0),
		FPlane(     61.4442210,      23.8565214, 125.039604,             1.0));

	// This is just the inverse of UE_TO_WGS_CONVERSION_MATRIX
	const static FMatrix44d WGS_TO_UE_CONVERSION_MATRIX = FMatrix44d(
		FPlane(  -2124300,    294079, -1.67043, 0.0),
		FPlane(   6371100,  -5623700, 0.822967, 0.0),
		FPlane(       0.0,       0.0,    100.0, 0.0),
		FPlane(-2.1468e07, 1.1609e08,   -12421, 1.0));

	const static FQuat UE_TO_REAL_ROTATION_QUAT = FMatrix44d(
		FPlane(-0.1371279649,  0.9905533409, 0.0, 0.0),
		FPlane(-0.9905533409, -0.1371279649, 0.0, 0.0),
		FPlane(          0.0,           0.0, 1.0, 0.0),
		FPlane(          0.0,           0.0, 0.0, 1.0)).ToQuat();

	const static FQuat REAL_TO_UE_ROTATION_QUAT = FMatrix44d(
		FPlane(-0.1371279649, -0.9905533409, 0.0, 0.0),
		FPlane( 0.9905533409, -0.1371279649, 0.0, 0.0),
		FPlane(          0.0,           0.0, 1.0, 0.0),
		FPlane(          0.0,           0.0, 0.0, 1.0)).ToQuat();
}

bool MathUtility::PointInsideTriangle(const FVector2D& point, const FVector2D& a, const FVector2D& b, const FVector2D& c)
{
	const FVector2D ss(point.X, point.Y);
	const FVector2D aa(a.X, a.Y);
	const FVector2D bb(b.X, b.Y);
	const FVector2D cc(c.X, c.Y);

	const float as_x = ss.X - aa.X;
	const float as_y = ss.Y - aa.Y;

	const bool s_ab = (bb.X - aa.X) * as_y - (bb.Y - aa.Y) * as_x > 0.0f;

	return (cc.X - aa.X) * as_y - (cc.Y - aa.Y) * as_x > 0.0 != s_ab
		&& (cc.X - bb.X) * (ss.Y - bb.Y) - (cc.Y - bb.Y) * (ss.X - bb.X) > 0.0f == s_ab;
}

bool MathUtility::PointInsideRectangle(const FVector2D& point, const FVector2D& a, const FVector2D& b, const FVector2D& c, const FVector2D& d)
{
	return (PointInsideTriangle(point, a, b, c)
		|| PointInsideTriangle(point, a, c, d));
}

bool MathUtility::PointInsideCone(const FVector2D& point, const FVector2D& coneOrigin, const FVector2D& coneDir, const float& coneAngle, const float& coneLength)
{
	const FVector2D dirNormalized = coneDir.GetSafeNormal();

	const FVector2D dirLeft = dirNormalized.GetRotated(-coneAngle * 0.5f);
	const FVector2D dirRight = dirNormalized.GetRotated(coneAngle * 0.5f);

	// left triangle vertex
	const FVector2D lp = coneOrigin + dirLeft * coneLength;

	// right triangle vertex
	FVector2D rp = coneOrigin + dirRight * coneLength;

	return PointInsideTriangle(point, coneOrigin, lp, rp);
}

bool MathUtility::PointBeyondLine(const FVector2D& point, const FVector2D& lineNormal, const FVector2D& linePos)
{
	return FVector2D::DotProduct(lineNormal, point - linePos) > 0;
}

bool MathUtility::PointInsideBoundingBox(const FVector2D& point, const FVector2D& boundingBoxMin, const FVector2D& boundingBoxMax)
{
	return point.X >= boundingBoxMin.X && point.Y >= boundingBoxMin.Y
		&& point.X <= boundingBoxMax.X && point.Y <= boundingBoxMax.Y;
}

bool MathUtility::AllPointsInFrontOfAnySinglePolygonSide(const FVector2D* points, const int& pointCount, const FVector2D* polygonCorners, const int& polygonCornerCount)
{
	// Calculate the center of the polygon to compare which way the polygon sides are facing
	FVector2D polygonCenter = FVector2D::ZeroVector;

	for (int i = 0; i < polygonCornerCount; i++)
	{
		polygonCenter += polygonCorners[i];
	}

	polygonCenter /= polygonCornerCount;

	// Check each side of the polygon against each point
	for (int i = 0; i < polygonCornerCount; i++)
	{
		// UE4 doesn't have a function for 2D vector projection so have to use 3D vectors for a moment
		const FVector sideDirection = FVector(polygonCorners[(i + 1) % polygonCornerCount] - polygonCorners[i], 0);

		if (sideDirection == FVector::ZeroVector)
		{
			continue;
		}

		const FVector aCenterToSide = FVector(polygonCorners[i] - polygonCenter, 0);
		const FVector2D sideNormal = FVector2D(aCenterToSide - aCenterToSide.ProjectOnTo(sideDirection));

		bool allPointsBeyondSide = true;

		for (int j = 0; j < pointCount; j++)
		{
			if (!MathUtility::PointBeyondLine(points[j], sideNormal, polygonCorners[i]))
			{
				allPointsBeyondSide = false;
				break;
			}
		}

		if (allPointsBeyondSide)
		{
			return true;
		}
	}

	return false;
}

bool MathUtility::PolygonIntersectingPolygon(const FVector2D* aCorners, const int& aCornerCount, const FVector2D* bCorners, const int& bCornerCount)
{
	// a and b should be polygons so three corners minimum on each
	if (aCornerCount < 3 || bCornerCount < 3)
	{
		return false;
	}

	// Notice the swapped a and b between calls. The polygons are intersecting if there is no side on
	// either polygon that has every point of the other polygon in front of it
	return !AllPointsInFrontOfAnySinglePolygonSide(aCorners, aCornerCount, bCorners, bCornerCount)
		&& !AllPointsInFrontOfAnySinglePolygonSide(bCorners, bCornerCount, aCorners, aCornerCount);
}

float MathUtility::SignedAngleCW(const FVector2D& a, const FVector2D& b)
{
	const float angle = atan2f(b.Y, b.X) - atan2f(a.Y, a.X);

	if (angle > PI)
	{
		return (angle - 2 * PI) * RAD_2_DEG;
	}
	else if (angle <= -PI)
	{
		return (angle + 2 * PI) * RAD_2_DEG;
	}

	return angle * RAD_2_DEG;
}

std::pair<FVector, FVector> MathUtility::SamplePointBezier2DPlane3Point(const FVector& p0, const FVector& p1, const FVector& p2, const float& t)
{
	const float x = p0.X * t * t + p1.X * 2 * t * (1.f - t) + p2.X * (1.f - t) * (1.f - t);
	const float y = p0.Y * t * t + p1.Y * 2 * t * (1.f - t) + p2.Y * (1.f - t) * (1.f - t);

	const FVector point(x, y, 0.0f);

	// 1st derivative: 2*(p0.X - 2*p1.X + p2.X)*t + 2*p1.X - 2*p2.X

	const float dx = 2 * (p0.X - 2 * p1.X + p2.X) * t + 2 * p1.X - 2 * p2.X;
	const float dy = 2 * (p0.Y - 2 * p1.Y + p2.Y) * t + 2 * p1.Y - 2 * p2.Y;

	FVector tangent(dx, dy, 0.f);
	tangent.Normalize();

	return std::make_pair(point, tangent);
}

FVector MathUtility::SamplePointBezier2DPlane4Point(const FVector& p0, const FVector& p1, const FVector& p2, const FVector& p3, const float& t)
{
	const float x =
		pow(1.f - t, 3) * p0.X +
		3 * t * pow(1.f - t, 2) * p1.X +
		3 * t * t * (1.f - t) * p2.X +
		t * t * t * p3.X;

	const float y =
		pow(1.f - t, 3) * p0.Y +
		3 * t * pow(1.f - t, 2) * p1.Y +
		3 * t * t * (1.f - t) * p2.Y +
		t * t * t * p3.Y;

	return FVector(x, y, 0.0f);
}

float MathUtility::BezierCurveLength(const FVector& p0, const FVector& p1, const FVector& p2)
{
	float length = 0.f;
	FVector prevPoint = p0;

	for (float t = 0.f; t < 1.f; t += 0.01f)
	{
		const auto pointTangent = SamplePointBezier2DPlane3Point(p0, p1, p2, t);
		const FVector point = pointTangent.first;

		length += sqrt(pow(point.X - prevPoint.X, 2) + pow(point.Y - prevPoint.Y, 2));

		prevPoint = point;
	}

	return length;

	/*// https://medium.com/@all2one/how-to-compute-the-length-of-a-spline-e44f5f04c40

	const auto begin_point_tangent = SamplePointBezier2DPlane3Point(p0, p1, p2, 0.001f);
	const auto end_point_tangent = SamplePointBezier2DPlane3Point(p0, p1, p2, 0.999f);

	// Cubic Hermite spline derivative coeffcients
	FVector const c0 = begin_point_tangent.second;
	FVector const c1 = 6.f * (end_point_tangent.first - begin_point_tangent.first) - 4.f * begin_point_tangent.second - 2.f * end_point_tangent.second;
	FVector const c2 = 6.f * (begin_point_tangent.first - end_point_tangent.first) + 3.f * (begin_point_tangent.second + end_point_tangent.second);

	auto const evaluate_derivative = [c0, c1, c2](float t) -> FVector
	{
		return c0 + t * (c1 + t * c2);
	};

	struct GaussLengendreCoefficient
	{
		float abscissa; // xi
		float weight;   // wi
	};

	static constexpr GaussLengendreCoefficient gauss_lengendre_coefficients[] =
	{
		{ 0.0f, 0.5688889f },
		{ -0.5384693f, 0.47862867f },
		{ 0.5384693f, 0.47862867f },
		{ -0.90617985f, 0.23692688f },
		{ 0.90617985f, 0.23692688f }
	};

	float length = 0.f;
	for (auto coefficient : gauss_lengendre_coefficients)
	{
		float const t = 0.5f * (1.f + coefficient.abscissa); // This and the final (0.5 *) below are needed for a change of interval to [0, 1] from [-1, 1]
		length += evaluate_derivative(t).Size() * coefficient.weight;
	}

	return 0.5f * length;*/
}

FVector MathUtility::UnrealEngineCoordsToWgs84AndAltitude(FVector coords)
{
	// The random 1.0 is there because the conversion requires homogeneous coordinates
	FVector4d projected = UE_TO_WGS_CONVERSION_MATRIX.TransformFVector4(FVector4d(coords, 1.0));

	// Division by W to return from homogeneous coordinates to Cartesian coordinates
	return FVector(projected / projected.W);
}

FVector MathUtility::Wgs84AndAltitudeToUnrealEngineCoords(FVector coords)
{
	// The random 1.0 is there because the conversion requires homogeneous coordinates
	FVector4d projected = WGS_TO_UE_CONVERSION_MATRIX.TransformFVector4(FVector4d(coords, 1.0));

	// Division by W to return from homogeneous coordinates to Cartesian coordinates
	return FVector(projected / projected.W);
}

FQuat MathUtility::UnrealEngineRotationToRealLifeRotation(FQuat rotation)
{
	return UE_TO_REAL_ROTATION_QUAT * rotation;
}

FQuat MathUtility::RealLifeRotationToUnrealEngineRotation(FQuat rotation)
{
	return REAL_TO_UE_ROTATION_QUAT * rotation;
}

FVector MathUtility::UnrealEngineDirectionToRealLifeDirection(FVector direction)
{
	return UE_TO_REAL_ROTATION_QUAT.RotateVector(direction);
}

FVector MathUtility::RealLifeDirectionToUnrealEngineDirection(FVector direction)
{
	return REAL_TO_UE_ROTATION_QUAT.RotateVector(direction);
}
