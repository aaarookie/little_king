#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "AbilitySystemComponent.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../ALKUnitHero.h"
#include "../ALKProjectile.h"
#include "../ULKUnitPassiveComponent.h"
#include "../ULKUnitAttributeSet.h"
#include "../ULKGameData.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameplayLibrary.h"
#include "../LKGameplayHelpers.h"
#include "../LKUndeadContent.h"

struct FLKD0TestAccess
{
    static void UseTable(ALKBattleGameMode* GM, UDataTable* Table)
    { GM->GetGameData()->UnitTable = Table; GM->UnitTableCached = Table; }
};

namespace
{
constexpr EAutomationTestFlags D0Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FD0World : FTestWorldWrapper
{
    ALKBattleGameMode* GM = nullptr;
    bool Open(FAutomationTestBase& Test, FName Encounter = NAME_None)
    {
        if (!CreateTestWorld(EWorldType::Game)) { ForwardErrorMessages(&Test); return false; }
        GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
        if (!BeginPlayInTestWorld()) { ForwardErrorMessages(&Test); return false; }
        GM = GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes = false; GM->GetGameData()->bDrawFieldBounds = false;
        if (!Encounter.IsNone() && !Test.TestTrue(TEXT("Configure undead encounter"), GM->ConfigureEnemyEncounter(Encounter))) { return false; }
        return true;
    }
    bool Start(FAutomationTestBase& Test)
    {
        for (int32 i = 0; i < 3; ++i)
        { if (!Test.TestEqual(TEXT("All three player heroes deploy"), GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[i], FVector((i - 1) * 700.f, -1300.f, 0.f)), ELKPlayResult::Success)) { return false; } }
        GM->ForceStartBattle(); return Test.TestEqual(TEXT("Battle starts"), GM->GetPhase(), ELKGamePhase::Battle);
    }
    ALKUnitBase* Hero(FName Id, ELKTeam Team = ELKTeam::Enemy)
    {
        for (TActorIterator<ALKUnitBase> It(GetTestWorld()); It; ++It) { if (It->GetUnitId() == Id && It->GetTeam() == Team) { return *It; } }
        return nullptr;
    }
    ALKUnitBase* Spawn(FName Id, ELKTeam Team = ELKTeam::Enemy, FVector Location = FVector(0.f, 300.f, 0.f))
    { return GM->SpawnUnitForTeam(Id, Team, Location); }
    void Health(ALKUnitBase* Unit, float Value)
    { Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), Value); }
    void Defeat(FName Id, int32 Count)
    {
        for (int32 i = 0; i < Count; ++i) { if (ALKUnitBase* Unit = Spawn(Id)) { Unit->Die(); } }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD0RecoveryTest, "LittleKing.D0.Heroes.IncapacitationAndRecovery", D0Flags)
