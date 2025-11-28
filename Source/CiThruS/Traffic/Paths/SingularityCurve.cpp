#include "SingularityCurve.h"

SingularityCurve::SingularityCurve(const FVector& position, const FVector& tangent)
	: position_(position), tangent_(tangent)
{
	
}

void SingularityCurve::GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const
{
	tangent = tangent_;
	position = position_;
}
