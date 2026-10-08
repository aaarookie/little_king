#include "LKNavigation.h"
#include "Algo/Reverse.h"

namespace
{
constexpr int32 VisibilityObstacleLimit = 12;
constexpr int32 GridTargetNodes = 6000;
constexpr int32 GridCacheLimit = 4;
constexpr int32 GridAxisLimit = 128;
constexpr int32 Directions[8][2] = { {-1,-1}, {0,-1}, {1,-1}, {-1,0}, {1,0}, {-1,1}, {0,1}, {1,1} };

/** Spatially indexed graph. Each node/edge is checked against the actual capsules, not raster-filled cells. */
struct FNavigationGrid
{
    LKNavigation::FBounds Bounds;
    TArray<LKNavigation::FObstacle> Obstacles;
    TArray<TArray<int32>> Buckets;
    TArray<uint8> Walkable, CheckedEdges, ClearEdges;
    TArray<uint32> ObstacleStamps;
    float Step = 40.f;
    int32 Columns = 0, Rows = 0;
    uint32 QueryStamp = 0;
    uint64 LastUsed = 0;

    bool Matches(const TArray<LKNavigation::FObstacle>& Other, const LKNavigation::FBounds& OtherBounds) const
    {
        if (Bounds.HalfExtent != OtherBounds.HalfExtent || Bounds.LeashCenter != OtherBounds.LeashCenter
            || Bounds.LeashRadius != OtherBounds.LeashRadius || Obstacles.Num() != Other.Num()) { return false; }
        for (int32 I = 0; I < Other.Num(); ++I)
        {
            if (Obstacles[I].Center != Other[I].Center || Obstacles[I].HalfSegment != Other[I].HalfSegment
                || Obstacles[I].Radius != Other[I].Radius || Obstacles[I].BoxHalfExtent != Other[I].BoxHalfExtent
                || Obstacles[I].BoxYaw != Other[I].BoxYaw) { return false; }
        }
        return true;
    }
    int32 XAt(double X) const { return FMath::Clamp(FMath::FloorToInt((X + Bounds.HalfExtent.X) / Step), 0, Columns - 1); }
    int32 YAt(double Y) const { return FMath::Clamp(FMath::FloorToInt((Y + Bounds.HalfExtent.Y) / Step), 0, Rows - 1); }
    FVector Point(int32 Index) const
    {
        return FVector(FMath::Min(-Bounds.HalfExtent.X + (Index % Columns) * Step, Bounds.HalfExtent.X),
            FMath::Min(-Bounds.HalfExtent.Y + (Index / Columns) * Step, Bounds.HalfExtent.Y), 0.f);
    }
    void Build(const TArray<LKNavigation::FObstacle>& InObstacles, const LKNavigation::FBounds& InBounds)
    {
        Bounds = InBounds; Obstacles = InObstacles;
        const double Width = Bounds.HalfExtent.X * 2.0, Height = Bounds.HalfExtent.Y * 2.0;
        Step = float(FMath::Max(40.0, FMath::Max(FMath::Sqrt(Width * Height / GridTargetNodes), FMath::Max(Width, Height) / GridAxisLimit)));
        do
        {
            Columns = FMath::CeilToInt(Width / Step) + 1; Rows = FMath::CeilToInt(Height / Step) + 1;
            if (Columns * Rows <= LKNavigation::GridNodeLimit) { break; }
            Step *= 1.05f;
        } while (true);
        const int32 Count = Columns * Rows;
        Buckets.SetNum(Count); Walkable.Init(1, Count); CheckedEdges.Init(0, Count); ClearEdges.Init(0, Count);
        ObstacleStamps.Init(0, Obstacles.Num());
        for (int32 I = 0; I < Obstacles.Num(); ++I)
        {
            const auto& O = Obstacles[I];
            const FVector BoxX = FRotator(0,O.BoxYaw,0).RotateVector(FVector(O.BoxHalfExtent.X+O.Radius,0,0));
            const FVector BoxY = FRotator(0,O.BoxYaw,0).RotateVector(FVector(0,O.BoxHalfExtent.Y+O.Radius,0));
            const FVector Extent = O.IsBox() ? BoxX.GetAbs()+BoxY.GetAbs() : O.HalfSegment.GetAbs() + FVector(O.Radius, O.Radius, 0.f);
            for (int32 Y = YAt(O.Center.Y - Extent.Y); Y <= YAt(O.Center.Y + Extent.Y); ++Y)
            { for (int32 X = XAt(O.Center.X - Extent.X); X <= XAt(O.Center.X + Extent.X); ++X) { Buckets[Y * Columns + X].Add(I); } }
        }
        for (int32 I = 0; I < Count; ++I)
        {
            const FVector P = Point(I);
            if (Bounds.LeashRadius > 0.f && FVector::DistSquared2D(P, Bounds.LeashCenter) > FMath::Square(Bounds.LeashRadius) + .1f)
            { Walkable[I] = 0; continue; }
            for (int32 O : Buckets[YAt(P.Y) * Columns + XAt(P.X)])
            { if (LKNavigation::PointDistance(P, Obstacles[O]) < Obstacles[O].Radius) { Walkable[I] = 0; break; } }
        }
    }
    bool SegmentClear(const FVector& A, const FVector& B, LKNavigation::FSearchStats* Stats)
    {
        if (++QueryStamp == 0) { ObstacleStamps.Init(0, Obstacles.Num()); ++QueryStamp; }
        for (int32 Y = YAt(FMath::Min(A.Y, B.Y)); Y <= YAt(FMath::Max(A.Y, B.Y)); ++Y)
        {
            for (int32 X = XAt(FMath::Min(A.X, B.X)); X <= XAt(FMath::Max(A.X, B.X)); ++X)
            {
                for (int32 Index : Buckets[Y * Columns + X])
                {
                    if (ObstacleStamps[Index] == QueryStamp) { continue; }
                    ObstacleStamps[Index] = QueryStamp;
                    const auto& O = Obstacles[Index];
                    if (Stats) { ++Stats->ExactSegmentTests; }
                    if (LKNavigation::SegmentHitsObstacle(A,B,O)) { return false; }
                }
            }
        }
        return true;
    }
    bool EdgeClear(int32 From, int32 To, int32 Direction, LKNavigation::FSearchStats* Stats)
    {
        const uint8 Mask = uint8(1 << Direction);
        if (!(CheckedEdges[From] & Mask))
        {
            const uint8 ReverseMask = uint8(1 << (7 - Direction));
            CheckedEdges[From] |= Mask; CheckedEdges[To] |= ReverseMask;
            if (SegmentClear(Point(From), Point(To), Stats)) { ClearEdges[From] |= Mask; ClearEdges[To] |= ReverseMask; }
        }
        return (ClearEdges[From] & Mask) != 0;
    }
    TArray<int32> Connections(const FVector& P, LKNavigation::FSearchStats* Stats)
    {
        TArray<int32> Result; const int32 CX = XAt(P.X), CY = YAt(P.Y);
        for (int32 Y = FMath::Max(0, CY - 2); Y <= FMath::Min(Rows - 1, CY + 2); ++Y)
        {
            for (int32 X = FMath::Max(0, CX - 2); X <= FMath::Min(Columns - 1, CX + 2); ++X)
            {
                const int32 Index = Y * Columns + X;
                if (Walkable[Index] && SegmentClear(P, Point(Index), Stats)) { Result.Add(Index); }
            }
        }
        return Result;
    }
};

FNavigationGrid& GridFor(const TArray<LKNavigation::FObstacle>& Obstacles, const LKNavigation::FBounds& Bounds, LKNavigation::FSearchStats* Stats)
{
    // All callers are gameplay/navigation on the game thread. Caches contain only value geometry;
    // construction, rotation, death or mover clearance changes automatically select a fresh graph.
    static TArray<TUniquePtr<FNavigationGrid>> Cache;
    static uint64 Serial = 0;
    for (auto& Entry : Cache)
    {
        if (Entry->Matches(Obstacles, Bounds))
        { Entry->LastUsed = ++Serial; if (Stats) { Stats->bReusedGrid = true; } return *Entry; }
    }
    int32 Index = Cache.Num();
    if (Index < GridCacheLimit) { Cache.Add(MakeUnique<FNavigationGrid>()); }
    else
    {
        Index = 0;
        for (int32 I = 1; I < Cache.Num(); ++I) { if (Cache[I]->LastUsed < Cache[Index]->LastUsed) { Index = I; } }
        Cache[Index] = MakeUnique<FNavigationGrid>();
    }
    Cache[Index]->Build(Obstacles, Bounds); Cache[Index]->LastUsed = ++Serial;
    return *Cache[Index];
}

bool FindGridPath(const FVector& Start, const FVector& Goal, const TArray<LKNavigation::FObstacle>& Obstacles,
    const LKNavigation::FBounds& Bounds, TArray<FVector>& OutPath, LKNavigation::FSearchStats* Stats)
{
    FNavigationGrid& Grid = GridFor(Obstacles, Bounds, Stats);
    const int32 Count = Grid.Columns * Grid.Rows;
    if (Stats) { Stats->bUsedGrid = true; Stats->GridNodes = Count; }
    const auto Starts = Grid.Connections(Start, Stats), Goals = Grid.Connections(Goal, Stats);
    if (Starts.IsEmpty() || Goals.IsEmpty()) { return false; }
    TArray<float> Cost; Cost.Init(TNumericLimits<float>::Max(), Count);
    TArray<int32> Previous; Previous.Init(INDEX_NONE, Count);
    TArray<uint8> Closed, IsGoal; Closed.Init(0, Count); IsGoal.Init(0, Count);
    for (int32 I : Goals) { IsGoal[I] = 1; }
    struct FOpenNode { int32 Index; float Score; float Distance; };
    const auto Compare = [](const FOpenNode& A, const FOpenNode& B)
    { return A.Score != B.Score ? A.Score < B.Score : A.Index < B.Index; };
    TArray<FOpenNode> Open;
    for (int32 I : Starts)
    {
        Cost[I] = float(FVector::Dist2D(Start, Grid.Point(I)));
        Open.HeapPush(FOpenNode{I, Cost[I] + float(FVector::Dist2D(Grid.Point(I), Goal)), Cost[I]}, Compare);
    }
    int32 Visited = 0;
    while (!Open.IsEmpty() && Visited < LKNavigation::GridNodeLimit)
    {
        FOpenNode Current; Open.HeapPop(Current, Compare, EAllowShrinking::No);
        const int32 Index = Current.Index;
        if (Closed[Index] || Current.Distance > Cost[Index]) { continue; }
        Closed[Index] = 1; ++Visited; if (Stats) { Stats->VisitedNodes = Visited; }
        if (IsGoal[Index])
        {
            TArray<FVector> Raw;
            for (int32 I = Index; I != INDEX_NONE; I = Previous[I]) { Raw.Add(Grid.Point(I)); }
            Algo::Reverse(Raw); Raw.Add(Goal);
            // Bounded look-ahead preserves exact collision checks without quadratic smoothing work.
            FVector Anchor = Start; int32 First = 0, SmoothBudget = 512;
            while (First < Raw.Num())
            {
                int32 Last = First;
                for (int32 I = FMath::Min(First + 12, Raw.Num() - 1); I > First && SmoothBudget > 0; --I)
                { --SmoothBudget; if (Grid.SegmentClear(Anchor, Raw[I], Stats)) { Last = I; break; } }
                OutPath.Add(Raw[Last]); Anchor = Raw[Last]; First = Last + 1;
            }
            return true;
        }
        const int32 CX = Index % Grid.Columns, CY = Index / Grid.Columns;
        for (int32 D = 0; D < 8; ++D)
        {
            const int32 X = CX + Directions[D][0], Y = CY + Directions[D][1];
            if (X < 0 || X >= Grid.Columns || Y < 0 || Y >= Grid.Rows) { continue; }
            const int32 Next = Y * Grid.Columns + X;
            if (Closed[Next] || !Grid.Walkable[Next]) { continue; }
            const float NextCost = Cost[Index] + float(FVector::Dist2D(Grid.Point(Index), Grid.Point(Next)));
            if (NextCost >= Cost[Next] || !Grid.EdgeClear(Index, Next, D, Stats)) { continue; }
            Cost[Next] = NextCost; Previous[Next] = Index;
            Open.HeapPush(FOpenNode{Next, NextCost + float(FVector::Dist2D(Grid.Point(Next), Goal)), NextCost}, Compare);
        }
    }
    return false;
}
}

