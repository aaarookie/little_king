#include "ULKUnitMovementComponent.h"
#include "ALKUnitBase.h"
#include "ALKUnitHero.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ULKUnitMovementComponent::ULKUnitMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void ULKUnitMovementComponent::CollectNavigation(TArray<LKNavigation::FObstacle>& Obstacles, LKNavigation::FBounds& Bounds) const
{
    const ALKUnitBase* Self = Cast<ALKUnitBase>(GetOwner());
    Bounds.HalfExtent = FieldHalfExtent - FVector2D(SeparationRadius + 1.f);
    if (const ALKUnitHero* Hero = Cast<ALKUnitHero>(Self))
    {
        if (Hero->GetCampMoveRadius() > 0.f)
        {
            Bounds.LeashCenter = Hero->GetCampCenter();
            Bounds.LeashRadius = Hero->GetCampMoveRadius() - SeparationRadius;
        }
    }
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        const ALKUnitBase* Other = *It;
        if (Other != Self && Other->IsAlive() && Other->IsBuilding())
        {
            Obstacles.Add({Other->GetActorLocation(), Other->GetBodyRadius() + SeparationRadius + 1.f});
        }
    }
}

bool ULKUnitMovementComponent::CanReach(const FVector& Target) const
{
    if (!GetOwner() || !GetWorld() || Target.ContainsNaN()) { return false; }
    TArray<LKNavigation::FObstacle> Obstacles;
    LKNavigation::FBounds Bounds;
    CollectNavigation(Obstacles, Bounds);
    TArray<FVector> TestPath;
    return LKNavigation::FindPath(GetOwner()->GetActorLocation(), FVector(Target.X, Target.Y, 0.f), Obstacles, Bounds, TestPath);
}

void ULKUnitMovementComponent::MoveToward(const FVector& InDestination, float InSpeed)
{
    if (!bMoving || FVector::DistSquared2D(InDestination, Destination) > FMath::Square(25.f)) { RepathTimer = 0.f; }
    bMoving = true;
    Destination = FVector(InDestination.X, InDestination.Y, 0.f);
    Speed = FMath::Max(0.f, InSpeed);
}

void ULKUnitMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ALKUnitBase* Self = Cast<ALKUnitBase>(GetOwner());
    if (!Self || !Self->IsAlive() || Self->IsBuilding() || DeltaTime <= 0.f
        || (!Self->IsCombatEnabled() && !Self->IsManualMoving())) { return; }
    RepathTimer -= DeltaTime;
    if (bMoving && Speed > 0.f)
    {
        TArray<LKNavigation::FObstacle> Obstacles;
        LKNavigation::FBounds Bounds;
        CollectNavigation(Obstacles, Bounds);
        if (RepathTimer <= 0.f)
        {
            LKNavigation::FindPath(Self->GetActorLocation(), Destination, Obstacles, Bounds, Path);
            PathDestination = Destination;
            RepathTimer = 0.3f;
        }
        float RemainingStep = Speed * DeltaTime;
        while (!Path.IsEmpty() && RemainingStep > 0.f)
        {
            const FVector Current = Self->GetActorLocation();
            const float Distance = FVector::Dist2D(Current, Path[0]);
            const float Step = FMath::Min(RemainingStep, Distance);
            const FVector Next = Current + (Path[0] - Current).GetSafeNormal2D() * Step;
            if (!LKNavigation::IsPointValid(Next, Obstacles, Bounds) || !LKNavigation::IsSegmentClear(Current, Next, Obstacles))
            {
                Path.Reset(); RepathTimer = 0.f; break;
            }
            Self->SetActorLocation(Next);
            RemainingStep -= Step;
            if (Distance <= Step + 0.1f) { Path.RemoveAt(0); } else { break; }
        }
        if (FVector::Dist2D(Self->GetActorLocation(), Destination) <= 2.f) { bMoving = false; }
    }
    if (Self->IsCombatEnabled()) { ApplySeparation(DeltaTime); }
    ClampToFieldBounds();
}

void ULKUnitMovementComponent::ApplySeparation(float DeltaTime)
{
    ALKUnitBase* Self = Cast<ALKUnitBase>(GetOwner());
    TArray<LKNavigation::FObstacle> Obstacles;
    LKNavigation::FBounds Bounds;
    CollectNavigation(Obstacles, Bounds);
    FVector Push = FVector::ZeroVector;
    // 单位中心/半径是规则来源，避免将 overlap 的 blocking 返回值当作数组非空。
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        ALKUnitBase* Other = *It;
        if (Other == Self || !Other->IsAlive() || Other->IsBuilding()) { continue; }
        FVector Delta = Self->GetActorLocation() - Other->GetActorLocation(); Delta.Z = 0.f;
        const float Distance = Delta.Size();
        const float Minimum = Self->GetBodyRadius() + Other->GetBodyRadius();
        if (Distance < Minimum)
        {
            const FVector Direction = Distance > 0.01f ? Delta / Distance
                : FVector(Self->GetUniqueID() < Other->GetUniqueID() ? -1.f : 1.f, 0.f, 0.f);
            Push += Direction * (Minimum - Distance) * FMath::Min(0.5f, DeltaTime * 8.f);
        }
    }
    Push = Push.GetClampedToMaxSize(SeparationRadius * 0.25f);
    for (int32 Attempt = 0; Attempt < 5; ++Attempt)
    {
        const FVector Next = Self->GetActorLocation() + Push;
        if (LKNavigation::IsPointValid(Next, Obstacles, Bounds) && LKNavigation::IsSegmentClear(Self->GetActorLocation(), Next, Obstacles))
        {
            Self->SetActorLocation(Next); break;
        }
        Push *= 0.5f;
    }
}

void ULKUnitMovementComponent::ClampToFieldBounds()
{
    ALKUnitBase* Self = Cast<ALKUnitBase>(GetOwner());
    if (!Self || !bHasFieldBounds) { return; }
    FVector Position = Self->GetActorLocation();
    Position.X = FMath::Clamp(Position.X, -FieldHalfExtent.X + SeparationRadius + 1.f, FieldHalfExtent.X - SeparationRadius - 1.f);
    Position.Y = FMath::Clamp(Position.Y, -FieldHalfExtent.Y + SeparationRadius + 1.f, FieldHalfExtent.Y - SeparationRadius - 1.f);
    if (const ALKUnitHero* Hero = Cast<ALKUnitHero>(Self))
    {
        if (Hero->GetCampMoveRadius() > 0.f)
        {
            Position = Hero->GetCampCenter() + (Position - Hero->GetCampCenter()).GetClampedToMaxSize2D(Hero->GetCampMoveRadius() - SeparationRadius);
        }
    }
    Self->SetActorLocation(Position);
}