bool FLKD0RecoveryTest::RunTest(const FString& Parameters)
{
    FD0World Env; if (!Env.Open(*this) || !Env.Start(*this)) { return false; }
    ALKUnitBase* Knight = Env.Hero("Hero_Knight", ELKTeam::Player);
    ALKUnitBase* Mage = Env.Hero("Hero_Mage", ELKTeam::Player);
    ALKUnitBase* Ranger = Env.Hero("Hero_Ranger", ELKTeam::Player);
    Env.Health(Knight, Knight->GetMaxHealth() * 0.2f);
    Env.Health(Ranger, Ranger->GetMaxHealth() * 0.9f);
    LKGameplay::ApplyDamage(Mage, 99999.f, nullptr);
    TestTrue(TEXT("Zero health hero becomes incapacitated"), Mage->IsIncapacitated());
    TestFalse(TEXT("Incapacitated hero cannot be targeted"), Mage->IsTargetable());
    TestEqual(TEXT("Normal healing cannot revive in combat"), LKGameplay::ApplyHeal(Mage, 500.f, Knight), 0.f);
    TestFalse(TEXT("Incapacitation removes spell territory trait"), Env.GM->HasGlobalSpellPlacement(ELKTeam::Player));
    TestEqual(TEXT("Hero has no destruction lifespan"), Mage->GetLifeSpan(), 0.f);
    Mage->Tick(1.f);
    TestTrue(TEXT("Hero survives past former death animation"), IsValid(Mage));
    Env.GM->ForceEndMatch(ELKTeam::Player);
    TestTrue(TEXT("Living hero adds forty percent"), FMath::IsNearlyEqual(Knight->GetHealth(), Knight->GetMaxHealth() * 0.6f, 0.01f));
    TestEqual(TEXT("Recovery clamps to maximum"), Ranger->GetHealth(), Ranger->GetMaxHealth());
    TestTrue(TEXT("Incapacitated hero returns at forty percent"), FMath::IsNearlyEqual(Mage->GetHealth(), Mage->GetMaxHealth() * 0.4f, 0.01f));
    TestTrue(TEXT("Hero recovered but does not resume combat"), Mage->IsAlive() && !Mage->IsCombatEnabled());
    const FLKBattleOutcome Result = Env.GM->GetBattleOutcome();
    TestTrue(TEXT("Outcome finalized with attempt identity"), Result.bFinalized && Result.AttemptId.IsValid());
    TestEqual(TEXT("All player hero snapshots present"), Result.PlayerHeroes.Num(), 3);
    const FLKHeroBattleOutcome* MageResult = Result.PlayerHeroes.FindByPredicate([](const FLKHeroBattleOutcome& H) { return H.RecoveredState.HeroId == "Hero_Mage"; });
    if (TestNotNull(TEXT("Mage snapshot"), MageResult)) { TestTrue(TEXT("Snapshot retains incapacitation and pre-recovery HP"), MageResult->bWasIncapacitated && MageResult->HealthBeforeRecovery == 0.f); }
    Env.GM->ForceEndMatch(ELKTeam::Enemy);
    TestEqual(TEXT("Repeated end never heals twice or changes winner"), Env.GM->GetBattleOutcome().Stats.Winner, ELKTeam::Player);
    TestEqual(TEXT("Post-battle recovery excluded from combat healing"), Env.GM->GetMatchStats().Player.Healing, 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD0NecroTest, "LittleKing.D0.Undead.SummoningAndSacrifice", D0Flags)
bool FLKD0NecroTest::RunTest(const FString& Parameters)
{
    FD0World Env; if (!Env.Open(*this) || !Env.Start(*this)) { return false; }
    ALKUnitBase* Necro = Env.Spawn("Hero_Necromancer", ELKTeam::Enemy, FVector(400.f, 0.f, 0.f));
    ALKUnitBase* Distant = Env.Spawn("Hero_Necromancer", ELKTeam::Enemy, FVector(900.f, 500.f, 0.f));
    if (!TestNotNull(TEXT("Necromancer fixture"), Necro) || !Distant) { return false; }
    const float Full = Necro->GetMaxHealth();
    for (FName VictimId : { FName("Unit_Swordsman"), FName("Unit_Archer") })
    {
        ALKUnitBase* Victim = Env.Spawn(VictimId, ELKTeam::Player, FVector(-100.f, -200.f, 0.f));
        const int32 Before = Env.GM->CountAliveUnits(ELKTeam::Enemy);
        LKGameplay::ApplyDamage(Victim, 10000.f, Necro);
        TestEqual(TEXT("Exactly one summon despite two necromancers"), Env.GM->CountAliveUnits(ELKTeam::Enemy), Before + 1);
        const FName Expected = VictimId == "Unit_Swordsman" ? FName("Unit_Skeleton") : FName("Unit_SkeletonArcher");
        ALKUnitBase* Summon = Env.Hero(Expected);
        if (TestNotNull(TEXT("Correct skeleton kind"), Summon))
        {
            TestEqual(TEXT("Conversion occurs at victim position"), Summon->GetActorLocation(), Victim->GetActorLocation());
            TestTrue(TEXT("Summon belongs to nearest necromancer"), Summon->GetOwner() == Necro);
        }
    }
    TestTrue(TEXT("Two summons cost sixteen percent of max HP"), FMath::IsNearlyEqual(Necro->GetHealth(), Full * 0.84f, 0.01f));
    TestEqual(TEXT("Other necromancer does not pay"), Distant->GetHealth(), Distant->GetMaxHealth());
    const float BeforeHero = Necro->GetHealth();
    Env.Hero("Hero_Mage", ELKTeam::Player)->Die();
    TestEqual(TEXT("Heroes do not trigger summoning"), Necro->GetHealth(), BeforeHero);
    Necro->RemoveTrait("Trait_Sacrifice");
    Env.Spawn("Unit_Swordsman", ELKTeam::Player, FVector(-200.f, -200.f, 0.f))->Die();
    TestEqual(TEXT("Removing trait removes cost but keeps passive"), Necro->GetHealth(), BeforeHero);
    Necro->AddTrait("Trait_Sacrifice");
    Env.GM->GetGameData()->MaxUnitsPerTeam = Env.GM->CountAliveUnits(ELKTeam::Enemy);
    Env.Spawn("Unit_Swordsman", ELKTeam::Player, FVector(-200.f, -200.f, 0.f))->Die();
    TestEqual(TEXT("Blocked summon does not charge sacrifice"), Necro->GetHealth(), BeforeHero);

    FD0World Last; if (!Last.Open(*this, "Patrol") || !Last.Start(*this)) { return false; }
    ALKUnitBase* LastNecro = Last.Hero("Hero_Necromancer");
    Last.Health(LastNecro, LastNecro->GetMaxHealth() * 0.04f);
    LastNecro->SetInvulnerable(-1.f);
    Last.Spawn("Unit_Swordsman", ELKTeam::Player, FVector(0.f, -300.f, 0.f))->Die();
    TestEqual(TEXT("Sacrifice can incapacitate last enemy hero despite invulnerability"), Last.GM->GetPhase(), ELKGamePhase::Result);
    TestEqual(TEXT("Sacrifice defeat awards player victory"), Last.GM->GetMatchStats().Winner, ELKTeam::Player);
    TestTrue(TEXT("Self cost is not credited as player's kill"), Last.GM->GetMatchStats().Player.Kills == 0);
    TestTrue(TEXT("Last necromancer receives post-battle recovery"), FMath::IsNearlyEqual(LastNecro->GetHealth(), LastNecro->GetMaxHealth() * 0.4f, 0.01f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD0GiantTest, "LittleKing.D0.Undead.GiantBones", D0Flags)
bool FLKD0GiantTest::RunTest(const FString& Parameters)
{
    FD0World Env; if (!Env.Open(*this) || !Env.Start(*this)) { return false; }
    ALKUnitBase* Giant = Env.Spawn("Hero_SkeletonGiant", ELKTeam::Enemy, FVector(500.f, 100.f, 0.f));
    Env.Health(Giant, 100.f);
    Env.Defeat("Unit_Skeleton", 1); Env.Defeat("Unit_SkeletonArcher", 1);
    TestTrue(TEXT("Both allied skeleton soldiers heal three percent each"), FMath::IsNearlyEqual(Giant->GetHealth(), 100.f + Giant->GetMaxHealth() * 0.06f, 0.01f));
    const float Before = Giant->GetHealth();
    Env.Defeat("Unit_Swordsman", 1);
    Env.Spawn("Unit_Skeleton", ELKTeam::Player, FVector(0.f, -300.f, 0.f))->Die();
    TestEqual(TEXT("Living species and hostile skeletons excluded"), Giant->GetHealth(), Before);
    Env.Health(Giant, Giant->GetMaxHealth() - 1.f); Env.Defeat("Unit_Skeleton", 1);
    TestEqual(TEXT("Giant healing does not overflow"), Giant->GetHealth(), Giant->GetMaxHealth());
    Giant->Die(); Env.Defeat("Unit_Skeleton", 1);
    TestTrue(TEXT("Giant passive cannot revive incapacitated giant"), Giant->IsIncapacitated());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD0KingTest, "LittleKing.D0.Undead.BoneRegenerationAndBatchOrder", D0Flags)
bool FLKD0KingTest::RunTest(const FString& Parameters)
{
    for (bool bKingFirst : { true, false })
    {
        FD0World Env; if (!Env.Open(*this, "Boss") || !Env.Start(*this)) { return false; }
        ALKUnitBase* King = Env.Hero("Boss_SkeletonKing");
        ULKUnitPassiveComponent* Passive = King->GetPassiveComponent();
        Env.Hero("Hero_Necromancer")->Die();
        TestEqual(TEXT("Necromancer is not a skeleton"), Passive->GetBoneCount(), 0);
        Env.Defeat("Hero_SkeletonGiant", 1); Env.Defeat("Unit_Skeleton", 4);
        TestEqual(TEXT("Skeleton hero counts five; soldier counts one"), Passive->GetBoneCount(), 9);
        ALKUnitBase* Skeleton = Env.Spawn("Unit_Skeleton");
        Env.GM->BeginCombatBatch();
        if (bKingFirst) { LKGameplay::ApplyDamage(King, 10000.f, Env.Hero("Hero_Knight", ELKTeam::Player)); }
        Skeleton->Die();
        if (!bKingFirst) { LKGameplay::ApplyDamage(King, 10000.f, Env.Hero("Hero_Knight", ELKTeam::Player)); }
        Env.GM->EndCombatBatch();
        TestEqual(TEXT("Same-batch revival precedes victory for both orders"), Env.GM->GetPhase(), ELKGamePhase::Battle);
        TestTrue(TEXT("King revived full and combat ready"), King->IsAlive() && King->IsCombatEnabled() && King->GetHealth() == King->GetMaxHealth());
        TestEqual(TEXT("Revived hero re-registered once"), Env.GM->GetHeroCount(ELKTeam::Enemy), 1);
        TestEqual(TEXT("Threshold rises to fifteen"), Passive->GetRevivalThreshold(), 15);
        TestEqual(TEXT("Points consumed"), Passive->GetBoneCount(), 0);
        TestEqual(TEXT("Revival does not erase original effective damage"), Env.GM->GetMatchStats().Player.Damage, King->GetMaxHealth());
        Env.Defeat("Unit_Skeleton", 17);
        TestEqual(TEXT("Counter capped at current threshold"), Passive->GetBoneCount(), 15);
        King->Die();
        TestEqual(TEXT("Second revival raises threshold to twenty"), Passive->GetRevivalThreshold(), 20);
        TestEqual(TEXT("Exactly two revivals"), Passive->GetRevivalCount(), 2);
        King->Die();
        TestEqual(TEXT("No charges ends battle"), Env.GM->GetPhase(), ELKGamePhase::Result);
        TestEqual(TEXT("King never counts own defeat"), Passive->GetBoneCount(), 0);
        TestTrue(TEXT("Defeated boss retained and recovered after match"), IsValid(King) && FMath::IsNearlyEqual(King->GetHealth(), King->GetMaxHealth() * 0.4f, 0.01f));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD0FearTest, "LittleKing.D0.Undead.RangedDamageClassification", D0Flags)
bool FLKD0FearTest::RunTest(const FString& Parameters)
{
    FD0World Env; if (!Env.Open(*this) || !Env.Start(*this)) { return false; }
    ALKUnitBase* King = Env.Spawn("Boss_SkeletonKing", ELKTeam::Enemy, FVector(0.f, 0.f, 0.f));
    ALKUnitBase* Archer = Env.Spawn("Unit_Archer", ELKTeam::Player, FVector(-400.f, 0.f, 0.f));
    ALKUnitBase* Tower = Env.Spawn("Building_ArrowTower", ELKTeam::Player, FVector(-800.f, 0.f, 0.f));
    ALKUnitBase* Knight = Env.Hero("Hero_Knight", ELKTeam::Player);
    for (const FLKCombatSource Source : {
        LKGameplay::MakeSource(Archer), LKGameplay::MakeSource(Tower),
        LKGameplay::MakeSource(Archer, ELKCombatSourceKind::Skill), LKGameplay::MakeSource(nullptr, ELKCombatSourceKind::Spell) })
    {
        Env.Health(King, King->GetMaxHealth());
        TestEqual(TEXT("Ranged attack/building/skill/spell takes seventy percent"), LKGameplay::ApplyDamage(King, 100.f, nullptr, false, &Source), 70.f);
    }
    Env.Health(King, King->GetMaxHealth());
    TestEqual(TEXT("Melee attack unaffected"), LKGameplay::ApplyDamage(King, 100.f, Knight), 100.f);
    const FLKCombatSource MeleeSkill = LKGameplay::MakeSource(Knight, ELKCombatSourceKind::Skill);
    TestEqual(TEXT("Melee skill unaffected"), LKGameplay::ApplyDamage(King, 100.f, Knight, false, &MeleeSkill), 100.f);
    const FLKCombatSource Overtime = LKGameplay::MakeSource(Archer, ELKCombatSourceKind::Overtime);
    TestEqual(TEXT("Overtime not reduced even with ranged instigator"), LKGameplay::ApplyDamage(King, 100.f, Archer, true, &Overtime), 100.f);
    Env.Health(King, King->GetMaxHealth());
    ALKProjectile* Projectile = Env.GM->AcquireProjectile(Archer->GetActorLocation(), 100.f, ELKTeam::Player, Archer, FVector(1.f, 0.f, 0.f));
    Archer->Die(); Archer->Destroy(); Projectile->Tick(1.f);
    TestEqual(TEXT("Projectile retains ranged classification after source destroyed"), King->GetHealth(), King->GetMaxHealth() - 70.f);
    King->RemoveTrait("Trait_FaceFear");
    TestEqual(TEXT("Removing fear trait removes reduction immediately"), LKGameplay::ApplyDamage(King, 100.f, Tower), 100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD0ContentTest, "LittleKing.D0.Content.RegistryAndEncounter", D0Flags)
bool FLKD0ContentTest::RunTest(const FString& Parameters)
{
    FD0World Env; if (!Env.Open(*this, "Elite")) { return false; }
    Env.GM->ForceStartBattle();
    TestEqual(TEXT("New enemy roster cannot bypass player's three-hero gate"), Env.GM->GetPhase(), ELKGamePhase::Deployment);
    TestEqual(TEXT("Elite has independent two-hero roster"), Env.GM->GetEnemyHeroIds().Num(), 2);
    if (!Env.Start(*this)) { return false; }
    TestFalse(TEXT("Cannot replace encounter during battle"), Env.GM->ConfigureEnemyEncounter("Boss"));
    Env.GM->Tick(0.1f);
    TestTrue(TEXT("Scripted encounter spawns skeleton wave"), Env.GM->CountAliveUnits(ELKTeam::Enemy) > 2);
    TestEqual(TEXT("Encounter does not secretly play normal cards"), Env.GM->GetMatchStats().Enemy.CardsPlayed, 0);
    const ULKGameData* Saved = LoadObject<ULKGameData>(nullptr, TEXT("/Game/Data/DA_GameData.DA_GameData"));
    if (!TestNotNull(TEXT("Saved data asset"), Saved)) { return false; }
    UDataTable* Table = Saved->UnitTable.LoadSynchronous();
    if (!TestNotNull(TEXT("Actual unit table"), Table)) { return false; }
    FLKD0TestAccess::UseTable(Env.GM, Table);
    for (const auto& Pair : LKUndeadContent::Units())
    {
        const FLKUnitRow* Row = Env.GM->GetUnitRow(Pair.Key);
        TestNotNull(TEXT("Built-in undead usable even when saved table has no new rows"), Row);
        // 原生 PresentationHUD 始终按 PlaceholderColor 绘制占位方框；数据表可保留作者用于调试的 Sprite。
        TestTrue(TEXT("New enemy has an opaque colored square"), Row && Row->PlaceholderColor.A > 0.f);
    }
    TestNull(TEXT("Unknown legacy ID still fails rather than silently substituting"), Env.GM->GetUnitRow("Missing_Unit"));
    for (FName Id : Table->GetRowNames())
    {
        const FLKUnitRow* Row = Table->FindRow<FLKUnitRow>(Id, TEXT("Catalog"));
        if (Row) { AddInfo(FString::Printf(TEXT("CatalogUnit %s HP=%.2f Damage=%.2f Range=%.2f Interval=%.2f Speed=%.2f Windup=%.2f SkillCooldown=%.2f SpawnInterval=%.2f"), *Id.ToString(), Row->BaseHealth, Row->AttackDamage, Row->AttackRange, Row->AttackInterval, Row->MoveSpeed, Row->AttackWindup, Row->SkillCooldown, Row->SpawnInterval)); }
    }
    for (const ULKCardDefinition* Card : Saved->CardLibrary)
    { if (Card) { AddInfo(FString::Printf(TEXT("CatalogCard %s Cost=%d SpellValue=%.2f Radius=%.2f Limit=%d"), *Card->CardId.ToString(), Card->Cost, Card->SpellValue, Card->SpellRadius, Card->BuildingTypeLimitOverride)); } }
    // 只读导出既有 GA 引脚，图鉴不使用旧教程里的推荐数值冒充实际资产值。
    for (const TCHAR* Name : { TEXT("GA_KnightHeal"), TEXT("GA_MageNova"), TEXT("GA_RangerShot") })
    {
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("/Game/blueprint/%s.%s"), Name, Name));
        if (!BP) { continue; }
        bool bNearestEnemyReceivesUnit = false;
        bool bHasDamageNode = false;
        bool bHasHealNode = false;
        for (UEdGraph* Graph : BP->UbergraphPages)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                bHasDamageNode |= NodeTitle.Contains(TEXT("LK_ApplyDamageInRadius"));
                bHasHealNode |= NodeTitle.Contains(TEXT("LK_ApplyHealInRadius"));
                for (const UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == "exec") { continue; }
                    FString Value = Pin->DefaultValue;
                    for (const UEdGraphPin* Link : Pin->LinkedTo) { Value += Link->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(); }
                    AddInfo(FString::Printf(TEXT("CatalogSkill %s [%s] %s=%s"), Name, *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Pin->PinName.ToString(), *Value));
                    if (NodeTitle.Contains(TEXT("LK_GetNearestEnemy")) && Pin->PinName == TEXT("Unit") && !Pin->LinkedTo.IsEmpty())
                    { bNearestEnemyReceivesUnit = true; }
                }
            }
        }
        if (FCString::Strcmp(Name, TEXT("GA_MageNova")) == 0)
        {
            TestTrue(TEXT("Mage target query receives its avatar"), bNearestEnemyReceivesUnit);
            TestTrue(TEXT("Mage nova uses damage"), bHasDamageNode);
        }
        else if (FCString::Strcmp(Name, TEXT("GA_RangerShot")) == 0)
        {
            TestTrue(TEXT("Ranger target query receives its avatar"), bNearestEnemyReceivesUnit);
            TestTrue(TEXT("Ranger shot uses damage"), bHasDamageNode);
            TestFalse(TEXT("Ranger shot no longer uses the old heal node"), bHasHealNode);
        }
    }
    return true;
}
#endif
