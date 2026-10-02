#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "AbilitySystemComponent.h"
#include "../ALKUnitBase.h"
#include "../ALKProjectile.h"
#include "../LKUnitContent.h"
#include "../LKGameplayHelpers.h"
#include "../ULKGameData.h"
#include "../ULKRunSubsystem.h"
#include "../ULKUnitAttributeSet.h"
#include "../ULKUnitStatusComponent.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKUnitAnimationComponent.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags V083UnitFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FV083UnitWorld
{
    FTestWorldWrapper Env;
    UWorld* World = nullptr;
    ULKGameData* Data = nullptr;
    bool Open()
    {
        if (!Env.CreateTestWorld(EWorldType::Game)) { return false; }
        World = Env.GetTestWorld();
        World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
        World->GetGameInstance()->GetSubsystem<ULKRunSubsystem>()->SetAutoSaveEnabled(false);
        if (!Env.BeginPlayInTestWorld()) { return false; }
        Data = NewObject<ULKGameData>(World);
        return true;
    }
    ALKUnitBase* Spawn(FVector Position, ELKTeam Team = ELKTeam::Player, bool bHealer = false,
        ELKUnitClass Class = ELKUnitClass::Soldier, float Windup = 0.f)
    {
        FLKUnitRow Row = *LKUnitContent::Find("Unit_TrollWarrior");
        Row.UnitClass = Class;
        Row.bBasicAttackHeals = bHealer;
        Row.BaseHealth = 200.f;
        Row.AttackDamage = 40.f;
        Row.AttackRange = 700.f;
        Row.AcquireRadius = 1000.f;
        Row.AttackWindup = Windup;
        Row.AttackInterval = 1.f;
        Row.HeroTraits.Reset();
        auto* Unit = World->SpawnActor<ALKUnitBase>();
        Unit->SetActorLocation(Position);
        Unit->InitUnit(Row, Data);
        Unit->SetTeam(Team);
        Unit->SetCombatEnabled(true);
        return Unit;
    }
    static void Health(ALKUnitBase* Unit, float Value)
    {
        Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), Value);
    }
    void AdvanceProjectiles(float Seconds)
    {
        TArray<ALKProjectile*> Projectiles;
        for (TActorIterator<ALKProjectile> It(World); It; ++It) { Projectiles.Add(*It); }
        for (ALKProjectile* Projectile : Projectiles) { if (IsValid(Projectile)) { Projectile->Tick(Seconds); } }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083ConcealmentTargets,
    "LittleKing.V083.Units.ConcealmentTargetsAndSources", V083UnitFlags)