float LKNavigation::PointDistance(const FVector& Point, const FObstacle& Obstacle)
{
    const FVector Flat(Point.X, Point.Y, 0.f);
    if (Obstacle.IsBox())
    {
        const FVector Local = FRotator(0,-Obstacle.BoxYaw,0).RotateVector(Flat-Obstacle.Center);
        // Signed L-infinity distance gives an exact oriented rectangle with conservative square clearance.
        return FMath::Max(FMath::Abs(Local.X)-Obstacle.BoxHalfExtent.X,FMath::Abs(Local.Y)-Obstacle.BoxHalfExtent.Y);
    }
    return FVector::Dist2D(Flat, FMath::ClosestPointOnSegment(Flat,
        Obstacle.Center - Obstacle.HalfSegment, Obstacle.Center + Obstacle.HalfSegment));
}

bool LKNavigation::IsPointValid(const FVector& Point, const TArray<FObstacle>& Obstacles, const FBounds& Bounds)
{
	if (Point.ContainsNaN() || FMath::Abs(Point.X) > Bounds.HalfExtent.X || FMath::Abs(Point.Y) > Bounds.HalfExtent.Y) { return false; }
	if (Bounds.LeashRadius > 0.f && FVector::DistSquared2D(Point, Bounds.LeashCenter) > FMath::Square(Bounds.LeashRadius) + 0.1f) { return false; }
	for (const FObstacle& Obstacle : Obstacles)
	{
		if (PointDistance(Point, Obstacle) < Obstacle.Radius) { return false; }
	}
	return true;
}

