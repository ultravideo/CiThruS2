#include "SingleCurve.h"
#include <algorithm>

SingleCurve::SingleCurve(
	const FVector& startPosition, const FVector& endPosition,
	const FVector& startTangent, const FVector& endTangent,
	const FVector& startCurveDirection, const FVector& endCurveDirection)
	: startPosition_(startPosition), endPosition_(endPosition),
	startTangent_(startTangent), endTangent_(endTangent),
	startCurveDirection_(startCurveDirection), endCurveDirection_(endCurveDirection),
	cumulativeLength_()
{
	FVector startToEnd = (endPosition - startPosition);

	// Calculate where the start and end tangent intersect and fit the largest possible circular arc in the corner between them
	// while keeping the tangents continuous and making sure the arc does not go past the start or end position
	double a = startToEnd.Length();
	double alpha = acos(std::min(startTangent.Dot(-endTangent), 1.0));
	double beta = acos(std::min(endTangent.Dot(startToEnd.GetSafeNormal()), 1.0));
	double gamma = acos(std::min(startTangent.Dot(startToEnd.GetSafeNormal()), 1.0));
	double b = a * sin(beta) / sin(alpha);
	double c = a * sin(gamma) / sin(alpha);

	tangentsIntersection_ = startPosition + startTangent * b;

	float circleDistance = std::min(b, c);

	startAnchor_ = tangentsIntersection_ - startTangent * circleDistance;
	endAnchor_ = endPosition - endTangent * (c - circleDistance);

	radius_ = tan(alpha * 0.5) * circleDistance;
	float gamma2 = PI - alpha;

	/*Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), startKeypointPos, startAnchor, FColor::Blue, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), startAnchor, pivot, FColor::Red, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), pivot, endAnchor, FColor::Red, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), endAnchor, endKeypointPos, FColor::Yellow, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), endAnchor, tangentsIntersection, FColor::Green, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), startAnchor, tangentsIntersection, FColor::Green, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), startKeypointPos, startKeypointPos + startPointTangent * 100, FColor::Orange, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), endKeypointPos, endKeypointPos + endPointTangent * 100, FColor::Orange, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), startKeypointPos, startKeypointPos + d1 * 100, FColor::Orange, 0.3f, 35.0f);
	Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), endKeypointPos, endKeypointPos + d2 * 100, FColor::Orange, 0.3f, 35.0f);*/

	//Debug::Log(FString::SanitizeFloat(a) + FString(" - ") + FString::SanitizeFloat(b) + FString(" - ") + FString::SanitizeFloat(c) + FString(" - ") + FString::SanitizeFloat(radius) + FString(" - ") + FString::SanitizeFloat(circleDistance));

	float segmentLengths[3] = { b - circleDistance, gamma2 * radius_, c - circleDistance };

	for (int i = 0; i < 3; i++)
	{
		cumulativeLength_[i] = segmentLengths[i];

		if (i > 0)
		{
			cumulativeLength_[i] += cumulativeLength_[i - 1];
		}
	}
}

void SingleCurve::GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const
{
	if (step < cumulativeLength_[0])
	{
		// We're on the straight line before the arc
		tangent = startTangent_;
		position = startPosition_ + (tangentsIntersection_ - startPosition_).GetSafeNormal() * step;
	}
	else if (step < cumulativeLength_[1])
	{
		// We're on the circular arc
		// Pivot is the rotation pivot, shaft is the vector being rotated, axis is the rotation axis
		FVector pivot = startAnchor_ + startCurveDirection_ * radius_;
		FVector segmentStart = startAnchor_;
		FVector shaft = segmentStart - pivot;
		FVector axis = shaft.Cross(endAnchor_ - pivot).GetSafeNormal();

		shaft = FQuat(axis, (step - cumulativeLength_[0]) / radius_) * shaft;

		//Debug::DrawTemporaryLine(trafficEntity_->GetWorld(), pivot, pivot + shaft, FColor::Magenta, 0.3f, 35.0f);

		tangent = axis.Cross(shaft).GetSafeNormal();

		position = pivot + shaft;
	}
	else
	{
		// We're on the straight line after the arc
		tangent = endTangent_;
		position = endAnchor_ + (endPosition_ - endAnchor_).GetSafeNormal() * (step - cumulativeLength_[1]);
	}
}
