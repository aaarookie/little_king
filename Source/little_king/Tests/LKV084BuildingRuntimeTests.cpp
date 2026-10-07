#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "../ALKUnitBase.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKSpellField.h"
#include "../LKSpellExecutor.h"
#include "../LKSkeletonCircle.h"
#include "../ALKUnitBuilding.h"
#include "../ALKUnitHero.h"
#include "../ALKProjectile.h"
#include "../LKBuildingGeometry.h"
#include "../LKUnitContent.h"
#include "../LKGameplayHelpers.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKRunSubsystem.h"
#include "../ULKUnitAttributeSet.h"
#include "../ULKUnitActiveComponent.h"
#include "../ULKUnitStatusComponent.h"
#include "../ULKUnitMovementComponent.h"

struct FLKV084RuntimeTestAccess
{
    static void Attack(ALKUnitBase* Unit, ALKUnitBase* Target) { Unit->PerformAttack(Target); }
    static void SetHeroCooldown(ALKUnitHero* Hero, float Seconds) { Hero->SkillCooldownRemaining = Seconds; }
    static FVector ServiceApproach(const ALKUnitBase* Unit) { return Unit->ServiceApproach; }
};

namespace
{
constexpr EAutomationTestFlags V084Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FBuildingWorld
{
    FTestWorldWrapper Env;
    UWorld* World = nullptr;
    ULKGameData* Data = nullptr;
    bool Open(bool bBattle = false)
    {
        if (!Env.CreateTestWorld(EWorldType::Game)) { return false; }
        World = Env.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = bBattle ? ALKBattleGameMode::StaticClass() : AGameModeBase::StaticClass();
        auto* Run = World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
        Run->ConfigureStorage(TEXT("LittleKing_V084_Runtime_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Run->SetAutoSaveEnabled(false);
        if (!Env.BeginPlayInTestWorld()) { return false; }
        if (bBattle)
        {
            auto* GM = World->GetAuthGameMode<ALKBattleGameMode>();
            World->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
            for (int32 I = 0; I < GM->AvailableHeroes.Num(); ++I)
            { if (GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[I], FVector((I-1)*700.f, -1300.f, 0.f)) != ELKPlayResult::Success) { return false; } }
            GM->ForceStartBattle(); Data = GM->GetGameData();
        }
        else { Data = NewObject<ULKGameData>(World); }
        Data->bDrawDebugShapes = false;
        return true;
    }
    ALKUnitBase* Spawn(FLKUnitRow Row, FVector Position = FVector::ZeroVector, ELKTeam Team = ELKTeam::Player)
    {
        Row.Sprite.Reset(); Row.HeroTraits.Reset();
        UClass* Class = Row.UnitClass == ELKUnitClass::Building ? ALKUnitBuilding::StaticClass()
            : Row.UnitClass == ELKUnitClass::Hero ? ALKUnitHero::StaticClass() : ALKUnitBase::StaticClass();
        ALKUnitBase* Unit = World->SpawnActor<ALKUnitBase>(Class);
        Unit->SetActorLocation(Position);
        Unit->InitUnit(Row, Data); Unit->SetTeam(Team); Unit->SetCombatEnabled(true);
        Unit->SetActorTickEnabled(false);
        Unit->GetMovementComponent()->SetComponentTickEnabled(false);
        return Unit;
    }
    ALKUnitBase* Soldier(FVector Position, ELKTeam Team = ELKTeam::Player)
    {
        FLKUnitRow Row = *LKUnitContent::Find("Unit_Swordsman");
        Row.BaseHealth = 500.f; Row.AttackDamage = 40.f; Row.AttackRange = 160.f;
        Row.AttackWindup = 0.f; Row.AttackInterval = 1.f; Row.AcquireRadius = 1000.f;
        return Spawn(Row, Position, Team);
    }
    ALKUnitBuilding* Building(FName Id, FVector Position, bool bComplete = true, ELKTeam Team = ELKTeam::Player)
    {
        FLKUnitRow Row = *LKUnitContent::Find(Id);
        if (bComplete) { Row.ConstructionSeconds = 0.f; }
        return CastChecked<ALKUnitBuilding>(Spawn(Row, Position, Team));
    }
    static void Health(ALKUnitBase* Unit, float Value)
    { Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), Value); }
    void AdvanceClock(float Seconds)
    {
        // UWorld clamps oversized frame deltas; small real world steps also advance the reflection clock faithfully.
        while (Seconds > KINDA_SMALL_NUMBER)
        { const float Step = FMath::Min(.05f, Seconds); Env.TickTestWorld(Step); Seconds -= Step; }
    }
    void Projectiles(float Seconds)
    {
        TArray<ALKProjectile*> All;
        for (TActorIterator<ALKProjectile> It(World); It; ++It) { All.Add(*It); }
        for (ALKProjectile* Projectile : All) { if (IsValid(Projectile)) { Projectile->Tick(Seconds); } }
    }
    void Enter(ALKUnitBuilding* Building)
    {
        Building->Tick(.01f);
        ALKUnitBase* Occupant = Building->GetOccupant();
        if (!Occupant) { return; }
        FVector Approach;
        if (Building->FindServiceApproach(Occupant, Approach))
        { Occupant->SetActorLocation(Approach); Occupant->Tick(.01f); }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084Construction, "LittleKing.V084.Runtime.WallConstructionAndDisplacement", V084Flags)
bool FLKV084Construction::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    ALKUnitBuilding* Wall = F.Building("Unit_WallCommon", FVector::ZeroVector, false);
    Wall->SetWallYaw(45.f);
    ALKUnitBase* Inside = F.Soldier(FVector(0.f, 5.f, 0.f));
    TestFalse(TEXT("Construction is not an obstacle"), LKBuildingGeometry::BlocksMovement(Wall));
    Wall->Tick(9.f);
    TestTrue(TEXT("Progress follows authored ten seconds"), FMath::IsNearlyEqual(Wall->GetConstructionProgress(), .9f));
    Wall->SetCombatEnabled(false); Wall->Tick(20.f);
    TestFalse(TEXT("Stopped combat does not complete construction"), Wall->IsConstructionComplete());
    Wall->SetCombatEnabled(true); Wall->Tick(1.f);
    TestTrue(TEXT("Completion enables collision"), LKBuildingGeometry::BlocksMovement(Wall));
    TestTrue(TEXT("Completion displaces existing mobile bodies outside wall"), Inside->GetMovementComponent()->CanStandAt(Inside->GetActorLocation()));
    TestEqual(TEXT("Construction does not secretly heal or reduce starting HP"), Wall->GetHealth(), 1000.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084Reflect, "LittleKing.V084.Runtime.ReflectionPerSourceAndNoRecursion", V084Flags)
bool FLKV084Reflect::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    ALKUnitBuilding* Wall = F.Building("Unit_WallEpic", FVector::ZeroVector);
    ALKUnitBase* A = F.Soldier(FVector(0.f, 200.f, 0.f), ELKTeam::Enemy);
    ALKUnitBase* B = F.Soldier(FVector(200.f, 200.f, 0.f), ELKTeam::Enemy);
    LKGameplay::ApplyDamage(Wall, 1.f, A); LKGameplay::ApplyDamage(Wall, 1.f, A); LKGameplay::ApplyDamage(Wall, 1.f, B);
    TestEqual(TEXT("Same attacker reflects only once in half a second"), A->GetHealth(), 490.f);
    TestEqual(TEXT("Another attacker has independent cooldown"), B->GetHealth(), 490.f);
    F.AdvanceClock(.51f);
    LKGameplay::ApplyDamage(Wall, 1.f, A);
    TestEqual(TEXT("Cooldown expires in world combat time"), A->GetHealth(), 480.f);
    const FLKCombatSource Cost = LKGameplay::MakeSource(B, ELKCombatSourceKind::HealthCost, "Cost");
    LKGameplay::ApplyDamage(Wall, 1.f, B, true, &Cost);
    TestEqual(TEXT("Health costs never trigger reflection"), B->GetHealth(), 490.f);
    ALKUnitBuilding* OtherWall = F.Building("Unit_WallImperial", FVector(500.f, 0.f, 0.f), true, ELKTeam::Enemy);
    const float Before = Wall->GetHealth();
    LKGameplay::ApplyDamage(OtherWall, 1.f, Wall);
    TestEqual(TEXT("Legendary reflection deals fifty without a reflection loop"), Wall->GetHealth(), Before - 50.f);
    TestEqual(TEXT("Reflected hit never bounces back"), OtherWall->GetHealth(), 19999.f);
    FBuildingWorld::Health(Wall, 1.f);
    F.AdvanceClock(.51f);
    LKGameplay::ApplyDamage(Wall, 2.f, B);
    TestTrue(TEXT("Destructive attack kills the wall"), Wall->IsDead());
    TestEqual(TEXT("Lethal attack still receives one reflection"), B->GetHealth(), 480.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084WallAura, "LittleKing.V084.Runtime.ImperialFrontRearStackAndCleanup", V084Flags)
bool FLKV084WallAura::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    ALKUnitBuilding* A = F.Building("Unit_WallImperial", FVector::ZeroVector);
    ALKUnitBuilding* B = F.Building("Unit_WallImperial", FVector(0.f, -20.f, 0.f));
    ALKUnitBuilding* Rear = F.Building("Unit_WallImperial", FVector(0.f, 250.f, 0.f));
    A->SetWallYaw(0.f); B->SetWallYaw(0.f); Rear->SetWallYaw(0.f);
    ALKUnitBase* Ally = F.Soldier(FVector(0.f, 130.f, 0.f));
    FBuildingWorld::Health(Ally, 250.f);
    TestTrue(TEXT("Two front bonuses add to twenty percent"), FMath::IsNearlyEqual(Ally->GetAttackDamage(), 48.f));
    Rear->Tick(1.f);
    TestEqual(TEXT("A different wall behind can heal simultaneously"), Ally->GetHealth(), 255.f);
    ALKUnitBuilding* Rear2 = F.Building("Unit_WallImperial", FVector(0.f, 280.f, 0.f)); Rear2->Tick(1.f);
    TestEqual(TEXT("Each overlapping rear aura contributes one percent"), Ally->GetHealth(), 260.f);
    A->Die();
    TestTrue(TEXT("Destroyed wall attack bonus disappears immediately"), FMath::IsNearlyEqual(Ally->GetAttackDamage(), 44.f));
    B->SetCombatEnabled(false);
    TestEqual(TEXT("Battle cleanup leaves no permanent attack modifier"), Ally->GetAttackDamage(), 40.f);
    Ally->SetActorLocation(FVector(400.f, 130.f, 0.f)); Rear->Tick(1.f);
    TestEqual(TEXT("Healing strip is bounded by wall length"), Ally->GetHealth(), 260.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084SiegeCart, "LittleKing.V084.Runtime.SiegeCartTargetsPercentDamageAndIdle", V084Flags)
bool FLKV084SiegeCart::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    ALKUnitBuilding* Cart = F.Building("Unit_SiegeCart", FVector::ZeroVector);
    ALKUnitBase* Enemy = F.Soldier(FVector(0.f, 150.f, 0.f), ELKTeam::Enemy);
    Cart->Tick(.01f);
    TestEqual(TEXT("Cart waits when only mobile enemy units exist"), Cart->GetState(), ELKUnitState::Idle);
    TestNull(TEXT("Cart never targets a soldier"), Cart->GetTarget());
    ALKUnitBuilding* Far = F.Building("Unit_MageTower", FVector(0.f, 900.f, 0.f), true, ELKTeam::Enemy);
    ALKUnitBuilding* Near = F.Building("Unit_HealingStation", FVector(0.f, 500.f, 0.f), true, ELKTeam::Enemy);
    Cart->Tick(.2f);
    TestEqual(TEXT("Nearest enemy building is selected across the field"), Cart->GetTarget(), static_cast<AActor*>(Near));
    TestEqual(TEXT("Cart must approach rather than attack at infinite range"), Cart->GetState(), ELKUnitState::Moving);
    TestEqual(TEXT("Cart retains a three-second basic interval"), Cart->GetAttackInterval(), 3.f);
    Cart->SetActorLocation(FVector(0.f, 400.f, 0.f));
    FLKV084RuntimeTestAccess::Attack(Cart, Near);
    TestEqual(TEXT("Ordinary building takes ten percent of maximum HP"), Near->GetHealth(), 1800.f);
    ALKUnitBuilding* Wall = F.Building("Unit_WallRare", FVector(350.f, 400.f, 0.f), true, ELKTeam::Enemy);
    Cart->SetActorLocation(FVector(350.f, 300.f, 0.f));
    FLKV084RuntimeTestAccess::Attack(Cart, Wall);
    TestEqual(TEXT("Walls take twenty percent of maximum HP"), Wall->GetHealth(), 4000.f);
    TestEqual(TEXT("Nearby soldier took no damage"), Enemy->GetHealth(), 500.f);
    Cart->GetStatusComponent()->Freeze(2.f); const float Before = Wall->GetHealth();
    FLKV084RuntimeTestAccess::Attack(Cart, Wall);
    TestEqual(TEXT("Control prevents ram attacks"), Wall->GetHealth(), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084Shelter, "LittleKing.V084.Runtime.MageShelterPriorityProtectionAndCooldown", V084Flags)
bool FLKV084Shelter::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    FLKUnitRow Apprentice = *LKUnitContent::Find("Unit_ApprenticeMage");
    ALKUnitBase* Merc = F.Spawn(Apprentice, FVector(0.f, -350.f, 0.f));
    FLKUnitRow HeroRow = *LKUnitContent::Find("Hero_Mage"); HeroRow.ActiveAbility = ELKActiveAbility::None;
    ALKUnitHero* Hero = CastChecked<ALKUnitHero>(F.Spawn(HeroRow, FVector(200.f, -350.f, 0.f)));
    ALKUnitBuilding* Tower = F.Building("Unit_MageTower", FVector::ZeroVector);
    F.Enter(Tower);
    TestEqual(TEXT("Mage hero outranks a mercenary"), Tower->GetOccupant(), static_cast<ALKUnitBase*>(Hero));
    TestTrue(TEXT("Arrival places the mage inside shelter"), Hero->IsSheltered());
    TestEqual(TEXT("Mage tower grants the resident double cooldown progress"), Hero->GetSkillCooldownRate(), 2.f);
    FLKV084RuntimeTestAccess::SetHeroCooldown(Hero, 10.f); Hero->Tick(1.f);
    TestEqual(TEXT("Actual hero cooldown advances twice as fast"), Hero->GetSkillCooldownRemaining(), 8.f);
    ALKUnitBase* Enemy = F.Soldier(FVector(0.f, 300.f, 0.f), ELKTeam::Enemy);
    Enemy->SetTarget(Hero);
    TestEqual(TEXT("Explicit attacks on a sheltered mage target the tower"), Enemy->GetTarget(), static_cast<AActor*>(Tower));
    const float Health = Hero->GetHealth();
    LKGameplay::ApplyDamage(Hero, 100.f, Enemy);
    TestEqual(TEXT("Ordinary damage cannot hit sheltered occupants"), Hero->GetHealth(), Health);
    Hero->AddTrait("Trait_MageSpellReach");
    TestTrue(TEXT("Shelter does not remove hero characteristics"), Hero->HasTraitEffect(ELKTraitEffect::GlobalSpellPlacement));
    Tower->Die();
    TestFalse(TEXT("Tower death immediately releases the mage"), Hero->IsSheltered());
    TestNull(TEXT("No stale shelter reservation after destruction"), Hero->GetServiceBuilding());
    TestEqual(TEXT("Cooldown rate returns to normal without a leaked buff"), Hero->GetSkillCooldownRate(), 1.f);
    Hero->Tick(1.f); TestEqual(TEXT("Remaining hero cooldown now ticks normally"), Hero->GetSkillCooldownRemaining(), 7.f);
    TestTrue(TEXT("Released occupant has a valid ground position"), Hero->GetMovementComponent()->CanStandAt(Hero->GetActorLocation()));
    ALKUnitBuilding* Second = F.Building("Unit_MageTower", FVector(-500.f, 0.f, 0.f));
    Hero->SetCombatEnabled(false); Hero->Die();
    F.Enter(Second);
    TestEqual(TEXT("A mercenary mage fills a tower when no living hero is available"), Second->GetOccupant(), Merc);
    const float BeforeCooldown = Merc->GetActiveComponent()->GetCooldownRemaining(); Merc->Tick(1.f);
    TestEqual(TEXT("Native mercenary cooldown also accelerates"), Merc->GetActiveComponent()->GetCooldownRemaining(), BeforeCooldown - 2.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084Station, "LittleKing.V084.Runtime.HealingStationPatientFlowAndCleanup", V084Flags)
bool FLKV084Station::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    FLKUnitRow PriestRow = *LKUnitContent::Find("Unit_AngelPriest_Rare"); PriestRow.AttackWindup = 0.f; PriestRow.AttackDamage = 40.f;
    ALKUnitBase* Priest = F.Spawn(PriestRow, FVector(0.f, -150.f, 0.f));
    ALKUnitBuilding* Station = F.Building("Unit_HealingStation", FVector::ZeroVector);
    ALKUnitBase* First = F.Soldier(FVector(-300.f, 0.f, 0.f));
    ALKUnitBase* Second = F.Soldier(FVector(300.f, 0.f, 0.f));
    ALKUnitBase* Enemy = F.Soldier(FVector(0.f, 400.f, 0.f), ELKTeam::Enemy);
    FBuildingWorld::Health(First, 100.f); FBuildingWorld::Health(Second, 200.f);
    F.Enter(Station); Station->Tick(.6f);
    TestTrue(TEXT("Priest occupies the healing station"), Priest->IsSheltered());
    TestEqual(TEXT("Station chooses the lowest health ratio below half"), Station->GetPatient(), First);
    TestEqual(TEXT("Only selected patient has withdrawn from combat"), First->GetPatientStation(), Station);
    TestNull(TEXT("Other injured ally waits without being withdrawn"), Second->GetPatientStation());
    First->SetTarget(Enemy); First->Tick(.01f);
    TestEqual(TEXT("Patient walks instead of attacking"), First->GetState(), ELKUnitState::Moving);
    FVector Approach; Station->FindServiceApproach(First, Approach); First->SetActorLocation(Approach); First->Tick(.01f);
    TestTrue(TEXT("Arrived patient is ready for healing"), Station->IsPatientReady());
    Priest->Tick(.01f); F.Projectiles(.5f);
    TestEqual(TEXT("Station heals one and a half times priest attack"), First->GetHealth(), 160.f);
    TestEqual(TEXT("Priest attacks do not damage enemies"), Enemy->GetHealth(), 500.f);
    FBuildingWorld::Health(First, First->GetMaxHealth()); Station->Tick(.6f);
    TestNull(TEXT("Full patient resumes normal combat"), First->GetPatientStation());
    TestEqual(TEXT("Next waiting patient is admitted"), Station->GetPatient(), Second);
    Station->Die();
    TestNull(TEXT("Station destruction releases current patient"), Second->GetPatientStation());
    TestFalse(TEXT("Station destruction releases the priest"), Priest->IsSheltered());
    TestEqual(TEXT("Healing multiplier returns to normal"), Priest->GetStationHealingMultiplier(Second), 1.f);
    ALKUnitBuilding* Replacement = F.Building("Unit_HealingStation", FVector(0.f, 300.f, 0.f));
    F.Enter(Replacement); Replacement->Tick(.6f);
    TestEqual(TEXT("Released priest can staff another station"), Replacement->GetOccupant(), Priest);
    TestEqual(TEXT("New station accepts the remaining wounded unit"), Replacement->GetPatient(), Second);
    Priest->Die();
    TestNull(TEXT("Losing the priest immediately cancels its patient appointment"), Second->GetPatientStation());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084OccupantCost, "LittleKing.V084.Runtime.ShelterQualityCostAndExclusiveReservation", V084Flags)