bool LKNavigation::SegmentHitsObstacle(const FVector& A, const FVector& B, const FObstacle& Obstacle)
{
    if (!Obstacle.IsBox())
    {
        FVector PA,PB;
        FMath::SegmentDistToSegmentSafe(FVector(A.X,A.Y,0),FVector(B.X,B.Y,0),
            Obstacle.Center-Obstacle.HalfSegment,Obstacle.Center+Obstacle.HalfSegment,PA,PB);
        return FVector::DistSquared2D(PA,PB)<FMath::Square(Obstacle.Radius);
    }
    const FRotator Rotation(0,-Obstacle.BoxYaw,0);
    const FVector P=Rotation.RotateVector(A-Obstacle.Center), D=Rotation.RotateVector(B-A);
    const FVector2D Extent=Obstacle.BoxHalfExtent+FVector2D(Obstacle.Radius);
    double Enter=0.,Exit=1.;
    for (int32 Axis=0;Axis<2;++Axis)
    {
        const double Half=Extent[Axis]-.001;
        if (FMath::Abs(D[Axis])<1.e-8) { if (FMath::Abs(P[Axis])>=Half) { return false; } continue; }
        const double T0=(-Half-P[Axis])/D[Axis],T1=(Half-P[Axis])/D[Axis];
        Enter=FMath::Max(Enter,FMath::Min(T0,T1)); Exit=FMath::Min(Exit,FMath::Max(T0,T1));
        if (Enter>Exit) { return false; }
    }
    return Exit>=0. && Enter<=1.;
}

