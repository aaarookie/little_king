#include "LKWorldMapContent.h"
#include "LKEncounterContent.h"

namespace
{
// 一次选定的地理种子：所有玩家/所有远征共享此版图，远征种子只改变内部网络。
constexpr int32 GeographySeed = 9152026;
FName RegionId(int32 Index)
{
    return FName(*FString::Printf(TEXT("World_Region%d"), Index));
}
FName NodeId(int32 Region, int32 Layer, int32 Index)
{
    return FName(*FString::Printf(TEXT("World_R%d_L%d_N%d"), Region, Layer, Index));
}
TArray<FVector2D> Clip(const TArray<FVector2D> &Polygon, FVector2D Normal, double Distance)
{
    TArray<FVector2D> Result;
    for (int32 Index = 0; Index < Polygon.Num(); ++Index)
    {
        const FVector2D A = Polygon[Index], B = Polygon[(Index + 1) % Polygon.Num()];
        const double DA = FVector2D::DotProduct(A, Normal) - Distance;
        const double DB = FVector2D::DotProduct(B, Normal) - Distance;
        if (DA <= 0)
        {
            Result.Add(A);
        }
        if ((DA <= 0) != (DB <= 0))
        {
            Result.Add(A + (B - A) * (DA / (DA - DB)));
        }
    }
    return Result;
}
FVector2D Position(const FLKWorldRegion &Region, int32 Layer, int32 Index, int32 Count, FRandomStream *Jitter)
{
    double MinX = 1, MaxX = 0;
    for (FVector2D P : Region.Polygon)
    {
        MinX = FMath::Min(MinX, P.X);
        MaxX = FMath::Max(MaxX, P.X);
    }
    // 避开多边形尖角；窄端挤满节点会使放大视图也互相遮挡。
    const double X = FMath::Lerp(MinX, MaxX, 0.14 + Layer * 0.145);
    double MinY = 1, MaxY = 0;
    for (int32 Edge = 0; Edge < Region.Polygon.Num(); ++Edge)
    {
        FVector2D A = Region.Polygon[Edge], B = Region.Polygon[(Edge + 1) % Region.Polygon.Num()];
        if (FMath::Abs(B.X - A.X) < 1.e-8 || X < FMath::Min(A.X, B.X) || X > FMath::Max(A.X, B.X))
        {
            continue;
        }
        const double Y = FMath::Lerp(A.Y, B.Y, (X - A.X) / (B.X - A.X));
        MinY = FMath::Min(MinY, Y);
        MaxY = FMath::Max(MaxY, Y);
    }
    const double Fraction = (Index + 1.) / (Count + 1.) + (Jitter ? Jitter->FRandRange(-0.035f, 0.035f) : 0.f);
    return FVector2D(X, FMath::Lerp(MinY + 0.025, MaxY - 0.025, Fraction));
}
} // namespace

