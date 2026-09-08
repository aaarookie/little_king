#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../LKRunTypes.h"
#include "../LKUnitContent.h"
#include "../ULKBattleHUDWidget.h"
#include "../ULKRunSubsystem.h"

namespace
{
constexpr EAutomationTestFlags D1Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

TArray<FLKRunHeroState> MakeHeroes()
{
    TArray<FLKRunHeroState> Heroes;
    for (const TPair<FName, float>& Entry : {
        TPair<FName, float>("Hero_Knight", 800.f),
        TPair<FName, float>("Hero_Mage", 450.f),
        TPair<FName, float>("Hero_Ranger", 500.f) })
    {
        FLKRunHeroState Hero;
        Hero.HeroId = Entry.Key;
        Hero.Health = Hero.MaxHealth = Entry.Value;
        Heroes.Add(Hero);
    }
    return Heroes;
}

TArray<FLKRunCardState> MakeCards()
{
    TArray<FLKRunCardState> Cards;
    for (FName Id : { FName("Unit_Swordsman"), FName("Unit_Archer"), FName("Unit_Shieldbearer"),
        FName("Spell_Fireball"), FName("Spell_HealWave"), FName("Building_ArrowTower"), FName("Building_Barracks") })
    {
        FLKRunCardState Card;
        Card.CardId = Id;
        Cards.Add(Card);
    }
    return Cards;
}

FLKBattleOutcome MakeOutcome(const FLKBattleContext& Context, ELKTeam Winner, float HealthFraction)
{
    FLKBattleOutcome Outcome;
    Outcome.RunId = Context.RunId;
    Outcome.NodeId = Context.NodeId;
    Outcome.AttemptId = Context.AttemptId;
    Outcome.bFinalized = true;
    Outcome.Stats.Winner = Winner;
    for (const FLKRunHeroState& Before : Context.PlayerHeroes)
    {
        FLKHeroBattleOutcome Hero;
        Hero.InstanceId = Before.HeroId;
        Hero.HealthBeforeRecovery = Before.MaxHealth * FMath::Max(0.f, HealthFraction - 0.4f);
        Hero.bWasIncapacitated = FMath::IsNearlyEqual(HealthFraction, 0.4f);
        Hero.RecoveredState = Before;
        Hero.RecoveredState.Health = Before.MaxHealth * HealthFraction;
        Outcome.PlayerHeroes.Add(Hero);
    }
    return Outcome;
}

UTextBlock* FindButtonLabel(UWidget* Widget)
{
    if (UTextBlock* Text = Cast<UTextBlock>(Widget)) { return Text; }
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
    {
        for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
        {
            if (UTextBlock* Text = FindButtonLabel(Panel->GetChildAt(Index))) { return Text; }
        }
    }
    return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD1RunFlowTest, "LittleKing.D1.Run.ThreeRoomFlow", D1Flags)
bool FLKD1RunFlowTest::RunTest(const FString& Parameters)
{
    UGameInstance* Instance = NewObject<UGameInstance>();
    ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
    const TArray<FLKRunHeroState> FreshHeroes = MakeHeroes();
    const TArray<FLKRunCardState> FreshCards = MakeCards();
    TestTrue(TEXT("Run starts with a valid unique party and deck"), Run->StartNewRun(FreshHeroes, FreshCards, 31415));
    TestFalse(TEXT("An active run cannot be overwritten"), Run->StartNewRun(FreshHeroes, FreshCards, 9));
    const FGuid FirstRunId = Run->GetRunState().RunId;

    FLKBattleContext Room1;
    TestTrue(TEXT("Room one can begin"), Run->BeginCurrentBattle(Room1));
    TestTrue(TEXT("Room one context has run identity"), Room1.bExpedition && Room1.RunId == FirstRunId && Room1.AttemptId.IsValid());
    TestEqual(TEXT("Room one encounter"), Room1.EncounterId, FName("Enc_Dyn_R1"));
    TestEqual(TEXT("Room one index"), Run->GetCurrentRoomIndex(), 1);
    TestEqual(TEXT("The run always has four battle slots"), Run->GetTotalRoomCount(), 4);

    FLKBattleContext Reentered;
    TestTrue(TEXT("Reload before settlement reuses pending battle"), Run->BeginCurrentBattle(Reentered));
    TestTrue(TEXT("Reload does not create a second attempt"), Reentered.AttemptId == Room1.AttemptId);

    FLKBattleOutcome BadIdentity = MakeOutcome(Room1, ELKTeam::Player, 0.6f);
    BadIdentity.AttemptId = FGuid::NewGuid();
    TestFalse(TEXT("Foreign attempt is rejected"), Run->SubmitBattleOutcome(BadIdentity));
    FLKBattleOutcome MissingHero = MakeOutcome(Room1, ELKTeam::Player, 0.6f);
    MissingHero.PlayerHeroes.Pop();
    TestFalse(TEXT("Incomplete hero snapshot is rejected"), Run->SubmitBattleOutcome(MissingHero));

    const FLKBattleOutcome Room1Win = MakeOutcome(Room1, ELKTeam::Player, 0.6f);
    TestTrue(TEXT("Room one win is accepted"), Run->SubmitBattleOutcome(Room1Win));
    TestFalse(TEXT("One attempt settles at most once"), Run->SubmitBattleOutcome(Room1Win));
    TestEqual(TEXT("Win before boss offers next room"), Run->GetRunPhase(), ELKRunPhase::ChoosingNode);
    TestEqual(TEXT("History records one battle"), Run->GetRunState().BattleHistory.Num(), 1);

    TestTrue(TEXT("Advance picks the first battle node of the next row"), Run->AdvanceToNextBattle());
    FLKBattleContext Room2;
    TestTrue(TEXT("Room two can begin"), Run->BeginCurrentBattle(Room2));
    TestEqual(TEXT("Room two encounter"), Room2.EncounterId, FName("Enc_Dyn_R2"));
    TestTrue(TEXT("Room two has a new attempt"), Room2.AttemptId != Room1.AttemptId);
    TestTrue(TEXT("Room two seed is deterministic and different"), Room2.Seed != Room1.Seed);
    TestTrue(TEXT("Recovered hero health crosses rooms"), FMath::IsNearlyEqual(Room2.PlayerHeroes[0].Health, Room2.PlayerHeroes[0].MaxHealth * 0.6f));

    TestTrue(TEXT("A defeat is accepted"), Run->SubmitBattleOutcome(MakeOutcome(Room2, ELKTeam::Enemy, 0.4f)));
    TestEqual(TEXT("Any defeat terminates the run"), Run->GetRunPhase(), ELKRunPhase::Failed);
    TestFalse(TEXT("A failed run cannot advance"), Run->AdvanceToNextBattle());

    TestTrue(TEXT("From-start creates a fresh run"), Run->RestartRun(FreshHeroes, FreshCards, 31415));
    TestTrue(TEXT("Restart changes run identity"), Run->GetRunState().RunId != FirstRunId);
    TestEqual(TEXT("Restart clears history"), Run->GetRunState().BattleHistory.Num(), 0);
    TestEqual(TEXT("Restart returns to room one"), Run->GetCurrentRoomIndex(), 1);
    TestEqual(TEXT("Restart restores full hero health"), Run->GetRunState().Heroes[0].Health, Run->GetRunState().Heroes[0].MaxHealth);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD1UnitRulesTest, "LittleKing.D1.Configuration.BuiltInUnitRules", D1Flags)
bool FLKD1UnitRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("All thirteen shipped combat units are registered"), LKUnitContent::Units().Num(), 13);
    for (const TPair<FName, FLKUnitRow>& Pair : LKUnitContent::Units())
    {
        const FLKUnitRow& Canonical = Pair.Value;
        FLKUnitRow Authored = Canonical;
        Authored.UnitId = "Wrong_Id";
        Authored.DisplayName = FText::FromString(TEXT("作者名称"));
        Authored.BaseHealth = 1234.f;
        Authored.MoveSpeed = 432.f;
        Authored.AttackRange = 987.f;
        Authored.AttackDamage = 76.f;
        Authored.AttackInterval = 2.5f;
        Authored.AttackWindup = 0.25f;
        Authored.SkillCooldown = 11.f;
        Authored.SpawnInterval = 12.f;
        Authored.SpriteScale = FVector2D(2.f, 3.f);
        Authored.AttackType = Canonical.AttackType == ELKAttackType::Melee ? ELKAttackType::Ranged : ELKAttackType::Melee;
        Authored.UnitClass = Canonical.UnitClass == ELKUnitClass::Building ? ELKUnitClass::Soldier : ELKUnitClass::Building;
        Authored.ProjectileId = "Wrong_Projectile";
        Authored.bSkeleton = !Canonical.bSkeleton;
        Authored.PassiveAbility = ELKPassiveAbility::BoneRegeneration;
        Authored.PassiveHealPercent = 0.9f;
        Authored.RevivalInitialThreshold = 99;
        Authored.RevivalThresholdStep = 77;
        Authored.PlaceholderColor = FLinearColor::Blue;
        Authored.HeroTraits = { "Wrong_Trait" };
        Authored.bIsMage = !Canonical.bIsMage;
        Authored.BuildingBehavior = ELKBuildingBehavior::Barracks;
        Authored.SpawnUnitId = "Wrong_Spawn";

        const FLKUnitRow Merged = LKUnitContent::MergeAuthoredTuning(Canonical, &Authored);
        TestEqual(TEXT("Stable ID comes from code"), Merged.UnitId, Canonical.UnitId);
        TestEqual(TEXT("Class comes from code"), Merged.UnitClass, Canonical.UnitClass);
        TestEqual(TEXT("Attack type comes from code"), Merged.AttackType, Canonical.AttackType);
        TestEqual(TEXT("Passive comes from code"), Merged.PassiveAbility, Canonical.PassiveAbility);
        TestTrue(TEXT("Species flag comes from code"), Merged.bSkeleton == Canonical.bSkeleton);
        TestTrue(TEXT("Traits come from code"), Merged.HeroTraits == Canonical.HeroTraits);
        TestEqual(TEXT("Building behavior comes from code"), Merged.BuildingBehavior, Canonical.BuildingBehavior);
        TestEqual(TEXT("Building spawn type comes from code"), Merged.SpawnUnitId, Canonical.SpawnUnitId);
        TestTrue(TEXT("Placeholder identity color comes from code"), Merged.PlaceholderColor == Canonical.PlaceholderColor);
        TestEqual(TEXT("Authored health remains tunable"), Merged.BaseHealth, 1234.f);
        TestEqual(TEXT("Authored attack remains tunable"), Merged.AttackDamage, 76.f);
        TestEqual(TEXT("Authored range remains tunable"), Merged.AttackRange, 987.f);
        TestEqual(TEXT("Authored cooldown remains tunable"), Merged.SkillCooldown, 11.f);
        TestEqual(TEXT("Authored spawn interval remains tunable"), Merged.SpawnInterval, 12.f);
        TestEqual(TEXT("Authored display name remains tunable"), Merged.DisplayName.ToString(), FString(TEXT("作者名称")));
        TestTrue(TEXT("Authored sprite scale remains tunable"), Merged.SpriteScale == FVector2D(2.f, 3.f));
    }
    TestNull(TEXT("Custom IDs remain fully data driven"), LKUnitContent::Find("Unit_Custom"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD1CompletionTest, "LittleKing.D1.Run.BossCompletion", D1Flags)
bool FLKD1CompletionTest::RunTest(const FString& Parameters)
{
    UGameInstance* Instance = NewObject<UGameInstance>();
    ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
    if (!TestTrue(TEXT("Run starts"), Run->StartNewRun(MakeHeroes(), MakeCards(), 2718))) { return false; }

    const TArray<FName> ExpectedEncounters = {
        "Enc_Dyn_R1", "Enc_Dyn_R2", "Enc_Dyn_R3", "Enc_Dyn_Boss" };
    for (int32 Room = 0; Room < ExpectedEncounters.Num(); ++Room)
    {
        FLKBattleContext Context;
        if (!TestTrue(TEXT("Battle context is available"), Run->BeginCurrentBattle(Context))) { return false; }
        TestEqual(TEXT("Dynamic route encounter matches battle slot"), Context.EncounterId, ExpectedEncounters[Room]);
        TestEqual(TEXT("Room index increments"), Run->GetCurrentRoomIndex(), Room + 1);
        if (!TestTrue(TEXT("Win is committed"), Run->SubmitBattleOutcome(MakeOutcome(Context, ELKTeam::Player, 1.f)))) { return false; }
        if (Room < ExpectedEncounters.Num() - 1)
        {
            TestEqual(TEXT("Earlier wins await node selection"), Run->GetRunPhase(), ELKRunPhase::ChoosingNode);
            TestTrue(TEXT("Next battle is prepared"), Run->AdvanceToNextBattle());
        }
    }
    TestEqual(TEXT("Boss victory completes the run"), Run->GetRunPhase(), ELKRunPhase::Completed);
    TestTrue(TEXT("Completed run is terminal"), Run->IsTerminal());
    TestFalse(TEXT("Boss result never exposes another row"), Run->CanSelectNextNode());
    TestEqual(TEXT("History contains exactly four battles"), Run->GetRunState().BattleHistory.Num(), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD1AuthoredIntegrationTest, "LittleKing.D1.Integration.AuthoredGameModeAndResultButton", D1Flags)
bool FLKD1AuthoredIntegrationTest::RunTest(const FString& Parameters)
{
    // D5：真实 GM 会在安全点自动写固定存档槽；测试前清理，避免读到其它测试/残留恢复状态。
    UGameplayStatics::DeleteGameInSlot(ULKRunSubsystem::GetRunSlotName(), 0);
    FTestWorldWrapper World;
    if (!World.CreateTestWorld(EWorldType::Game)) { World.ForwardErrorMessages(this); return false; }
    TSubclassOf<ALKBattleGameMode> AuthoredMode = LoadClass<ALKBattleGameMode>(
        nullptr, TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
    if (!TestTrue(TEXT("Authored battle GameMode loads"), bool(AuthoredMode))) { return false; }
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AuthoredMode;
    if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }

    ALKBattleGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
    if (!TestNotNull(TEXT("Authored GameMode starts"), GM)) { return false; }
    World.GetTestWorld()->SpawnActor<ALKPlayerController>();
    GM->Tick(0.f);
    // 恢复语义不应触发（无存档时自动开新远征）。
    TestFalse(TEXT("No recovery flow without a save"), GM->IsRecoveryJunction() || GM->IsRecoveryTerminal());
    TestTrue(TEXT("Authored game automatically starts a D1 expedition"), GM->IsExpeditionBattle());
    TestEqual(TEXT("First world is room one"), GM->GetExpeditionRoomIndex(), 1);
    // D4：房一敌方为动态阵容（普通房从英雄池随机 1 名；具体由固定种子决定）。
    const TArray<FName> EnemyIds = GM->GetEnemyHeroIds();
    TestEqual(TEXT("Normal room fields exactly one random enemy hero"), EnemyIds.Num(), 1);
    TestTrue(TEXT("Random hero comes from the enemy hero pool"), EnemyIds.Num() == 1
        && (EnemyIds[0] == "Hero_Necromancer" || EnemyIds[0] == "Hero_SkeletonGiant"));
    const FLKUnitRow* Shield = GM->GetUnitRow("Unit_Shieldbearer");
    const FLKUnitRow* Tower = GM->GetUnitRow("Building_ArrowTower");
    const FLKUnitRow* Barracks = GM->GetUnitRow("Building_Barracks");
    TestTrue(TEXT("Old shieldbearer keeps code-owned taunt"), Shield && Shield->HeroTraits.Contains("Taunt"));
    TestTrue(TEXT("Old arrow tower keeps turret behavior"), Tower && Tower->BuildingBehavior == ELKBuildingBehavior::Turret);
    TestTrue(TEXT("Old barracks keeps soldier production identity"), Barracks
        && Barracks->BuildingBehavior == ELKBuildingBehavior::Barracks && Barracks->SpawnUnitId == "Unit_Swordsman");

    const TArray<FVector> Camps = { FVector(-700.f, -1300.f, 0.f), FVector(0.f, -1300.f, 0.f), FVector(700.f, -1300.f, 0.f) };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        TestEqual(TEXT("Every expedition hero deploys"), GM->DeployHero(ELKTeam::Player,
            Index == 0 ? FName("Hero_Knight") : (Index == 1 ? FName("Hero_Mage") : FName("Hero_Ranger")), Camps[Index]), ELKPlayResult::Success);
    }
    GM->ForceStartBattle();
    TestEqual(TEXT("Expedition battle starts after all three heroes deploy"), GM->GetPhase(), ELKGamePhase::Battle);
    GM->ForceEndMatch(ELKTeam::Player);
    TestEqual(TEXT("First victory reaches result"), GM->GetPhase(), ELKGamePhase::Result);

    // D3：胜利先进入奖励窗口（ChoosingReward）——结果已提交、"下一关"被锁定，先选择/跳过奖励。
    ULKRunSubsystem* Run = World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    TestTrue(TEXT("Result is committed before HUD notification"), Run && Run->GetRunPhase() == ELKRunPhase::ChoosingReward);
    TestTrue(TEXT("First victory offers a reward batch"), Run && Run->HasPendingRewardChoice() && Run->GetPendingRewardCount() >= 2);
    TestFalse(TEXT("Reward window blocks direct advance"), GM->IsResultActionNext());
    TestFalse(TEXT("Result action is ignored while reward is pending"), GM->RequestResultAction());
    ULKBattleHUDWidget* HUD = GM->GetBattleHUDWidget();
    // NullRHI 没有 GameViewport，不会驱动 HUD 的 NativeConstruct/Tick；显式调用同一幂等恢复入口。
    if (HUD) { HUD->RefreshRewardPanel(); }
    TestTrue(TEXT("Native reward panel opens for the pending batch"), HUD && HUD->IsRewardPanelOpen());
    TestTrue(TEXT("HUD skip consumes the batch and returns to choosing node"), HUD && HUD->SkipRunReward());
    TestTrue(TEXT("Reward panel closes after a successful skip"), HUD && !HUD->IsRewardPanelOpen());
    // D4：奖励消费后进入节点选择——下一排可见（2 个），结算按钮被面板取代。
    TestTrue(TEXT("Victory exposes the next row after reward"), GM->IsResultActionNext());
    TestEqual(TEXT("Next row contains two selectable nodes"), GM->GetNextNodeCount(), 2);
    TestEqual(TEXT("Victory button label announces route choice"), GM->GetResultActionLabel().ToString(), FString(TEXT("选择路线")));
    if (HUD) { HUD->RefreshNodeSelectPanel(); }
    TestTrue(TEXT("Native node select panel opens with next row"), HUD && HUD->IsNodeSelectPanelOpen());
    TestFalse(TEXT("Result action refuses while a row is selectable"), GM->RequestResultAction());

    if (TestNotNull(TEXT("Authored HUD is created"), HUD))
    {
        UButton* Button = Cast<UButton>(HUD->GetWidgetFromName(TEXT("Btn_Restart")));
        if (TestNotNull(TEXT("Existing result button is found"), Button))
        {
            TestTrue(TEXT("Existing result button is rebound to native D1 action"), Button->OnClicked.IsBound());
            TestNotNull(TEXT("Existing result button contains a writable text label"), FindButtonLabel(Button));
        }
    }
    // 清理本测试自动写下的固定槽，避免影响后续测试与真实 PIE。
    UGameplayStatics::DeleteGameInSlot(ULKRunSubsystem::GetRunSlotName(), 0);
    return true;
}

#endif
