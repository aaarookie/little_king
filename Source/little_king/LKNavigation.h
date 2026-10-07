#pragma once
#include "CoreMinimal.h"

/** 圆/胶囊障碍寻路：小场景可见图，大场景有界网格 A*；输入/输出均为 XY 平面。 */
namespace LKNavigation
{
    /** Circle, or capsule when HalfSegment is nonzero. Radius already includes mover clearance. */
	struct FObstacle { FVector Center; float Radius; FVector HalfSegment = FVector::ZeroVector; };
    float PointDistance(const FVector& Point, const FObstacle& Obstacle);
	struct FBounds
	{
		FVector2D HalfExtent = FVector2D(1200.f, 2000.f);
		FVector LeashCenter = FVector::ZeroVector;
		float LeashRadius = 0.f; // <= 0 表示无营地约束
	};
	bool IsPointValid(const FVector& Point, const TArray<FObstacle>& Obstacles, const FBounds& Bounds);
	bool IsSegmentClear(const FVector& A, const FVector& B, const TArray<FObstacle>& Obstacles);
	/** Optional diagnostics for deterministic budget assertions and non-gating performance logs. */
	struct FSearchStats
	{
		bool bUsedGrid = false;
		bool bReusedGrid = false;
		int32 GridNodes = 0;
		int32 VisitedNodes = 0;
		int32 ExactSegmentTests = 0;
	};
	constexpr int32 GridNodeLimit = 8192;
	/** Main-thread API. Four recent pure-geometry grids are cached; no actors/worlds are retained. */
	bool FindPath(const FVector& Start, const FVector& Goal, const TArray<FObstacle>& Obstacles,
		const FBounds& Bounds, TArray<FVector>& OutPath, FSearchStats* OutStats = nullptr);
}
