#include "PlayerCarPawn.h"
#include "TrafficScenarios.h"
#include "../MathUtility.h"
#include "EngineUtils.h"

const float ACCELERATION_MULTIPLIER = 500.0f;
const float BRAKE_MULTIPLIER = 1500.0f;

const float MAX_TURN_ANGLE = 45.0f;

const float BASE_RPM = 1000.0f;
const float MAX_RPM = 6000.0f;

const float RAY_LENGTH_DOWN = 10000.0f;
const float RAY_LENGTH_UP = 100.0f;

static float CalculateTiltAngle(float posHeight, float negHeight, float distance)
{
    float heightDiff = abs(posHeight - negHeight);
    float tiltAngle = atan(heightDiff / distance) * 180.0f / PI;

    if (tiltAngle > 45.0f)
    {
        tiltAngle = 45.0f;
    }

    if (posHeight >= negHeight)
    {
        return tiltAngle;
    }
    else
    {
        return -tiltAngle;
    }
}

static float GetAverageHeight(float height1, float height2)
{
    return (height1 + height2) / 2.0f;
}

// Validate hit object to be other than the car itself
static bool CheckHit(const FHitResult& hitObject, const FString& selfName)
{
    return hitObject.GetActor() == nullptr
        || hitObject.GetActor()->GetParentActor() == nullptr
        || hitObject.GetActor()->GetParentActor()->GetName() != selfName;
}

APlayerCarPawn::APlayerCarPawn()
{
    // Set this pawn to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;
}

void APlayerCarPawn::Tick(float deltaTime)
{
    Super::Tick(deltaTime);

    APlayerCarPawn::MoveCar(deltaTime);
}

// Called to bind functionality to input
void APlayerCarPawn::SetupPlayerInputComponent(UInputComponent* playerInputComponent)
{
    Super::SetupPlayerInputComponent(playerInputComponent);

    // Respond every frame to the values of our two movement axes, "MoveX" and "MoveY".
    InputComponent->BindAxis("TurnRight", this, &APlayerCarPawn::Turn);
    InputComponent->BindAxis("Accelerate", this, &APlayerCarPawn::Accelerate);
    InputComponent->BindAxis("Brake", this, &APlayerCarPawn::Brake);
    InputComponent->BindAction("HandBrake", IE_Pressed, this, &APlayerCarPawn::ActivateHandBrake);
    InputComponent->BindAction("HandBrake", IE_Released, this, &APlayerCarPawn::ReleaseHandBrake);
    InputComponent->BindAction("GearUp", IE_Pressed, this, &APlayerCarPawn::ShiftGearUp);
    InputComponent->BindAction("GearDown", IE_Pressed, this, &APlayerCarPawn::ShiftGearDown);
}

// Reset all car private values to defaults
void APlayerCarPawn::ResetCar()
{
    acceleration_ = 0.0f;
    tireAngle_ = 0.0f;
    velocity_ = 0.0f;
    momentumVector_ = { 0.0f, 0.0f, 0.0f };
    brakes_ = 0.0f;
    rpm_ = 0;
    gear_ = Gear::Neutral;
    handBrake_ = false;
}

void APlayerCarPawn::Turn(float axisValue)
{
    // Turn tires according to the player input
    tireAngle_ = FMath::Clamp(axisValue, -1.0f, 1.0f) * MAX_TURN_ANGLE;
}

void APlayerCarPawn::Accelerate(float axisValue)
{
    acceleration_ = FMath::Clamp(axisValue, 0.0f, 1.0f) * ACCELERATION_MULTIPLIER;
}

void APlayerCarPawn::Brake(float axisValue)
{
    brakes_ = FMath::Clamp(axisValue, 0.0f, 1.0f) * BRAKE_MULTIPLIER;
}

void APlayerCarPawn::ShiftGearUp()
{
    if ((gear_ == Gear::Reverse || gear_ == Gear::Neutral) && velocity_ > 1.0f)
    {
        return;
    }

    if (gear_ != Gear::Third)
    {
        gear_ = Gear(gear_ + 1);
    }
}

