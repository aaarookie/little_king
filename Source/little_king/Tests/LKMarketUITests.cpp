#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "LKWorldMapTestHelpers.h"
#include "../ULKRunNodeSelectWidget.h"
#include "../ULKHomeHUDWidget.h"
#include "../ALKHomeGameMode.h"
#include "../ALKHomePlayerController.h"
#include "../ULKSilverComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

namespace
{
constexpr auto MarketFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FMarketUIScope
{
    FString Base=TEXT("LittleKing_MarketUI_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FMarketUIScope()
    {
        ULKProfileSubsystem::SetSlotNameOverrideForTest(Base);
        ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
        ULKRunSubsystem::SetRunSlotNameOverrideForTest(Base+TEXT("_Run"));
    }
    ~FMarketUIScope()
    {
        for (const FString& Slot:{ULKProfileSubsystem::GetSlotNameA(),ULKProfileSubsystem::GetSlotNameB(),ULKRunSubsystem::GetRunSlotName()})
        { UGameplayStatics::DeleteGameInSlot(Slot,0); }
        ULKProfileSubsystem::ResetTestHooks();ULKRunSubsystem::SetRunSlotNameOverrideForTest(FString());
    }
};
bool ClickMarket(UUserWidget* Widget,FName Name)
{
    auto* Entry=Cast<UUserWidget>(Widget->GetWidgetFromName(Name));
    auto* Button=Entry?Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))):nullptr;
    if (!Button || !Button->GetIsEnabled()) { return false; }
    Button->OnClicked.Broadcast();return true;
}
bool ReachMarket(ULKRunSubsystem* Run)
{
    for (int32 Step=0;Step<50;++Step)
    {
        if (Run->HasServiceNode())
        { if (Run->GetNode(Run->GetRunState().CurrentNodeId).Type==ELKDungeonNodeType::Market) { return true; }
          if (!Run->ResolveServiceNode(TEXT("RestHeal"))) { return false; } }
        else if (Run->HasPendingRewardChoice()) { if (!Run->SkipReward()) { return false; } }
        else if (Run->GetRunPhase()==ELKRunPhase::EnteringBattle)
        {
            FLKBattleContext C;if (!Run->BeginCurrentBattle(C)) { return false; }
            FLKBattleOutcome R;R.RunId=C.RunId;R.NodeId=C.NodeId;R.AttemptId=C.AttemptId;R.bFinalized=true;R.Stats.Winner=ELKTeam::Player;
            for (const auto& Hero:C.PlayerHeroes) { FLKHeroBattleOutcome H;H.InstanceId=Hero.HeroId;H.RecoveredState=Hero;R.PlayerHeroes.Add(H); }
            if (!Run->SubmitBattleOutcome(R)) { return false; }
        }
        else { auto Next=Run->GetNextNodeIds();if (Next.IsEmpty() || Run->SelectNode(Next[0])==ELKNodeSelectionResult::Rejected) { return false; } }
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMarketUIFlowTest,"LittleKing.Market.UI.CategoriesPurchaseAndInventory",MarketFlags)
bool FLKMarketUIFlowTest::RunTest(const FString& Parameters)
{
    FMarketUIScope Scope;FTestWorldWrapper World;
    if (!World.CreateTestWorld(EWorldType::Game)) { return false; }
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode=ALKHomeGameMode::StaticClass();
    if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this);return false; }
    auto* GM=World.GetTestWorld()->GetAuthGameMode<ALKHomeGameMode>();
    World.GetTestWorld()->SpawnActor<ALKHomePlayerController>();GM->Tick(0.f);
    auto* Profile=GM->GetProfileSubsystem();Profile->AddGold(300);
    auto* Run=World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    FLKExpeditionStartRequest Request;
    if (!TestTrue(TEXT("Real home request builds"),LKWorldMapTest::Request(Profile,Request))) { return false; }
    Request.StartingGold=100;Request.Seed=9152026;
    if (!TestTrue(TEXT("Start and reach real first market"),Run->StartNewRun(Request)&&ReachMarket(Run))) { return false; }
    auto* UI=CreateWidget<ULKRunNodeSelectWidget>(World.GetTestWorld(),ULKRunNodeSelectWidget::StaticClass());
    UI->InitializeRunView(Run);const auto Slate=UI->TakeWidget();UI->RefreshNodeSelect();
    auto* Grid=Cast<UUniformGridPanel>(UI->GetWidgetFromName(TEXT("MarketOfferGrid")));
    const auto Offers=Run->GetMarketOffers();
    TestTrue(TEXT("All offers displayed in catalog"),Grid && Grid->GetChildrenCount()==Offers.Num());
    TestTrue(TEXT("Items filter clickable"),ClickMarket(UI,TEXT("MarketTab2")));
    TestEqual(TEXT("Item filter shows one item"),Grid->GetChildrenCount(),1);
    const int32 ItemIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::Item;});
    if (!TestTrue(TEXT("Item available"),ItemIndex!=INDEX_NONE)) { return false; }
    TestTrue(TEXT("Item row opens detail"),ClickMarket(UI,FName(*FString::Printf(TEXT("MarketOffer%d"),ItemIndex))));
    auto* Detail=Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("NodeDetail")));
    TestTrue(TEXT("Placeholder has honest no-attribute detail"),Detail && Detail->GetText().ToString().Contains(TEXT("暂无属性")));
    const int32 Before=Run->GetWalletGold();
    TestTrue(TEXT("Real confirm purchases item"),ClickMarket(UI,TEXT("ConfirmMarketPurchase")));
    TestEqual(TEXT("Exact item price charged"),Run->GetWalletGold(),Before-Offers[ItemIndex].Price);
    TestEqual(TEXT("One independent item instance in bag"),Run->GetCarriedItems().Num(),1);
    TestTrue(TEXT("Sold row remains inspectable"),ClickMarket(UI,FName(*FString::Printf(TEXT("MarketOffer%d"),ItemIndex))));
    TestFalse(TEXT("Sold item cannot be purchased twice"),ClickMarket(UI,TEXT("ConfirmMarketPurchase")));
    TestTrue(TEXT("Bag tab opens"),ClickMarket(UI,TEXT("MarketTab4")));
    TestEqual(TEXT("Bag displays item"),Grid->GetChildrenCount(),1);
    TestTrue(TEXT("Bag detail opens"),ClickMarket(UI,TEXT("RunItem0")));
    TestTrue(TEXT("Item detail explains persistence"),Detail->GetText().ToString().Contains(TEXT("带回家园")));
    TestTrue(TEXT("Temporary cards filter opens"),ClickMarket(UI,TEXT("MarketTab1")));
    const int32 SpellIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::TemporarySpell;});
    if (!TestTrue(TEXT("Temporary spell exists"),SpellIndex!=INDEX_NONE)) { return false; }
    TestTrue(TEXT("Spell opens"),ClickMarket(UI,FName(*FString::Printf(TEXT("MarketOffer%d"),SpellIndex))));
    TestTrue(TEXT("Temporary lifetime explicit"),Detail->GetText().ToString().Contains(TEXT("仅本次远征")));
    if (!TestTrue(TEXT("Temporary spell purchase succeeds"),ClickMarket(UI,TEXT("ConfirmMarketPurchase")))) { return false; }
    TestTrue(TEXT("Spell enters current deck"),Run->GetRunState().Cards.ContainsByPredicate([&](const auto& C){return C.CardId==Offers[SpellIndex].CardId;}));
    TestFalse(TEXT("Temporary spell does not unlock permanent collection"),Profile->GetProfile().UnlockedCardIds.Contains(Offers[SpellIndex].CardId));
    const auto Item=Run->GetCarriedItems()[0];
    Run->AbandonRun();
    TestTrue(TEXT("Abandon settles the actual run"),Run->IsTerminal());
    TestTrue(TEXT("Settlement applies"),Run->ApplyPendingSettlementToProfile());
    TestTrue(TEXT("Item identity reaches home exactly once"),Profile->GetOwnedItems().ContainsByPredicate([&](const auto& I){return I.InstanceId==Item.InstanceId;}));
    auto Model=GM->BuildPanelModel(ELKHomePanel::Barracks,Profile->GetSavedLoadout(),NAME_None,2);
    TestTrue(TEXT("Home warehouse exposes the item"),Model.Rows.ContainsByPredicate([](const auto& R){return R.Detail.ToString().Contains(TEXT("暂无属性"));}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSilverMetricsTest,"LittleKing.Balance.SilverOverflowMetrics",MarketFlags)
bool FLKSilverMetricsTest::RunTest(const FString& Parameters)
{
    auto* Silver=NewObject<ULKSilverComponent>();Silver->Init(2.f,5.f);
    Silver->TickSilver(4.f);
    TestEqual(TEXT("Silver still caps normally"),Silver->GetSilver(),5.f);
    TestEqual(TEXT("Overflow records lost generation"),Silver->GetOverflow(),3.f);
    TestEqual(TEXT("Partial tick at cap accounted exactly"),Silver->GetAtCapSeconds(),1.5f);
    Silver->TrySpend(2.f);Silver->TickSilver(1.f);
    TestEqual(TEXT("Refilling to cap without excess adds no waste"),Silver->GetOverflow(),3.f);
    Silver->Init(1.f,5.f);
    TestEqual(TEXT("New match resets observations"),Silver->GetOverflow(),0.f);
    Silver->Init(0.f,5.f); Silver->AddSilver(5.f); Silver->TickSilver(2.f);
    TestEqual(TEXT("Zero generation still records time spent at cap"),Silver->GetAtCapSeconds(),2.f);
    TestEqual(TEXT("Granted silver is not passive generation overflow"),Silver->GetOverflow(),0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMarketUIRefreshTest,"LittleKing.Market.UI.StablePollingAndExternalChanges",MarketFlags)
bool FLKMarketUIRefreshTest::RunTest(const FString& Parameters)
{
    FMarketUIScope Scope; FTestWorldWrapper World;
    if (!World.CreateTestWorld(EWorldType::Game)) { return false; }
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode=ALKHomeGameMode::StaticClass();
    if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
    auto* GM=World.GetTestWorld()->GetAuthGameMode<ALKHomeGameMode>();
    World.GetTestWorld()->SpawnActor<ALKHomePlayerController>(); GM->Tick(0.f);
    auto* Profile=GM->GetProfileSubsystem(); Profile->AddGold(300);
    auto* Run=World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    FLKExpeditionStartRequest Request;
    if (!TestTrue(TEXT("Home request builds"),LKWorldMapTest::Request(Profile,Request))) { return false; }
    Request.StartingGold=100; Request.Seed=9152026;
    if (!TestTrue(TEXT("Reach market"),Run->StartNewRun(Request)&&ReachMarket(Run))) { return false; }
    auto* UI=CreateWidget<ULKRunNodeSelectWidget>(World.GetTestWorld(),ULKRunNodeSelectWidget::StaticClass());
    UI->InitializeRunView(Run); const auto Slate=UI->TakeWidget(); UI->RefreshNodeSelect();
    auto* Grid=Cast<UUniformGridPanel>(UI->GetWidgetFromName(TEXT("MarketOfferGrid")));
    auto* Summary=Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("WorldMapSummary")));
    if (!Grid || !Summary) { AddError(TEXT("Native market tree missing")); return false; }

    auto* Tab=UI->GetWidgetFromName(TEXT("MarketTab2"));
    if (!TestNotNull(TEXT("Item category tab exists"),Tab)) { return false; }
    const auto TabSlate=Tab->GetCachedWidget();
    TestTrue(TEXT("Tab has a live Slate capture target"),TabSlate.IsValid());
    for (int32 Tick=0; Tick<5; ++Tick) { UI->RefreshNodeSelectIfChanged(); }
    TestTrue(TEXT("Idle HUD polls preserve the same tab UObject"),UI->GetWidgetFromName(TEXT("MarketTab2"))==Tab);
    TestTrue(TEXT("Idle HUD polls preserve Slate mouse capture target"),UI->GetWidgetFromName(TEXT("MarketTab2"))->GetCachedWidget()==TabSlate);

    const auto Offers=Run->GetMarketOffers();
    const int32 ItemIndex=Offers.IndexOfByPredicate([](const auto& O){return O.Kind==ELKMarketOfferKind::Item;});
    if (!TestTrue(TEXT("Market includes an item"),ItemIndex!=INDEX_NONE)) { return false; }
    const FName ItemRow(*FString::Printf(TEXT("MarketOffer%d"),ItemIndex));
    TestTrue(TEXT("Local item selection refreshes immediately"),ClickMarket(UI,ItemRow));
    auto* Confirm=UI->GetWidgetFromName(TEXT("ConfirmMarketPurchase"));
    if (!TestNotNull(TEXT("Selected item has a confirm action"),Confirm)) { return false; }
    auto* FixedActions=Cast<UVerticalBox>(UI->GetWidgetFromName(TEXT("FixedServiceActions")));
    if (!TestNotNull(TEXT("Fixed service action area exists"),FixedActions)) { return false; }
    TestTrue(TEXT("Purchase stays outside scrolling details"),Confirm->GetParent()==FixedActions);
    TestTrue(TEXT("Fixed actions and leave-market action share sidebar"),FixedActions->GetParent()==UI->GetWidgetFromName(TEXT("ConfirmNodeButton"))->GetParent());
    TestEqual(TEXT("Market tabs replace redundant inventory button"),UI->GetWidgetFromName(TEXT("RunInventoryButton"))->GetVisibility(),ESlateVisibility::Collapsed);
    const auto ConfirmSlate=Confirm->GetCachedWidget();
    TestTrue(TEXT("Confirm action has live Slate content"),ConfirmSlate.IsValid());
    UI->RefreshNodeSelectIfChanged(); UI->RefreshNodeSelectIfChanged();
    TestTrue(TEXT("Unchanged selected purchase preserves action UObject"),UI->GetWidgetFromName(TEXT("ConfirmMarketPurchase"))==Confirm);
    TestTrue(TEXT("Unchanged selected purchase preserves action Slate"),UI->GetWidgetFromName(TEXT("ConfirmMarketPurchase"))->GetCachedWidget()==ConfirmSlate);

    TestTrue(TEXT("Local bag tab changes immediately without a run mutation"),ClickMarket(UI,TEXT("MarketTab4")));
    TestEqual(TEXT("Bag is initially empty"),Grid->GetChildrenCount(),0);
    TestTrue(TEXT("Bag return stays in the fixed action area"),UI->GetWidgetFromName(TEXT("CloseRunInventory"))->GetParent()==FixedActions);
    TestEqual(TEXT("Bag has one fixed action, no stale purchase action"),FixedActions->GetChildrenCount(),1);
    auto* BagTab=UI->GetWidgetFromName(TEXT("MarketTab4"));
    UI->RefreshNodeSelectIfChanged();
    TestTrue(TEXT("Next HUD poll preserves locally refreshed tab"),UI->GetWidgetFromName(TEXT("MarketTab4"))==BagTab);

    const FString OldSummary=Summary->GetText().ToString();
    if (!TestTrue(TEXT("External wallet transaction succeeds"),Run->ChangeWalletGold(1,FGuid::NewGuid()))) { return false; }
    UI->RefreshNodeSelectIfChanged();
    TestTrue(TEXT("External wallet change refreshes the summary"),Summary->GetText().ToString()!=OldSummary);

    const FString SnapshotSlot=Scope.Base+TEXT("_Snapshot");
    ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(SnapshotSlot,0); };
    if (!TestTrue(TEXT("Save pre-purchase snapshot"),Run->SaveExpeditionToSlot(SnapshotSlot))) { return false; }
    FString Error;
    if (!TestTrue(TEXT("Purchase through a different caller"),Run->PurchaseMarketOffer(ItemIndex,{},Error))) { return false; }
    UI->RefreshNodeSelectIfChanged();
    TestEqual(TEXT("External purchase immediately reaches visible bag"),Grid->GetChildrenCount(),1);
    auto* Item=UI->GetWidgetFromName(TEXT("RunItem0"));
    UI->RefreshNodeSelectIfChanged();
    TestTrue(TEXT("Post-purchase idle poll retains new item button"),UI->GetWidgetFromName(TEXT("RunItem0"))==Item);

    if (!TestTrue(TEXT("External reload of the same run succeeds"),Run->LoadExpeditionFromSlot(SnapshotSlot,true))) { return false; }
    UI->RefreshNodeSelectIfChanged();
    TestEqual(TEXT("Reloaded inventory refreshes even with same run and node IDs"),Grid->GetChildrenCount(),0);
    TestTrue(TEXT("Return from bag still acts immediately"),ClickMarket(UI,TEXT("CloseRunInventory")));
    TestEqual(TEXT("Returning restores offer catalog"),Grid->GetChildrenCount(),Offers.Num());
    if (!TestTrue(TEXT("External service completion succeeds"),Run->ResolveServiceNode(TEXT("LeaveMarket")))) { return false; }
    UI->RefreshNodeSelectIfChanged();
    auto* Next=UI->GetWidgetFromName(TEXT("NextNode0Button"));
    TestNotNull(TEXT("External phase change replaces market with route choices"),Next);
    TestEqual(TEXT("Route view restores inventory shortcut"),UI->GetWidgetFromName(TEXT("RunInventoryButton"))->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
    TestEqual(TEXT("Route view clears old fixed service actions"),FixedActions->GetChildrenCount(),0);
    UI->RefreshNodeSelectIfChanged();
    TestTrue(TEXT("Idle route polling also preserves choice buttons"),UI->GetWidgetFromName(TEXT("NextNode0Button"))==Next);
    return true;
}
#endif
