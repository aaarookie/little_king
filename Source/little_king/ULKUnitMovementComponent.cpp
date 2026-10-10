#include "ULKUnitMovementComponent.h"
#include "ALKUnitBase.h"
#include "ALKBattleTerrain.h"
#include "ALKUnitHero.h"
#include "ALKUnitBuilding.h"
#include "LKBuildingGeometry.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ULKPresentationSubsystem.h"
#include "ULKUnitStatusComponent.h"

ULKUnitMovementComponent::ULKUnitMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void ULKUnitMovementComponent::CollectNavigation(TArray<LKNavigation::FObstacle>& Obstacles, LKNavigation::FBounds& Bounds) const
{
    const ALKUnitBase* Self = Cast<ALKUnitBase>(GetOwner());
    Bounds.HalfExtent = FieldHalfExtent - FVector2D(SeparationRadius + 1.f);
    ALKBattleTerrain::AppendNavigationObstacles(GetWorld(),Obstacles,SeparationRadius+1.f);
    // 营地圈不再作为移动拴绳（英雄活动不限距离；圈仅作显示与未来营地 buff 范围）。
    for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
    {
        const ALKUnitBase* Other = *It;
        if (Other != Self && LKBuildingGeometry::BlocksMovement(Other))
        {
            Obstacles.Add(LKBuildingGeometry::Shape(Other, SeparationRadius + 1.f));
        }
    }
}