bool FLKV084OccupantCost::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    FLKUnitRow Low = *LKUnitContent::Find("Unit_ApprenticeMage"); Low.Quality = ELKQuality::Uncommon;
    FLKUnitRow Cheap = Low; Cheap.UnitId = "Test_CheapMage"; Cheap.Quality = ELKQuality::Rare;
    FLKUnitRow Expensive = Cheap; Expensive.UnitId = "Test_ExpensiveMage";
    ALKUnitBase* LowUnit = F.Spawn(Low, FVector(-400.f, -200.f, 0.f));
    ALKUnitBase* CheapUnit = F.Spawn(Cheap, FVector(0.f, -200.f, 0.f));
    ALKUnitBase* ExpensiveUnit = F.Spawn(Expensive, FVector(400.f, -200.f, 0.f));
    for (auto Pair : { TPair<FName, int32>(Low.UnitId, 20), TPair<FName, int32>(Cheap.UnitId, 3), TPair<FName, int32>(Expensive.UnitId, 8) })
    {
        ULKCardDefinition* Card = NewObject<ULKCardDefinition>(F.Data); Card->SpawnUnitId = Pair.Key; Card->Cost = Pair.Value;
        F.Data->CardLibrary.Add(Card);
    }
    ALKUnitBuilding* A = F.Building("Unit_MageTower", FVector::ZeroVector);
    ALKUnitBuilding* B = F.Building("Unit_MageTower", FVector(500.f, 500.f, 0.f));
    A->Tick(.01f); B->Tick(.01f);
    TestEqual(TEXT("Same-quality mercenary with higher silver cost wins"), A->GetOccupant(), ExpensiveUnit);
    TestEqual(TEXT("Second tower cannot reserve the already selected mage"), B->GetOccupant(), CheapUnit);
    TestNull(TEXT("Quality outranks a more expensive lower-quality mercenary"), LowUnit->GetServiceBuilding());
    A->SetCombatEnabled(false);
    TestNull(TEXT("Combat cleanup releases reservations even before arrival"), ExpensiveUnit->GetServiceBuilding());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084ShelterControl, "LittleKing.V084.Runtime.ShelterRejectsHostileControlAndAllowsHealing", V084Flags)