bool FLKV083ConcealmentTargets::RunTest(const FString&)
{
    FV083UnitWorld F; if (!F.Open()) { return false; }
    ALKUnitBase* Observer = F.Spawn(FVector::ZeroVector);
    ALKUnitBase* Hidden = F.Spawn(FVector(0.f, 150.f, 0.f), ELKTeam::Enemy);
    ALKUnitBase* Alternative = F.Spawn(FVector(0.f, 500.f, 0.f), ELKTeam::Enemy);
    Hidden->AddTrait("Taunt");
    Observer->SetForcedTarget(Hidden, 10.f);
    TestEqual(TEXT("Taunting focus target is initially visible"), Observer->GetTarget(), static_cast<AActor*>(Hidden));

    Hidden->GetStatusComponent()->ApplyConcealment("Cloud_A", 5.f);
    TestTrue(TEXT("Concealment is active"), Hidden->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Ground effects retain targetability"), Hidden->IsTargetable());
    TestFalse(TEXT("Unit discovery cannot find a hidden taunter"), Observer->CanDiscoverTarget(Hidden));
    TestFalse(TEXT("Concealment clears forced focus"), Observer->HasForcedTarget());
    TestEqual(TEXT("Current attacker immediately retargets"), Observer->GetTarget(), static_cast<AActor*>(Alternative));
    Observer->SetTarget(Hidden);
    TestNull(TEXT("Directly choosing a hidden unit is rejected"), Observer->GetTarget());
    Observer->Tick(.16f);
    TestEqual(TEXT("Automatic search reacquires an alternative"), Observer->GetTarget(), static_cast<AActor*>(Alternative));
    const float Before = Hidden->GetHealth();
    LKGameplay::ApplyDamage(Hidden, 10.f, Observer);
    TestEqual(TEXT("Concealment does not grant area damage immunity"), Hidden->GetHealth(), Before - 10.f);

    Hidden->GetStatusComponent()->ApplyConcealment("Cloud_B", 3.f);
    Hidden->GetStatusComponent()->RemoveConcealment("Cloud_A");
    TestTrue(TEXT("Removing one source preserves the other cloud"), Hidden->GetStatusComponent()->IsConcealed());
    Hidden->GetStatusComponent()->TickStatus(3.f);
    TestFalse(TEXT("The remaining source expires"), Hidden->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Leaving all clouds restores discovery"), Observer->CanDiscoverTarget(Hidden));
    Hidden->GetStatusComponent()->ApplyConcealment("Cloud_C", 5.f);
    Hidden->SetCombatEnabled(false);
    TestFalse(TEXT("Battle cleanup removes concealment sources"), Hidden->GetStatusComponent()->IsConcealed());
    Hidden->SetCombatEnabled(true);
    Hidden->GetStatusComponent()->ApplyConcealment(NAME_None, 5.f);
    Hidden->GetStatusComponent()->ApplyConcealment("Invalid", std::numeric_limits<float>::infinity());
    TestFalse(TEXT("Invalid concealment parameters are ignored"), Hidden->GetStatusComponent()->IsConcealed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083HealerBasicAttack,
    "LittleKing.V083.Units.HealerPriorityProjectileAndConcealment", V083UnitFlags)
bool FLKV083HealerBasicAttack::RunTest(const FString&)
{
    FV083UnitWorld F; if (!F.Open()) { return false; }
    ALKUnitBase* Healer = F.Spawn(FVector::ZeroVector, ELKTeam::Player, true);
    ALKUnitBase* Ally = F.Spawn(FVector(-200.f, 0.f, 0.f));
    ALKUnitBase* Injured = F.Spawn(FVector(0.f, 400.f, 0.f));
    ALKUnitBase* Enemy = F.Spawn(FVector(0.f, 200.f, 0.f), ELKTeam::Enemy);
    ALKUnitBase* FallenHero = F.Spawn(FVector(300.f, 0.f, 0.f), ELKTeam::Player, false, ELKUnitClass::Hero);
    FV083UnitWorld::Health(Ally, 100.f);
    FV083UnitWorld::Health(Injured, 50.f);
    FV083UnitWorld::Health(Enemy, 10.f);
    FallenHero->Die();
    Enemy->AddTrait("Taunt");
    Healer->SetForcedTarget(Enemy, 10.f);
    TestFalse(TEXT("Healer refuses enemy focus commands"), Healer->HasForcedTarget());
    Healer->Tick(.01f);
    TestEqual(TEXT("Lowest friendly health percentage has priority"), Healer->GetTarget(), static_cast<AActor*>(Injured));
    TestFalse(TEXT("Incapacitated heroes are never healing targets"), Healer->CanPursueTarget(FallenHero));
    TestFalse(TEXT("Enemy taunt cannot redirect healing"), Healer->CanPursueTarget(Enemy));
    F.AdvanceProjectiles(.6f);
    TestEqual(TEXT("Ranged healing bolt restores attack-value health"), Injured->GetHealth(), 90.f);
    TestEqual(TEXT("Friendly healing never damages enemies on its path"), Enemy->GetHealth(), 10.f);

    Healer->Tick(1.1f); // Send a second bolt, then hide its chosen target before impact.
    Injured->GetStatusComponent()->ApplyConcealment("Cloud", 5.f);
    TestEqual(TEXT("Healer immediately changes to a discoverable injured ally"), Healer->GetTarget(), static_cast<AActor*>(Ally));
    F.AdvanceProjectiles(.6f);
    TestEqual(TEXT("An in-flight friendly bolt does not heal a hidden ally"), Injured->GetHealth(), 90.f);
    Injured->GetStatusComponent()->RemoveConcealment("Cloud");
    FV083UnitWorld::Health(Injured, 200.f);
    FV083UnitWorld::Health(Ally, 200.f);
    Healer->Tick(1.1f);
    TestNull(TEXT("Healer waits when every eligible ally is full"), Healer->GetTarget());
    TestEqual(TEXT("Incapacitated hero remains at zero"), FallenHero->GetHealth(), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083WindGeometry,
    "LittleKing.V083.Units.WindGeometryFacingAndControl", V083UnitFlags)
bool FLKV083WindGeometry::RunTest(const FString&)
{
    FV083UnitWorld F; if (!F.Open()) { return false; }
    ALKUnitBase* Unit = F.Spawn(FVector::ZeroVector);
    ALKUnitBase* Building = F.Spawn(FVector(0.f, 300.f, 0.f), ELKTeam::Enemy, false, ELKUnitClass::Building);
    Unit->ApplyWindDisplacement(FVector(0.f, 1000.f, 0.f));
    TestTrue(TEXT("Wind cannot cross a building body"), Unit->GetActorLocation().Y < 200.f);
    TestTrue(TEXT("Wind leaves a legal ground position"), Unit->GetMovementComponent()->CanStandAt(Unit->GetActorLocation()));
    const FVector StaticPosition = Building->GetActorLocation();
    Building->ApplyWindDisplacement(FVector(0.f, 1000.f, 0.f));
    TestEqual(TEXT("Buildings remain fixed"), Building->GetActorLocation(), StaticPosition);

    Unit->SetActorLocation(FVector(600.f, 1500.f, 0.f));
    Unit->ApplyWindDisplacement(FVector(0.f, 2000.f, 0.f));
    TestTrue(TEXT("Wind respects the field edge with body clearance"), Unit->GetActorLocation().Y <= F.Data->FieldHalfHeight - Unit->GetBodyRadius());
    Unit->GetAnimationComponent()->TickComponent(.01f, LEVELTICK_All, nullptr);
    Unit->SetActorLocation(FVector(600.f, 0.f, 0.f));
    Unit->SetVisualFacingRight(false);
    Unit->GetStatusComponent()->Freeze(3.f);
    const float WalkBefore = Unit->GetAnimationComponent()->GetWalkPhase();
    Unit->ApplyWindDisplacement(FVector(0.f, 50.f, 0.f));
    Unit->GetMovementComponent()->TickComponent(.1f, LEVELTICK_All, nullptr);
    Unit->GetAnimationComponent()->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Frozen units are displaced by wind"), Unit->GetActorLocation().Y, 50.);
    TestTrue(TEXT("Forced displacement updates horizontal facing"), Unit->GetAnimationComponent()->IsFacingRight());
    TestEqual(TEXT("Frozen units slide without walking through frozen poses"), Unit->GetAnimationComponent()->GetWalkPhase(), WalkBefore);
    Unit->GetStatusComponent()->TickStatus(3.f);
    Unit->ApplyWindDisplacement(FVector(0.f, 50.f, 0.f));
    Unit->GetMovementComponent()->TickComponent(.1f, LEVELTICK_All, nullptr);
    Unit->GetAnimationComponent()->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Uncontrolled wind travel uses the walk clip"), Unit->GetAnimationComponent()->GetVisualState(), FName("Move"));
    TestTrue(TEXT("Wind motion survives component tick ordering"), !FMath::IsNearlyEqual(Unit->GetAnimationComponent()->GetWalkPhase(), WalkBefore));
    const FVector BeforeInvalid = Unit->GetActorLocation();
    Unit->ApplyWindDisplacement(FVector(0.f, std::numeric_limits<float>::quiet_NaN(), 0.f));
    TestEqual(TEXT("Non-finite wind displacement is rejected"), Unit->GetActorLocation(), BeforeInvalid);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083WindOverridesVoluntaryMovement,
    "LittleKing.V083.Units.WindOverridesWalkingWithoutStun", V083UnitFlags)
bool FLKV083WindOverridesVoluntaryMovement::RunTest(const FString&)
{
    FV083UnitWorld F; if (!F.Open()) { return false; }
    ALKUnitBase* Unit = F.Spawn(FVector::ZeroVector, ELKTeam::Player, false, ELKUnitClass::Soldier, .15f);
    ALKUnitBase* Enemy = F.Spawn(FVector(0.f, 400.f, 0.f), ELKTeam::Enemy);
    Unit->GetStatusComponent()->ApplyWind("Gust_A", 5.f);
    TestTrue(TEXT("The field registers a wind source"), Unit->GetStatusComponent()->HasWind("Gust_A"));
    TestFalse(TEXT("Wind is not freeze or stun"), Unit->IsControlled());
    Unit->ApplyWindDisplacement(FVector(0.f, 220.f, 0.f));
    Unit->GetMovementComponent()->MoveToward(FVector(0.f, -1000.f, 0.f), 300.f);
    Unit->GetMovementComponent()->TickComponent(1.f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Faster leftward voluntary walking cannot counteract rightward wind"), Unit->GetActorLocation().Y, 220.);
    Unit->GetMovementComponent()->MoveSkillDelta(FVector(0.f, -20.f, 0.f));
    TestEqual(TEXT("Other forced displacement remains available during wind"), Unit->GetActorLocation().Y, 200.);

    Unit->Tick(.01f); // Begin the normal .15-second windup.
    TestEqual(TEXT("A valid target can still be acquired during wind"), Unit->GetTarget(), static_cast<AActor*>(Enemy));
    TestEqual(TEXT("The windup has not dealt premature damage"), Enemy->GetHealth(), 200.f);
    Unit->ApplyWindDisplacement(FVector(0.f, 10.f, 0.f));
    Unit->Tick(.2f);
    TestEqual(TEXT("Subsequent wind displacement does not cancel the normal attack"), Enemy->GetHealth(), 160.f);

    Unit->GetStatusComponent()->ApplyWind("Gust_B", 3.f);
    Unit->GetStatusComponent()->RemoveWind("Gust_A");
    TestTrue(TEXT("Leaving one field preserves overlapping wind"), Unit->GetStatusComponent()->IsWindDriven());
    Unit->GetStatusComponent()->TickStatus(3.f);
    TestFalse(TEXT("Unrefreshed wind sources expire"), Unit->GetStatusComponent()->IsWindDriven());
    Unit->GetMovementComponent()->MoveToward(FVector(0.f, -1000.f, 0.f), 300.f);
    Unit->GetMovementComponent()->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Voluntary movement resumes after wind expires"), Unit->GetActorLocation().Y < 210.);

    Unit->GetStatusComponent()->ApplyWind("Gust_C", 5.f);
    Unit->SetCombatEnabled(false);
    TestFalse(TEXT("Battle cleanup removes all wind sources"), Unit->GetStatusComponent()->IsWindDriven());
    Enemy->GetStatusComponent()->ApplyWind(NAME_None, 5.f);
    Enemy->GetStatusComponent()->ApplyWind("Invalid", std::numeric_limits<float>::infinity());
    TestFalse(TEXT("Invalid wind parameters cannot suppress walking"), Enemy->GetStatusComponent()->IsWindDriven());
    return true;
}
#endif
