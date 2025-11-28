#pragma once

#include "ICurve.h"

// A "curve" that's actually just a single point, but the important thing is that it still has a tangent so that cars can orient themselves etc
class SingularityCurve : public ICurve
{
public:
	virtual void GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const override;
	inline virtual float GetLength() const override { return 0.0f; }
	inline virtual std::shared_ptr<ICurve> Clone() const override { return std::make_shared<SingularityCurve>(*this); }

	virtual float GetCurvatureAt(float step) const { /*UE_LOG(LogTemp, Warning, TEXT("On straight line"));*/ return 0.0f; };

	SingularityCurve(const FVector& position, const FVector& tangent);
	virtual ~SingularityCurve() { };

protected:
	FVector position_;
	FVector tangent_;
};
