#pragma once
#include "CoreMinimal.h"
#include "LKNavigation.h"
#include "LKTypes.h"
class ALKUnitBase;
class ALKUnitBuilding;
class UWorld;
struct FLKUnitRow;

/** One footprint definition shared by placement, navigation, projectiles and wall UI. */
namespace LKBuildingGeometry
{
    LKNavigation::FObstacle WallShape(const FVector& Center, float Yaw, float Length, float Thickness, float Padding = 0.f);
    LKNavigation::FObstacle Shape(const ALKUnitBase* Unit, float Padding = 0.f);
    bool BlocksMovement(const ALKUnitBase* Unit);
    float SurfaceDistance(const FVector& Point, const ALKUnitBase* Unit);
    bool Overlaps(const LKNavigation::FObstacle& A, const LKNavigation::FObstacle& B);
    ALKUnitBuilding* FindMatchingWall(UWorld* World, ELKTeam Team, const FVector& Center, float Yaw);
    bool CanPlaceWall(UWorld* World, ELKTeam Team, const FLKUnitRow& Row, const FVector& Center, float Yaw,
        const FVector2D& HalfExtent, const ALKUnitBuilding* Ignored = nullptr);
    void DisplaceOverlappingUnits(ALKUnitBuilding* Wall);
    /** First hit of a swept point against the padded footprint; -1 means clear. */
    float RayHit(const FVector& Start, const FVector& Direction, float Travel, const LKNavigation::FObstacle& Obstacle);
}
