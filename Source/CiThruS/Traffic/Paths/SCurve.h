#pragma once

#include "ICurve.h"

// An S-shaped curve made of two circular arcs curving in the opposite directions
class SCurve : public ICurve
{
public:
	virtual void GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const override;
	inline virtual float GetLength() const override { return cumulativeLength_[1]; }
	inline virtual std::shared_ptr<ICurve> Clone() const override { return std::make_shared<SCurve>(*this); }

	SCurve(
		const FVector& startPosition, const FVector& endPosition,
		const FVector& startTangent, const FVector& endTangent,
		const FVector& startCurveDirection, const FVector& endCurveDirection);

protected:
	FVector startPosition_;
	FVector endPosition_;
	FVector startTangent_;
	FVector endTangent_;
	FVector startCurveDirection_;
	FVector endCurveDirection_;

	FVector pivot1_;
	FVector pivot2_;
	float radius1_;
	float radius2_;
	float cumulativeLength_[2];

	static void CalculateRadiiAndPivots(
		const FVector& end1, const FVector& end2,
		const FVector& curveDirection1, const FVector& curveDirection2,
		FVector& pivot1, FVector& pivot2,
		float& radius1, float& radius2);
};
