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
#include "LKWorldArt.h"
#include "LKPresentationStyle.h"
#include "Engine/Texture2D.h"

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
    for (const FLKWorldRegion& Region : Snapshot.WorldRegions)
    { RegionTextures.FindOrAdd(Region.RegionId) = LKWorldArt::GroundTexture(Region.RegionId); }
    for (ELKDungeonNodeType Type : {ELKDungeonNodeType::Battle, ELKDungeonNodeType::Elite, ELKDungeonNodeType::Boss, ELKDungeonNodeType::Market, ELKDungeonNodeType::Rest})
    { NodeTextures.FindOrAdd(uint8(Type)) = LKWorldArt::NodeIcon(Type); }
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
    const FSlateFontInfo Font = LKPresentationStyle::Font(Focus.IsNone() ? 8 : 13);
    for (const FLKWorldRegion &Region : Snapshot.WorldRegions)
    {
        if (!Focus.IsNone() && Focus != Region.RegionId)
        {
            continue;
        }
        TArray<FSlateVertex> Vertices;
        TArray<SlateIndex> Indices;
        TArray<FVector2D> Outline;
        FVector2D Min(1,1), Max(0,0);
        for (FVector2D P : Region.Polygon) { Min.X=FMath::Min(Min.X,P.X); Min.Y=FMath::Min(Min.Y,P.Y); Max.X=FMath::Max(Max.X,P.X); Max.Y=FMath::Max(Max.Y,P.Y); }
        FSlateBrush RegionBrush = *Brush;
        const FVector2D PixelExtent=(Max-Min)/(ViewMax-ViewMin)*(Size-FVector2D(48,60));
        const FVector2D UVScale=PixelExtent/FMath::Max(PixelExtent.X,PixelExtent.Y);
        const TObjectPtr<UTexture2D>* Texture = RegionTextures.Find(Region.RegionId);
        if (Texture && *Texture) { RegionBrush.SetResourceObject(*Texture); }
        const FSlateResourceHandle RegionResource = Texture && *Texture ? FSlateApplication::Get().GetRenderer()->GetResourceHandle(RegionBrush) : Resource;
        for (FVector2D Point : Region.Polygon)
        {
            const FVector2D P = Project(Point, Size);
            Outline.Add(P);
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
                                                                            FVector2f(P), FVector2f(((Point-Min)/(Max-Min)-FVector2D(.5,.5))*UVScale+FVector2D(.5,.5)),
                                                                            (Texture && *Texture ? FLinearColor(.32f,.36f,.30f) : Region.Color).ToFColor(true)));
        }
        for (int32 I = 1; I + 1 < Vertices.Num(); ++I)
        {
            Indices.Append({0, SlateIndex(I), SlateIndex(I + 1)});
        }
        FSlateDrawElement::MakeCustomVerts(Elements, Layer + 1, RegionResource, Vertices, Indices, nullptr, 0, 0);
        if (!Outline.IsEmpty())
        {
            const FVector2D First = Outline[0];
            Outline.Add(First);
            FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(), Outline,
                                         ESlateDrawEffect::None, LKPresentationStyle::Gold() * .75f, true, 2);
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
                Available ? FLinearColor(1.f, .78f, .28f) : FLinearColor(.78f, .76f, .61f, .65f);
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
        const double Width = Focus.IsNone() ? 18 : 32;
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
        const TObjectPtr<UTexture2D>* Icon = NodeTextures.Find(uint8(Node.Type));
        if (Icon && *Icon)
        {
            FSlateBrush IconBrush;
            IconBrush.SetResourceObject(*Icon);
            IconBrush.ImageSize = FVector2D(Width, Width);
            FSlateDrawElement::MakeBox(Elements, Layer + 6,
                Geometry.ToPaintGeometry(FVector2D(Width,Width),FSlateLayoutTransform(P-FVector2D(Width*.5))),
                &IconBrush, ESlateDrawEffect::None, Node.bResolved ? FLinearColor(.70f,.75f,.66f) : FLinearColor::White);
        }
        else { FSlateDrawElement::MakeText(
            Elements, Layer + 6,
            Geometry.ToPaintGeometry(FVector2D(Width, Width),
                                     FSlateLayoutTransform(P - (Focus.IsNone() ? FVector2D(5, 6) : FVector2D(8, 10)))),
            Glyph, Font, ESlateDrawEffect::None, FLinearColor::White); }
        if (!Focus.IsNone() && (Node.Layer == 0 || Node.Layer == 4))
        {
            const FString Label = Node.Type == ELKDungeonNodeType::Event ? TEXT("起点") : Node.Layer == 0 ? TEXT("入口") : TEXT("出口");
            FSlateDrawElement::MakeText(Elements, Layer + 7, Geometry.ToPaintGeometry(FVector2D(40,16),FSlateLayoutTransform(P+FVector2D(22,-6))),
                Label, LKPresentationStyle::Font(10), ESlateDrawEffect::None, LKPresentationStyle::Paper());
        }
    }
    int32 LegendIndex = 0;
    for (ELKDungeonNodeType Type : {ELKDungeonNodeType::Battle, ELKDungeonNodeType::Elite, ELKDungeonNodeType::Boss, ELKDungeonNodeType::Market, ELKDungeonNodeType::Rest})
    {
        const TObjectPtr<UTexture2D>* Icon = NodeTextures.Find(uint8(Type));
        if (!Icon || !*Icon) { continue; }
        FSlateBrush IconBrush; IconBrush.SetResourceObject(*Icon);
        const FVector2D P(16 + LegendIndex++ * (Size.X-32) / 5.f, 4);
        FSlateDrawElement::MakeBox(Elements, Layer + 8, Geometry.ToPaintGeometry(FVector2D(22,22),FSlateLayoutTransform(P)),&IconBrush);
        FSlateDrawElement::MakeText(Elements,Layer+8,Geometry.ToPaintGeometry(FVector2D(80,20),FSlateLayoutTransform(P+FVector2D(26,3))),
            LKWorldMapContent::NodeTitle(Type), LKPresentationStyle::Font(10),ESlateDrawEffect::None,LKPresentationStyle::Paper());
    }
    return Layer + 8;
}
