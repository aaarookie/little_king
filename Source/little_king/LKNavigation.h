#pragma once
#include "CoreMinimal.h"

/** 简单静态圆形障碍的可见图寻路；输入/输出均为战场 XY 平面。 */
namespace LKNavigation
{
	struct FObstacle { FVector Center; float Radius; };
	struct FBounds
	{
		FVector2D HalfExtent = FVector2D(1200.f, 2000.f);
		FVector LeashCenter = FVector::ZeroVector;
		float LeashRadius = 0.f; // <= 0 表示无营地约束
	};
	bool IsPointValid(const FVector& Point, const TArray<FObstacle>& Obstacles, const FBounds& Bounds);
	bool IsSegmentClear(const FVector& A, const FVector& B, const TArray<FObstacle>& Obstacles);
	bool FindPath(const FVector& Start, const FVector& Goal, const TArray<FObstacle>& Obstacles,
		const FBounds& Bounds, TArray<FVector>& OutPath);
}
