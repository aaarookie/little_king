#include "LKBuildingGeometry.h"
#include "ALKBattleTerrain.h"
#include "ALKUnitBuilding.h"
#include "LKDataTypes.h"
#include "ULKUnitMovementComponent.h"
#include "EngineUtils.h"

LKNavigation::FObstacle LKBuildingGeometry::WallShape(const FVector& Center, float Yaw, float Length, float Thickness, float Padding)
{
    const float Radius = FMath::Max(1.f, Thickness * .5f);
    return {FVector(Center.X,Center.Y,0.f), Radius + Padding,
        FRotator(0.f,Yaw,0.f).Vector() * FMath::Max(0.f, Length * .5f - Radius)};
}

LKNavigation::FObstacle LKBuildingGeometry::Shape(const ALKUnitBase* Unit, float Padding)
{
    if (const ALKUnitBuilding* Wall = Cast<ALKUnitBuilding>(Unit); Wall && Wall->IsWall())
    { return WallShape(Wall->GetActorLocation(),Wall->GetWallYaw(),Wall->GetWallLength(),Wall->GetWallThickness(),Padding); }
    return {Unit->GetActorLocation(), Unit->GetBodyRadius()+Padding};
}

bool LKBuildingGeometry::BlocksMovement(const ALKUnitBase* Unit)
{
    if (!Unit || !Unit->IsAlive() || !Unit->IsBuilding()) { return false; }
    const ALKUnitBuilding* Wall = Cast<ALKUnitBuilding>(Unit);
    return !Wall || !Wall->IsWall() || Wall->IsConstructionComplete();
}

float LKBuildingGeometry::SurfaceDistance(const FVector& Point, const ALKUnitBase* Unit)
{
    const auto Footprint=Shape(Unit);
    return FMath::Max(0.f,LKNavigation::PointDistance(Point,Footprint)-Footprint.Radius);
}

bool LKBuildingGeometry::Overlaps(const LKNavigation::FObstacle& A, const LKNavigation::FObstacle& B)
{
    FVector PA,PB;
    FMath::SegmentDistToSegmentSafe(A.Center-A.HalfSegment,A.Center+A.HalfSegment,
        B.Center-B.HalfSegment,B.Center+B.HalfSegment,PA,PB);
    return FVector::DistSquared2D(PA,PB)<FMath::Square(A.Radius+B.Radius);
}

ALKUnitBuilding* LKBuildingGeometry::FindMatchingWall(UWorld* World, ELKTeam Team, const FVector& Center, float Yaw)
{
    if (!World || Center.ContainsNaN() || !FMath::IsFinite(Yaw)) { return nullptr; }
    ALKUnitBuilding* Best=nullptr;
    float BestDistance=FMath::Square(35.f);
    for (TActorIterator<ALKUnitBuilding> It(World);It;++It)
    {
        if (!It->IsAlive() || !It->IsWall() || It->GetTeam()!=Team) { continue; }
        // A reversed front is still the same physical wall: repairs preserve its original front.
        const float Angle=FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,It->GetWallYaw()));
        if (FMath::Min(Angle,FMath::Abs(180.f-Angle))>10.f) { continue; }
        const float Distance=FVector::DistSquared2D(Center,It->GetActorLocation());
        if (Distance<BestDistance || (FMath::IsNearlyEqual(Distance,BestDistance) && (!Best || It->GetUniqueID()<Best->GetUniqueID())))
        { Best=*It; BestDistance=Distance; }
    }
    return Best;
}

bool LKBuildingGeometry::CanPlaceWall(UWorld* World, ELKTeam Team, const FLKUnitRow& Row, const FVector& Center, float Yaw,
    const FVector2D& HalfExtent, const ALKUnitBuilding* Ignored)
{
    if (!World || Center.ContainsNaN() || !FMath::IsFinite(Yaw) || !FMath::IsFinite(Row.WallLength)
        || Row.WallLength<=0.f || !FMath::IsFinite(Row.WallThickness) || Row.WallThickness<=0.f || Row.WallThickness>Row.WallLength) { return false; }
    const auto Wall=WallShape(Center,Yaw,Row.WallLength,Row.WallThickness);
    if(ALKBattleTerrain::IsObstacleBlocked(World,Wall)) { return false; }
    const FVector Extent=Wall.HalfSegment.GetAbs()+FVector(Wall.Radius,Wall.Radius,0.f);
    if (FMath::Abs(Center.X)+Extent.X>=HalfExtent.X || FMath::Abs(Center.Y)+Extent.Y>=HalfExtent.Y
        || (Team==ELKTeam::Player ? Center.Y+Extent.Y>0.f : Center.Y-Extent.Y<0.f)) { return false; }
    for (TActorIterator<ALKUnitBase> It(World);It;++It)
    {
        if (*It==Ignored || !It->IsAlive() || !It->IsBuilding()) { continue; }
        if (const auto* Other=Cast<ALKUnitBuilding>(*It); Other && Other->IsWall()) { continue; }
        if (Overlaps(Wall,Shape(*It,2.f))) { return false; }
    }
    return true;
}

void LKBuildingGeometry::DisplaceOverlappingUnits(ALKUnitBuilding* Wall)
{
    if (!Wall || !Wall->IsAlive() || !Wall->IsWall() || !Wall->IsConstructionComplete()) { return; }
    for (TActorIterator<ALKUnitBase> It(Wall->GetWorld());It;++It)
    {
        if (!It->IsAlive() || (It->IsBuilding() && !It->IsMobileBuilding()) || It->IsSheltered()) { continue; }
        if (!Overlaps(Shape(Wall),Shape(*It,1.f))) { continue; }
        const FVector Origin=It->GetActorLocation();
        // Construction can complete over walking units. Move to the nearest available point, without damage.
        bool bFound=false;
        for (int32 Ring=1;Ring<=20 && !bFound;++Ring)
        {
            for(int32 I=0;I<32;++I)
            {
                const float Angle=I*2.f*PI/32.f;
                const FVector Point=Origin+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0.f)*(Ring*20.f);
                if (It->GetMovementComponent()->TeleportToFreePoint(Point)) { bFound=true; break; }
            }
        }
    }
}

float LKBuildingGeometry::RayHit(const FVector& Start, const FVector& Direction, float Travel, const LKNavigation::FObstacle& Obstacle)
{
    // Convex capsule: reject clear segment, then bisect the first non-clear prefix.
    const TArray<LKNavigation::FObstacle> Single={Obstacle};
    if (LKNavigation::PointDistance(Start,Obstacle)<=Obstacle.Radius) { return 0.f; }
    if (LKNavigation::IsSegmentClear(Start,Start+Direction*Travel,Single)) { return -1.f; }
    float Low=0.f,High=Travel;
    for(int I=0;I<18;++I)
    {
        const float Mid=(Low+High)*.5f;
        if (LKNavigation::IsSegmentClear(Start,Start+Direction*Mid,Single)) { Low=Mid; } else { High=Mid; }
    }
    return High;
}
