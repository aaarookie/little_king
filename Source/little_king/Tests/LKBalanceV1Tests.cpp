#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitHero.h"
#include "../LKBalanceRules.h"
#include "../LKSkeletonCircle.h"
#include "../LKCardRules.h"
#include "../LKEncounterContent.h"
#include "../LKWorldMapContent.h"
#include "../LKHomeContent.h"
#include "../ULKGameData.h"
#include "../ULKDeckState.h"
#include "../ULKSilverComponent.h"

struct FLKBalanceTestAccess
{
    static bool Encounter(ALKBattleGameMode* GM, const FLKEncounterRow& Row) { return GM->ApplyEnemyEncounter(Row); }
    static void SetConfig(ALKBattleGameMode* GM, ULKGameData* Data) { GM->GameData=Data; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceRulesTest, "LittleKing.BalanceV1.RulesAndAuthoredAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKBalanceRulesTest::RunTest(const FString& Parameters)
{
    const auto& Regions = LKWorldMapContent::Regions();
    const int32 Depths[] = {0,1,1,2,3};
    for (int32 I = 0; I < Regions.Num(); ++I)
    { TestEqual(TEXT("Geographic branch depth"), LKBalanceRules::ComputeRegionDepth(Regions, Regions[I].RegionId), Depths[I]); }
    TestEqual(TEXT("Initial HP scale"), LKBalanceRules::EnemyHealthScaleForProgress(0.f), 1.f);
    TestEqual(TEXT("Final HP scale"), LKBalanceRules::EnemyHealthScaleForProgress(1.f), 2.f);
    TestEqual(TEXT("Final attack scale"), LKBalanceRules::EnemyDamageScaleForProgress(1.f), 1.5f);
    float LastHP=0.f, LastDamage=0.f;
    for (int32 D=0; D<=19; ++D)
    {
        float HP, Damage; LKBalanceRules::ScalesForDepth(D, HP, Damage);
        TestTrue(TEXT("Both curves rise monotonically"), HP>LastHP && Damage>LastDamage);
        LastHP=HP; LastDamage=Damage;
    }
    for (FName Id : LKCardRules::EnemyOnlyCardIds()) { TestFalse(TEXT("Enemy cards cannot be acquired"), LKCardRules::IsPlayerObtainable(Id)); }
    ULKDeckState* Deck=NewObject<ULKDeckState>();
    Deck->CardAllowed=[](FName Id) { return LKCardRules::IsPlayerObtainable(Id); };
    TestTrue(TEXT("Legal player deck initializes"),Deck->InitDeck(LKCardRules::ReplacementCardIds(),4,12));
    const TArray<FName> Before=Deck->GetAllCards();
    for (FName Id : LKCardRules::EnemyOnlyCardIds())
    {
        TestFalse(TEXT("Enemy card cannot be added"),Deck->AddCardToDeck(Id));
        TestFalse(TEXT("Enemy card cannot be transformed into hand"),Deck->TransformCard(Before[0],Id));
        TArray<FName> Bad=Before; Bad[0]=Id;
        TestFalse(TEXT("Enemy card cannot initialize player deck"),Deck->InitDeck(Bad,4,12));
        TestTrue(TEXT("Rejected operations preserve hand and queue"),Before==Deck->GetAllCards());
    }
    ULKGameData* Data = LoadObject<ULKGameData>(nullptr,TEXT("/Game/Data/DA_GameData.DA_GameData"));
    if (!TestNotNull(TEXT("Authored configuration"),Data)) { return false; }
    TestEqual(TEXT("Authored overtime starts at 300 seconds"), Data->BattleTimeLimit, 300.f);
    UDataTable* Units=Data->UnitTable.LoadSynchronous();
    if (!TestNotNull(TEXT("Authored unit table"), Units)) { return false; }
    for (const auto& Pair : TMap<FName,float>{{"Hero_Knight",6000.f},{"Hero_Mage",4200.f},{"Hero_Ranger",4800.f},
        {"Hero_Necromancer",18000.f},{"Hero_SkeletonGiant",10000.f},{"Boss_SkeletonKing",8000.f},
        {"Unit_Skeleton",240.f},{"Unit_SkeletonArcher",160.f}})
    {
        const FLKUnitRow* Row=Units->FindRow<FLKUnitRow>(Pair.Key,TEXT("Balance test"),false);
        if (TestNotNull(*Pair.Key.ToString(),Row)) { TestEqual(TEXT("Authored HP matches balance"),Row->BaseHealth,Pair.Value); }
    }
    TArray<FLKEncounterRow> Catalog; FString Error;
    TestTrue(TEXT("Authored encounters validate"),LKEncounterContent::BuildCatalog(Data->EncounterTable.LoadSynchronous(),Catalog,Error));
    for (const auto& Row:Catalog)
    {
        TestTrue(TEXT("Dense finite wave schedule"),Row.Waves.Num()>=46 && Row.Waves.Num()<=62);
        TestTrue(TEXT("Tactical circle enabled"),Row.EnemySpell.bEnabled);
        TestEqual(TEXT("Independent channel starts empty"),Row.EnemyStartingSilver,0.f);
        FLKEncounterRow Invalid=Row, Out; Invalid.EnemyHeroHealthScale.Reset();
        TestFalse(TEXT("Corrupt current snapshot is not silently repaired"),LKEncounterContent::NormalizeAndValidate(Invalid.EncounterId,Invalid,Out,Error));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceTacticalTransactionTest,"LittleKing.BalanceV1.TacticalTransaction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLKBalanceTacticalTransactionTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Env;
    if (!Env.CreateTestWorld(EWorldType::Game)) { return false; }
    Env.GetTestWorld()->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
    if (!Env.BeginPlayInTestWorld()) { return false; }
    ALKBattleGameMode* GM=Env.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
    Env.GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
    if (!TestTrue(TEXT("Configure undead encounter"),GM->ConfigureEnemyEncounter("Patrol"))) { return false; }
    for (int32 I=0; I<GM->AvailableHeroes.Num(); ++I)
    { GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700.f,-1300.f,0)); }
    const FVector Target(0,-1177,0);
    TestNull(TEXT("No tactical cast during deployment"),GM->CastEnemyTacticalSpell("Spell_SkeletonCircle",Target));
    GM->ForceStartBattle();
    ULKSilverComponent* Silver=GM->GetTeamSilver(ELKTeam::Enemy);
    Silver->AddSilver(4.f);
    TestNull(TEXT("Insufficient balance cannot cast"),GM->CastEnemyTacticalSpell("Spell_SkeletonCircle",Target));
    TestEqual(TEXT("Failed cast keeps money"),Silver->GetSilver(),4.f);
    Silver->AddSilver(1.f);
    TestNull(TEXT("Empty target cannot spend"),GM->CastEnemyTacticalSpell("Spell_SkeletonCircle",FVector(1000,0,0)));
    TestEqual(TEXT("Empty cast keeps money"),Silver->GetSilver(),5.f);
    ALKSkeletonCircle* Circle=GM->CastEnemyTacticalSpell("Spell_SkeletonCircle",Target);
    if (!TestNotNull(TEXT("Enemy can cast on the player's half without a mage"),Circle)) { return false; }
    TestEqual(TEXT("Exactly five silver spent"),Silver->GetSilver(),0.f);
    Silver->AddSilver(5.f);
    TestNull(TEXT("One active circle cap"),GM->CastEnemyTacticalSpell("Spell_SkeletonCircle",Target));
    TestEqual(TEXT("Active cap keeps money"),Silver->GetSilver(),5.f);
    GM->ForceEndMatch(ELKTeam::Player);
    TestEqual(TEXT("Result cleans tracked circles"),GM->GetActiveTacticalCircleCount(),0);
    TestTrue(TEXT("Actor receives end cleanup"),Circle->IsFinished());
    return true;
}

// Explicit performance test: actual actors, collision, projectiles, skills, waves and economy.
// Deterministic basic policy, not a claim to model expert play or a substitute for player feedback.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceBattleSamplingTest,"LittleKing.BalanceV1.BattleSampling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)
bool FLKBalanceBattleSamplingTest::RunTest(const FString& Parameters)
{
    ULKGameData* Authored=LoadObject<ULKGameData>(nullptr,TEXT("/Game/Data/DA_GameData.DA_GameData"));
    if (!TestNotNull(TEXT("Data for actual battle sampling"),Authored)) { return false; }
    // Independent seeds plus a wounded, level-three treasury cohort. Never used for tuning.
    const bool bHoldout=FParse::Param(FCommandLine::Get(),TEXT("BalanceV1Holdout"));
    FString CSV=TEXT("depth,rank,deck,seed,duration,winner,overtime,cards_player,circles,damage_player,damage_enemy,kills_player,kills_enemy,max_enemy_units,treasury_level,initial_health_ratio,healing_player,healing_enemy\n");
    int32 Sample=0;
    for (int32 Cohort=0; Cohort<(bHoldout?2:1); ++Cohort)
    for (int32 Depth : {0,9,19}) for (FName EncounterId : {FName("Patrol"),FName("Elite"),FName("Boss")}) for (int32 DeckIndex : {0,1})
    {
        if (Cohort==1 && Depth==0) { continue; }
        const int32 TreasuryLevel=Cohort==1?3:1;
        const float InitialHealthRatio=Cohort==1?.4f:1.f;
        FTestWorldWrapper Env;
        if (!Env.CreateTestWorld(EWorldType::Game)) { Env.ForwardErrorMessages(this); return false; }
        Env.GetTestWorld()->GetWorldSettings()->DefaultGameMode=ALKBattleGameMode::StaticClass();
        ULKGameData* Config=DuplicateObject<ULKGameData>(Authored,GetTransientPackage());
        Config->bEnableExpeditionFlow=false;
        Config->BattleSeed=(bHoldout?99001:41001)+Sample*97;
        Config->SilverPerSecond=Authored->SilverPerSecond*LKHomeContent::TreasurySpeedMultiplier(TreasuryLevel);
        Config->SilverCap=Authored->SilverCap+LKHomeContent::TreasuryCapBonus(TreasuryLevel);
        Config->EnemyEncounterId=EncounterId;
        Config->bDrawDebugShapes=false; Config->bDrawFieldBounds=false;
        Config->DefaultPlayerDeck=LKCardRules::ReplacementCardIds();
        if (DeckIndex==1) { Config->DefaultPlayerDeck={"Unit_ElfWarrior","Unit_ElfArcher","Unit_ElfPriest","Unit_GoblinRogue","Unit_ApprenticeMage","Spell_Fireball","Spell_HealWave","Building_ArrowTower"}; }
        Env.GetTestWorld()->SetGameMode(FURL());
        FLKBalanceTestAccess::SetConfig(Env.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>(),Config);
        bool Begun=Env.BeginPlayInTestWorld();
        if (!Begun) { Env.ForwardErrorMessages(this); return false; }
        ALKBattleGameMode* GM=Env.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (!TestNotNull(TEXT("Sampling GameMode"),GM)) { return false; }
        Env.GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        FLKEncounterRow Encounter=GM->GetCurrentEncounter();
        LKBalanceRules::ApplySnapshot(Encounter,Depth);
        if (!TestTrue(TEXT("Apply progress snapshot"),FLKBalanceTestAccess::Encounter(GM,Encounter))) { return false; }
        for (int32 I=0; I<GM->AvailableHeroes.Num(); ++I)
        { TestEqual(TEXT("Deploy sample hero"),GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700.f,-650.f,0.f)),ELKPlayResult::Success); }
        if (InitialHealthRatio<1.f)
        {
            for (TActorIterator<ALKUnitBase> It(Env.GetTestWorld()); It; ++It)
            {
                if (!It->IsHero() || It->GetTeam()!=ELKTeam::Player) { continue; }
                FLKRunHeroState State; State.HeroId=It->GetUnitId(); State.MaxHealth=It->GetMaxHealth();
                State.BaseMaxHealth=It->GetBaseMaxHealth(); State.Health=State.MaxHealth*InitialHealthRatio; State.Traits=It->GetTraits();
                TestTrue(TEXT("Apply wounded entry snapshot"),It->ApplyRunHeroState(State));
            }
        }
        GM->ForceStartBattle();
        if (!TestEqual(TEXT("Sampling battle starts"),GM->GetPhase(),ELKGamePhase::Battle)) { return false; }
        int32 MaxEnemy=0; FRandomStream Policy(Config->BattleSeed+6001);
        constexpr float Step=.05f;
        for (int32 Frame=0; Frame<9000 && GM->GetPhase()==ELKGamePhase::Battle; ++Frame)
        {
            if (Frame%10==0)
            {
                ULKDeckState* Deck=GM->GetTeamDeck(ELKTeam::Player);
                for (int32 H=0; Deck && H<Deck->GetHandSize(); ++H)
                {
                    const ULKCardDefinition* Card=GM->FindCard(Deck->GetHandCard(H));
                    if (!Card || GM->GetTeamSilver(ELKTeam::Player)->GetSilver()<Card->Cost) { continue; }
                    FVector Point;
                    if (Card->CardType==ELKCardType::Spell)
                    {
                        ALKUnitBase* Target=nullptr;
                        for (TActorIterator<ALKUnitBase> It(Env.GetTestWorld()); It; ++It)
                        {
                            if (!It->IsTargetable() || It->IsCamp()) { continue; }
                            if (Card->SpellEffect==ELKSpellEffect::Heal)
                            { if (It->GetTeam()==ELKTeam::Player && It->IsHero() && It->GetHealth()<It->GetMaxHealth()*.9f && (!Target || It->GetHealth()/It->GetMaxHealth()<Target->GetHealth()/Target->GetMaxHealth())) { Target=*It; } }
                            else if (It->GetTeam()==ELKTeam::Enemy && (!Target || It->IsHero())) { Target=*It; }
                        }
                        if (!Target) { continue; } Point=Target->GetActorLocation();
                    }
                    else
                    {
                        if (!GM->FindFreeSpawnLocation(FVector(0,-450.f,0),600.f,Point,Policy,12) || Point.Y>-80.f) { continue; }
                    }
                    if (GM->ValidateCardPlay(ELKTeam::Player,H,Point)==ELKPlayResult::Success && GM->PlayCardForTeam(ELKTeam::Player,H,Point)==ELKPlayResult::Success) { break; }
                }
            }
            MaxEnemy=FMath::Max(MaxEnemy,GM->CountAliveUnits(ELKTeam::Enemy));
            Env.TickTestWorld(Step);
        }
        const FLKMatchStats S=GM->GetMatchStats();
        TestEqual(TEXT("Sample finishes within observation window"),GM->GetPhase(),ELKGamePhase::Result);
        TestTrue(TEXT("Enemy team respects population cap"),MaxEnemy<=GM->GetGameData()->MaxUnitsPerTeam);
        CSV+=FString::Printf(TEXT("%d,%s,%d,%d,%.2f,%d,%d,%d,%d,%.1f,%.1f,%d,%d,%d,%d,%.2f,%.1f,%.1f\n"),Depth,*EncounterId.ToString(),DeckIndex,S.Seed,S.Duration,int32(S.Winner),int32(S.bOvertime),S.Player.CardsPlayed,S.Enemy.CardsPlayed,S.Player.Damage,S.Enemy.Damage,S.Player.Kills,S.Enemy.Kills,MaxEnemy,TreasuryLevel,InitialHealthRatio,S.Player.Healing,S.Enemy.Healing);
        ++Sample;
    }
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("BalanceV1Validation");
    IFileManager::Get().MakeDirectory(*Dir,true);
    TestTrue(TEXT("Write reproducible measurements"),FFileHelper::SaveStringToFile(CSV,*(Dir/(bHoldout?TEXT("battle-holdout.csv"):TEXT("battle-samples.csv"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    return true;
}
#endif
