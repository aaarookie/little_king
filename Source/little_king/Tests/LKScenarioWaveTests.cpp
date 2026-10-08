#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "../ALKBattleGameMode.h"
#include "../ALKOpponentBrain.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../LKEncounterContent.h"
#include "../LKSkeletonCircle.h"
#include "../ULKCardDefinition.h"
#include "../ULKDeckState.h"
#include "../ULKGameData.h"
#include "../ULKRunSaveGame.h"
#include "../ULKRunSubsystem.h"
#include "../ULKSilverComponent.h"

namespace
{
constexpr EAutomationTestFlags ScenarioFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FScenarioWorld
{
    FTestWorldWrapper Env;
    UWorld* World = nullptr;
    ALKBattleGameMode* GM = nullptr;
    ALKOpponentBrain* Brain = nullptr;
    bool Open()
    {
        if (!Env.CreateTestWorld(EWorldType::Game)) { return false; }
        World = Env.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
        auto* Run = World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
        Run->ConfigureStorage(TEXT("LittleKing_ScenarioWaves_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Run->SetAutoSaveEnabled(false);
        if (!Env.BeginPlayInTestWorld()) { return false; }
        GM = World->GetAuthGameMode<ALKBattleGameMode>();
        World->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        for (int32 I = 0; I < GM->AvailableHeroes.Num(); ++I)
        {
            if (GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[I], FVector((I-1)*700.f,-1300.f,0.f))
                != ELKPlayResult::Success) { return false; }
        }
        GM->ForceStartBattle();
        GM->GetGameData()->bDrawDebugShapes = false;
        for (TActorIterator<ALKOpponentBrain> It(World); It; ++It) { Brain = *It; break; }
        if (!Brain) { return false; }
        Brain->SetScriptedWaves({});
        return GM->GetPhase() == ELKGamePhase::Battle;
    }
    ALKUnitBase* Hero(ELKTeam Team)
    {
        for (TActorIterator<ALKUnitBase> It(World); It; ++It)
        { if (It->IsHero() && It->GetTeam() == Team) { return *It; } }
        return nullptr;
    }
};
FLKWaveEntry EventWave(ELKWaveTrigger Trigger, float Delay = 0.f)
{
    FLKWaveEntry Wave;
    Wave.UnitId = "Unit_Skeleton";
    Wave.Trigger = Trigger; Wave.TriggerTargetTag = "authored-hero";
    Wave.Time = Delay; Wave.Count = 1;
    Wave.bUseSpawnLocation = true;
    Wave.SpawnLocation = FVector(850.f,100.f,0.f); Wave.SpawnRadius = 150.f;
    return Wave;
}
FLKEncounterRow SimpleEncounter()
{
    FLKEncounterRow E = *LKEncounterContent::FindBuiltIn("Patrol");
    E.Waves.Reset(); E.EnemySpell.bEnabled = false;
    E.bEnemyUsesCards = true; E.EnemyCards = {"Unit_Skeleton"};
    E.AI.bEnableFocus = E.AI.bEnableCounter = E.AI.bEnablePush = false;
    E.EnemySilverPerSecond = 2.f; E.EnemySilverCap = 20.f; E.EnemyStartingSilver = 10.f;
    return E;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKScenarioEnemyDeckTest,"LittleKing.Scenario.EnemyDeckIsolation",ScenarioFlags)
bool FLKScenarioEnemyDeckTest::RunTest(const FString& Parameters)
{
    ULKDeckState* Deck = NewObject<ULKDeckState>();
    TestFalse(TEXT("Player single-card deck remains invalid"), Deck->InitDeck({"Unit_Skeleton"},1,7));
    TestTrue(TEXT("Enemy may use one card"), Deck->InitEnemyDeck({"Unit_Skeleton"},4,7));
    TestEqual(TEXT("One enemy hand slot"),Deck->GetHandSize(),1);
    TestEqual(TEXT("Single card cycles immediately"),Deck->PlayCard(0),FName("Unit_Skeleton"));
    TestEqual(TEXT("No empty slot or duplicated card"),Deck->GetAllCards().Num(),1);
    TestEqual(TEXT("Single cooldown card can be cast"),Deck->PlayCard(0,2.f),FName("Unit_Skeleton"));
    TestFalse(TEXT("Cooldown blocks immediate recast"),Deck->CanPlayWithCooldown(0,2.f));
    Deck->AdvanceCooldowns(2.f);
    TestTrue(TEXT("Single card becomes available again"),Deck->CanPlayWithCooldown(0,2.f));
    TestTrue(TEXT("Two-card enemy deck accepted"),Deck->InitEnemyDeck({"Unit_Skeleton","Unit_SkeletonArcher"},4,7));
    TestEqual(TEXT("Two-card deck has one active slot and one reserve"),Deck->GetHandSize(),1);
    FName Before = Deck->GetHandCard(0); Deck->PlayCard(0);
    TestTrue(TEXT("Two-card hand rotates"),Before != Deck->GetHandCard(0));
    TestTrue(TEXT("Player initialization resets enemy mode"),Deck->InitDeck({"a","b","c","d","e"},4,7));
    TestFalse(TEXT("Player cannot cool its only spare and a second card"),Deck->InitDeck({"a"},1,7));
    TestEqual(TEXT("Rejected initializer preserves player hand"),Deck->GetHandSize(),4);
    TestFalse(TEXT("Enemy duplicate cards rejected"),Deck->InitEnemyDeck({"a","a"},4,7));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKScenarioWaveEventsTest,"LittleKing.Scenario.EventLatchAndDelayedWaves",ScenarioFlags)
bool FLKScenarioWaveEventsTest::RunTest(const FString& Parameters)
{
    FScenarioWorld S; if (!TestTrue(TEXT("Battle fixture opens"),S.Open())) { return false; }
    ALKUnitBase* Hero = S.Hero(ELKTeam::Enemy); if (!TestNotNull(TEXT("Enemy hero"),Hero)) { return false; }
    Hero->Tags.Add("authored-hero");
    FLKWaveEntry Death = EventWave(ELKWaveTrigger::HeroFirstDeath,2.f);
    Death.SpawnLocation = FVector(-850.f,100.f,0.f);
    FLKWaveEntry Never = EventWave(ELKWaveTrigger::HeroFirstDeath); Never.TriggerTargetTag = "unused-hero";
    S.Brain->SetScriptedWaves({Death,EventWave(ELKWaveTrigger::HeroHealthBelow),Never});
    const int32 Initial = S.GM->CountAliveUnits(ELKTeam::Enemy);
    TestFalse(TEXT("Untriggered conditions never stall victory"),S.Brain->HasOutstandingScenarioWaves());
    S.Brain->NotifyUnitHealthEvent(Hero,50.f,100.f,1.f);
    S.Brain->ResolvePendingScenarioEvents(1.f);
    TestEqual(TEXT("Exactly 50 percent is not below"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial);
    S.Brain->NotifyUnitHealthEvent(Hero,49.f,100.f,2.f);
    S.Brain->NotifyUnitHealthEvent(Hero,100.f,100.f,2.f);
    TestTrue(TEXT("Transient low-health event stays latched after same-frame heal"),S.Brain->HasPendingScenarioReinforcements());
    S.Brain->ResolvePendingScenarioEvents(2.f);
    TestEqual(TEXT("Low-health reinforcement spawns once"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+1);
    S.Brain->NotifyUnitHealthEvent(Hero,10.f,100.f,3.f); S.Brain->ResolvePendingScenarioEvents(3.f);
    TestEqual(TEXT("Later threshold crossings do not retrigger"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+1);
    S.Brain->NotifyUnitFirstDeath(Hero,4.f); S.Brain->NotifyUnitFirstDeath(Hero,5.f);
    S.Brain->ResolvePendingScenarioEvents(5.f);
    TestEqual(TEXT("Death delay waits from FIRST event"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+1);
    TestTrue(TEXT("Latched delayed wave blocks premature victory"),S.Brain->HasOutstandingScenarioWaves());
    S.Brain->ResolvePendingScenarioEvents(6.f);
    TestEqual(TEXT("Death wave fires at first event plus delay"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+2);
    S.Brain->NotifyUnitFirstDeath(Hero,7.f); S.Brain->ResolvePendingScenarioEvents(10.f);
    TestEqual(TEXT("Resurrection and later death cannot duplicate wave"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+2);
    TestFalse(TEXT("Unmet unrelated condition does not leave pending work"),S.Brain->HasOutstandingScenarioWaves());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKScenarioWaveRetryTest,"LittleKing.Scenario.TimedPositionCapAndExpiry",ScenarioFlags)
bool FLKScenarioWaveRetryTest::RunTest(const FString& Parameters)
{
    FScenarioWorld S; if (!TestTrue(TEXT("Battle fixture opens"),S.Open())) { return false; }
    const int32 Initial = S.GM->CountAliveUnits(ELKTeam::Enemy);
    FLKWaveEntry Wave = EventWave(ELKWaveTrigger::Time,3.f); Wave.Count=2; Wave.SpawnRadius=0.f;
    S.GM->GetGameData()->MaxUnitsPerTeam=Initial+1;
    S.Brain->SetScriptedWaves({Wave});
    TestTrue(TEXT("Future time wave blocks custom-map clear"),S.Brain->HasOutstandingScenarioWaves());
    S.Brain->ResolvePendingScenarioEvents(2.f);
    TestEqual(TEXT("No early spawn"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial);
    S.Brain->ResolvePendingScenarioEvents(3.f);
    TestEqual(TEXT("Unit cap respected"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+1);
    bool bExact=false;
    for(TActorIterator<ALKUnitBase> It(S.World);It;++It)
    { if(It->GetTeam()==ELKTeam::Enemy && It->GetUnitId()=="Unit_Skeleton" && It->GetActorLocation().Equals(Wave.SpawnLocation,.1f)) { bExact=true; } }
    TestTrue(TEXT("Zero-radius wave uses exact authored point"),bExact);
    TestTrue(TEXT("Unspawned remainder retries"),S.Brain->HasOutstandingScenarioWaves());
    TestFalse(TEXT("Legacy hero victory need not wait for ordinary timed retry"),S.Brain->HasPendingScenarioReinforcements());
    S.Brain->ResolvePendingScenarioEvents(6.01f);
    TestEqual(TEXT("Remainder expires after three seconds"),S.Brain->GetDroppedReinforcements(),1);
    TestFalse(TEXT("Expired retry cannot leave battle stuck"),S.Brain->HasOutstandingScenarioWaves());
    S.Brain->ResetBattleState();
    TestFalse(TEXT("Reset removes all scheduled state"),S.Brain->HasOutstandingScenarioWaves());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKScenarioDualSpawnTest,"LittleKing.Scenario.FreeWavesAndPaidCards",ScenarioFlags)
bool FLKScenarioDualSpawnTest::RunTest(const FString& Parameters)
{
    FScenarioWorld S; if (!TestTrue(TEXT("Battle fixture opens"),S.Open())) { return false; }
    FLKEncounterRow E = SimpleEncounter(); E.Waves={EventWave(ELKWaveTrigger::Time)};
    if (!TestTrue(TEXT("One-card encounter config accepted"),S.Brain->ConfigureEncounter(E))) { return false; }
    const int32 Initial = S.GM->CountAliveUnits(ELKTeam::Enemy);
    const float Cost = S.GM->FindCard("Unit_Skeleton")->Cost;
    S.Brain->TickBrain(.1f,0.f);
    TestEqual(TEXT("Free event wave and paid deployment coexist"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+2);
    TestTrue(TEXT("Only card charged, silver regenerates"),FMath::IsNearlyEqual(S.Brain->GetSilver()->GetSilver(),10.2f-Cost));
    S.Brain->TickBrain(1.f,1.f);
    TestEqual(TEXT("Single-card enemy can deploy again"),S.GM->CountAliveUnits(ELKTeam::Enemy),Initial+3);
    E.EnemyCards={"Spell_SkeletonCircle"}; E.Waves.Reset(); E.EnemySpell.bEnabled=true;
    TestTrue(TEXT("Spell-only deck accepted"),S.Brain->ConfigureEncounter(E));
    TestFalse(TEXT("Legacy independent spell channel suppressed when card is in deck"),S.Brain->GetSpellSettings().bEnabled);
    S.Brain->TickBrain(.1f,0.f);
    TestEqual(TEXT("Normal card transaction registers circle"),S.GM->GetActiveTacticalCircleCount(),1);
    TestTrue(TEXT("Circle costs exactly five silver once"),FMath::IsNearlyEqual(S.Brain->GetSilver()->GetSilver(),5.2f));
    const float Before=S.Brain->GetSilver()->GetSilver(); S.Brain->TickBrain(1.f,1.f);
    TestEqual(TEXT("Active circle cap prevents duplicate field"),S.GM->GetActiveTacticalCircleCount(),1);
    TestTrue(TEXT("Rejected second circle does not spend"),FMath::IsNearlyEqual(S.Brain->GetSilver()->GetSilver(),Before+2.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKScenarioSnapshotTest,"LittleKing.Scenario.SnapshotAndValidation",ScenarioFlags)
bool FLKScenarioSnapshotTest::RunTest(const FString& Parameters)
{
    FLKEncounterRow E=SimpleEncounter(); E.Waves={EventWave(ELKWaveTrigger::HeroHealthBelow,3.f)};
    FLKEncounterRow Out; FString Error;
    TestTrue(TEXT("Conditional wave and small deck validate"),LKEncounterContent::NormalizeAndValidate(E.EncounterId,E,Out,Error));
    E.Waves[0].TriggerTargetTag=NAME_None;
    TestFalse(TEXT("Missing condition target rejected"),LKEncounterContent::NormalizeAndValidate(E.EncounterId,E,Out,Error));
    E.Waves[0].TriggerTargetTag="authored-hero"; E.Waves[0].HealthThreshold=1.1f;
    TestFalse(TEXT("Out-of-range HP threshold rejected"),LKEncounterContent::NormalizeAndValidate(E.EncounterId,E,Out,Error));
    E.Waves[0].HealthThreshold=.4f; E.Waves[0].SpawnRadius=-1.f;
    TestFalse(TEXT("Negative radius rejected"),LKEncounterContent::NormalizeAndValidate(E.EncounterId,E,Out,Error));
    E.Waves[0].SpawnRadius=123.f;
    ULKRunSaveGame* Save=NewObject<ULKRunSaveGame>();
    Save->RunState.PendingBattle.Encounter=E;
    TArray<uint8> Bytes; TestTrue(TEXT("Snapshot serializes without writing user saves"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
    ULKRunSaveGame* Read=Cast<ULKRunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Snapshot reloads"),Read)) { return false; }
    const FLKEncounterRow& Loaded=Read->RunState.PendingBattle.Encounter;
    TestEqual(TEXT("Wave count round trips"),Loaded.Waves.Num(),1);
    if (!Loaded.Waves.IsEmpty())
    {
        const auto& W=Loaded.Waves[0];
        TestEqual(TEXT("Trigger preserved"),W.Trigger,ELKWaveTrigger::HeroHealthBelow);
        TestEqual(TEXT("Target stable ID preserved"),W.TriggerTargetTag,FName("authored-hero"));
        TestEqual(TEXT("Event delay preserved"),W.Time,3.f);
        TestEqual(TEXT("Health threshold preserved"),W.HealthThreshold,.4f);
        TestTrue(TEXT("Exact spawn flag preserved"),W.bUseSpawnLocation);
        TestEqual(TEXT("Position preserved"),W.SpawnLocation,E.Waves[0].SpawnLocation);
        TestEqual(TEXT("Radius preserved"),W.SpawnRadius,123.f);
    }
    TestEqual(TEXT("Economy rate preserved"),Loaded.EnemySilverPerSecond,2.f);
    TestEqual(TEXT("Small enemy deck preserved"),Loaded.EnemyCards.Num(),1);
    E.Waves.Reset();
    TestTrue(TEXT("Cards-only battle can omit event waves"),LKEncounterContent::NormalizeAndValidate(E.EncounterId,E,Out,Error));
    return true;
}
#endif
