#include "CollisionRectangle.h"
#include "../MathUtility.h"
#include "../Debug.h"

CollisionRectangle::CollisionRectangle(const FVector& dimensions)
	: corners_(), top_(), bottom_(), dimensions_(dimensions)
{
	UpdateShape();
}

CollisionRectangle::CollisionRectangle(const FVector& dimensions, const FVector& position, const FQuat& rotation)
	: corners_(), top_(), bottom_(), position_(position), rotation_(rotation), dimensions_(dimensions)
{
	UpdateShape();
}

bool CollisionRectangle::IsIntersecting(const CollisionRectangle& otherRectangle) const
{
	return top_ > otherRectangle.bottom_ && bottom_ < otherRectangle.top_
		&& MathUtility::PolygonIntersectingPolygon(corners_, 4, otherRectangle.corners_, 4);
}

bool CollisionRectangle::IsIntersecting(const FVector2D& point) const
{
	return MathUtility::PointInsideRectangle(point, corners_[0], corners_[1], corners_[2], corners_[3]);
}

bool CollisionRectangle::IsIntersecting2DTriangle(const FVector2D& corner0, const FVector2D& corner1, const FVector2D& corner2) const
{
	FVector2D corners[3] = { corner0, corner1, corner2 };
	return MathUtility::PolygonIntersectingPolygon(corners_, 4, corners, 3);
}

void CollisionRectangle::Visualize(UWorld* world, const float& deltaTime) const
{
	// Draw top
	Debug::DrawTemporaryLine(world, FVector(corners_[0], top_), FVector(corners_[1], top_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[1], top_), FVector(corners_[2], top_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[2], top_), FVector(corners_[3], top_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[3], top_), FVector(corners_[0], top_), FColor::Red, deltaTime * 1.1f, 15.0f);

	// Draw bottom
	Debug::DrawTemporaryLine(world, FVector(corners_[0], bottom_), FVector(corners_[1], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[1], bottom_), FVector(corners_[2], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[2], bottom_), FVector(corners_[3], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[3], bottom_), FVector(corners_[0], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);

	// Draw sides
	Debug::DrawTemporaryLine(world, FVector(corners_[0], top_), FVector(corners_[0], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[1], top_), FVector(corners_[1], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[2], top_), FVector(corners_[2], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);
	Debug::DrawTemporaryLine(world, FVector(corners_[3], top_), FVector(corners_[3], bottom_), FColor::Red, deltaTime * 1.1f, 15.0f);
}

void CollisionRectangle::SetPosition(const FVector& position)
{
	position_ = position;

	UpdateShape();
}

void CollisionRectangle::SetRotation(const FQuat& rotation)
{
	rotation_ = rotation;

	UpdateShape();
}

void CollisionRectangle::SetDimensions(const FVector& dimensions)
{
	// Don't allow negative dimensions
	dimensions_ = FVector(
		FMath::Max(dimensions.X, 0.0f),
		FMath::Max(dimensions.Y, 0.0f),
		FMath::Max(dimensions.Z, 0.0f));

	UpdateShape();
}

void CollisionRectangle::UpdateShape()
{
	// Calculate new corner positions
	// The rotation needs to be calculated in local space before applying position
	FVector newCorners[4] =
	{
		position_ + rotation_ * FVector(-dimensions_.X * 0.5f, -dimensions_.Y * 0.5f, 0.0f),
		position_ + rotation_ * FVector(dimensions_.X * 0.5f, -dimensions_.Y * 0.5f, 0.0f),
		position_ + rotation_ * FVector(dimensions_.X * 0.5f, dimensions_.Y * 0.5f, 0.0f),
		position_ + rotation_ * FVector(-dimensions_.X * 0.5f, dimensions_.Y * 0.5f, 0.0f)
	};

	float maxZ = position_.Z;
	float minZ = position_.Z;

	for (int i = 0; i < 4; i++)
	{
		if (newCorners[i].Z > maxZ)
		{
			maxZ = newCorners[i].Z;
		}

		if (newCorners[i].Z < minZ)
		{
			minZ = newCorners[i].Z;
		}

		// Flatten 3D corners to 2D
		corners_[i] = FVector2D(newCorners[i]);
	}

	// Adding the min Z and max Z here means that the rectangle's height changes according
	// to the rotation along the x or y axes. This should be a good enough way to include
	// the rotation in the shape of the rectangle because the corners are 2D and cannot
	// include x or y rotation
	top_ = maxZ + dimensions_.Z * 0.5f;
	bottom_ = minZ - dimensions_.Z * 0.5f;
}
