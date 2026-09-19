#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "LKWorldMapTestHelpers.h"
#include "../LKEncounterContent.h"
#include "../ULKRunSaveGame.h"
#include "../ULKRunNodeSelectWidget.h"
#include "../ULKWorldMapWidget.h"
#include "../ALKHomeGameMode.h"
#include "../ALKBattleGameMode.h"
#include "../ULKHomeHUDWidget.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FScope
{
    FString Base = TEXT("LittleKing_WorldMapTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ULKProfileSubsystem *Profile;
    ULKRunSubsystem *Run;
    FScope()
    {
        ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
        Profile = NewObject<ULKProfileSubsystem>(NewObject<UGameInstance>());
        Profile->ConfigureStorage(Base);
        Profile->EnsureProfile();
        Run = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
        Run->ConfigureStorage(Base + TEXT("_Run"));
        Run->SetProfileSubsystemOverrideForTest(Profile);
    }
    ~FScope()
    {
        for (const FString &S : {Base + TEXT("_A"), Base + TEXT("_B"), Base + TEXT("_Run"), Base + TEXT("_Crash")})
        {
            if (UGameplayStatics::DoesSaveGameExist(S, 0))
            {
                UGameplayStatics::DeleteGameInSlot(S, 0);
            }
        }
        ULKProfileSubsystem::ResetTestHooks();
    }
    FLKExpeditionStartRequest Request(int32 Gold = 0)
    {
        FLKExpeditionStartRequest R;
        LKWorldMapTest::Request(Profile, R);
        R.Seed = 9152026;
        R.StartingGold = Gold;
        return R;
    }
};
FLKBattleOutcome Outcome(const FLKBattleContext &Context, bool Win = true)
{
    FLKBattleOutcome Result;
    Result.RunId = Context.RunId;
    Result.NodeId = Context.NodeId;
    Result.AttemptId = Context.AttemptId;
    Result.bFinalized = true;
    Result.Stats.Winner = Win ? ELKTeam::Player : ELKTeam::Enemy;
    for (const FLKRunHeroState &Hero : Context.PlayerHeroes)
    {
        FLKHeroBattleOutcome Item;
        Item.InstanceId = Hero.HeroId;
        Item.RecoveredState = Hero;
        Item.RecoveredState.Health = Hero.MaxHealth * .4f;
        Result.PlayerHeroes.Add(Item);
    }
    return Result;
}
bool Click(UUserWidget *Widget, const TCHAR *Name)
{
    UUserWidget *Entry = Cast<UUserWidget>(Widget->GetWidgetFromName(Name));
    UButton *Button = Entry ? Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))) : nullptr;
    if (!Button || !Button->GetIsEnabled())
    {
        return false;
    }
    Button->OnClicked.Broadcast();
    return true;
}
FString Network(const FLKRunState &State)
{
    FString Text;
    for (const FLKDungeonNode &N : State.Nodes)
    {
        Text += N.NodeId.ToString() + FString::FromInt(int32(N.Type));
        for (FName Id : N.NextNodeIds)
        {
            Text += Id.ToString();
        }
    }
    return Text;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldGraphTest, "LittleKing.Optimization2.WorldGraph200Seeds", Flags)