const TArray<FLKWorldRegion> &LKWorldMapContent::Regions()
{
    static const TArray<FLKWorldRegion> Layout = [] {
        FRandomStream Stream(GeographySeed);
        TArray<FVector2D> Centers = {{.10, .50}, {.33, .20}, {.33, .80}, {.61, .50}, {.89, .50}};
        for (FVector2D &Center : Centers)
        {
            Center += FVector2D(Stream.FRandRange(-.025f, .025f), Stream.FRandRange(-.035f, .035f));
        }
        const TArray<FString> Names = {TEXT("西境原野"), TEXT("霜落高地"), TEXT("暮色湿地"), TEXT("断壁关隘"),
                                       TEXT("白骨王庭")};
        const TArray<FLinearColor> Colors = {
            {.09f, .19f, .16f}, {.09f, .16f, .24f}, {.17f, .12f, .23f}, {.25f, .18f, .10f}, {.24f, .095f, .10f}};
        TArray<FLKWorldRegion> Result;
        for (int32 Index = 0; Index < 5; ++Index)
        {
            FLKWorldRegion Region;
            Region.RegionId = RegionId(Index);
            Region.DisplayName = FText::FromString(Names[Index]);
            Region.Color = Colors[Index];
            Region.Polygon = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (int32 Other = 0; Other < 5; ++Other)
            {
                if (Other != Index)
                {
                    Region.Polygon = Clip(Region.Polygon, Centers[Other] - Centers[Index],
                                          (Centers[Other].SizeSquared() - Centers[Index].SizeSquared()) * .5);
                }
            }
            if (Index == 0)
            {
                Region.NextRegionIds = {RegionId(1), RegionId(2)};
            }
            else if (Index == 1 || Index == 2)
            {
                Region.NextRegionIds = {RegionId(3)};
            }
            else if (Index == 3)
            {
                Region.NextRegionIds = {RegionId(4)};
            }
            for (int32 N = 0; N < (Index == 0 ? 1 : 2); ++N)
            {
                Region.EntryNodeIds.Add(NodeId(Index, 0, N));
            }
            for (int32 N = 0; N < (Index == 4 ? 1 : 2); ++N)
            {
                Region.ExitNodeIds.Add(NodeId(Index, 4, N));
            }
            Result.Add(Region);
        }
        return Result;
    }();
    return Layout;
}
FName LKWorldMapContent::StartNodeId()
{
    return NodeId(0, 0, 0);
}
bool LKWorldMapContent::IsCombat(ELKDungeonNodeType Type)
{
    return Type == ELKDungeonNodeType::Battle || Type == ELKDungeonNodeType::Elite || Type == ELKDungeonNodeType::Boss;
}
FText LKWorldMapContent::NodeTitle(ELKDungeonNodeType Type)
{
    switch (Type)
    {
    case ELKDungeonNodeType::Battle:
        return FText::FromString(TEXT("普通遭遇"));
    case ELKDungeonNodeType::Elite:
        return FText::FromString(TEXT("精英战"));
    case ELKDungeonNodeType::Boss:
        return FText::FromString(TEXT("Boss 战"));
    case ELKDungeonNodeType::Market:
        return FText::FromString(TEXT("市场"));
    case ELKDungeonNodeType::Rest:
        return FText::FromString(TEXT("休息"));
    default:
        return FText::FromString(TEXT("起点 / 事件"));
    }
}
FText LKWorldMapContent::NodeDescription(ELKDungeonNodeType Type)
{
    switch (Type)
    {
    case ELKDungeonNodeType::Battle:
        return FText::FromString(TEXT("1 名敌方英雄及佣兵，胜利获得 10 金币。"));
    case ELKDungeonNodeType::Elite:
        return FText::FromString(TEXT("2 名敌方英雄及佣兵，胜利获得 20 金币。"));
    case ELKDungeonNodeType::Boss:
        return FText::FromString(TEXT("1 名 Boss、2 名英雄及佣兵，胜利获得 40 金币。"));
    case ELKDungeonNodeType::Market:
        return FText::FromString(TEXT("使用远征金币交易的场所。商品后续加入，目前可查看钱包并离开。"));
    case ELKDungeonNodeType::Rest:
        return FText::FromString(TEXT("全体英雄恢复 30% 最大生命。其他营地功能后续加入。"));
    default:
        return FText::FromString(TEXT("远征固定起点，选择沿箭头相连的下一站。"));
    }
}
bool LKWorldMapContent::Generate(FLKRunState &State, FString &OutError)
{
    const FLKEncounterRow *Normal = LKEncounterContent::Find(State.Encounters, TEXT("Encounter_UndeadPatrol"));
    const FLKEncounterRow *Elite = LKEncounterContent::Find(State.Encounters, TEXT("Encounter_UndeadElite"));
    const FLKEncounterRow *Boss = LKEncounterContent::Find(State.Encounters, TEXT("Encounter_SkeletonKing"));
    if (!Normal || !Elite || !Boss)
    {
        OutError = TEXT("缺少战斗遭遇模板");
        return false;
    }
    // 必须复制模板，追加动态遭遇会使原 TArray 指针失效。
    const TArray<FLKEncounterRow> Templates = {*Normal, *Elite, *Boss};
    TArray<FName> Heroes, Bosses;
    LKEncounterContent::CollectRosterPools(State.Encounters, Heroes, Bosses);
    State.WorldMapVersion = LayoutVersion;
    State.WorldRegions = Regions();
    State.Nodes.Reset();
    FRandomStream Stream(State.Seed);
    for (int32 R = 0; R < State.WorldRegions.Num(); ++R)
    {
        const FLKWorldRegion &Region = State.WorldRegions[R];
        const int32 Target = Stream.RandRange(10, 20);
        TArray<int32> Counts = {Region.EntryNodeIds.Num(), 2, 2, 2, Region.ExitNodeIds.Num()};
        int32 Count = Counts[0] + 6 + Counts[4];
        while (Count < Target)
        {
            const int32 Layer = Stream.RandRange(1, 3);
            if (Counts[Layer] < (Layer == 3 ? 4 : 7))
            {
                ++Counts[Layer];
                ++Count;
            }
        }
        TArray<TArray<int32>> Layers;
        for (int32 Layer = 0; Layer < 5; ++Layer)
        {
            TArray<int32> Indices;
            for (int32 N = 0; N < Counts[Layer]; ++N)
            {
                FLKDungeonNode Node;
                Node.NodeId = NodeId(R, Layer, N);
                Node.RegionId = Region.RegionId;
                Node.Layer = Layer;
                Node.MapPosition =
                    Position(Region, Layer, N, Counts[Layer], Layer == 0 || Layer == 4 ? nullptr : &Stream);
                const int32 Roll = Stream.RandRange(0, 99);
                Node.Type = Roll < 48   ? ELKDungeonNodeType::Battle
                            : Roll < 67 ? ELKDungeonNodeType::Elite
                            : Roll < 83 ? ELKDungeonNodeType::Rest
                                        : ELKDungeonNodeType::Market;
                // 每区域都具备市场和休息；入口/出口固定，内部类型与连线仍由远征种子决定。
                if (Layer == 1 && N == 0)
                {
                    Node.Type = ELKDungeonNodeType::Market;
                }
                if (Layer == 1 && N == 1)
                {
                    Node.Type = ELKDungeonNodeType::Battle;
                }
                if (Layer == 2 && N == 0)
                {
                    Node.Type = ELKDungeonNodeType::Rest;
                }
                if (Layer == 0)
                {
                    Node.Type = ELKDungeonNodeType::Battle;
                }
                if (Layer == 4)
                {
                    Node.Type = R == 4 ? ELKDungeonNodeType::Boss : ELKDungeonNodeType::Elite;
                }
                if (Node.NodeId == StartNodeId())
                {
                    Node.Type = ELKDungeonNodeType::Event;
                    Node.bResolved = true;
                }
                if (IsCombat(Node.Type))
                {
                    const int32 Tier = Node.Type == ELKDungeonNodeType::Boss    ? 2
                                       : Node.Type == ELKDungeonNodeType::Elite ? 1
                                                                                : 0;
                    Node.EncounterId = FName(*(TEXT("Encounter_") + Node.NodeId.ToString()));
                    FLKEncounterRow Dynamic;
                    if (!LKEncounterContent::MakeDynamicEncounter(Node.EncounterId, Templates[Tier],
                                                                  ELKEncounterRank(Tier), Heroes, Bosses, Stream,
                                                                  Dynamic, OutError))
                    {
                        return false;
                    }
                    State.Encounters.Add(Dynamic);
                }
                Indices.Add(State.Nodes.Add(Node));
            }
            Layers.Add(Indices);
        }
        for (int32 Layer = 0; Layer < 4; ++Layer)
        {
            const TArray<int32> &From = Layers[Layer];
            const TArray<int32> &To = Layers[Layer + 1];
            // 覆盖每个出点和入点，杜绝孤岛；其余边随机，所有边只向下一层。
            for (int32 I = 0; I < From.Num(); ++I)
            {
                State.Nodes[From[I]].NextNodeIds.AddUnique(
                    State.Nodes[To[FMath::Min(To.Num() - 1, I * To.Num() / From.Num())]].NodeId);
            }
            for (int32 J = 0; J < To.Num(); ++J)
            {
                State.Nodes[From[FMath::Min(From.Num() - 1, J * From.Num() / To.Num())]].NextNodeIds.AddUnique(
                    State.Nodes[To[J]].NodeId);
            }
            for (int32 I : From)
            {
                if (Stream.FRand() < .65f)
                {
                    State.Nodes[I].NextNodeIds.AddUnique(State.Nodes[To[Stream.RandRange(0, To.Num() - 1)]].NodeId);
                }
            }
        }
    }
    for (const FLKWorldRegion &Region : State.WorldRegions)
    {
        for (FName Exit : Region.ExitNodeIds)
        {
            FLKDungeonNode *Node =
                State.Nodes.FindByPredicate([Exit](const FLKDungeonNode &N) { return N.NodeId == Exit; });
            for (FName NextRegion : Region.NextRegionIds)
            {
                const FLKWorldRegion *Target = State.WorldRegions.FindByPredicate(
                    [NextRegion](const FLKWorldRegion &R) { return R.RegionId == NextRegion; });
                if (Node && Target)
                {
                    for (FName Entry : Target->EntryNodeIds)
                    {
                        Node->NextNodeIds.AddUnique(Entry);
                    }
                }
            }
        }
    }
    State.CurrentNodeId = StartNodeId();
    State.RegionId = State.WorldRegions[0].RegionId;
    State.VisitedNodeIds = {State.CurrentNodeId};
    State.Phase = ELKRunPhase::ChoosingNode;
    return Validate(State, OutError);
}
bool LKWorldMapContent::Contains(const FLKWorldRegion &Region, FVector2D Point)
{
    bool Inside = false;
    for (int32 I = 0, J = Region.Polygon.Num() - 1; I < Region.Polygon.Num(); J = I++)
    {
        const FVector2D A = Region.Polygon[I], B = Region.Polygon[J];
        if (((A.Y > Point.Y) != (B.Y > Point.Y)) && Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
        {
            Inside = !Inside;
        }
    }
    return Inside;
}
bool LKWorldMapContent::Validate(const FLKRunState &State, FString &OutError)
{
    if (State.WorldMapVersion != LayoutVersion || State.WorldRegions.Num() != 5)
    {
        OutError = TEXT("世界版图版本或区域数量无效");
        return false;
    }
    TMap<FName, int32> Index;
    TMap<FName, int32> RegionOrder;
    for (int32 I = 0; I < State.WorldRegions.Num(); ++I)
    {
        const FLKWorldRegion &Region = State.WorldRegions[I];
        if (Region.RegionId.IsNone() || RegionOrder.Contains(Region.RegionId) || Region.Polygon.Num() < 3 ||
            Region.EntryNodeIds.IsEmpty() || Region.ExitNodeIds.IsEmpty())
        {
            OutError = TEXT("区域边界或入口出口无效");
            return false;
        }
        RegionOrder.Add(Region.RegionId, I);
    }
    for (int32 I = 0; I < State.Nodes.Num(); ++I)
    {
        const FLKDungeonNode &Node = State.Nodes[I];
        if (Node.Layer < 0 || Node.Layer > 4 ||
            (!IsCombat(Node.Type) && Node.Type != ELKDungeonNodeType::Market && Node.Type != ELKDungeonNodeType::Rest &&
             !(Node.Type == ELKDungeonNodeType::Event && Node.NodeId == StartNodeId())))
        {
            OutError = TEXT("不支持的节点类型或层数");
            return false;
        }
        if (Node.NodeId.IsNone() || Index.Contains(Node.NodeId) || !RegionOrder.Contains(Node.RegionId) ||
            !FMath::IsFinite(Node.MapPosition.X) || !FMath::IsFinite(Node.MapPosition.Y) ||
            !Contains(State.WorldRegions[RegionOrder[Node.RegionId]], Node.MapPosition))
        {
            OutError = TEXT("节点 ID、区域或坐标无效");
            return false;
        }
        if (IsCombat(Node.Type) && !LKEncounterContent::Find(State.Encounters, Node.EncounterId))
        {
            OutError = TEXT("战斗节点没有遭遇");
            return false;
        }
        Index.Add(Node.NodeId, I);
    }
    if (!Index.Contains(StartNodeId()))
    {
        OutError = TEXT("固定起点缺失");
        return false;
    }
    for (const FLKWorldRegion &Region : State.WorldRegions)
    {
        int32 Count = 0;
        for (const FLKDungeonNode &Node : State.Nodes)
        {
            Count += Node.RegionId == Region.RegionId ? 1 : 0;
        }
        if (Count < 10 || Count > 20)
        {
            OutError = TEXT("区域节点数量超出 10～20");
            return false;
        }
        for (FName Id : Region.EntryNodeIds)
        {
            if (!Index.Contains(Id) || State.Nodes[Index[Id]].RegionId != Region.RegionId ||
                State.Nodes[Index[Id]].Layer != 0)
            {
                OutError = TEXT("区域入口无效");
                return false;
            }
        }
        for (FName Id : Region.ExitNodeIds)
        {
            if (!Index.Contains(Id) || State.Nodes[Index[Id]].RegionId != Region.RegionId ||
                State.Nodes[Index[Id]].Layer != 4)
            {
                OutError = TEXT("区域出口无效");
                return false;
            }
        }
    }
    for (const FLKDungeonNode &Node : State.Nodes)
    {
        TSet<FName> Unique;
        for (FName NextId : Node.NextNodeIds)
        {
            if (!Index.Contains(NextId) || Unique.Contains(NextId))
            {
                OutError = TEXT("路线包含悬空或重复边");
                return false;
            }
            Unique.Add(NextId);
            const FLKDungeonNode &Next = State.Nodes[Index[NextId]];
            const FLKWorldRegion &Region = State.WorldRegions[RegionOrder[Node.RegionId]];
            if (Node.RegionId == Next.RegionId)
            {
                if (Next.Layer != Node.Layer + 1)
                {
                    OutError = TEXT("区域内路线未向下一层前进");
                    return false;
                }
            }
            else if (RegionOrder[Next.RegionId] <= RegionOrder[Node.RegionId] ||
                     !Region.NextRegionIds.Contains(Next.RegionId) || !Region.ExitNodeIds.Contains(Node.NodeId) ||
                     !State.WorldRegions[RegionOrder[Next.RegionId]].EntryNodeIds.Contains(Next.NodeId))
            {
                OutError = TEXT("跨区域连接不合法");
                return false;
            }
        }
        if (Node.NextNodeIds.IsEmpty() &&
            (Node.RegionId != State.WorldRegions.Last().RegionId || Node.Type != ELKDungeonNodeType::Boss))
        {
            OutError = TEXT("非最终首领形成死路");
            return false;
        }
    }
    TSet<FName> Reached;
    TArray<FName> Queue = {StartNodeId()};
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        if (Reached.Contains(Queue[I]))
        {
            continue;
        }
        Reached.Add(Queue[I]);
        Queue.Append(State.Nodes[Index[Queue[I]]].NextNodeIds);
    }
    if (Reached.Num() != State.Nodes.Num())
    {
        OutError = TEXT("存在从固定起点不可达的节点");
        return false;
    }
    return true;
}
