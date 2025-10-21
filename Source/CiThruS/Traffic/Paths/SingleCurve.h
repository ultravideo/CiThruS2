#pragma once

#include "ICurve.h"

// A curve made of two straight lines and a single circular arc connecting them
class SingleCurve : public ICurve
{
public:
	virtual void GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const override;
	inline virtual float GetLength() const override { return cumulativeLength_[2]; }
	inline virtual std::shared_ptr<ICurve> Clone() const override { return std::make_shared<SingleCurve>(*this); }

	virtual float GetCurveProgress(float segmentProgress) const override;
	virtual float GetCurvatureAt(float segmentProgress) const override;

	SingleCurve(
		const FVector& startPosition, const FVector& endPosition,
		const FVector& startTangent, const FVector& endTangent,
		const FVector& startCurveDirection, const FVector& endCurveDirection);
	virtual ~SingleCurve() { };

protected:
	FVector startPosition_;
	FVector endPosition_;
	FVector startTangent_;
	FVector endTangent_;
	FVector startCurveDirection_;
	FVector endCurveDirection_;

	FVector tangentsIntersection_;
	FVector startAnchor_;
	FVector endAnchor_;
	float radius_;
	float cumulativeLength_[3];
};