bool FLKWorldGraphTest::RunTest(const FString &Parameters)
{
    TArray<FLKEncounterRow> Catalog;
    FString Error;
    LKEncounterContent::BuildCatalog(nullptr, Catalog, Error);
    FString First;
    TMap<FName, FVector2D> FixedPositions;
    TSet<FString> Networks;
    for (int32 Seed = 0; Seed < 200; ++Seed)
    {
        FLKRunState State;
        State.Seed = Seed;
        State.Encounters = Catalog;
        if (!TestTrue(*FString::Printf(TEXT("Seed %d: %s"), Seed, *Error), LKWorldMapContent::Generate(State, Error)))
        {
            return false;
        }
        Networks.Add(Network(State));
        if (Seed == 0)
        {
            First = Network(State);
        }
        for (const FLKWorldRegion &R : State.WorldRegions)
        {
            const int32 Count =
                State.Nodes.FilterByPredicate([&](const FLKDungeonNode &N) { return N.RegionId == R.RegionId; }).Num();
            TestTrue(TEXT("Every region contains 10 to 20 nodes"), Count >= 10 && Count <= 20);
            for (FName NextId : R.NextRegionIds)
            {
                const FLKWorldRegion *Next = State.WorldRegions.FindByPredicate(
                    [NextId](const FLKWorldRegion &X) { return X.RegionId == NextId; });
                int32 SharedVertices = 0;
                for (FVector2D P : R.Polygon)
                {
                    for (FVector2D Q : Next->Polygon)
                    {
                        if (P.Equals(Q, 1.e-5))
                        {
                            ++SharedVertices;
                            break;
                        }
                    }
                }
                TestTrue(TEXT("Cross-region travel only crosses a shared geographical edge"), SharedVertices >= 2);
            }
            for (const FLKDungeonNode &Node : State.Nodes)
            {
                if (Node.RegionId != R.RegionId)
                {
                    continue;
                }
                TestTrue(TEXT("Node remains inside its fixed region"),
                         LKWorldMapContent::Contains(R, Node.MapPosition));
                if (R.EntryNodeIds.Contains(Node.NodeId) || R.ExitNodeIds.Contains(Node.NodeId))
                {
                    if (Seed == 0)
                    {
                        FixedPositions.Add(Node.NodeId, Node.MapPosition);
                    }
                    else
                    {
                        TestTrue(TEXT("Entries and exits retain exact coordinates"),
                                 FixedPositions.FindChecked(Node.NodeId).Equals(Node.MapPosition, 1.e-8));
                    }
                }
                if (LKWorldMapContent::IsCombat(Node.Type))
                {
                    const FLKEncounterRow *E = LKEncounterContent::Find(State.Encounters, Node.EncounterId);
                    TestEqual(TEXT("Normal/elite/boss encounter roster size"), E->EnemyHeroIds.Num(),
                              Node.Type == ELKDungeonNodeType::Battle  ? 1
                              : Node.Type == ELKDungeonNodeType::Elite ? 2
                                                                       : 3);
                    if (Node.Type == ELKDungeonNodeType::Boss)
                    {
                        TestTrue(TEXT("Boss encounter contains skeleton king"),
                                 E->EnemyHeroIds.Contains(TEXT("Boss_SkeletonKing")));
                    }
                }
            }
        }
        for (FName Entry : State.WorldRegions[3].EntryNodeIds)
        {
            TSet<FName> Origins;
            for (const FLKDungeonNode &N : State.Nodes)
            {
                if (N.NextNodeIds.Contains(Entry))
                {
                    Origins.Add(N.RegionId);
                }
            }
            TestTrue(TEXT("Merge entry accepts both adjacent predecessor regions"),
                     Origins.Contains(State.WorldRegions[1].RegionId) &&
                         Origins.Contains(State.WorldRegions[2].RegionId));
        }
        if (Seed == 0)
        {
            FLKRunState Bad = State;
            Bad.Nodes[1].NextNodeIds.Add(State.CurrentNodeId);
            TestFalse(TEXT("Cycle/backward link rejected"), LKWorldMapContent::Validate(Bad, Error));
            Bad = State;
            Bad.Nodes[0].NextNodeIds.Reset();
            TestFalse(TEXT("Orphaned subgraph rejected"), LKWorldMapContent::Validate(Bad, Error));
        }
    }
    TestTrue(TEXT("Expedition seeds generate distinct networks"), Networks.Num() > 190);
    FLKRunState Repeat;
    Repeat.Seed = 0;
    Repeat.Encounters = Catalog;
    LKWorldMapContent::Generate(Repeat, Error);
    TestEqual(TEXT("Same seed reproduces network"), Network(Repeat), First);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldWalletTest, "LittleKing.Optimization2.WalletSettlementAndCrashRecovery", Flags)
