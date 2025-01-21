#include "StraightLineCurve.h"
#include <algorithm>

StraightLineCurve::StraightLineCurve(const FVector& startPosition, const FVector& endPosition)
	: startPosition_(startPosition), endPosition_(endPosition)
{
	
}

void StraightLineCurve::GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const
{
	tangent = (endPosition_ - startPosition_).GetSafeNormal();
	position = startPosition_ + tangent * step;
}

float StraightLineCurve::GetLength() const
{
	return (endPosition_ - startPosition_).Length();
}
