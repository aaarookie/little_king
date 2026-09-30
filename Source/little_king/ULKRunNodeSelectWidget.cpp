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
#include "LKCardRules.h"
#include "LKResearchContent.h"
#include "ULKCardDefinition.h"

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
                                              "绿边：当前位置   白边：已选中 · 仅展示当前区域"),
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
    UScrollBox *DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ServiceDetailScroll"));
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
    ServiceActions=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("ServiceActions"));
    DetailScroll->AddChild(ServiceActions);
    DetailScroll->AddChild(Detail);
    PrimaryAction = AddAction(Sidebar, TEXT("ConfirmNodeButton"), TEXT("前往此节点"), 1, false);
    AddAction(Sidebar, TEXT("ReturnMenuButton"), TEXT("退出游戏 · 保存远征"), 2);
    AbandonAction=AddAction(Sidebar,TEXT("AbandonRunButton"),TEXT("放弃远征 · 返回家园"),3);
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
        if(LastCurrentNodeId!=State.CurrentNodeId && Status) { Status->SetText(FText::GetEmpty()); }
        SelectedNodeId = Run->HasServiceNode()        ? State.CurrentNodeId
                         : DisplayedNodeIds.IsEmpty() ? NAME_None
                                                      : DisplayedNodeIds[0];
        LastCurrentNodeId = State.CurrentNodeId; SelectedMarketIndex=INDEX_NONE; MarketRemoved.Reset(); bConfirmAbandon=false;
    }
    FString RegionName = TEXT("旧版路线");
    if (const FLKWorldRegion *Region = State.WorldRegions.FindByPredicate(
            [&State](const FLKWorldRegion &R) { return R.RegionId == State.RegionId; }))
    {
        RegionName = Region->DisplayName.ToString();
    }
    Summary->SetText(FText::FromString(
        FString::Printf(TEXT("%s  /  已完成 %d 场战斗  /  部队容量 %d/%d（%d 张）  /  远征金币 %d（携带 %d）"),
                        *RegionName, State.BattleHistory.Num(), Run->GetDeckCapacityUsed(), State.DeckCapacityMaximum, State.Cards.Num(), Run->GetWalletGold(), State.StartingGold)));
    RegionTabs->ClearChildren();
    RegionIds.Reset();
    RegionTabs->SetVisibility(ESlateVisibility::Collapsed);
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
    if (Run->GetNode(NodeId).NodeId.IsNone() || (Run->GetNode(NodeId).RegionId!=Run->GetRunState().RegionId && !DisplayedNodeIds.Contains(NodeId)))
    {
        SelectedNodeId=NAME_None; Map->SetMapState(Run->GetRunState(),SelectedNodeId); RefreshDetail();
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
        // Service actions take precedence over a large decorative badge.
        if(auto* Badge=Cast<USizeBox>(NodeIllustration->GetParent()->GetParent()))
        { Badge->SetVisibility(Texture && !Service ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    }
    FString Copy = LKWorldMapContent::NodeTitle(Node.Type).ToString() + TEXT("\n\n") +
                   LKWorldMapContent::NodeDescription(Node.Type).ToString();
    if (const FLKEncounterRow *Encounter = LKEncounterContent::Find(State.Encounters, Node.EncounterId))
    {
        Copy += FString::Printf(TEXT("\n深度 %d · 基础生命 ×%.2f / 攻击 ×%.2f"),
            Encounter->Depth, Encounter->EnemyHealthScale, Encounter->EnemyDamageScale);
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
    ServiceActions->ClearChildren();
    auto CardName=[&](FName Id)
    { const auto* C=OwnerHUD?OwnerHUD->GetCardDefinition(Id):nullptr; const auto* U=LKUnitContent::Find(Id); const auto* R=LKResearchContent::Find(Id);
      return C?C->CardName.ToString():U?U->DisplayName.ToString():R?R->Name.ToString():Id=="Spell_Fireball"?FString(TEXT("火球术")):Id=="Spell_HealWave"?FString(TEXT("治疗波")):Id.ToString(); };
    if (Service && Node.Type==ELKDungeonNodeType::Rest)
    {
        Copy+=TEXT("\n\n本次任选其一：恢复生命，或升级一张法术／建筑。升级仅本轮有效，数值 ×1.10。");
        for (int32 I=0;I<State.Cards.Num();++I)
        { const auto& C=State.Cards[I]; if (!LKCardRules::IsSpell(C.CardId) && !LKCardRules::IsBuilding(C.CardId)) { continue; }
          AddAction(ServiceActions,FString::Printf(TEXT("RestUpgrade%d"),I),FString::Printf(TEXT("升级 %s · Lv%d → %d"),*CardName(C.CardId),C.UpgradeLevel,C.UpgradeLevel+1),3000+I,C.UpgradeLevel<50); }
    }
    if (Service && Node.Type==ELKDungeonNodeType::Market)
    {
        for (int32 I=0;I<Node.MarketOffers.Num();++I)
        { const auto& O=Node.MarketOffers[I]; const bool Owned=State.Cards.ContainsByPredicate([&](const auto& C){return C.CardId==O.CardId;});
          const TCHAR* Kind=O.Kind==ELKMarketOfferKind::Mercenary?TEXT("临时佣兵"):O.Kind==ELKMarketOfferKind::SpellBook?TEXT("法术书"):TEXT("建筑图纸");
          AddAction(ServiceActions,FString::Printf(TEXT("MarketOffer%d"),I),FString::Printf(TEXT("%s · %s\n%d 金币%s"),*CardName(O.CardId),Kind,O.Price,O.bSold?TEXT(" · 已售出"):Owned&&O.Kind==ELKMarketOfferKind::Mercenary?TEXT(" · 已持有"):TEXT("")),2000+I,!O.bSold && State.WalletGold>=O.Price && !(Owned&&O.Kind==ELKMarketOfferKind::Mercenary)); }
        if (Node.MarketOffers.IsValidIndex(SelectedMarketIndex))
        {
            const auto& O=Node.MarketOffers[SelectedMarketIndex];
            if (O.Kind==ELKMarketOfferKind::Mercenary)
            {
                Copy+=TEXT("\n\n勾选弃牌后确认；固定上限仅允许最少必要弃牌，区间容量允许任意合法组合。至少保留五种循环卡。");
                for (int32 I=0;I<State.Cards.Num();++I)
                { const auto& C=State.Cards[I]; AddAction(ServiceActions,FString::Printf(TEXT("MarketReplace%d"),I),FString::Printf(TEXT("%s %s · %d 格 · Lv%d"),MarketRemoved.Contains(C.CardId)?TEXT("[✓]"):TEXT("[ ]"),*CardName(C.CardId),LKCardRules::Slots(C.CardId),C.UpgradeLevel),4000+I); }
            }
            FString Error; const bool Valid=O.Kind!=ELKMarketOfferKind::Mercenary || LKCardRules::ValidateReplacement(State.Cards,O.CardId,MarketRemoved,State.DeckCapacityMinimum,State.DeckCapacityMaximum,Error);
            Copy+=FString::Printf(TEXT("\n\n已选：%s\n%s"),*CardName(O.CardId),Valid?TEXT("可以购买，材料在本轮结束后带回研究。"):*Error);
            AddAction(ServiceActions,TEXT("ConfirmMarketPurchase"),TEXT("确认购买"),5000,Valid && !O.bSold && State.WalletGold>=O.Price);
        }
    }
    FLKHomePanelAction Abandon; Abandon.ActionId=TEXT("Start"); Abandon.Label=FText::FromString(bConfirmAbandon?TEXT("确认放弃 · 全额金币带回"):TEXT("放弃远征 · 返回家园")); AbandonAction->SetupAction(3,Abandon);
    if (bConfirmAbandon) { AddAction(ServiceActions,TEXT("CancelAbandon"),TEXT("取消放弃"),4); }
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
    if (Index>=2000)
    {
        FString Error;
        if (Index==5000)
        { const bool OK=Run->PurchaseMarketOffer(SelectedMarketIndex,MarketRemoved,Error); if (OK) { SelectedMarketIndex=INDEX_NONE; MarketRemoved.Reset(); } Status->SetText(FText::FromString(OK?TEXT("购买已保存"):Error)); }
        else if (Index>=4000)
        { const auto Cards=Run->GetRunState().Cards; if (Cards.IsValidIndex(Index-4000)) { const FName Id=Cards[Index-4000].CardId; if (MarketRemoved.Contains(Id)) { MarketRemoved.Remove(Id); } else { MarketRemoved.Add(Id); } } }
        else if (Index>=3000)
        { const auto Cards=Run->GetRunState().Cards; if (Cards.IsValidIndex(Index-3000)) { const auto C=Cards[Index-3000]; const bool OK=Run->UpgradeAtRest(C.CardId,C.UpgradeLevel,Error); Status->SetText(FText::FromString(OK?TEXT("已升级，本次休息机会已使用"):Error)); SelectedNodeId=NAME_None; } }
        else { SelectedMarketIndex=Index-2000; MarketRemoved.Reset(); }
        RefreshNodeSelect(); return;
    }
    if (Index==3 || Index==4)
    {
        if (Index==4) { bConfirmAbandon=false; RefreshDetail(); return; }
        if (!bConfirmAbandon) { bConfirmAbandon=true; RefreshDetail(); return; }
        ALKBattleGameMode* GM=OwnerHUD?OwnerHUD->GetBattleGameMode():nullptr;
        if (!GM || !GM->AbandonToHome()) { Status->SetText(FText::FromString(TEXT("放弃未保存，请重试"))); } return;
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
        if (!GM || !GM->ExitToStartMenu())
        {
            Status->SetText(FText::FromString(TEXT("保存或退出未完成，请重试。")));
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
