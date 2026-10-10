#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "../ALKBattleGameMode.h"
#include "../ALKBattleTerrain.h"
#include "../ALKOpponentBrain.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../LKAIObservation.h"
#include "../LKAIDecision.h"
#include "../LKEncounterContent.h"
#include "../LKSpellExecutor.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKDeckState.h"
#include "../ULKSilverComponent.h"
#include "../ULKUnitStatusComponent.h"
#include "../ULKUnitAttributeSet.h"

struct FLKOpponentTestAccess
{
    static bool Play(ALKOpponentBrain* B) { return B->TryPlayOneCard(); }
    static void Observe(ALKOpponentBrain* B) { B->RefreshObservation(); B->UpdateIntent(); }
    static void Focus(ALKOpponentBrain* B) { B->DoFocus(); }
    static void Resolve(ALKOpponentBrain* B) { B->ResolveFocus(); }
    static ALKUnitBase* Pending(ALKOpponentBrain* B) { return B->PendingFocusTarget.Get(); }
    static bool Tactical(ALKOpponentBrain* B) { return B->TryCastTacticalSpell(1.f); }
    static int32 RandomSeed(ALKOpponentBrain* B) { return B->DecisionRandom.GetCurrentSeed(); }
};

namespace
{
constexpr EAutomationTestFlags OpponentFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FOpponentWorld : FTestWorldWrapper
{
    ALKBattleGameMode* GM = nullptr;
    ALKOpponentBrain* Brain = nullptr;
    bool Open(FAutomationTestBase& Test, bool bTacticalEncounter=false)
    {
        if (!CreateTestWorld(EWorldType::Game)) { ForwardErrorMessages(&Test); return false; }
        GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
        if (!BeginPlayInTestWorld()) { ForwardErrorMessages(&Test); return false; }
        GM = GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (!GM) { return false; }
        GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes = GM->GetGameData()->bDrawFieldBounds = false;
        // Tactical casts also require the authoritative encounter gate in GameMode.
        // Updating only Brain after battle start cannot enable that independent gate.
        if (bTacticalEncounter && !GM->ConfigureEnemyEncounter("Patrol")) { return false; }
        for (int32 I=0; I<GM->AvailableHeroes.Num(); ++I)
        { if (GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700.f,-1300.f,0.f)) != ELKPlayResult::Success) { return false; } }
        GM->ForceStartBattle();
        for (TActorIterator<ALKOpponentBrain> It(GetTestWorld()); It; ++It) { Brain = *It; break; }
        if (!Brain) { return false; }
        Brain->SetScriptedWaves({});
        return GM->GetPhase() == ELKGamePhase::Battle;
    }
    bool Deck(FName Card)
    {
        FLKEncounterRow E = *LKEncounterContent::FindBuiltIn("Patrol");
        E.Waves.Reset(); E.EnemySpell.bEnabled = false; E.bEnemyUsesCards = true;
        E.EnemyCards = {Card}; E.EnemySilverPerSecond = 0.f; E.EnemySilverCap = 30.f; E.EnemyStartingSilver = 30.f;
        E.AI.bEnableFocus = E.AI.bEnableCounter = E.AI.bEnablePush = false;
        return Brain->ConfigureEncounter(E);
    }
    ALKUnitBase* Spawn(FName Id, FVector P, ELKTeam Team=ELKTeam::Enemy) { return GM->SpawnUnitForTeam(Id,Team,P); }
    void HP(ALKUnitBase* U, float Value) { U->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(),Value); }
    void HidePlayers(bool bHide)
    {
        for (TActorIterator<ALKUnitBase> It(GetTestWorld()); It; ++It)
        { if (It->GetTeam()==ELKTeam::Player && It->IsTargetable())
            { if (bHide) { It->GetStatusComponent()->ApplyConcealment("AI_Test",100.f); }
              else { It->GetStatusComponent()->RemoveConcealment("AI_Test"); } } }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKOpponentHiddenObservation,"LittleKing.Opponent.FairObservationAndDecision",OpponentFlags)
