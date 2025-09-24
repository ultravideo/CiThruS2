#include "SCurve.h"
#include <algorithm>

SCurve::SCurve(
	// World pointer needed by debug drawing functions
	/* const UWorld *world, */
	const FVector& startPosition, const FVector& endPosition,
	const FVector& startTangent, const FVector& endTangent,
	const FVector& startCurveDirection, const FVector& endCurveDirection)
	: startPosition_(startPosition), endPosition_(endPosition),
	startTangent_(startTangent), endTangent_(endTangent),
	startCurveDirection_(startCurveDirection), endCurveDirection_(endCurveDirection),
	cumulativeLength_()
{
	initCurve(/* world, */ startPosition, endPosition, startTangent, endTangent, startCurveDirection, endCurveDirection);
}

void SCurve::initCurve(
	/* const UWorld *world, */
	const FVector& startPosition, const FVector& endPosition,
	const FVector& startTangent, const FVector& endTangent,
	const FVector& startCurveDirection, const FVector& endCurveDirection)
{
	double startIdealAngleCos = startTangent.Dot(endCurveDirection);
	double endIdealAngleCos = endTangent.Dot(startCurveDirection);

	if (startIdealAngleCos < endIdealAngleCos)
	{
/*		UE_LOG(LogTemp, Warning, TEXT("SCurve constructor taking branch 1"));*/
		CalculateRadiiAndPivots(startPosition, endPosition, startCurveDirection, endCurveDirection, pivot1_, pivot2_, radius1_, radius2_);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DIDN'T THINK THIS COULD HAPPEN! SCurve constructor taking branch 2"));
		CalculateRadiiAndPivots(endPosition, startPosition, endCurveDirection, startCurveDirection, pivot2_, pivot1_, radius2_, radius1_);
	}

/* 	
	if (world) 
	{
		DrawDebugSphere(world, pivot1_, 10.0f, 16, FColor::Red, false, 10.0f, 0, 2.0f);
		DrawDebugSphere(world, pivot2_, 10.0f, 16, FColor::Green, false, 10.0f, 0, 2.0f);
		DrawDebugCircle(world, pivot1_, radius1_, 16, FColor::Red, false, 10.0f, 0, 2.0, startTangent, startCurveDirection );
		DrawDebugCircle(world, pivot2_, radius2_, 16, FColor::Green, false, 10.0f, 0, 2.0, endTangent, endCurveDirection);
	}
	*/
	
	FVector pivotToPivotDir = (pivot2_ - pivot1_).GetSafeNormal();

	float gamma1 = acos((-startCurveDirection).Dot(pivotToPivotDir));
	float gamma2 = acos((-endCurveDirection).Dot(-pivotToPivotDir));

	/*
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), startKeypointPos, c1, FColor::Blue, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), c1, c1 + dm * (radius1 + radius2), FColor::Red, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), c2, endKeypointPos, FColor::Yellow, 0.3f, 35.0f);
	*/

	//Debug::Log(FString::SanitizeFloat((float)((radius1 + radius2) - (c1 - c2).Length())) + FString(" - ") + FString::SanitizeFloat((float)(radius2 - (endKeypointPos - c2).Length())));

	float segmentLengths[2] = { gamma1 * radius1_, gamma2 * radius2_ };

	for (int i = 0; i < 2; i++)
	{
		cumulativeLength_[i] = segmentLengths[i];

		if (i > 0)
		{
			cumulativeLength_[i] += cumulativeLength_[i - 1];
		}
	}
}

void SCurve::GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const
{
	FVector pivotToPivotDir = (pivot2_ - pivot1_).GetSafeNormal();

	// Pivot is the rotation pivot, shaft is the vector being rotated, axis is the rotation axis
	FVector pivot;
	FVector shaft;
	FVector axis;

	if (step < cumulativeLength_[0])
	{
		// We're on the first circular arc
		FVector segmentStart = startPosition_;

		pivot = pivot1_;
		shaft = segmentStart - pivot;
		axis = shaft.Cross(pivotToPivotDir).GetSafeNormal();

		shaft = FQuat(axis, step / radius1_) * shaft;

		//Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), pivot, pivot + shaft, FColor::Magenta, 0.3f, 35.0f);
	}
	else
	{
		// We're on the second circular arc
		FVector segmentStart = pivot1_ + pivotToPivotDir * radius1_;

		pivot = pivot2_;
		shaft = segmentStart - pivot;
		axis = shaft.Cross(-endCurveDirection_).GetSafeNormal();

		shaft = FQuat(axis, (step - cumulativeLength_[0]) / radius2_) * shaft;

		//Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), pivot, pivot + shaft, FColor::Magenta, 0.3f, 35.0f);
	}

	tangent = axis.Cross(shaft).GetSafeNormal();
	position = pivot + shaft;
}

void SCurve::CalculateRadiiAndPivots(
	const FVector& end1, const FVector& end2,
	const FVector& curveDirection1, const FVector& curveDirection2,
	FVector& pivot1, FVector& pivot2,
	float& radius1, float& radius2)
{
	// Calculate the radii and pivots (centers) of the two circles that form the two circular arcs used by the curve.
	// There are multiple solutions for the radii, so we need to decide one radius first and then calculate the
	// other based on it. It's important that the decided radius is not too big so that the other radius has a
	// valid solution: half of the length of the distance between the points seems to work okay. On the other
	// hand, if the decided radius is too small, it's going to create curve shapes with very sudden turns which
	// looks bad, but this can be easily solved by just moving the keypoints themselves further away from each other.
	radius1 = (end2 - end1).Length() / 2.0;
	pivot1 = end1 + curveDirection1 * radius1;

	FVector kVector = pivot1 - end2;

	double k1 = kVector.Length();
	double k1CosAlpha2 = curveDirection2.Dot(kVector);

	radius2 = (k1 * k1 - radius1 * radius1) / (2.0 * (radius1 + k1CosAlpha2));

	if (radius2 < 0.0f)
	{
		// This doesn't quite work right, but the program only goes here if the positions and tangents are inappropriate,
		// and CurveFactory checks for that and decides to create another type of curve instead if they are
		double cosBeta = curveDirection2.Dot((pivot1 - (end2 + curveDirection2 * radius2)).GetSafeNormal());

		double a = radius2;
		double d = radius1 + radius2;

		radius2 = -(a * a - 2 * a * d * cosBeta + d * d - radius1 * radius1) / (2 * a - 2 * d * cosBeta - 2 * radius1);
	}

	pivot2 = end2 + curveDirection2 * radius2;
}
