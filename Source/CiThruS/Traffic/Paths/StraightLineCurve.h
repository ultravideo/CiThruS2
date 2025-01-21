#pragma once

#include "ICurve.h"

// A "curve" that's actually just a straight line
class StraightLineCurve : public ICurve
{
public:
	virtual void GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const override;
	virtual float GetLength() const override;
	inline virtual std::shared_ptr<ICurve> Clone() const override { return std::make_shared<StraightLineCurve>(*this); }

	StraightLineCurve(const FVector& startPosition, const FVector& endPosition);

protected:
	FVector startPosition_;
	FVector endPosition_;
};