bool FLKWorldWalletTest::RunTest(const FString &Parameters)
{
    FScope S;
    TestTrue(TEXT("Fund home"), S.Profile->AddGold(300));
    FLKExpeditionStartRequest First, Second;
    LKWorldMapTest::Request(S.Profile, First);
    LKWorldMapTest::Request(S.Profile, Second);
    TestTrue(TEXT("Formal departures draw fresh map seeds"), First.Seed != Second.Seed);
    TestFalse(TEXT("Treasury carry limit enforced"), S.Run->StartNewRun(S.Request(101)));
    TestEqual(TEXT("Rejected departure costs nothing"), S.Profile->GetGold(), 300);
    TestTrue(TEXT("Depart with 50"), S.Run->StartNewRun(S.Request(50)));
    TestEqual(TEXT("Home deducted once"), S.Profile->GetGold(), 250);
    TestEqual(TEXT("Wallet funded"), S.Run->GetWalletGold(), 50);
    TestTrue(TEXT("Initial load reconciliation"), S.Run->ReconcileDepartureFunding());
    TestEqual(TEXT("Reconciliation does not repeat debit"), S.Profile->GetGold(), 250);
    FGuid Credit = FGuid::NewGuid();
    TestTrue(TEXT("Node reward may exceed departure cap"), S.Run->ChangeWalletGold(500, Credit));
    TestTrue(TEXT("Repeated reward is idempotent"), S.Run->ChangeWalletGold(500, Credit));
    TestEqual(TEXT("Reward granted once"), S.Run->GetWalletGold(), 550);
    TestFalse(TEXT("Overdraft rejected"), S.Run->ChangeWalletGold(-551, FGuid::NewGuid()));
    TestTrue(TEXT("Market purchase extension can debit wallet"), S.Run->ChangeWalletGold(-30, FGuid::NewGuid()));
    TestTrue(TEXT("Save wallet"), S.Run->SaveExpedition());
    ULKRunSubsystem *Reload = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
    Reload->ConfigureStorage(S.Base + TEXT("_Run"));
    Reload->SetProfileSubsystemOverrideForTest(S.Profile);
    TestTrue(TEXT("Read back full random map and wallet"), Reload->LoadExpedition());
    TestEqual(TEXT("Saved graph unchanged"), Network(Reload->GetRunState()), Network(S.Run->GetRunState()));
    TestTrue(TEXT("Persisted transaction deduplicated"), Reload->ChangeWalletGold(500, Credit));
    TestEqual(TEXT("Wallet after restore"), Reload->GetWalletGold(), 520);
    TestTrue(TEXT("Abandon returns entire remaining wallet"), Reload->AbandonCurrentRun());
    TestEqual(TEXT("Bank receives all unspent gold"), S.Profile->GetGold(), 770);
    TestTrue(TEXT("Settlement retry succeeds"), Reload->ApplyPendingSettlementToProfile());
    TestEqual(TEXT("Settlement credited once"), S.Profile->GetGold(), 770);
    // 模拟 Profile 已入账、Run 的已入账标记未写成功，冷启动不可再扣一次携带金币。
    ULKRunSaveGame *Crash = NewObject<ULKRunSaveGame>();
    Crash->RunState = Reload->GetRunState();
    Crash->RunState.PendingSettlement.bProfileApplied = false;
    UGameplayStatics::SaveGameToSlot(Crash, S.Base + TEXT("_Crash"), 0);
    TestTrue(TEXT("Load interrupted handoff"), Reload->LoadExpeditionFromSlot(S.Base + TEXT("_Crash"), true));
    TestTrue(TEXT("Reconcile credited receipt without a second debit"), Reload->ReconcileDepartureFunding());
    TestTrue(TEXT("Complete interrupted receipt"), Reload->ApplyPendingSettlementToProfile());
    TestEqual(TEXT("Crash window preserves total"), S.Profile->GetGold(), 770);
    TestTrue(TEXT("Reserve for an interrupted departure"), S.Profile->ReserveDepartureGold(FGuid::NewGuid(), 25));
    TestTrue(TEXT("Known missing run refunds orphaned departure"), S.Profile->ReconcileDepartureGold(FGuid()));
    TestEqual(TEXT("Orphan refunded exactly"), S.Profile->GetGold(), 770);
    TestTrue(TEXT("Repeated orphan reconciliation is safe"), S.Profile->ReconcileDepartureGold(FGuid()));
    TestEqual(TEXT("No repeated refund"), S.Profile->GetGold(), 770);
    for (int32 Level = 1; Level <= 5; ++Level)
    {
        TestEqual(TEXT("Treasury carry progression"), LKHomeContent::TreasuryDepartureGoldCap(Level),
                  100 + (Level - 1) * 50);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldPathsTest, "LittleKing.Optimization2.BranchProgressionAndFullGold", Flags)
bool FLKWorldPathsTest::RunTest(const FString &Parameters)
{
    for (int32 Scenario = 0; Scenario < 12; ++Scenario)
    {
        FScope S;
        S.Profile->AddGold(100);
        S.Profile->SetAutoSaveEnabled(false);
        S.Run->SetAutoSaveEnabled(false);
        FLKExpeditionStartRequest Request = S.Request(50);
        Request.Seed = Scenario;
        if (!TestTrue(TEXT("Start world map"), S.Run->StartNewRun(Request)))
        {
            return false;
        }
        TestFalse(TEXT("Cannot jump to future region"),
                  S.Run->SelectNode(TEXT("World_R4_L4_N0")) != ELKNodeSelectionResult::Rejected);
        FRandomStream Choices(Scenario);
        TSet<FName> TraveledRegions;
        for (int32 Step = 0; Step < 100 && !S.Run->IsTerminal(); ++Step)
        {
            TraveledRegions.Add(S.Run->GetRegionId());
            if (S.Run->HasServiceNode())
            {
                const FLKRunState Before = S.Run->GetRunState();
                const bool Rest = S.Run->GetNode(Before.CurrentNodeId).Type == ELKDungeonNodeType::Rest;
                TestFalse(TEXT("Service rejects unknown action"), S.Run->ResolveServiceNode(TEXT("Unknown")));
                TestTrue(TEXT("Service completes"),
                         S.Run->ResolveServiceNode(Rest ? TEXT("RestHeal") : TEXT("LeaveMarket")));
                const FLKRunState After = S.Run->GetRunState();
                for (int32 I = 0; I < After.Heroes.Num(); ++I)
                {
                    TestTrue(TEXT("Rest heals capped 30 percent; market leaves health unchanged"),
                             FMath::IsNearlyEqual(
                                 After.Heroes[I].Health,
                                 Rest ? FMath::Min(Before.Heroes[I].MaxHealth,
                                                   Before.Heroes[I].Health + .3f * Before.Heroes[I].MaxHealth)
                                      : Before.Heroes[I].Health));
                }
                TestFalse(TEXT("Cannot resolve service twice"),
                          S.Run->ResolveServiceNode(Rest ? TEXT("RestHeal") : TEXT("LeaveMarket")));
            }
            else if (S.Run->CanSelectNextNode())
            {
                const TArray<FName> Next = S.Run->GetNextNodeIds();
                if (!TestFalse(TEXT("Route never stalls"), Next.IsEmpty()))
                {
                    return false;
                }
                TestTrue(TEXT("Enter adjacent node"), S.Run->SelectNode(Next[Choices.RandRange(0, Next.Num() - 1)]) !=
                                                          ELKNodeSelectionResult::Rejected);
            }
            else if (S.Run->HasPendingRewardChoice())
            {
                S.Run->SkipReward();
            }
            else
            {
                FLKBattleContext Context;
                if (!TestTrue(TEXT("Begin battle"), S.Run->BeginCurrentBattle(Context)))
                {
                    return false;
                }
                const bool Win = Scenario % 2 == 0 || S.Run->GetRunState().BattleHistory.Num() < 3;
                TestTrue(TEXT("Battle outcome commits"), S.Run->SubmitBattleOutcome(Outcome(Context, Win)));
            }
        }
        TestTrue(TEXT("Chosen route reaches a terminal state"), S.Run->IsTerminal());
        const bool Win = Scenario % 2 == 0;
        TestEqual(TEXT("Expected terminal outcome"), S.Run->GetRunPhase(),
                  Win ? ELKRunPhase::Completed : ELKRunPhase::Failed);
        TestEqual(TEXT("Success/failure brings back all wallet gold"), S.Profile->GetGold(),
                  100 + S.Run->GetPendingGold() + (Win ? 20 : 0));
        if (Win)
        {
            TestEqual(TEXT("Upper/lower branch traverses four of five regions"), TraveledRegions.Num(), 4);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldUITest, "LittleKing.Optimization2.MapServiceAndGateUI", Flags)
bool FLKWorldUITest::RunTest(const FString &Parameters)
{
    FScope S;
    S.Profile->AddGold(300);
    TestTrue(TEXT("Start"), S.Run->StartNewRun(S.Request(50)));
    FTestWorldWrapper World;
    World.CreateTestWorld(EWorldType::Game);
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    if (!World.BeginPlayInTestWorld())
    {
        World.ForwardErrorMessages(this);
        return false;
    }
    ULKRunNodeSelectWidget *Widget =
        CreateWidget<ULKRunNodeSelectWidget>(World.GetTestWorld(), ULKRunNodeSelectWidget::StaticClass());
    Widget->InitializeRunView(S.Run);
    Widget->TakeWidget();
    Widget->RefreshNodeSelect();
    TestEqual(TEXT("All available options exposed"), Widget->GetDisplayedNodeCount(), S.Run->GetNextNodeCount());
    TestTrue(TEXT("UI is not limited to the old two choices"), Widget->GetDisplayedNodeCount() > 2);
    TestTrue(TEXT("Overview button"), Click(Widget, TEXT("MapOverviewButton")));
    TestTrue(TEXT("Region zoom button"), Click(Widget, TEXT("Region0Button")));
    Widget->SelectMapNode(TEXT("World_R4_L4_N0"));
    TestFalse(TEXT("Future node cannot be confirmed"), Click(Widget, TEXT("ConfirmNodeButton")));
    Widget->SelectMapNode(TEXT("World_R0_L1_N0"));
    TestTrue(TEXT("Enter market through real UI button"), Click(Widget, TEXT("ConfirmNodeButton")));
    TestTrue(TEXT("Market held open"), S.Run->HasServiceNode());
    TestTrue(TEXT("Service saved"), S.Run->LoadExpeditionFromSlot(S.Base + TEXT("_Run"), true));
    Widget->RefreshNodeSelect();
    TestTrue(TEXT("Leave restored market through UI"), Click(Widget, TEXT("ConfirmNodeButton")));
    TestTrue(TEXT("Ready to choose next"), S.Run->CanSelectNextNode());
    TestEqual(TEXT("No invented market purchase"), S.Run->GetWalletGold(), 50);
    ULKWorldMapWidget *Map = Cast<ULKWorldMapWidget>(Widget->GetWidgetFromName(TEXT("WorldMapCanvas")));
    if (TestNotNull(TEXT("Native map canvas"), Map))
    {
        Map->FocusRegion(NAME_None);
        const FVector2D Size(950, 600);
        const FLKDungeonNode Node = S.Run->GetNode(LKWorldMapContent::StartNodeId());
        const FVector2D Pixel = FVector2D(24, 36) + Node.MapPosition * (Size - FVector2D(48, 60));
        TestEqual(TEXT("Node hit target matches displayed position"), Map->HitTestNode(Pixel, Size), Node.NodeId);
        const TSharedRef<SWidget> MapSlate = Map->TakeWidget();
        TestTrue(TEXT("Paint-only map participates in Slate hit testing"),
                 MapSlate->GetVisibility().IsHitTestVisible());
        const FPointerEvent Mouse(0, Pixel, Pixel, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0,
                                  FModifierKeysState());
        const FReply Reply = MapSlate->OnMouseButtonDown(FGeometry::MakeRoot(Size, FSlateLayoutTransform()), Mouse);
        TestTrue(TEXT("Slate mouse click handled by map"), Reply.IsEventHandled());
        TestEqual(TEXT("Map click selects the clicked node"), Widget->GetSelectedNode(), Node.NodeId);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldArrivalTest, "LittleKing.Optimization2.BattleWorldPreservesMapAndService",
                                 Flags)
bool FLKWorldArrivalTest::RunTest(const FString &Parameters)
{
    for (int32 PhaseCase = 0; PhaseCase < 3; ++PhaseCase)
    {
        FScope S;
        FTestWorldWrapper World;
        World.CreateTestWorld(EWorldType::Game);
        UGameInstance *Instance = World.GetTestWorld()->GetGameInstance();
        ULKProfileSubsystem *Profile = Instance->GetSubsystem<ULKProfileSubsystem>();
        Profile->ConfigureStorage(S.Base);
        Profile->EnsureProfile();
        ULKRunSubsystem *Run = Instance->GetSubsystem<ULKRunSubsystem>();
        Run->ConfigureStorage(S.Base + TEXT("_Run"));
        TestTrue(TEXT("Formal departure"), Run->StartNewRun(S.Request()));
        if (PhaseCase > 0)
        {
            Run->SelectNode(TEXT("World_R0_L1_N0"));
        }
        if (PhaseCase == 2)
        {
            Run->ResolveServiceNode(TEXT("LeaveMarket"));
            Run->SelectNode(TEXT("World_R0_L2_N0"));
        }
        const FLKRunState Before = Run->GetRunState();
        World.GetTestWorld()->GetWorldSettings()->DefaultGameMode =
            LoadClass<AGameModeBase>(nullptr, TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
        if (!World.BeginPlayInTestWorld())
        {
            World.ForwardErrorMessages(this);
            return false;
        }
        ALKBattleGameMode *GM = World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (!TestNotNull(TEXT("Authored battle game mode"), GM))
        {
            return false;
        }
        TestTrue(TEXT("Map/service arrival uses junction world"), GM->IsRecoveryJunction());
        TestEqual(TEXT("Arrival never silently picks first battle"), Run->GetRunState().CurrentNodeId,
                  Before.CurrentNodeId);
        TestEqual(TEXT("Service action remains pending"), Run->GetRunPhase(), Before.Phase);
        TestFalse(TEXT("Map cannot start a deployment battle"), GM->CanStartBattle());
        TestTrue(TEXT("Map and services allow safe return home"), GM->ReturnToHome());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKWorldMigrationTest, "LittleKing.Optimization2.OldMapMigrationAndDepartureFailure", Flags)
bool FLKWorldMigrationTest::RunTest(const FString& Parameters)
{
    FScope S;
    S.Profile->AddGold(300);
    S.Run->ConfigureStorage(S.Base + TEXT("?:/Unwritable"));
    TestFalse(TEXT("Unwritable initial run rejects departure"), S.Run->StartNewRun(S.Request(50)));
    TestFalse(TEXT("Failed departure publishes no new run"), S.Run->HasRun());
    TestEqual(TEXT("Failed initial save refunds carry gold"), S.Profile->GetGold(), 300);
    TestFalse(TEXT("Failed departure clears its funding reservation"), S.Profile->GetProfile().FundedRunId.IsValid());
    S.Run->ConfigureStorage(S.Base + TEXT("_Run"));
    FLKExpeditionStartRequest Legacy = S.Request();
    Legacy.bUseWorldMap = false;
    TestTrue(TEXT("Create former route for migration"), S.Run->StartNewRun(Legacy));
    FLKBattleContext Context;
    TestTrue(TEXT("Old route battle begins"), S.Run->BeginCurrentBattle(Context));
    TestTrue(TEXT("Old route earns pending gold"), S.Run->SubmitBattleOutcome(Outcome(Context)));
    ULKRunSaveGame* Old = NewObject<ULKRunSaveGame>();
    Old->RunState = S.Run->GetRunState();
    Old->RunState.SchemaVersion = 5;
    Old->RunState.WalletGold = 0;
    UGameplayStatics::SaveGameToSlot(Old, S.Base + TEXT("_Crash"), 0);
    TestTrue(TEXT("Schema 5 active route loads"), S.Run->LoadExpeditionFromSlot(S.Base + TEXT("_Crash"), true));
    TestEqual(TEXT("Active legacy earnings become wallet balance"), S.Run->GetWalletGold(), Old->RunState.PendingGold);
    TestEqual(TEXT("Legacy network preserved"), Network(S.Run->GetRunState()), Network(Old->RunState));
    TestEqual(TEXT("Migration does not generate a world map"), S.Run->GetRunState().WorldMapVersion, 0);
    Old->RunState.Phase = ELKRunPhase::Failed;
    Old->RunState.PendingSettlement.SettlementId = FGuid::NewGuid();
    Old->RunState.PendingSettlement.RunId = Old->RunState.RunId;
    Old->RunState.PendingSettlement.ProfileId = Old->RunState.ProfileId;
    Old->RunState.PendingSettlement.TerminalPhase = ELKRunPhase::Failed;
    Old->RunState.PendingSettlement.GoldAmount = 5; // 曾按旧 50% 规则冻结的 10 金币收益。
    UGameplayStatics::SaveGameToSlot(Old, S.Base + TEXT("_Crash"), 0);
    TestTrue(TEXT("Frozen legacy settlement loads"), S.Run->LoadExpeditionFromSlot(S.Base + TEXT("_Crash"), true));
    TestEqual(TEXT("Frozen receipt amount is not repriced"), S.Run->GetPendingSettlement().GoldAmount, 5);
    return true;
}
#endif
