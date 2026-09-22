#include "ULKRunNodeSelectWidget.h"
#include "ULKJourneyPresentationSubsystem.h"
#include "LKPresentationStyle.h"
#include "ULKPresentationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "LKWorldArt.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "ALKBattleGameMode.h"
#include "ULKBattleHUDWidget.h"
#include "ULKRunSubsystem.h"
#include "ULKWorldMapWidget.h"
#include "ULKHomeListButtonWidget.h"
#include "LKWorldMapContent.h"
#include "LKUnitContent.h"
#include "LKEncounterContent.h"

namespace
{
UTextBlock *MakeText(UWidgetTree *Tree, const FString &Copy, int32 Size, FName Name = NAME_None)
{
    UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = Size;
    Text->SetFont(LKPresentationStyle::Font(Font.Size));
    Text->SetText(FText::FromString(Copy));
    Text->SetAutoWrapText(true);
    Text->SetColorAndOpacity(LKPresentationStyle::Paper());
    return Text;
}
} // namespace
void ULKRunNodeSelectWidget::InitializeNodeSelect(ULKBattleHUDWidget *InOwnerHUD)
{
    OwnerHUD = InOwnerHUD;
}
void ULKRunNodeSelectWidget::InitializeRunView(ULKRunSubsystem *InRun)
{
    ExplicitRun = InRun;
}
ULKRunSubsystem *ULKRunNodeSelectWidget::GetRun() const
{
    if (ExplicitRun)
    {
        return ExplicitRun;
    }
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<ULKRunSubsystem>() : nullptr;
}
TSharedRef<SWidget> ULKRunNodeSelectWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildNativeTree();
    }
    return Super::RebuildWidget();
}
void ULKRunNodeSelectWidget::NativeConstruct()
{
    Super::NativeConstruct(); ULKJourneyPresentationSubsystem::Reveal(this);
    RefreshNodeSelect();
}
void ULKRunNodeSelectWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    FeedbackRemaining = FMath::Max(0.f, FeedbackRemaining-DeltaTime);
    if (Status) { Status->SetColorAndOpacity(FeedbackRemaining > 0.f ? LKPresentationStyle::Gold() : LKPresentationStyle::Paper()); }
    if (NodeIllustration)
    {
        const float Pulse = FeedbackRemaining > 0.f ? 1.f+.06f*FMath::Sin(FeedbackRemaining*PI*3.f) : 1.f;
        NodeIllustration->SetRenderScale(FVector2D(Pulse,Pulse));
    }
}
ULKHomeListButtonWidget *ULKRunNodeSelectWidget::AddAction(UPanelWidget *Parent, const FString &Name,
                                                           const FString &Label, int32 Index, bool bEnabled)
{
    ULKHomeListButtonWidget *Entry =
        CreateWidget<ULKHomeListButtonWidget>(this, ULKHomeListButtonWidget::StaticClass(), FName(*Name));
    FLKHomePanelAction Action;
    Action.Label = FText::FromString(Label);
    Action.bEnabled = bEnabled;
    Action.ActionId = TEXT("Start");
    Entry->SetupAction(Index, Action);
    Entry->OnHomeListClicked.BindUObject(this, &ULKRunNodeSelectWidget::HandleAction);
    if (UVerticalBox *Column = Cast<UVerticalBox>(Parent))
    {
        Column->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0, 3));
    }
    else if (UHorizontalBox *Row = Cast<UHorizontalBox>(Parent))
    {
        UHorizontalBoxSlot *Layout = Row->AddChildToHorizontalBox(Entry);
        Layout->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Layout->SetPadding(FMargin(3, 0));
    }
    return Entry;
}
void ULKRunNodeSelectWidget::BuildNativeTree()
{
    UBorder *Back = WidgetTree->ConstructWidget<UBorder>();
    Back->SetBrushColor(LKPresentationStyle::Ink());
    Back->SetPadding(FMargin(22));
    WidgetTree->RootWidget = Back;
    UScaleBox *Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Back->SetContent(Scale);
    USizeBox *Frame = WidgetTree->ConstructWidget<USizeBox>();
    Frame->SetWidthOverride(1320);
    Frame->SetHeightOverride(810);
    Scale->SetContent(Frame);
    UVerticalBox *Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Frame->SetContent(Layout);
    Layout->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("远征 · 五境之路"), 30));
    Summary = MakeText(WidgetTree, TEXT(""), 17, TEXT("WorldMapSummary"));
    Layout->AddChildToVerticalBox(Summary)->SetPadding(FMargin(0, 6, 0, 12));
    RegionTabs = WidgetTree->ConstructWidget<UHorizontalBox>();
    Layout->AddChildToVerticalBox(RegionTabs)->SetPadding(FMargin(0, 0, 0, 12));
    UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>();
    Layout->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox *MapColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    Body->AddChildToHorizontalBox(MapColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Map = CreateWidget<ULKWorldMapWidget>(this, ULKWorldMapWidget::StaticClass(), TEXT("WorldMapCanvas"));
    Map->OnNodePicked.BindUObject(this, &ULKRunNodeSelectWidget::SelectMapNode);
    MapColumn->AddChildToVerticalBox(Map)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    MapColumn
        ->AddChildToVerticalBox(MakeText(WidgetTree,
                                         TEXT("沿箭头前进 · 区域入口、出口固定\n金边：可前往   "
                                              "绿边：当前位置   白边：已选中 · 点击区域按钮可放大查看"),
                                         15))
        ->SetPadding(FMargin(0, 10));
    USizeBox *SidebarSize = WidgetTree->ConstructWidget<USizeBox>();
    SidebarSize->SetWidthOverride(315);
    Body->AddChildToHorizontalBox(SidebarSize)->SetPadding(FMargin(18, 0, 0, 0));
    UVerticalBox *Sidebar = WidgetTree->ConstructWidget<UVerticalBox>();
    SidebarSize->SetContent(Sidebar);
    ChoicesTitle = MakeText(WidgetTree, TEXT("下一站"), 23);
    Sidebar->AddChildToVerticalBox(ChoicesTitle)->SetPadding(FMargin(0, 0, 0, 8));
    ChoicesFrame = WidgetTree->ConstructWidget<USizeBox>();
    ChoicesFrame->SetHeightOverride(205);
    Sidebar->AddChildToVerticalBox(ChoicesFrame);
    UScrollBox *ChoicesScroll = WidgetTree->ConstructWidget<UScrollBox>();
    ChoicesFrame->SetContent(ChoicesScroll);
    Choices = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NextNodeChoices"));
    ChoicesScroll->AddChild(Choices);
    UScrollBox *DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
    Sidebar->AddChildToVerticalBox(DetailScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    USizeBox* BadgeSize = WidgetTree->ConstructWidget<USizeBox>();
    BadgeSize->SetWidthOverride(96); BadgeSize->SetHeightOverride(96);
    NodeIllustration = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("NodeIllustration"));
    NodeIllustration->SetVisibility(ESlateVisibility::HitTestInvisible);
    UScaleBox* BadgeScale = WidgetTree->ConstructWidget<UScaleBox>();
    BadgeScale->SetStretch(EStretch::ScaleToFit);
    BadgeScale->SetContent(NodeIllustration);
    BadgeSize->SetContent(BadgeScale);
    DetailScroll->AddChild(BadgeSize);
    Detail = MakeText(WidgetTree, TEXT(""), 17, TEXT("NodeDetail"));
    DetailScroll->AddChild(Detail);
    PrimaryAction = AddAction(Sidebar, TEXT("ConfirmNodeButton"), TEXT("前往此节点"), 1, false);
    AddAction(Sidebar, TEXT("ReturnHomeButton"), TEXT("暂回家园 · 保留远征"), 2);
    Status = MakeText(WidgetTree, TEXT(""), 16, TEXT("WorldMapStatus"));
    USizeBox *StatusSize = WidgetTree->ConstructWidget<USizeBox>();
    StatusSize->SetHeightOverride(44);
    StatusSize->SetContent(Status);
    Layout->AddChildToVerticalBox(StatusSize)->SetPadding(FMargin(0, 12, 0, 0));
}
void ULKRunNodeSelectWidget::RefreshNodeSelect()
{
    ULKRunSubsystem *Run = GetRun();
    if (!Run || !Map)
    {
        return;
    }
    const FLKRunState State = Run->GetRunState();
    ChoicesFrame->SetVisibility(Run->HasServiceNode() ? ESlateVisibility::Collapsed
                                                      : ESlateVisibility::SelfHitTestInvisible);
    ChoicesTitle->SetText(FText::FromString(Run->HasServiceNode() ? TEXT("当前节点") : TEXT("下一站")));
    DisplayedNodeIds = Run->CanSelectNextNode() ? Run->GetNextNodeIds() : TArray<FName>();
    if (LastCurrentNodeId != State.CurrentNodeId || SelectedNodeId.IsNone())
    {
        SelectedNodeId = Run->HasServiceNode()        ? State.CurrentNodeId
                         : DisplayedNodeIds.IsEmpty() ? NAME_None
                                                      : DisplayedNodeIds[0];
        LastCurrentNodeId = State.CurrentNodeId;
    }
    FString RegionName = TEXT("旧版路线");
    if (const FLKWorldRegion *Region = State.WorldRegions.FindByPredicate(
            [&State](const FLKWorldRegion &R) { return R.RegionId == State.RegionId; }))
    {
        RegionName = Region->DisplayName.ToString();
    }
    Summary->SetText(FText::FromString(
        FString::Printf(TEXT("%s  /  已完成 %d 场战斗  /  部队容量 %d/8（%d 张）  /  远征金币 %d（携带 %d）"),
                        *RegionName, State.BattleHistory.Num(), Run->GetDeckCapacityUsed(), State.Cards.Num(), Run->GetWalletGold(), State.StartingGold)));
    RegionTabs->ClearChildren();
    RegionIds.Reset();
    AddAction(RegionTabs, TEXT("MapOverviewButton"), TEXT("全图"), 10);
    for (int32 I = 0; I < State.WorldRegions.Num(); ++I)
    {
        RegionIds.Add(State.WorldRegions[I].RegionId);
        AddAction(RegionTabs, FString::Printf(TEXT("Region%dButton"), I), State.WorldRegions[I].DisplayName.ToString(),
                  11 + I);
    }
    Choices->ClearChildren();
    for (int32 I = 0; I < DisplayedNodeIds.Num(); ++I)
    {
        const FLKDungeonNode Node = Run->GetNode(DisplayedNodeIds[I]);
        FString Label = FString::Printf(TEXT("%d · %s"), I + 1, *LKWorldMapContent::NodeTitle(Node.Type).ToString());
        if (Node.RegionId != State.RegionId)
        {
            Label += TEXT(" · 跨区");
        }
        AddAction(Choices, FString::Printf(TEXT("NextNode%dButton"), I), Label, 100 + I);
    }
    Map->SetMapState(State, SelectedNodeId);
    RefreshDetail();
    OnNodeDataReadyBP(DisplayedNodeIds.Num());
    // This page may refresh during the battle HUD tick. New row widgets need a prepass
    // before scroll/box layout; otherwise their first-frame desired heights are zero.
    ForceLayoutPrepass();
}
void ULKRunNodeSelectWidget::SelectMapNode(FName NodeId)
{
    ULKRunSubsystem *Run = GetRun();
    if (!Run || Run->HasServiceNode())
    {
        return;
    }
    if (Run->GetNode(NodeId).NodeId.IsNone())
    {
        return;
    }
    SelectedNodeId = NodeId;
    Map->SetMapState(Run->GetRunState(), SelectedNodeId);
    RefreshDetail();
}
void ULKRunNodeSelectWidget::RefreshDetail()
{
    ULKRunSubsystem *Run = GetRun();
    if (!Run)
    {
        return;
    }
    const FLKRunState State = Run->GetRunState();
    const bool Service = Run->HasServiceNode();
    const FLKDungeonNode Node = Run->GetNode(Service ? State.CurrentNodeId : SelectedNodeId);
    if (NodeIllustration)
    {
        UTexture2D* Texture = LKWorldArt::NodeIcon(Node.Type);
        NodeIllustration->SetBrushFromTexture(Texture);
        NodeIllustration->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    FString Copy = LKWorldMapContent::NodeTitle(Node.Type).ToString() + TEXT("\n\n") +
                   LKWorldMapContent::NodeDescription(Node.Type).ToString();
    if (const FLKEncounterRow *Encounter = LKEncounterContent::Find(State.Encounters, Node.EncounterId))
    {
        Copy += TEXT("\n\n敌方阵容：");
        for (FName HeroId : Encounter->EnemyHeroIds)
        {
            const FLKUnitRow *Unit = LKUnitContent::Find(HeroId);
            Copy += TEXT("\n") + (Unit ? Unit->DisplayName.ToString() : HeroId.ToString());
        }
    }
    if (Service)
    {
        Copy += FString::Printf(TEXT("\n\n钱包：%d 金币"), State.WalletGold);
        for (const FLKRunHeroState &Hero : State.Heroes)
        {
            const FLKUnitRow *Unit = LKUnitContent::Find(Hero.HeroId);
            Copy += FString::Printf(TEXT("\n%s %.0f / %.0f"),
                                    Unit ? *Unit->DisplayName.ToString() : *Hero.HeroId.ToString(), Hero.Health,
                                    Hero.MaxHealth);
        }
    }
    else if (!DisplayedNodeIds.Contains(Node.NodeId))
    {
        Copy += TEXT("\n\n仅可查看，当前位置不能前往此节点。");
    }
    Detail->SetText(FText::FromString(Copy));
    FLKHomePanelAction Action;
    Action.ActionId = TEXT("Start");
    Action.bEnabled = Service || DisplayedNodeIds.Contains(Node.NodeId);
    Action.Label = FText::FromString(
        Service ? (Node.Type == ELKDungeonNodeType::Rest ? TEXT("休息 · 恢复 30% 生命") : TEXT("离开市场"))
                : TEXT("前往此节点"));
    PrimaryAction->SetupAction(1, Action);
}
void ULKRunNodeSelectWidget::HandleAction(int32 Index)
{
    ULKRunSubsystem *Run = GetRun();
    if (!Run)
    {
        return;
    }
    if (Index >= 100)
    {
        const int32 Choice = Index - 100;
        if (DisplayedNodeIds.IsValidIndex(Choice))
        {
            SelectMapNode(DisplayedNodeIds[Choice]);
        }
        return;
    }
    if (Index >= 10)
    {
        const int32 Region = Index - 11;
        Map->FocusRegion(RegionIds.IsValidIndex(Region) ? RegionIds[Region] : NAME_None);
        return;
    }
    if (Index == 2)
    {
        ALKBattleGameMode *GM = OwnerHUD ? OwnerHUD->GetBattleGameMode() : nullptr;
        if (!GM || !GM->ReturnToHome())
        {
            Status->SetText(FText::FromString(TEXT("暂时无法返回家园，请检查存档后重试。")));
        }
        return;
    }
    if (Index != 1)
    {
        return;
    }
    if (Run->HasServiceNode())
    {
        const bool Rest = Run->GetNode(Run->GetRunState().CurrentNodeId).Type == ELKDungeonNodeType::Rest;
        const bool Success = Run->ResolveServiceNode(Rest ? TEXT("RestHeal") : TEXT("LeaveMarket"));
        if (Success)
        {
            FeedbackRemaining = 1.f;
            ULKPresentationSubsystem::Sound(GetWorld(), Rest ? FName("Rest") : FName("NodeEnter"), FVector::ZeroVector, true);
            SelectedNodeId = NAME_None;
        }
        Status->SetText(FText::FromString(Success ? (Rest ? TEXT("已休息，英雄恢复 30% 最大生命。请选择下一站。")
                                                          : TEXT("已离开市场。请选择下一站。"))
                                                  : TEXT("操作未保存，请重试。")));
    }
    else
    {
        const int32 Choice = DisplayedNodeIds.IndexOfByKey(SelectedNodeId);
        if (Choice == INDEX_NONE)
        {
            return;
        }
        const ELKNodeSelectionResult Result =
            OwnerHUD ? OwnerHUD->SelectNextNode(Choice) : Run->SelectNode(SelectedNodeId);
        if (Result != ELKNodeSelectionResult::Rejected)
        { FeedbackRemaining = 1.f; ULKPresentationSubsystem::Sound(GetWorld(), Run->GetNode(SelectedNodeId).Type == ELKDungeonNodeType::Market ? FName("Market") : FName("NodeEnter"), FVector::ZeroVector, true); }
        if (Result == ELKNodeSelectionResult::BattleEntered)
        {
            SetIsEnabled(false);
            return;
        }
        Status->SetText(FText::FromString(
            Result == ELKNodeSelectionResult::Rejected ? TEXT("该节点不可前往或保存失败，请重试。") : TEXT("")));
    }
    RefreshNodeSelect();
}