bool FLKV084ShelterControl::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open(true)) { return false; }
    auto* GM = F.World->GetAuthGameMode<ALKBattleGameMode>();
    ALKUnitBuilding* Tower = F.Building("Unit_MageTower", FVector(0.f, -400.f, 0.f));
    F.Enter(Tower);
    ALKUnitBase* Mage = Tower->GetOccupant();
    if (!TestNotNull(TEXT("Tower found a mage"), Mage) || !TestTrue(TEXT("Mage entered tower"), Mage->IsSheltered())) { return false; }
    const FVector Center = Tower->GetActorLocation();
    const ULKCardDefinition* Freeze = GM->FindCard("Spell_Freeze");
    TestTrue(TEXT("Enemy freezes the building area"), LKSpellExecutor::Execute(GM, Freeze, ELKTeam::Enemy, Center, nullptr, true));
    TestTrue(TEXT("Exposed tower receives freeze"), Tower->GetStatusComponent()->IsFrozen());
    TestFalse(TEXT("Resident is protected from hostile freeze"), Mage->GetStatusComponent()->IsFrozen());
    FBuildingWorld::Health(Mage, Mage->GetMaxHealth() * .5f);
    const float BeforeHealing = Mage->GetHealth();
    TestTrue(TEXT("Friendly healing remains castable"), LKSpellExecutor::Execute(GM, GM->FindCard("Spell_HealWave"), ELKTeam::Player, Center, nullptr, true));
    TestTrue(TEXT("Friendly healing reaches an injured sheltered hero"), Mage->GetHealth() > BeforeHealing);
    TestTrue(TEXT("Enemy black cloud can surround tower"), LKSpellExecutor::Execute(GM, GM->FindCard("Spell_BlackCloud"), ELKTeam::Enemy, Center, nullptr, true));
    TestTrue(TEXT("Cloud affects exposed tower"), Tower->GetStatusComponent()->IsConcealed());
    TestFalse(TEXT("Cloud does not affect indoor resident"), Mage->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Enemy hurricane is cast"), LKSpellExecutor::Execute(GM, GM->FindCard("Spell_Hurricane"), ELKTeam::Enemy, Center, nullptr, true));
    for (TActorIterator<ALKSpellField> It(F.World); It; ++It) { It->Tick(.1f); }
    TestFalse(TEXT("Shelter occupant gains no wind state"), Mage->GetStatusComponent()->IsWindDriven());
    TestEqual(TEXT("Shelter occupant remains in its building"), Mage->GetActorLocation(), Center);
    ALKSkeletonCircle* Poison = F.World->SpawnActor<ALKSkeletonCircle>();
    FLKSkeletonCircleParams Params; Params.WarnSeconds = 0.f; Params.Radius = 300.f;
    TestTrue(TEXT("Hostile poison circle initializes"), Poison->InitializeCircle(GM, ELKTeam::Enemy, Center, Params, 84, "Spell_SkeletonCircle"));
    Poison->Tick(.01f);
    TestEqual(TEXT("Poison slow cannot enter shelter"), Mage->GetStatusComponent()->AreaSlowMoveMultiplier(), 1.f);
    TestTrue(TEXT("Poison still slows exposed tower"), Tower->GetStatusComponent()->AreaSlowMoveMultiplier() < 1.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084SustainedBreach, "LittleKing.V084.Runtime.TauntedEnclosurePreservesAttackWindup", V084Flags)
bool FLKV084SustainedBreach::RunTest(const FString&)
{
    FBuildingWorld F; if (!F.Open()) { return false; }
    ALKUnitBase* Target = F.Soldier(FVector::ZeroVector, ELKTeam::Enemy);
    Target->AddTrait("Taunt");
    FLKUnitRow Row = *LKUnitContent::Find("Unit_Swordsman");
    Row.AttackDamage = 40.f; Row.AttackRange = 160.f; Row.AttackInterval = 1.f;
    Row.AttackWindup = .35f; Row.AcquireRadius = 1000.f;
    ALKUnitBase* Attacker = F.Spawn(Row, FVector(0.f, -300.f, 0.f));
    TArray<ALKUnitBuilding*> Walls;
    for (const FVector& Position : { FVector(0.f, -150.f, 0.f), FVector(0.f, 150.f, 0.f), FVector(-150.f, 0.f, 0.f), FVector(150.f, 0.f, 0.f) })
    {
        ALKUnitBuilding* Wall = F.Building("Unit_WallCommon", Position, true, ELKTeam::Enemy);
        Wall->SetWallYaw(FMath::IsNearlyZero(Position.X) ? 0.f : 90.f); Walls.Add(Wall);
    }
    const float VictimBefore = Target->GetHealth();
    Attacker->SetTarget(Target);
    for (int32 Frame = 0; Frame < 45; ++Frame)
    { F.AdvanceClock(.05f); Attacker->Tick(.05f); }
    TestEqual(TEXT("Enclosed taunter is not struck through the wall"), Target->GetHealth(), VictimBefore);
    TestTrue(TEXT("Sustained acquisition does not reset every attack windup"), Walls[0]->GetHealth() <= 920.f);
    TestEqual(TEXT("Current attack remains on the reachable front wall"), Attacker->GetTarget(), static_cast<AActor*>(Walls[0]));
    Walls[0]->Die(); Attacker->Tick(.2f);
    TestEqual(TEXT("Once opened, original taunter becomes the target again"), Attacker->GetTarget(), static_cast<AActor*>(Target));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084OpenWallDetour,"LittleKing.V084.Runtime.TroopsWalkAroundAnOpenWall",V084Flags)
bool FLKV084OpenWallDetour::RunTest(const FString&)
{
    FBuildingWorld F;if(!F.Open()) { return false; }
    auto* Target=F.Soldier(FVector(0,350,0),ELKTeam::Enemy);
    auto* Attacker=F.Soldier(FVector(0,-350,0));
    auto* Wall=F.Building("Unit_WallCommon",FVector::ZeroVector,true,ELKTeam::Enemy);Wall->SetWallYaw(0);
    Attacker->Tick(.05f);
    TestEqual(TEXT("Ordinary troop keeps opponent as target instead of attacking a bypassable wall"),Attacker->GetTarget(),static_cast<AActor*>(Target));
    for(int32 I=0;I<120;++I)
    {
        F.AdvanceClock(.05f);Attacker->Tick(.05f);
        Attacker->GetMovementComponent()->TickComponent(.05f,LEVELTICK_All,nullptr);
        TestTrue(TEXT("Detour never penetrates completed wall"),LKBuildingGeometry::SurfaceDistance(Attacker->GetActorLocation(),Wall)>=Attacker->GetBodyRadius());
    }
    TestTrue(TEXT("Troop reaches the far side"),Attacker->GetActorLocation().Y>100.f);
    TestEqual(TEXT("Bypassed wall remains undamaged"),Wall->GetHealth(),Wall->GetMaxHealth());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV084BlockedServiceEntrance, "LittleKing.V084.Runtime.CrowdedServiceEntrancesRerouteOrRelease", V084Flags)
bool FLKV084BlockedServiceEntrance::RunTest(const FString&)
{
    {
        FBuildingWorld F; if (!F.Open()) { return false; }
        ALKUnitBuilding* Tower = F.Building("Unit_MageTower", FVector::ZeroVector);
        ALKUnitBase* Mage = F.Spawn(*LKUnitContent::Find("Unit_ApprenticeMage"), FVector(0.f, -350.f, 0.f));
        const float Radius = Tower->GetBodyRadius() + Mage->GetBodyRadius() + 8.f;
        ALKUnitBase* FirstBlocker = F.Soldier(FVector(0.f, -Radius, 0.f));
        Tower->Tick(.01f);
        TestEqual(TEXT("Mage reserves a tower despite one blocked entrance"), Tower->GetOccupant(), Mage);
        const FVector Initial = FLKV084RuntimeTestAccess::ServiceApproach(Mage);
        TestFalse(TEXT("Initial choice avoids the occupied closest entrance"), Initial.Equals(FirstBlocker->GetActorLocation(), 1.f));
        TestTrue(TEXT("Initial entrance has room for the full body"), Mage->GetMovementComponent()->CanStandAt(Initial, true));
        F.Soldier(Initial);
        // Deliberately approach within the arrival threshold after another unit takes this point.
        Mage->SetActorLocation(Initial); Mage->Tick(.01f);
        const FVector Rerouted = FLKV084RuntimeTestAccess::ServiceApproach(Mage);
        TestFalse(TEXT("Arrival check never enters through an occupied point"), Mage->IsSheltered());
        TestFalse(TEXT("An occupied reservation switches to a different entrance"), Rerouted.Equals(Initial, 18.f));
        TestTrue(TEXT("Replacement entrance has valid unoccupied clearance"), Mage->GetMovementComponent()->CanStandAt(Rerouted, true));
        Mage->SetActorLocation(Rerouted); Mage->Tick(.01f);
        TestTrue(TEXT("Mage completes entry after rerouting"), Mage->IsSheltered());
    }
    {
        FBuildingWorld F; if (!F.Open()) { return false; }
        ALKUnitBuilding* Station = F.Building("Unit_HealingStation", FVector::ZeroVector);
        F.Spawn(*LKUnitContent::Find("Unit_AngelPriest_Rare"), FVector(0.f, -350.f, 0.f));
        F.Enter(Station);
        ALKUnitBase* Patient = F.Soldier(FVector(-350.f, 0.f, 0.f));
        FBuildingWorld::Health(Patient, 100.f); Station->Tick(.6f);
        TestEqual(TEXT("Station initially accepts the wounded unit"), Station->GetPatient(), Patient);
        const FVector Initial = FLKV084RuntimeTestAccess::ServiceApproach(Patient);
        ALKUnitBase* Blocker = F.Soldier(Initial);
        Patient->Tick(.51f);
        TestEqual(TEXT("Patient retains appointment when another entrance is open"), Patient->GetPatientStation(), Station);
        TestFalse(TEXT("Patient changes entrance when its reserved point becomes occupied"), FLKV084RuntimeTestAccess::ServiceApproach(Patient).Equals(Initial, 1.f));
        TestTrue(TEXT("Rerouted patient destination is unoccupied"), Patient->GetMovementComponent()->CanStandAt(FLKV084RuntimeTestAccess::ServiceApproach(Patient), true));
        Blocker->SetActorLocation(FVector(700.f, 700.f, 0.f));
        const float Radius = Station->GetBodyRadius() + Patient->GetBodyRadius() + 8.f;
        // Six non-overlapping bodies fill every possible approach direction around the station.
        for (int32 I = 0; I < 6; ++I)
        { const float Angle = I * PI / 3.f; F.Soldier(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius); }
        Patient->Tick(.51f);
        TestNull(TEXT("All entrances blocked cancels the patient reservation"), Patient->GetPatientStation());
        TestNull(TEXT("Blocked appointment releases the station's patient slot"), Station->GetPatient());
        TestFalse(TEXT("Patient is not permanently withdrawn from combat"), Patient->IsBuildingServiceMoving());
        Station->Tick(.6f);
        TestNull(TEXT("Station does not reserve the same patient again while every entrance is blocked"), Station->GetPatient());
    }
    return true;
}
#endif
