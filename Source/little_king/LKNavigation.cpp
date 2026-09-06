#include "LKNavigation.h"

bool LKNavigation::IsPointValid(const FVector& Point, const TArray<FObstacle>& Obstacles, const FBounds& Bounds)
{
	if (Point.ContainsNaN() || FMath::Abs(Point.X) > Bounds.HalfExtent.X || FMath::Abs(Point.Y) > Bounds.HalfExtent.Y) { return false; }
	if (Bounds.LeashRadius > 0.f && FVector::DistSquared2D(Point, Bounds.LeashCenter) > FMath::Square(Bounds.LeashRadius) + 0.1f) { return false; }
	for (const FObstacle& Obstacle : Obstacles)
	{
		if (FVector::DistSquared2D(Point, Obstacle.Center) < FMath::Square(Obstacle.Radius)) { return false; }
	}
	return true;
}

bool LKNavigation::IsSegmentClear(const FVector& A, const FVector& B, const TArray<FObstacle>& Obstacles)
{
	FVector AB = B - A; AB.Z = 0.f;
	const float LengthSquared = AB.SizeSquared();
	for (const FObstacle& Obstacle : Obstacles)
	{
		FVector AC = Obstacle.Center - A; AC.Z = 0.f;
		const float T = LengthSquared > SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(AC, AB) / LengthSquared, 0.f, 1.f) : 0.f;
		if (FVector::DistSquared2D(A + AB * T, Obstacle.Center) < FMath::Square(Obstacle.Radius)) { return false; }
	}
	return true;
}

bool LKNavigation::FindPath(const FVector& Start, const FVector& Goal, const TArray<FObstacle>& Obstacles,
	const FBounds& Bounds, TArray<FVector>& OutPath)
{
	OutPath.Reset();
	if (!IsPointValid(Start, Obstacles, Bounds) || !IsPointValid(Goal, Obstacles, Bounds)) { return false; }
	if (IsSegmentClear(Start, Goal, Obstacles)) { OutPath.Add(Goal); return true; }
	TArray<FVector> Nodes = { Start, Goal };
	constexpr int32 Sides = 16;
	for (const FObstacle& Obstacle : Obstacles)
	{
		const float Radius = (Obstacle.Radius + 2.f) / FMath::Cos(PI / Sides);
		for (int32 i = 0; i < Sides; ++i)
		{
			const float Angle = i * 2.f * PI / Sides;
			const FVector Point = Obstacle.Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
			if (IsPointValid(Point, Obstacles, Bounds)) { Nodes.Add(Point); }
		}
	}
	TArray<float> Cost; Cost.Init(TNumericLimits<float>::Max(), Nodes.Num()); Cost[0] = 0.f;
	TArray<int32> Previous; Previous.Init(INDEX_NONE, Nodes.Num());
	TArray<bool> Closed; Closed.Init(false, Nodes.Num());
	for (int32 Iteration = 0; Iteration < Nodes.Num(); ++Iteration)
	{
		int32 Current = INDEX_NONE;
		float Best = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			const float Score = Cost[i] + FVector::Dist2D(Nodes[i], Goal);
			if (!Closed[i] && Score < Best) { Current = i; Best = Score; }
		}
		if (Current == INDEX_NONE) { return false; }
		if (Current == 1)
		{
			for (int32 i = 1; i != 0; i = Previous[i]) { OutPath.Insert(Nodes[i], 0); }
			return true;
		}
		Closed[Current] = true;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Closed[i]) { continue; }
			const float Next = Cost[Current] + FVector::Dist2D(Nodes[Current], Nodes[i]);
			if (Next < Cost[i] && IsSegmentClear(Nodes[Current], Nodes[i], Obstacles))
			{
				Cost[i] = Next; Previous[i] = Current;
			}
		}
	}
	return false;
}