bool LKNavigation::IsSegmentClear(const FVector& A, const FVector& B, const TArray<FObstacle>& Obstacles)
{
	for (const FObstacle& Obstacle : Obstacles)
	{
        if (SegmentHitsObstacle(A,B,Obstacle)) { return false; }
	}
	return true;
}

bool LKNavigation::FindPath(const FVector& Start, const FVector& Goal, const TArray<FObstacle>& Obstacles,
	const FBounds& Bounds, TArray<FVector>& OutPath, FSearchStats* OutStats)
{
	OutPath.Reset();
    if (OutStats) { *OutStats = FSearchStats(); }
    if (Bounds.HalfExtent.ContainsNaN() || Bounds.HalfExtent.X <= 0.f || Bounds.HalfExtent.Y <= 0.f
        || Bounds.LeashCenter.ContainsNaN() || !FMath::IsFinite(Bounds.LeashRadius)) { return false; }
    for (const FObstacle& O : Obstacles)
    { if (O.Center.ContainsNaN() || O.HalfSegment.ContainsNaN() || !FMath::IsFinite(O.Radius) || O.Radius < 0.f
        || O.BoxHalfExtent.ContainsNaN() || O.BoxHalfExtent.X<0.f || O.BoxHalfExtent.Y<0.f || !FMath::IsFinite(O.BoxYaw)) { return false; } }
	if (!IsPointValid(Start, Obstacles, Bounds) || !IsPointValid(Goal, Obstacles, Bounds)) { return false; }
	if (IsSegmentClear(Start, Goal, Obstacles)) { OutPath.Add(Goal); return true; }
    if (Obstacles.Num() > VisibilityObstacleLimit) { return FindGridPath(Start, Goal, Obstacles, Bounds, OutPath, OutStats); }
	TArray<FVector> Nodes = { Start, Goal };
	constexpr int32 Sides = 16;
	for (const FObstacle& Obstacle : Obstacles)
	{
        if (Obstacle.IsBox())
        {
            const FVector2D Extent=Obstacle.BoxHalfExtent+FVector2D(Obstacle.Radius+2.f);
            for (int32 X : {-1,1}) { for (int32 Y : {-1,1})
            {
                const FVector Point=Obstacle.Center+FRotator(0,Obstacle.BoxYaw,0).RotateVector(FVector(X*Extent.X,Y*Extent.Y,0));
                if (IsPointValid(Point,Obstacles,Bounds)) { Nodes.Add(Point); }
            } }
            continue;
        }
		const float Radius = (Obstacle.Radius + 2.f) / FMath::Cos(PI / Sides);
		for (int32 i = 0; i < Sides; ++i)
		{
			const float Angle = i * 2.f * PI / Sides;
            const FVector Normal(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
            const FVector End = FVector::DotProduct(Normal, Obstacle.HalfSegment) >= 0.f ? Obstacle.HalfSegment : -Obstacle.HalfSegment;
			const FVector Point = Obstacle.Center + End + Normal * Radius;
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
