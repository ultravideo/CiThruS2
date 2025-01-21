#pragma once

#include "CoreMinimal.h"

#include <memory>

// Interface for using different types of curves
class ICurve
{
public:
	// Get the position and tangent on the curve at step units away from the curve's starting point
	virtual void GetPositionAndTangent(const float& step, FVector& position, FVector& tangent) const = 0;

	// Get the total length of the curve
	virtual float GetLength() const = 0;

	// Create a copy of the curve
	virtual std::shared_ptr<ICurve> Clone() const = 0;

protected:
	ICurve() { };
};
