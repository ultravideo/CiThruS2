#pragma once

#include "Math/Vector.h"
#include "Math/Quat.h"

// A 3D rectangle for collision checks that is limited to always being aligned with the Z/up axis for optimization.
class CollisionRectangle
{
public:
	CollisionRectangle() { };
	CollisionRectangle(const FVector& dimensions);
	CollisionRectangle(const FVector& dimensions, const FVector& position, const FQuat& rotation);

	bool IsIntersecting(const CollisionRectangle& otherRectangle) const;
	bool IsIntersecting(const FVector2D& point) const;
	bool IsIntersecting2DTriangle(const FVector2D& corner0, const FVector2D& corner1, const FVector2D& corner2) const;

	void Visualize(UWorld* world, const float& deltaTime) const;

	void SetPosition(const FVector& position);
	void SetRotation(const FQuat& rotation);
	void SetDimensions(const FVector& dimensions);

	inline FVector GetPosition() const { return position_; };
	inline FQuat GetRotation() const { return rotation_; };
	inline FVector GetDimensions() const { return dimensions_; };

protected:
	FVector2D corners_[4];
	float top_;
	float bottom_;

	FVector position_;
	FQuat rotation_;
	FVector dimensions_;

	void UpdateShape();
};
