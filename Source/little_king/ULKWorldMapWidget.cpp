#include "ULKWorldMapWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Rendering/DrawElementTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "LKWorldMapContent.h"

TSharedRef<SWidget> ULKWorldMapWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UBorder *Empty = WidgetTree->ConstructWidget<UBorder>();
        Empty->SetBrushColor(FLinearColor::Transparent);
        Empty->SetVisibility(ESlateVisibility::HitTestInvisible);
        WidgetTree->RootWidget = Empty;
    }
    SetClipping(EWidgetClipping::ClipToBounds);
    // UUserWidget 默认 SelfHitTestInvisible，纯绘制画布必须主动参与鼠标命中。
    SetVisibility(ESlateVisibility::Visible);
    return Super::RebuildWidget();
}
void ULKWorldMapWidget::SetMapState(const FLKRunState &InState, FName InSelected)
{
    Snapshot = InState;
    Selected = InSelected;
    if (Snapshot.WorldRegions.IsEmpty())
    {
        FLKWorldRegion Region;
        Region.RegionId = TEXT("Legacy");
        Region.DisplayName = FText::FromString(TEXT("旧版路线"));
        Region.Color = FLinearColor(.08f, .14f, .19f);
        Region.Polygon = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        Snapshot.WorldRegions.Add(Region);
        for (FLKDungeonNode &Node : Snapshot.Nodes)
        {
            const FString Name = Node.NodeId.ToString();
            Node.Layer = Name.Contains(TEXT("Start")) ? 0
                         : Name.Contains(TEXT("R1"))  ? 1
                         : Name.Contains(TEXT("R2"))  ? 2
                         : Name.Contains(TEXT("R3"))  ? 3
                                                      : 4;
            Node.RegionId = Region.RegionId;
            Node.MapPosition = FVector2D(.1 + Node.Layer * .2, Name.EndsWith(TEXT("A"))   ? .35
                                                               : Name.EndsWith(TEXT("B")) ? .65
                                                                                          : .5);
        }
    }
    FocusRegion(Focus);
}
void ULKWorldMapWidget::FocusRegion(FName RegionId)
{
    Focus = RegionId;
    ViewMin = FVector2D::ZeroVector;
    ViewMax = FVector2D(1, 1);
    if (const FLKWorldRegion *Region = Snapshot.WorldRegions.FindByPredicate(
            [RegionId](const FLKWorldRegion &R) { return R.RegionId == RegionId; }))
    {
        ViewMin = FVector2D(1, 1);
        ViewMax = FVector2D::ZeroVector;
        for (FVector2D P : Region->Polygon)
        {
            ViewMin.X = FMath::Min(ViewMin.X, P.X);
            ViewMin.Y = FMath::Min(ViewMin.Y, P.Y);
            ViewMax.X = FMath::Max(ViewMax.X, P.X);
            ViewMax.Y = FMath::Max(ViewMax.Y, P.Y);
        }
        ViewMin -= FVector2D(.015, .015);
        ViewMax += FVector2D(.015, .015);
    }
}
FVector2D ULKWorldMapWidget::Project(FVector2D Point, FVector2D Size) const
{
    return FVector2D(24, 36) + (Point - ViewMin) / (ViewMax - ViewMin) * (Size - FVector2D(48, 60));
}
FName ULKWorldMapWidget::HitTestNode(FVector2D LocalPoint, FVector2D Size) const
{
    FName Best;
    double Distance = 24 * 24;
    for (const FLKDungeonNode &Node : Snapshot.Nodes)
    {
        if (!Focus.IsNone() && Node.RegionId != Focus)
        {
            continue;
        }
        const double D = (LocalPoint - Project(Node.MapPosition, Size)).SizeSquared();
        if (D < Distance)
        {
            Distance = D;
            Best = Node.NodeId;
        }
    }
    return Best;
}
FReply ULKWorldMapWidget::NativeOnMouseButtonDown(const FGeometry &Geometry, const FPointerEvent &Event)
{
    if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const FName Node =
            HitTestNode(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), Geometry.GetLocalSize());
        if (!Node.IsNone() && OnNodePicked.IsBound())
        {
            OnNodePicked.Execute(Node);
        }
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(Geometry, Event);
}
int32 ULKWorldMapWidget::NativePaint(const FPaintArgs &Args, const FGeometry &Geometry, const FSlateRect &CullingRect,
                                     FSlateWindowElementList &Elements, int32 Layer, const FWidgetStyle &Style,
                                     bool Enabled) const
{
    Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, Enabled);
    const FVector2D Size = Geometry.GetLocalSize();
    const FSlateBrush *Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Focus.IsNone() ? 8 : 13);
    for (const FLKWorldRegion &Region : Snapshot.WorldRegions)
    {
        if (!Focus.IsNone() && Focus != Region.RegionId)
        {
            continue;
        }
        TArray<FSlateVertex> Vertices;
        TArray<SlateIndex> Indices;
        TArray<FVector2D> Outline;
        for (FVector2D Point : Region.Polygon)
        {
            const FVector2D P = Project(Point, Size);
            Outline.Add(P);
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
                                                                            FVector2f(P), FVector2f::ZeroVector,
                                                                            Region.Color.ToFColor(true)));
        }
        for (int32 I = 1; I + 1 < Vertices.Num(); ++I)
        {
            Indices.Append({0, SlateIndex(I), SlateIndex(I + 1)});
        }
        FSlateDrawElement::MakeCustomVerts(Elements, Layer + 1, Resource, Vertices, Indices, nullptr, 0, 0);
        if (!Outline.IsEmpty())
        {
            const FVector2D First = Outline[0];
            Outline.Add(First);
            FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(), Outline,
                                         ESlateDrawEffect::None, FLinearColor(.35f, .43f, .48f), true, 2);
        }
    }
    const FLKDungeonNode *Current =
        Snapshot.Nodes.FindByPredicate([this](const FLKDungeonNode &N) { return N.NodeId == Snapshot.CurrentNodeId; });
    for (const FLKDungeonNode &Node : Snapshot.Nodes)
    {
        for (FName NextId : Node.NextNodeIds)
        {
            const FLKDungeonNode *Next =
                Snapshot.Nodes.FindByPredicate([NextId](const FLKDungeonNode &N) { return N.NodeId == NextId; });
            if (!Next || (!Focus.IsNone() && (Node.RegionId != Focus || Next->RegionId != Focus)))
            {
                continue;
            }
            const FVector2D A = Project(Node.MapPosition, Size), B = Project(Next->MapPosition, Size),
                            Dir = (B - A).GetSafeNormal();
            const bool Available = Node.NodeId == Snapshot.CurrentNodeId && Snapshot.Phase == ELKRunPhase::ChoosingNode;
            const FLinearColor Color =
                Available ? FLinearColor(.98f, .73f, .28f) : FLinearColor(.40f, .50f, .56f, .38f);
            FSlateDrawElement::MakeLines(Elements, Layer + 3, Geometry.ToPaintGeometry(),
                                         TArray<FVector2D>{A + Dir * 12, B - Dir * 12}, ESlateDrawEffect::None, Color,
                                         true, Available ? 2.5f : 1.f);
            const FVector2D Tip = A + (B - A) * .66, Side(-Dir.Y, Dir.X);
            FSlateDrawElement::MakeLines(Elements, Layer + 3, Geometry.ToPaintGeometry(),
                                         TArray<FVector2D>{Tip - Dir * 6 + Side * 3, Tip, Tip - Dir * 6 - Side * 3},
                                         ESlateDrawEffect::None, Color, true, 1.2f);
        }
    }
    for (const FLKDungeonNode &Node : Snapshot.Nodes)
    {
        if (!Focus.IsNone() && Node.RegionId != Focus)
        {
            continue;
        }
        const bool Available = Current && Snapshot.Phase == ELKRunPhase::ChoosingNode &&
                               Current->NextNodeIds.Contains(Node.NodeId) && !Node.bResolved;
        const bool IsCurrent = Node.NodeId == Snapshot.CurrentNodeId;
        const FVector2D P = Project(Node.MapPosition, Size);
        const double Width = Focus.IsNone() ? 13 : 24;
        const FLinearColor Border = Node.NodeId == Selected ? FLinearColor::White
                                    : Available             ? FLinearColor(1, .7f, .15f)
                                    : IsCurrent             ? FLinearColor(.3f, 1, .7f)
                                                            : FLinearColor(.32f, .40f, .46f);
        const FLinearColor Fill = Node.bResolved                            ? FLinearColor(.12f, .27f, .22f)
                                  : Node.Type == ELKDungeonNodeType::Boss   ? FLinearColor(.4f, .1f, .13f)
                                  : Node.Type == ELKDungeonNodeType::Market ? FLinearColor(.31f, .23f, .10f)
                                  : Node.Type == ELKDungeonNodeType::Rest   ? FLinearColor(.1f, .28f, .25f)
                                  : Node.Type == ELKDungeonNodeType::Elite  ? FLinearColor(.28f, .17f, .30f)
                                                                            : FLinearColor(.13f, .21f, .28f);
        FSlateDrawElement::MakeBox(
            Elements, Layer + 4,
            Geometry.ToPaintGeometry(FVector2D(Width + 4, Width + 4),
                                     FSlateLayoutTransform(P - FVector2D(Width / 2 + 2, Width / 2 + 2))),
            Brush, ESlateDrawEffect::None, Border);
        FSlateDrawElement::MakeBox(Elements, Layer + 5,
                                   Geometry.ToPaintGeometry(FVector2D(Width, Width),
                                                            FSlateLayoutTransform(P - FVector2D(Width / 2, Width / 2))),
                                   Brush, ESlateDrawEffect::None, Fill);
        FString Glyph = Node.Type == ELKDungeonNodeType::Boss     ? TEXT("王")
                        : Node.Type == ELKDungeonNodeType::Elite  ? TEXT("精")
                        : Node.Type == ELKDungeonNodeType::Rest   ? TEXT("休")
                        : Node.Type == ELKDungeonNodeType::Market ? TEXT("商")
                        : Node.Type == ELKDungeonNodeType::Event  ? TEXT("起")
                                                                  : TEXT("普");
        FSlateDrawElement::MakeText(
            Elements, Layer + 6,
            Geometry.ToPaintGeometry(FVector2D(Width, Width),
                                     FSlateLayoutTransform(P - (Focus.IsNone() ? FVector2D(5, 6) : FVector2D(8, 10)))),
            Glyph, Font, ESlateDrawEffect::None, FLinearColor::White);
    }
    return Layer + 6;
}