void APlayerCarPawn::ShiftGearDown()
{
    if ((gear_ == Gear::First || gear_ == Gear::Neutral) && velocity_ > 1.0f)
    {
        return;
    }

    if (gear_ != Gear::Reverse)
    {
        gear_ = Gear(gear_ - 1);
    }
}

float APlayerCarPawn::GetWheelHeight(const FVector& rayPos)
{
    FHitResult hitObject;
    FCollisionQueryParams traceParams;

    FVector rayStart = rayPos;
    FVector rayEnd = rayPos;
    rayStart.Z += RAY_LENGTH_UP;
    rayEnd.Z -= RAY_LENGTH_DOWN;

    if (GetWorld()->LineTraceSingleByChannel(hitObject, rayStart, rayEnd, ECC_Visibility, traceParams))
    {
        if (CheckHit(hitObject, GetName()))
        {
            return hitObject.Location.Z;
        }
       
        rayStart.Z -= RAY_LENGTH_UP;

        if (GetWorld()->LineTraceSingleByChannel(hitObject, rayStart, rayEnd, ECC_Visibility, traceParams)
            && CheckHit(hitObject, GetName()))
        {
            return hitObject.Location.Z;
        }
    }

    return 0.0f;
}

void APlayerCarPawn::MoveCar(const float& deltaTime)
{
    /*---Car Movement---*/
    // Apply air resistance etc. natural slowdown
    if (momentumVector_.X != 0.0f || momentumVector_.Y != 0.0f)
    {
        velocity_ = sqrt(pow(momentumVector_.X, 2) + pow(momentumVector_.Y, 2));

        if (velocity_ >= 50.0f)
        {
            float velocityDiff = ((velocity_ * 0.05f) + (1000.0f / sqrt(velocity_))) * deltaTime;
            velocity_ -= velocityDiff;
        }
        else if (acceleration_ == 0.0f || gear_ == Gear::Neutral)
        {
            velocity_ = 0.0f;
            momentumVector_ *= 0.0f;
        }
    }
    else
    {
        velocity_ = 0.0f;
        momentumVector_ = { 0.0f, 0.0f, 0.0f };
    }

    // Brakes
    if (handBrake_)
    {
        // Always apply full brakes when handbrake is used
        brakes_ = BRAKE_MULTIPLIER;
    }

    if (brakes_ != 0.0f)
    {
        if (brakes_ * deltaTime > abs(velocity_))
        {
            velocity_ = 0.0f;
        }
        else
        {
            velocity_ = (abs(velocity_) - brakes_ * deltaTime) * (velocity_ < 0.0f ? -1.0f : 1.0f);
        }
    }
    // If brakes are not pressed, apply acceleration
    else if (gear_ != Gear::Neutral)
    {
        velocity_ += (((float)abs(gear_ - 1) / 3.0f + 1.0f) / 3.0f) * acceleration_ * deltaTime;
    }

    // Automatic gear switching and velocity clamp according to the current gear
    if (velocity_ * (float)FMath::Clamp(gear_ - 1, -1, 1) < gearSpeedLimits[gear_].first)
    {
        ShiftGearDown();

        if (velocity_ * (float)FMath::Clamp(gear_ - 1, -1, 1) < gearSpeedLimits[gear_].first)
        {
            velocity_ = gearSpeedLimits[gear_].first * (float)FMath::Clamp(gear_ - 1, -1, 1);
        }
    }
    else if (velocity_ * (float)FMath::Clamp(gear_ - 1, -1, 1) > gearSpeedLimits[gear_].second)
    {
        ShiftGearUp();

        if (velocity_ * (float)FMath::Clamp(gear_ - 1, -1, 1) > gearSpeedLimits[gear_].second)
        {
            velocity_ = gearSpeedLimits[gear_].second * (float)FMath::Clamp(gear_ - 1, -1, 1);
        }
    }

    // Turn the car, if the car is moving
    if (!GetVelocity().IsZero() || velocity_ != 0.0f)
    {
        FRotator rotation = GetActorRotation();
        float deltaRotation = tireAngle_ * deltaTime * FMath::Clamp(velocity_ / 545.0f, -1.0f, 1.0f) * (float)FMath::Clamp(gear_ - 1, -1, 1);

        // If going more than 30 km/h, breaking will result in drifting
        if (abs(velocity_) > KMH(30))
        {
            deltaRotation *= 1.0f + (handBrake_ ? 1.0f : 0.0f) * 1.5f;
        }

        rotation.Yaw += deltaRotation;
        SetActorRotation(rotation);
    }

    // Apply momentum if there is enough velocity that it makes sense
    if (velocity_ > 50.0f)
    {
        FVector forwardVector = GetActorForwardVector() * (float)FMath::Clamp(gear_ - 1, -1, 1);
        FVector normalizedMomentum = momentumVector_ / velocity_;
        float momentumDotProduct = FVector::DotProduct(normalizedMomentum, forwardVector);

        // How much forward movement affets the tangent, depends on the difference of momentum and forward directions
        // if the angle of these directions is 90 degrees, the effect is the smalles (drifting)
        float forwardMultiplier = 0.05f + abs(momentumDotProduct) * 0.05f;

        FVector tangent = normalizedMomentum * (0.9f + (brakes_ / BRAKE_MULTIPLIER) * forwardMultiplier)
            + (forwardVector * ((1.0f - brakes_ / BRAKE_MULTIPLIER) * forwardMultiplier));

        // Make sure that the length of tangent is 1, so no velocity is lost/gained with this
        tangent /= sqrt(pow(tangent.X, 2) + pow(tangent.Y, 2));
        momentumVector_ = tangent * velocity_;
    }
    else
    {
        momentumVector_ = GetActorForwardVector() * velocity_ * (float)FMath::Clamp(gear_ - 1, -1, 1);
    }

    FVector newLocation = GetActorLocation() + (momentumVector_ * deltaTime);
    SetActorLocation(newLocation);

    framePosition_ = newLocation;

    /*---RPM Control---*/
    // Moving and accelerating
    if (abs(acceleration_) > 0.0f && abs(velocity_) > 0.0f && gear_ != Gear::Neutral)
    {
        if (gear_ == Gear::Reverse)
        {
            rpm_ = BASE_RPM + (int)((abs(acceleration_) / ACCELERATION_MULTIPLIER) * (MAX_RPM - BASE_RPM) / (gearSpeedLimits[gear_].first / velocity_));
        }
        else
        {
            rpm_ = BASE_RPM + (int)((abs(acceleration_) / ACCELERATION_MULTIPLIER) * (MAX_RPM - BASE_RPM) / (gearSpeedLimits[gear_].second / abs(velocity_)));
        }
    }
    // Moving, but no acceleration
    else if (abs(velocity_) > 0.0f)
    {
        rpm_ -= (rpm_ - (rpm_ / abs(velocity_))) * deltaTime;
    }
    // Not moving, but "accelerating" (neutral gear + gas)
    else if (abs(acceleration_) > 0.0f)
    {
        rpm_ = BASE_RPM + (int)((MAX_RPM - BASE_RPM) / (1.0f / (abs(acceleration_) / ACCELERATION_MULTIPLIER)));
    }
    // Not moving nor accelerating
    else
    {
        rpm_ -= (float)(rpm_ - (float)rpm_ * 0.05f) * deltaTime;
    }

    if (rpm_ < 1000)
    {
        rpm_ = 1000;
    }

    /*---Gravity---*/
    // Keep wheels on the ground and tilt the vehicle according to the terrain
    FRotator carRotation = GetActorRotation();
    FVector newPos = GetActorLocation();

    float heightFrontLeft = GetWheelHeight(frontLeftRay_->GetActorLocation());
    float heightFrontRight = GetWheelHeight(frontRightRay_->GetActorLocation());
    float heightRearLeft = GetWheelHeight(rearLeftRay_->GetActorLocation());
    float heightRearRight = GetWheelHeight(rearRightRay_->GetActorLocation());

    float sumZPos = heightFrontLeft + heightFrontRight + heightRearLeft + heightRearRight;

    // How many wheel heights are not zero
    int collisions =
        (heightFrontLeft != 0.0 ? 1 : 0)
        + (heightFrontRight != 0.0 ? 1 : 0)
        + (heightRearLeft != 0.0 ? 1 : 0)
        + (heightRearRight != 0.0 ? 1 : 0);

    // Fail safe: if car is clipped through world, try raycast upwards and reset car to the surface
    if (collisions == 0)
    {
        FHitResult hitObject;
        FCollisionQueryParams traceParams;

        FVector rayStart = GetActorLocation();
        FVector rayEnd = rayStart;
        // 80 is offset to car roof
        rayStart.Z += 150;
        // Cast same length ray up, as would normally be casted down
        rayEnd.Z += RAY_LENGTH_DOWN;

        if (GetWorld()->LineTraceSingleByChannel(hitObject, rayStart, rayEnd, ECC_Visibility, traceParams))
        {
            newPos.Z = hitObject.Location.Z;
        }
    }
    else
    {
        float tiltAngle = 0.0f;

        // Calculate average height modifier and apply it to the car position
        newPos.Z = (sumZPos / (float)collisions);
        float heightFrontSide = GetAverageHeight(heightFrontLeft, heightFrontRight);
        float heightRearSide = GetAverageHeight(heightRearLeft, heightRearRight);
        float heightRightSide = GetAverageHeight(heightRearRight, heightFrontRight);
        float heightLeftSide = GetAverageHeight(heightRearLeft, heightFrontLeft);

        // Calculate car front/back tilt (Pitch)
        if (heightFrontSide != 0.0f && heightRearSide != 0.0f)
        {
            tiltAngle = CalculateTiltAngle(heightFrontSide, heightRearSide, 630.0f);

            if (abs(tiltAngle - carRotation.Pitch) > 45.0f * deltaTime)
            {
                tiltAngle = 45.0f * (tiltAngle - carRotation.Pitch > 0 ? 1 : -1) * deltaTime + carRotation.Pitch;
            }

            carRotation.Pitch = tiltAngle;
        }

        // Calculate car right/left tilt (Roll)
        if (heightLeftSide != 0.0f && heightRightSide != 0.0f)
        {
            tiltAngle = CalculateTiltAngle(heightLeftSide, heightRightSide, 220.0f);

            if (abs(tiltAngle - carRotation.Roll) > 45.0f * deltaTime)
            {
                tiltAngle = 45.0f * (tiltAngle - carRotation.Roll > 0 ? 1 : -1) * deltaTime + carRotation.Roll;
            }

            carRotation.Roll = tiltAngle;
        }

        // Apply tilts to the car
        SetActorRotation(carRotation);
    }

    // Add 80 to the height to offset the car so the tires are on the ground
    newPos.Z += 70.0f;
    SetActorLocation(newPos);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, deltaTime, FColor::Yellow, FString::Printf(TEXT("velocity: %.2f"), velocity_ * 360.0f / 10000.0f));
        GEngine->AddOnScreenDebugMessage(-1, deltaTime, FColor::Yellow, FString::Printf(TEXT("RPM: %i"), rpm_));
        GEngine->AddOnScreenDebugMessage(-1, deltaTime, FColor::Yellow, FString::Printf(TEXT("Gear: %i"), gear_ - 1));
        //GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Yellow, FString::Printf(TEXT("acceleration: %.2f"), acceleration_/accelerationMultiplier));
        //GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Yellow, FString::Printf(TEXT("brakes: %.2f"), brakes_/brakeMultiplier));
        //GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Yellow, FString::Printf(TEXT("turn: %.2f"), tireAngle_/maxTurnAngle));
    }
}