ALKUnitBuilding* ULKUnitMovementComponent::FindBlockingEnemyWall(const FVector& Target)
{
    ALKUnitBase* Self=Cast<ALKUnitBase>(GetOwner());
    if (!Self || !GetWorld() || Self->BasicAttackHeals()) { return nullptr; }
    const float Now=GetWorld()->GetTimeSeconds();
    if (Now<BreachRetryTime && FVector::DistSquared2D(Target,BreachDestination)<FMath::Square(80.f))
    { return BreachWall.IsValid() && BreachWall->IsAlive() ? BreachWall.Get() : nullptr; }
    BreachRetryTime=Now+.4f; BreachDestination=Target; BreachWall.Reset();
    if (CanReach(Target)) { return nullptr; }
    TArray<ALKUnitBuilding*> Candidates;
    for(TActorIterator<ALKUnitBuilding> It(GetWorld());It;++It)
    {
        if (It->IsAlive() && It->IsWall() && It->IsConstructionComplete() && It->GetTeam()!=Self->GetTeam()) { Candidates.Add(*It); }
    }
    Candidates.Sort([&](const ALKUnitBuilding& A,const ALKUnitBuilding& B)
    { return LKBuildingGeometry::SurfaceDistance(Self->GetActorLocation(),&A)<LKBuildingGeometry::SurfaceDistance(Self->GetActorLocation(),&B); });
    // Reachable perimeter, not nearest center: an inner wall must not hide the outer wall that can be broken.
    for(ALKUnitBuilding* Wall:Candidates)
    {
        const FVector Approach=Self->GetChaseDestination(Wall);
        if (LKBuildingGeometry::SurfaceDistance(Self->GetActorLocation(),Wall)<=Self->GetAttackRange() || CanReach(Approach))
        { BreachWall=Wall; return Wall; }
    }
    return nullptr;
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

bool ULKUnitMovementComponent::HasClearMeleePath(const ALKUnitBase* Target) const
{
    if (!Target || !GetOwner()) { return false; }
    const FVector Start=GetOwner()->GetActorLocation();
    FVector HitPoint=Target->GetActorLocation();
    if (const auto* Wall=Cast<ALKUnitBuilding>(Target); Wall && Wall->IsWall())
    {
        const FVector Direction=(HitPoint-Start).GetSafeNormal2D();
        const float Enter=LKBuildingGeometry::RayHit(Start,Direction,FVector::Dist2D(Start,HitPoint),LKBuildingGeometry::Shape(Wall));
        if (Enter>=0.f) { HitPoint=Start+Direction*Enter; }
    }
    TArray<LKNavigation::FObstacle> Obstacles;
    for(TActorIterator<ALKUnitBuilding> It(GetWorld());It;++It)
    {
        if (*It!=Target && *It!=GetOwner() && It->IsWall() && LKBuildingGeometry::BlocksMovement(*It))
        { Obstacles.Add(LKBuildingGeometry::Shape(*It)); }
    }
    const ALKUnitBase* Self=Cast<ALKUnitBase>(GetOwner());
    const ALKBattleTerrain* SharedHighland=Self ? ALKBattleTerrain::HighlandAt(Self) : nullptr;
    if(SharedHighland!=ALKBattleTerrain::HighlandAt(Target)) { SharedHighland=nullptr; }
    for(TActorIterator<ALKBattleTerrain> It(GetWorld());It;++It)
    { if(It->IsBlocking() && *It!=SharedHighland) { Obstacles.Add(It->Shape()); } }
    return LKNavigation::IsSegmentClear(Start,HitPoint,Obstacles);
}

bool ULKUnitMovementComponent::CanStandAt(const FVector& Location, bool bIncludeUnits) const
{
    if (!GetOwner() || !GetWorld() || Location.ContainsNaN()) { return false; }
    TArray<LKNavigation::FObstacle> Obstacles;
    LKNavigation::FBounds Bounds;
    CollectNavigation(Obstacles, Bounds);
    if (!LKNavigation::IsPointValid(Location, Obstacles, Bounds)) { return false; }
    if (bIncludeUnits)
    {
        for (TActorIterator<ALKUnitBase> It(GetWorld()); It; ++It)
        {
            if (*It != GetOwner() && It->IsAlive() && !It->IsSheltered()
                && LKBuildingGeometry::Overlaps({FVector(Location.X,Location.Y,0.f),SeparationRadius+1.f},LKBuildingGeometry::Shape(*It))) { return false; }
        }
    }
    return true;
}

bool ULKUnitMovementComponent::TeleportToFreePoint(const FVector& Location)
{
    const auto* Self=Cast<ALKUnitBase>(GetOwner());
    if (!Self || Self->IsHighlandResident() || !CanStandAt(Location, true)) { return false; }
    Stop(); Path.Reset(); RepathTimer = 0.f;
    GetOwner()->SetActorLocation(FVector(Location.X, Location.Y, 0.f));
    return true;
}

FVector ULKUnitMovementComponent::MoveSkillDelta(const FVector& Delta)
{
    ALKUnitBase* Self = Cast<ALKUnitBase>(GetOwner());
    if (!Self || !Self->IsAlive() || (Self->IsBuilding() && !Self->IsMobileBuilding()) || Self->IsSheltered() || Self->IsHighlandResident() || Delta.ContainsNaN()) { return Self ? Self->GetActorLocation() : FVector::ZeroVector; }
    Stop(); Path.Reset(); RepathTimer = 0.f;
    TArray<LKNavigation::FObstacle> Obstacles;
    LKNavigation::FBounds Bounds;
    CollectNavigation(Obstacles, Bounds);
    const FVector Start = Self->GetActorLocation();
    FVector Step(Delta.X, Delta.Y, 0.f);
    auto CanMove = [&](float Fraction)
    {
        const FVector End = Start + Step * Fraction;
        return LKNavigation::IsPointValid(End, Obstacles, Bounds) && LKNavigation::IsSegmentClear(Start, End, Obstacles);
    };
    float Fraction = 1.f;
    if (!CanMove(Fraction))
    {
        float Low = 0.f, High = 1.f;
        // Segment-clear tests catch narrow obstacles even when the destination is beyond them.
        for (int32 I = 0; I < 16; ++I)
        {
            const float Middle = (Low + High) * .5f;
            if (CanMove(Middle)) { Low = Middle; } else { High = Middle; }
        }
        Fraction = Low;
    }
    if (CanMove(Fraction)) { Self->SetActorLocation(Start + Step * Fraction); }
    return Self->GetActorLocation();
}

void ULKUnitMovementComponent::MoveToward(const FVector& InDestination, float InSpeed)
{
    if(const auto* Self=Cast<ALKUnitBase>(GetOwner()); Self && Self->IsHighlandResident()) { Stop(); return; }
    if (!bMoving || FVector::DistSquared2D(InDestination, Destination) > FMath::Square(25.f)) { RepathTimer = 0.f; }
    bMoving = true;
    Destination = FVector(InDestination.X, InDestination.Y, 0.f);
    Speed = FMath::Max(0.f, InSpeed);
}

FVector ULKUnitMovementComponent::MoveWindDelta(const FVector& Delta)
{
    const FVector Before = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    const FVector After = MoveSkillDelta(Delta);
    // Keep a separate accumulator: the field can tick before or after this component.
    WindVisualTravel += After - Before;
    return After;
}

void ULKUnitMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    VisualTravel = FVector::ZeroVector;
    ALKUnitBase* Self = Cast<ALKUnitBase>(GetOwner());
    if (Self && Self->IsHighlandResident()) { Stop(); return; }
    if (Self && (Self->IsSkillMoving() || Self->IsControlled())) { return; }
    if (!Self || !Self->IsAlive() || (Self->IsBuilding() && !Self->IsMobileBuilding()) || Self->IsSheltered() || DeltaTime <= 0.f
        || (!Self->IsCombatEnabled() && !Self->IsManualMoving())) { return; }
    RepathTimer -= DeltaTime;
    if (bMoving && Speed > 0.f && !Self->GetStatusComponent()->IsWindDriven())
    {
        const FVector BeforeMovement = Self->GetActorLocation();
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
        VisualTravel = Self->GetActorLocation() - BeforeMovement;
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
        if (Other == Self || !Other->IsAlive() || Other->IsBuilding() || Other->IsSheltered()) { continue; }
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
    Self->SetActorLocation(Position);
}