bool FLKOpponentHiddenObservation::RunTest(const FString&)
{
    FOpponentWorld S; if (!TestTrue(TEXT("World opens"),S.Open(*this))) { return false; }
    auto* Hidden = S.Spawn("Unit_Swordsman",FVector(700,250,0),ELKTeam::Player);
    auto* Visible = S.Spawn("Unit_Archer",FVector(-500,250,0),ELKTeam::Player);
    if (!Hidden || !Visible || !S.Deck("Spell_Fireball")) { return false; }
    Hidden->GetStatusComponent()->ApplyConcealment("AI_Test",100.f);
    FLKOpponentTestAccess::Observe(S.Brain);
    const auto First = S.Brain->GetObservation();
    TestNull(TEXT("Concealed enemy absent"),First.FindEnemy(Hidden->GetFName()));
    TestNotNull(TEXT("Discovered enemy included"),First.FindEnemy(Visible->GetFName()));
    const int32 RNG = FLKOpponentTestAccess::RandomSeed(S.Brain);
    TArray<FLKAICandidate> A,B;
    LKAIDecision::Candidates(*S.GM->FindCard("Spell_Fireball"),nullptr,First,30.f,ELKAIIntent::Pressure,0,A);
    Hidden->SetActorLocation(FVector(-700,-300,0)); S.HP(Hidden,1.f);
    FLKOpponentTestAccess::Observe(S.Brain);
    const auto Second = S.Brain->GetObservation();
    LKAIDecision::Candidates(*S.GM->FindCard("Spell_Fireball"),nullptr,Second,30.f,ELKAIIntent::Pressure,0,B);
    TestEqual(TEXT("Hidden real changes do not change observed cardinality"),Second.Enemies.Num(),First.Enemies.Num());
    TestEqual(TEXT("Pure planning consumes no decision RNG"),FLKOpponentTestAccess::RandomSeed(S.Brain),RNG);
    TestEqual(TEXT("Same observation produces same candidate count"),A.Num(),B.Num());
    for (int32 I=0; I<FMath::Min(A.Num(),B.Num()); ++I)
    { TestEqual(TEXT("Same action location"),A[I].Location,B[I].Location); TestEqual(TEXT("Same action utility"),A[I].Utility.Total(),B[I].Utility.Total()); }
    // Observation caches cannot justify an action after concealment changes.
    S.HidePlayers(true);
    const float Before = S.Brain->GetSilver()->GetSilver();
    TestFalse(TEXT("Execution refresh rejects now-hidden damage targets"),FLKOpponentTestAccess::Play(S.Brain));
    TestEqual(TEXT("Cancelled plan spends nothing"),S.Brain->GetSilver()->GetSilver(),Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKOpponentBlessing,"LittleKing.Opponent.BlessingUtilityAndOnceOnlyPayment",OpponentFlags)
bool FLKOpponentBlessing::RunTest(const FString&)
{
    FOpponentWorld S; if (!S.Open(*this) || !S.Deck("Spell_DivineBlessing")) { return false; }
    auto* Soldier = S.Spawn("Unit_Colossus",FVector(650,200,0));
    if (!TestNotNull(TEXT("High-health ally exists"),Soldier)) { return false; }
    const auto* Card = S.GM->FindCard("Spell_DivineBlessing");
    TestEqual(TEXT("Blessing has zero radius"),Card->SpellRadius,0.f);
    const float Before = S.Brain->GetSilver()->GetSilver();
    TestFalse(TEXT("Do not spend blessing on full-health army"),FLKOpponentTestAccess::Play(S.Brain));
    S.HP(Soldier,1.f);
    TestTrue(TEXT("Zero-radius global blessing has useful valid candidate"),FLKOpponentTestAccess::Play(S.Brain));
    TestEqual(TEXT("Soldier fully restored"),Soldier->GetHealth(),Soldier->GetMaxHealth());
    TestEqual(TEXT("Exactly one payment"),S.Brain->GetSilver()->GetSilver(),Before-Card->Cost);
    S.HP(Soldier,1.f);
    TestFalse(TEXT("Cooldown blocks immediate repeated decision"),FLKOpponentTestAccess::Play(S.Brain));
    TestEqual(TEXT("No repeated payment"),S.Brain->GetSilver()->GetSilver(),Before-Card->Cost);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKOpponentSummons,"LittleKing.Opponent.ReinforcementsRespectWholeGroupCapacity",OpponentFlags)
bool FLKOpponentSummons::RunTest(const FString&)
{
    FOpponentWorld S; if (!S.Open(*this) || !S.Deck("Spell_MariaNovice")) { return false; }
    const int32 Count = S.GM->CountAliveUnits(ELKTeam::Enemy);
    S.GM->GetGameData()->MaxUnitsPerTeam = Count+2;
    const float Before = S.Brain->GetSilver()->GetSilver();
    TestFalse(TEXT("Three summons cannot use two remaining slots"),FLKOpponentTestAccess::Play(S.Brain));
    TestEqual(TEXT("No partial summons"),S.GM->CountAliveUnits(ELKTeam::Enemy),Count);
    TestEqual(TEXT("Capacity rejection is free"),S.Brain->GetSilver()->GetSilver(),Before);
    S.GM->GetGameData()->MaxUnitsPerTeam = Count+3;
    TestTrue(TEXT("Exactly enough capacity succeeds"),FLKOpponentTestAccess::Play(S.Brain));
    TestEqual(TEXT("Entire group spawned"),S.GM->CountAliveUnits(ELKTeam::Enemy),Count+3);
    TestEqual(TEXT("Single payment for whole group"),S.Brain->GetSilver()->GetSilver(),Before-S.GM->FindCard("Spell_MariaNovice")->Cost);
    TestFalse(TEXT("Next attempt respects full cap"),FLKOpponentTestAccess::Play(S.Brain));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKOpponentControlUtility,"LittleKing.Opponent.CloudAndDirectionalHurricane",OpponentFlags)
bool FLKOpponentControlUtility::RunTest(const FString&)
{
    FOpponentWorld S; if (!S.Open(*this)) { return false; }
    auto* Friend = S.Spawn("Unit_Colossus",FVector(650,250,0));
    auto* Enemy = S.Spawn("Unit_Swordsman",FVector(650,-250,0),ELKTeam::Player);
    if (!Friend || !Enemy) { return false; }
    S.HP(Friend,10.f);
    // Isolate the geometry: +Y moves the injured friend away from the visible enemy,
    // and the enemy is outside the 350cm cloud/rectangle half depth.
    FLKAIObservation O = LKAIObservation::Capture(S.GM,ELKTeam::Enemy,0,1);
    O.Allies.RemoveAll([&](const auto& U){return U.Id!=Friend->GetFName();});
    O.Enemies.RemoveAll([&](const auto& U){return U.Id!=Enemy->GetFName();});
    for (FName Id : {FName("Spell_BlackCloud"),FName("Spell_Hurricane")})
    {
        const auto* Card = S.GM->FindCard(Id);
        const auto Score = LKAIDecision::Score(*Card,nullptr,O,Friend->GetActorLocation(),30.f,ELKAIIntent::Protect);
        TestTrue(TEXT("Protective placement has positive utility"),Score.Total()>.1f);
        TestEqual(TEXT("Candidate uses legal common executor placement"),LKSpellExecutor::Validate(S.GM,Card,ELKTeam::Enemy,Friend->GetActorLocation()),ELKPlayResult::Success);
        FLKAIObservation Empty = O; Empty.Allies.Reset(); Empty.Enemies.Reset();
        TestTrue(TEXT("Empty control area never has positive utility"),LKAIDecision::Score(*Card,nullptr,Empty,FVector::ZeroVector,30.f,ELKAIIntent::Protect).Total()<=0.f);
    }
    // Real paid-card transactions, with the same effect-specific planner.
    if (!S.Deck("Spell_Hurricane")) { return false; }
    TestTrue(TEXT("Hurricane is actually played for a threatened injured ally"),FLKOpponentTestAccess::Play(S.Brain));
    if (!S.Deck("Spell_BlackCloud")) { return false; }
    TestTrue(TEXT("Cloud is actually played for threatened injured ally"),FLKOpponentTestAccess::Play(S.Brain));
    TestTrue(TEXT("Cloud execution conceals ally"),Friend->GetStatusComponent()->IsConcealed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKOpponentFocusCancellation,"LittleKing.Opponent.FocusCancellationAndMemberBudget",OpponentFlags)
bool FLKOpponentFocusCancellation::RunTest(const FString&)
{
    FOpponentWorld S; if (!S.Open(*this)) { return false; }
    for (int32 I=0; I<5; ++I) { if (!S.Spawn("Unit_Swordsman",FVector(-800+I*250,250,0))) { return false; } }
    FLKOpponentTestAccess::Focus(S.Brain);
    auto* Target = FLKOpponentTestAccess::Pending(S.Brain);
    if (!TestNotNull(TEXT("Focus has discovered target"),Target)) { return false; }
    Target->GetStatusComponent()->ApplyConcealment("AI_Test",100.f);
    FLKOpponentTestAccess::Resolve(S.Brain);
    TestFalse(TEXT("Hidden target warning cancelled"),Target->IsUnderFocusWarning());
    TestTrue(TEXT("Cancellation is explicit in trace"),S.Brain->GetDecisionHistory().Last().Result.StartsWith(TEXT("FocusCancelled")));
    for (TActorIterator<ALKUnitBase> It(S.GetTestWorld()); It; ++It)
    { if (It->GetTeam()==ELKTeam::Enemy) { TestFalse(TEXT("No member receives cancelled order"),It->HasForcedTarget()); } }
    Target->GetStatusComponent()->RemoveConcealment("AI_Test");
    FLKOpponentTestAccess::Focus(S.Brain); FLKOpponentTestAccess::Resolve(S.Brain);
    int32 Focused=0;
    for (TActorIterator<ALKUnitBase> It(S.GetTestWorld()); It; ++It)
    { if (It->GetTeam()==ELKTeam::Enemy && It->HasForcedTarget()) { ++Focused; } }
    TestTrue(TEXT("Enemy intent has a bounded non-empty squad"),Focused>0 && Focused<=3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKOpponentTacticalFairness,"LittleKing.Opponent.TacticalCircleCannotAimAtHiddenArmy",OpponentFlags)
bool FLKOpponentTacticalFairness::RunTest(const FString&)
{
    FOpponentWorld S; if (!S.Open(*this,true)) { return false; }
    FLKEncounterRow E=S.GM->GetCurrentEncounter(); E.Waves.Reset(); E.bEnemyUsesCards=false;
    if (!TestTrue(TEXT("Authoritative encounter enables tactical channel"),E.EnemySpell.bEnabled)) { return false; }
    E.EnemyStartingSilver=10.f; E.EnemySpell.FirstCastTime=0.f; E.EnemySpell.StopTime=180.f;
    if (!S.Brain->ConfigureEncounter(E)) { return false; }
    S.HidePlayers(true);
    TestFalse(TEXT("No circle targeted with no discovered enemy"),FLKOpponentTestAccess::Tactical(S.Brain));
    TestEqual(TEXT("Hidden target rejection costs nothing"),S.Brain->GetSilver()->GetSilver(),10.f);
    S.HidePlayers(false);
    TestTrue(TEXT("Discovered army permits tactical circle"),FLKOpponentTestAccess::Tactical(S.Brain));
    TestEqual(TEXT("One circle and one payment"),S.Brain->GetSilver()->GetSilver(),10.f-S.GM->FindCard(E.EnemySpell.SpellId)->Cost);
    TestFalse(TEXT("Cooldown and active cap prevent duplicate cast"),FLKOpponentTestAccess::Tactical(S.Brain));
    TestEqual(TEXT("Only one successful tactical cast"),S.Brain->GetTacticalSpellCasts(),1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKOpponentPlacementRotation,"LittleKing.Opponent.BlockedCandidatesDoNotStarveLaterPlacements",OpponentFlags)
bool FLKOpponentPlacementRotation::RunTest(const FString&)
{
    FOpponentWorld S; if (!S.Open(*this) || !S.Deck("Unit_Skeleton")) { return false; }
    FLKOpponentTestAccess::Observe(S.Brain);
    const auto* Card = S.GM->FindCard("Unit_Skeleton");
    TArray<FLKAICandidate> Candidates;
    LKAIDecision::Candidates(*Card,S.GM->GetUnitRow(Card->SpawnUnitId),S.Brain->GetObservation(),
        S.Brain->GetSilver()->GetSilver(),S.Brain->GetIntent(),0,Candidates);
    if (!TestTrue(TEXT("More candidates than the per-card validation budget"),Candidates.Num()>2)) { return false; }
    for (int32 I=0; I<2; ++I)
    {
        auto* Obstacle = S.GetTestWorld()->SpawnActor<ALKBattleTerrain>();
        if (!TestTrue(TEXT("First two ranked candidates blocked by terrain"),Obstacle && Obstacle->Initialize(
            ELKBattleTerrainKind::Rock,Candidates[I].Location,FVector2D(100,100)))) { return false; }
        TestNotEqual(TEXT("Blocked point is rejected by real placement validation"),
            S.GM->ValidateCardPlay(ELKTeam::Enemy,0,Candidates[I].Location),ELKPlayResult::Success);
    }
    bool HasLaterLegalPoint=false;
    for (int32 I=2; I<Candidates.Num(); ++I)
    { HasLaterLegalPoint |= S.GM->ValidateCardPlay(ELKTeam::Enemy,0,Candidates[I].Location)==ELKPlayResult::Success; }
    if (!TestTrue(TEXT("Later candidate is legal"),HasLaterLegalPoint)) { return false; }
    const float Before = S.Brain->GetSilver()->GetSilver();
    const int32 UnitsBefore = S.GM->CountAliveUnits(ELKTeam::Enemy);
    TestFalse(TEXT("First decision respects two checks despite blocked points"),FLKOpponentTestAccess::Play(S.Brain));
    TestEqual(TEXT("Failed candidates cost nothing"),S.Brain->GetSilver()->GetSilver(),Before);
    bool Played=false;
    for (int32 I=0; I<Candidates.Num()/2 && !Played; ++I)
    { Played=FLKOpponentTestAccess::Play(S.Brain); }
    TestTrue(TEXT("Bounded subsequent decisions reach a legal later candidate"),Played);
    TestEqual(TEXT("Rotation executes exactly one spawn"),S.GM->CountAliveUnits(ELKTeam::Enemy),UnitsBefore+1);
    TestEqual(TEXT("Rotation executes exactly one payment"),S.Brain->GetSilver()->GetSilver(),Before-Card->Cost);
    for (const auto& Trace : S.Brain->GetDecisionHistory())
    { TestTrue(TEXT("Each single-card decision stays within two checks and global budget"),Trace.ValidationChecks<=2 && Trace.ValidationChecks<=LKAIDecision::MaxValidationChecks); }
    return true;
}
#endif
