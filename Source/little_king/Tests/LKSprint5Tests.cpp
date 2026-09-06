#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SphereComponent.h"
#include "PaperSpriteComponent.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKPresentationHUD.h"
#include "../ULKCardDefinition.h"
#include "../ALKHeroCamp.h"
#include "../ALKUnitHero.h"
#include "../ALKUnitBuilding.h"
#include "../ALKProjectile.h"
#include "../ULKDeckState.h"
#include "../ULKGameData.h"
#include "../ULKSilverComponent.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKTraitAuraComponent.h"
#include "../LKGameplayHelpers.h"
#include "../LKNavigation.h"
#include "../ULKGameplayLibrary.h"

struct FLKSprint5TestAccess
{
    static FLKUnitRow& Row(ALKBattleGameMode* GM, FName Id) { return GM->FallbackUnitRows.FindChecked(Id); }
    static void Overtime(ALKBattleGameMode* GM, float Seconds) { GM->bOvertimeActive = true; GM->TickOvertime(Seconds); }
    static float Cooldown(ALKUnitBase* Unit) { return Unit->AttackCooldownRemaining; }
    static void BeginWindup(ALKUnitBase* Unit) { Unit->TryAttack(0.f); }
    static void FinishWindup(ALKUnitBase* Unit, float Seconds) { Unit->TryAttack(Seconds); }
    static bool WindingUp(ALKUnitBase* Unit) { return Unit->bWindupActive; }
};

namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

struct FLKBattleTestWorld : FTestWorldWrapper
{
    ALKBattleGameMode* GM = nullptr;
    ALKPlayerController* PC = nullptr;
    bool Open(FAutomationTestBase& Test)
    {
        if (!CreateTestWorld(EWorldType::Game)) { ForwardErrorMessages(&Test); return false; }
        GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
        if (!BeginPlayInTestWorld()) { ForwardErrorMessages(&Test); return false; }
        GM = GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        PC = GetTestWorld()->SpawnActor<ALKPlayerController>();
        if (!Test.TestNotNull(TEXT("Battle GameMode"), GM) || !Test.TestNotNull(TEXT("Player controller"), PC)) { return false; }
        GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes = false;
        GM->GetGameData()->bDrawFieldBounds = false;
        return Test.TestNotNull(TEXT("Initialized player deck"), GM->GetTeamDeck(ELKTeam::Player));
    }
    bool DeployAndStart(FAutomationTestBase& Test)
    {
        for (int32 i = 0; i < GM->AvailableHeroes.Num(); ++i)
        {
            const FVector Point((i - 1) * 700.f, -1300.f, 0.f);
            if (!Test.TestEqual(TEXT("Deploy hero and camp"), GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[i], Point), ELKPlayResult::Success)) { return false; }
        }
        GM->ForceStartBattle();
        return Test.TestEqual(TEXT("Battle started"), GM->GetPhase(), ELKGamePhase::Battle);
    }
    ALKUnitHero* Hero(ELKTeam Team, FName Id) const
    {
        for (TActorIterator<ALKUnitHero> It(GetTestWorld()); It; ++It)
        { if (It->GetTeam() == Team && It->GetUnitId() == Id) { return *It; } }
        return nullptr;
    }
};

bool Unique(const TArray<FName>& Hand)
{
    TSet<FName> Seen;
    for (FName Id : Hand) { if (!Id.IsNone()) { if (Seen.Contains(Id)) { return false; } Seen.Add(Id); } }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKUniqueHandTest, "LittleKing.Sprint5.Deck.UniqueHandAndCycling", Flags)
bool FLKUniqueHandTest::RunTest(const FString& Parameters)
{
    ULKDeckState* Deck = NewObject<ULKDeckState>();
    ULKDeckState* Mirror = NewObject<ULKDeckState>();
    const TArray<FName> Cards = {"A", "A", "B", "B", "C", "D", "E", "F", "G", NAME_None};
    TestTrue(TEXT("Duplicate input normalizes to seven kinds"), Deck->InitDeck(Cards, 4, 97));
    Mirror->InitDeck(Cards, 4, 97);
    TestTrue(TEXT("Seed reproduces opening hand and queue"), Deck->GetAllCards() == Mirror->GetAllCards());
    TArray<FName> ExpectedHand = Deck->GetHand();
    TArray<FName> ExpectedQueue = Deck->GetAllCards(); ExpectedQueue.RemoveAt(0, 4);
    for (int32 i = 0; i < 120; ++i)
    {
        const int32 Slot = (i * 7 + i / 5) % 4;
        const FName Played = ExpectedHand[Slot];
        TestEqual(TEXT("Next card is queue head"), Deck->GetNextCard(), ExpectedQueue[0]);
        ExpectedHand[Slot] = ExpectedQueue[0]; ExpectedQueue.RemoveAt(0); ExpectedQueue.Add(Played);
        TestEqual(TEXT("Play returns selected card"), Deck->PlayCard(Slot), Played);
        TestTrue(TEXT("Replacement stays in original slot; other slots unchanged"), Deck->GetHand() == ExpectedHand);
        TestTrue(TEXT("Hand remains full and unique"), Deck->IsReady() && !Deck->GetHand().Contains(NAME_None) && Unique(Deck->GetHand()));
        TestFalse(TEXT("Played card enters queue, not hand"), Deck->GetHand().Contains(Played));
        TestEqual(TEXT("Seven unique cards conserved"), Deck->GetAllCards().Num(), 7);
    }
    const TArray<FName> Before = Deck->GetAllCards();
    TestFalse(TEXT("Too few distinct kinds rejected"), Deck->InitDeck({"A", "A", "B", "C", "D"}, 4, 2));
    TestTrue(TEXT("Invalid reinit preserves complete old state"), Deck->GetAllCards() == Before);
    TestTrue(TEXT("Invalid slot rejected"), Deck->PlayCard(-1).IsNone() && Deck->PlayCard(4).IsNone());
    TestFalse(TEXT("Extra draw cannot jump queue"), Deck->DrawCard());
    TestTrue(TEXT("Invalid actions preserve queue"), Deck->GetAllCards() == Before);
    TestFalse(TEXT("Duplicate addition rejected"), Deck->AddCardToDeck("A"));
    TestFalse(TEXT("Duplicate upgrade rejected"), Deck->TransformCard("A", "B"));
    TestFalse(TEXT("Empty addition rejected"), Deck->AddCardToDeck(NAME_None));
    const FName InHand = Deck->GetHandCard(1), Next = Deck->GetNextCard();
    TestTrue(TEXT("Removing a hand card succeeds above minimum"), Deck->RemoveCardFromDeck(InHand));
    TestEqual(TEXT("Removal refills same slot immediately"), Deck->GetHandCard(1), Next);
    TestTrue(TEXT("Removing a queued card succeeds above minimum"), Deck->RemoveCardFromDeck(Deck->GetNextCard()));
    TestFalse(TEXT("Minimum five kinds cannot be reduced"), Deck->RemoveCardFromDeck(Deck->GetHandCard(0)));
    TestTrue(TEXT("Hand upgrade replaces one kind in place"), Deck->TransformCard(Deck->GetHandCard(2), "Upgrade"));
    TestEqual(TEXT("Upgraded slot retained"), Deck->GetHandCard(2), FName("Upgrade"));
    TestTrue(TEXT("Queue upgrade retains queue order"), Deck->TransformCard(Deck->GetNextCard(), "QueueUpgrade"));
    TestEqual(TEXT("Upgraded queue head retained"), Deck->GetNextCard(), FName("QueueUpgrade"));
    for (int32 i = 0; i < 50; ++i)
    {
        const FName Played = Deck->PlayCard(i % 4);
        TestFalse(TEXT("Minimum deck never draws a blank"), Deck->GetHand().Contains(NAME_None));
        TestTrue(TEXT("Minimum deck stays unique"), Unique(Deck->GetHand()));
        TestEqual(TEXT("Played card waits as next card in minimum deck"), Deck->GetNextCard(), Played);
    }
    TestTrue(TEXT("New reward appends to queue"), Deck->AddCardToDeck("NewReward"));
    TestEqual(TEXT("New reward is at tail"), Deck->GetAllCards().Last(), FName("NewReward"));
    ULKDeckState* Invalid = NewObject<ULKDeckState>();
    TestFalse(TEXT("Empty config rejected"), Invalid->InitDeck({}, 4));
    TestFalse(TEXT("Invalid config is never ready for battle"), Invalid->IsReady());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKNavigationTest, "LittleKing.Sprint5.Movement.CampPathGeometry", Flags)
bool FLKNavigationTest::RunTest(const FString& Parameters)
{
    LKNavigation::FBounds Bounds; Bounds.HalfExtent = FVector2D(1000.f); Bounds.LeashRadius = 500.f;
    TArray<LKNavigation::FObstacle> Obstacles = {{FVector::ZeroVector, 120.f}};
    const FVector Start(-350.f, 0.f, 0.f), Goal(350.f, 0.f, 0.f);
    TArray<FVector> Path;
    TestTrue(TEXT("Path routes around solid camp"), LKNavigation::FindPath(Start, Goal, Obstacles, Bounds, Path));
    TestTrue(TEXT("Path bends"), Path.Num() > 1);
    FVector Previous = Start;
    for (const FVector& Point : Path)
    {
        TestTrue(TEXT("Waypoint inside leash and field"), LKNavigation::IsPointValid(Point, Obstacles, Bounds));
        TestTrue(TEXT("Whole segment avoids camp"), LKNavigation::IsSegmentClear(Previous, Point, Obstacles));
        Previous = Point;
    }
    TestFalse(TEXT("Reject destination inside camp"), LKNavigation::FindPath(Start, FVector::ZeroVector, Obstacles, Bounds, Path));
    TestFalse(TEXT("Reject destination beyond leash"), LKNavigation::FindPath(Start, FVector(600.f, 0.f, 0.f), Obstacles, Bounds, Path));
    Bounds.LeashRadius = 0.f;
    TestFalse(TEXT("Reject destination beyond field"), LKNavigation::FindPath(Start, FVector(1100.f, 0.f, 0.f), Obstacles, Bounds, Path));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKDeploymentTest, "LittleKing.Sprint5.Heroes.DeploymentAndManualMovement", Flags)
bool FLKDeploymentTest::RunTest(const FString& Parameters)
{
    FLKBattleTestWorld Env; if (!Env.Open(*this)) { return false; }
    Env.GM->Tick(3600.f); Env.GM->ForceStartBattle();
    TestEqual(TEXT("No timeout or auto-fill start"), Env.GM->GetPhase(), ELKGamePhase::Deployment);
    TestEqual(TEXT("First deployment"), Env.GM->DeployHero(ELKTeam::Player, "Hero_Knight", FVector(-700.f, -1300.f, 0.f)), ELKPlayResult::Success);
    Env.GM->ForceStartBattle(); TestFalse(TEXT("One hero cannot start"), Env.GM->GetPhase() == ELKGamePhase::Battle);
    TestEqual(TEXT("Occupied camp location rejected"), Env.GM->DeployHero(ELKTeam::Player, "Hero_Mage", FVector(-700.f, -1300.f, 0.f)), ELKPlayResult::InvalidLocation);
    ALKUnitHero* Hero = Env.Hero(ELKTeam::Player, "Hero_Knight");
    if (!TestNotNull(TEXT("Hero exists"), Hero) || !TestNotNull(TEXT("Camp exists"), Hero->GetCamp())) { return false; }
    TestFalse(TEXT("Camp is not attackable"), Hero->GetCamp()->IsTargetable());
    TestTrue(TEXT("Camp is a building"), Hero->GetCamp()->IsBuilding());
    const FVector Destination = Hero->GetCampCenter() + FVector(0.f, -250.f, 0.f);
    TestFalse(TEXT("Cannot walk through destination camp"), Hero->CommandMove(Hero->GetCampCenter()));
    TestFalse(TEXT("Cannot walk beyond camp range"), Hero->CommandMove(Hero->GetCampCenter() + FVector(2000.f, 0.f, 0.f)));
    TestTrue(TEXT("Can reposition before battle without attack"), Hero->CommandMove(Destination));
    ULKUnitMovementComponent* Movement = Hero->FindComponentByClass<ULKUnitMovementComponent>();
    for (int32 i = 0; i < 350; ++i)
    {
        Hero->Tick(0.01f); Movement->TickComponent(0.01f, LEVELTICK_All, nullptr);
        TestTrue(TEXT("Whole hero remains within camp"), FVector::Dist2D(Hero->GetActorLocation(), Hero->GetCampCenter()) <= Hero->GetCampMoveRadius() - Hero->GetBodyRadius() + 0.1f);
        TestTrue(TEXT("Hero never crosses solid camp"), FVector::Dist2D(Hero->GetActorLocation(), Hero->GetCampCenter()) >= Hero->GetBodyRadius() + Hero->GetCamp()->GetBodyRadius());
    }
    TestTrue(TEXT("Arrived around camp"), FVector::Dist2D(Hero->GetActorLocation(), Destination) <= 5.f);
    TestFalse(TEXT("Manual movement ends at arrival"), Hero->IsManualMoving());
    Env.GM->DeployHero(ELKTeam::Player, "Hero_Mage", FVector(0.f, -1300.f, 0.f));
    Env.GM->DeployHero(ELKTeam::Player, "Hero_Ranger", FVector(700.f, -1300.f, 0.f));
    TestTrue(TEXT("All three unlock start"), Env.GM->CanStartBattle()); Env.GM->ForceStartBattle();
    TestTrue(TEXT("Repeat command has no cooldown"), Hero->CommandMove(Destination + FVector(80.f, 0.f, 0.f)));
    Env.GM->ForceEndMatch(ELKTeam::Player);
    const FVector Frozen = Hero->GetActorLocation(); Hero->Tick(0.1f); Movement->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Result cancels manual movement"), Hero->GetActorLocation().Equals(Frozen));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTraitsTest, "LittleKing.Sprint5.Heroes.TraitsTauntAndSpellTerritory", Flags)
bool FLKTraitsTest::RunTest(const FString& Parameters)
{
    FLKBattleTestWorld Env; if (!Env.Open(*this) || !Env.DeployAndStart(*this)) { return false; }
    ALKUnitHero* Mage = Env.Hero(ELKTeam::Player, "Hero_Mage");
    TestTrue(TEXT("Mage trait enables enemy-half casting"), Env.GM->CanPlaceSpellAt(ELKTeam::Player, FVector(0.f, 600.f, 0.f)));
    Mage->RemoveTrait("Trait_MageSpellReach");
    TestFalse(TEXT("Removing trait immediately changes territory"), Env.GM->CanPlaceSpellAt(ELKTeam::Player, FVector(0.f, 600.f, 0.f)));
    TestTrue(TEXT("Home-half spell stays available"), Env.GM->CanPlaceSpellAt(ELKTeam::Player, FVector(0.f, -600.f, 0.f)));
    Mage->AddTrait("Trait_MageSpellReach");
    TestTrue(TEXT("Runtime addition restores territory"), Env.GM->HasGlobalSpellPlacement(ELKTeam::Player));
    Mage->Die(); TestFalse(TEXT("Dead trait owner grants no reach"), Env.GM->HasGlobalSpellPlacement(ELKTeam::Player));
    TestTrue(TEXT("Mage death does not lock all spells"), Env.GM->CanCastSpell(ELKTeam::Player));
    ALKUnitHero* Knight = Env.Hero(ELKTeam::Player, "Hero_Knight");
    Knight->SetActorLocation(FVector(-300.f, 0.f, 0.f));
    ALKUnitBase* Melee = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Player, FVector(-200.f, 0.f, 0.f));
    ALKUnitBase* Ranged = Env.GM->SpawnUnitForTeam("Unit_Archer", ELKTeam::Player, FVector(-300.f, 150.f, 0.f));
    ALKUnitBase* Attacker = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Enemy, FVector(100.f, 0.f, 0.f));
    if (!Melee || !Ranged || !Attacker) { AddError(TEXT("Fixture spawn failed")); return false; }
    Knight->FindComponentByClass<ULKTraitAuraComponent>()->TickAura(1.f);
    TestTrue(TEXT("Knight taunts allied melee soldier"), Melee->IsTaunting());
    TestFalse(TEXT("Ranged excluded from aura"), Ranged->IsTaunting());
    TestFalse(TEXT("Hero excluded from aura"), Knight->IsTaunting());
    Attacker->SetForcedTarget(Ranged, 5.f);
    TestTrue(TEXT("Taunt overrides focus"), Attacker->GetTarget() == Melee);
    TestTrue(TEXT("Skill target lookup also respects taunt"), ULKGameplayLibrary::LK_GetNearestEnemy(Attacker) == Melee);
    Attacker->SetTarget(Ranged); TestTrue(TEXT("Taunt overrides direct target setter"), Attacker->GetTarget() == Melee);
    Knight->RemoveTrait("Trait_KnightTauntAura");
    TestFalse(TEXT("Removing aura withdraws status immediately"), Melee->IsTaunting());
    Attacker->Tick(0.2f); TestTrue(TEXT("Focus resumes after taunt is removed"), Attacker->GetTarget() == Ranged);
    Attacker->Tick(5.1f); TestFalse(TEXT("Expired focus releases stale target"), Attacker->HasForcedTarget());
    TestTrue(TEXT("Closest target selected after focus expiry"), Attacker->GetTarget() == Melee);
    Knight->AddTrait("Trait_KnightTauntAura"); Knight->Die();
    TestFalse(TEXT("Knight death removes aura"), Melee->IsTaunting());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKDamageCardsTest, "LittleKing.Sprint5.Combat.ActualAmountsAndCardTransaction", Flags)
bool FLKDamageCardsTest::RunTest(const FString& Parameters)
{
    FLKBattleTestWorld Env; if (!Env.Open(*this) || !Env.DeployAndStart(*this)) { return false; }
    ALKUnitBase* Target = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Enemy, FVector(0.f, 400.f, 0.f));
    ALKUnitBase* Source = Env.Hero(ELKTeam::Player, "Hero_Mage");
    TestEqual(TEXT("Full-health effective healing is zero"), LKGameplay::ApplyHeal(Target, 100.f, Target), 0.f);
    TestEqual(TEXT("Healing cannot create hidden extra HP"), LKGameplay::ApplyDamage(Target, 10.f, Source), 10.f);
    TestEqual(TEXT("Only missing HP healed"), LKGameplay::ApplyHeal(Target, 100.f, Target), 10.f);
    Target->SetInvulnerable(-1.f);
    TestEqual(TEXT("Invulnerability gives zero actual damage"), LKGameplay::ApplyDamage(Target, 100.f, Source), 0.f);
    Target->SetInvulnerable(0.f);
    const float HP = Target->GetHealth();
    TestEqual(TEXT("Overkill reports remaining HP"), LKGameplay::ApplyDamage(Target, 10000.f, Source), HP);
    TestEqual(TEXT("Actual hostile damage accumulated"), Env.GM->GetMatchStats().Player.Damage, HP + 10.f);
    TestEqual(TEXT("Kill counted once"), Env.GM->GetMatchStats().Player.Kills, 1);
    ULKSilverComponent* Silver = Env.GM->GetTeamSilver(ELKTeam::Player); Silver->AddSilver(1000.f);
    ULKDeckState* Deck = Env.GM->GetTeamDeck(ELKTeam::Player);
    Deck->InitDeck({"Unit_Swordsman", "Spell_Fireball", "Unit_Archer", "Spell_HealWave", "Building_Barracks"}, 4, 7);
    while (!Deck->GetHand().Contains(FName("Unit_Swordsman"))) { Deck->PlayCard(0); }
    const int32 Slot = Deck->GetHand().IndexOfByKey(FName("Unit_Swordsman"));
    const TArray<FName> BeforeHand = Deck->GetHand(); const float BeforeSilver = Silver->GetSilver();
    FLKUnitRow& Broken = FLKSprint5TestAccess::Row(Env.GM, "Unit_Swordsman"); Broken.BaseHealth = -1.f;
    TestEqual(TEXT("Spawn failure rejected after reservation"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, FVector(0.f, -500.f, 0.f)), ELKPlayResult::InvalidCardData);
    TestEqual(TEXT("Failed resolve refunds silver"), Silver->GetSilver(), BeforeSilver);
    TestTrue(TEXT("Failed resolve leaves hand unchanged"), Deck->GetHand() == BeforeHand);
    TestEqual(TEXT("Failed play not counted"), Env.GM->GetMatchStats().Player.CardsPlayed, 0);
    Broken.BaseHealth = 140.f;
    ULKCardDefinition* Card = Env.GM->FindCard("Unit_Swordsman"); Card->Cost = 0;
    TestEqual(TEXT("Free card succeeds"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, FVector(0.f, -500.f, 0.f)), ELKPlayResult::Success);
    TestEqual(TEXT("Free card does not charge"), Silver->GetSilver(), BeforeSilver);
    TestFalse(TEXT("Successful card cycles without same-ID refill"), Deck->GetHand().Contains(FName("Unit_Swordsman")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKAttackProjectileTest, "LittleKing.Sprint5.Combat.WindupCollisionAndProjectilePool", Flags)
bool FLKAttackProjectileTest::RunTest(const FString& Parameters)
{
    FLKBattleTestWorld Env; if (!Env.Open(*this) || !Env.DeployAndStart(*this)) { return false; }
    ALKUnitBase* Attacker = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Player, FVector(-100.f, 0.f, 0.f));
    ALKUnitBase* Target = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Enemy, FVector(30.f, 0.f, 0.f));
    ALKUnitBase* Other = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Enemy, FVector(-100.f, 130.f, 0.f));
    Attacker->SetTarget(Target); FLKSprint5TestAccess::BeginWindup(Attacker);
    TestTrue(TEXT("Attack starts windup"), FLKSprint5TestAccess::WindingUp(Attacker));
    Attacker->SetTarget(Other); TestFalse(TEXT("Target switch cancels old windup"), FLKSprint5TestAccess::WindingUp(Attacker));
    FLKSprint5TestAccess::FinishWindup(Attacker, 1.f);
    TestEqual(TEXT("Cannot redirect old hit to new target"), Other->GetHealth(), Other->GetMaxHealth());
    const float Cooldown = FLKSprint5TestAccess::Cooldown(Attacker);
    Target->SetActorLocation(FVector(600.f, 0.f, 0.f)); Other->SetActorLocation(FVector(600.f, 200.f, 0.f));
    Attacker->Tick(0.5f);
    TestTrue(TEXT("Cooldown advances while chasing"), FLKSprint5TestAccess::Cooldown(Attacker) < Cooldown);
    USphereComponent* Body = Attacker->FindComponentByClass<USphereComponent>();
    const float BodySize = Body->GetScaledSphereRadius();
    Attacker->GetSpriteComponent()->SetRelativeScale3D(FVector(5.f, 2.f, 7.f));
    TestEqual(TEXT("Visual scaling never changes collision"), Body->GetScaledSphereRadius(), BodySize);
    int32 Total, Active; Env.GM->GetProjectilePoolStats(Total, Active);
    TestEqual(TEXT("Pool prewarmed"), Total, 32); TestEqual(TEXT("Pool starts inactive"), Active, 0);
    ALKProjectile* Projectile = Env.GM->AcquireProjectile(FVector(0.f, 0.f, 0.f), 25.f, ELKTeam::Player, Attacker, FVector(1.f, 0.f, 0.f));
    const float BeforeHP = Target->GetHealth(); Attacker->Die();
    Projectile->Tick(1.f);
    TestEqual(TEXT("Swept flight hits crossed target at low frame rate"), Target->GetHealth(), BeforeHP - 25.f);
    TestEqual(TEXT("Dead launcher's team keeps damage credit"), Env.GM->GetMatchStats().Player.Damage, 25.f);
    TestFalse(TEXT("Hit projectile recycled"), Projectile->IsPooledActive());
    TestFalse(TEXT("Idle pool actor tick disabled"), Projectile->IsActorTickEnabled());
    TestFalse(TEXT("Idle pool actor collision disabled"), Projectile->GetActorEnableCollision());
    TestTrue(TEXT("Idle pool actor hidden"), Projectile->IsHidden());
    ALKProjectile* Reused = Env.GM->AcquireProjectile(FVector::ZeroVector, 10.f, ELKTeam::Enemy, Other, FVector(0.f, -1.f, 0.f));
    TestTrue(TEXT("Pool reuses actor"), Reused == Projectile);
    Env.GM->ForceEndMatch(ELKTeam::Player); Env.GM->GetProjectilePoolStats(Total, Active);
    TestEqual(TEXT("Result releases all active projectiles"), Active, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSimultaneousTest, "LittleKing.Sprint5.Combat.SimultaneousEliminationAndLastHit", Flags)
bool FLKSimultaneousTest::RunTest(const FString& Parameters)
{
    FLKBattleTestWorld Env; if (!Env.Open(*this) || !Env.DeployAndStart(*this)) { return false; }
    ALKUnitBase* Soldier = Env.GM->SpawnUnitForTeam("Unit_Swordsman", ELKTeam::Enemy, FVector(0.f, 500.f, 0.f));
    LKGameplay::ApplyDamage(Soldier, 20.f, Env.Hero(ELKTeam::Player, "Hero_Mage"));
    Env.GM->GetGameData()->OvertimeWeaknessBasePct = 2.f;
    Env.GM->GetGameData()->OvertimeWeaknessGrowth = 0.f;
    FLKSprint5TestAccess::Overtime(Env.GM, Env.GM->GetGameData()->OvertimeWeaknessTick);
    TestEqual(TEXT("Both teams processed before victory"), Env.GM->GetHeroCount(ELKTeam::Player) + Env.GM->GetHeroCount(ELKTeam::Enemy), 0);
    TestEqual(TEXT("Overtime ends match"), Env.GM->GetPhase(), ELKGamePhase::Result);
    TestTrue(TEXT("Simultaneous flag recorded"), Env.GM->GetMatchStats().bSimultaneousElimination);
    TestEqual(TEXT("Actual hostile damage breaks tie without side order bias"), Env.GM->GetMatchStats().Winner, ELKTeam::Player);
    TestEqual(TEXT("Overtime damage is not player damage"), Env.GM->GetMatchStats().Player.Damage, 20.f);
    FLKBattleTestWorld Last; if (!Last.Open(*this) || !Last.DeployAndStart(*this)) { return false; }
    ALKUnitBase* Source = Last.Hero(ELKTeam::Player, "Hero_Mage"); float TotalDamage = 0.f;
    for (FName Id : Last.GM->AvailableHeroes)
    {
        ALKUnitHero* Enemy = Last.Hero(ELKTeam::Enemy, Id); TotalDamage += Enemy->GetHealth();
        LKGameplay::ApplyDamage(Enemy, 10000.f, Source);
    }
    TestEqual(TEXT("Last hit included before final stats"), Last.GM->GetMatchStats().Player.Damage, TotalDamage);
    TestEqual(TEXT("Final hero kill included"), Last.GM->GetMatchStats().Player.Kills, 3);
    TestEqual(TEXT("Result is terminal"), LKGameplay::ApplyDamage(Source, 100.f, nullptr), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKLiveBattleTest, "LittleKing.Sprint5.Integration.LiveBattleAndBarracks", Flags)
bool FLKLiveBattleTest::RunTest(const FString& Parameters)
{
    FLKBattleTestWorld Env; if (!Env.Open(*this) || !Env.DeployAndStart(*this)) { return false; }
    ALKUnitBase* Barracks = Env.GM->SpawnUnitForTeam("Building_Barracks", ELKTeam::Player, FVector(0.f, -600.f, 0.f));
    if (!TestNotNull(TEXT("Barracks fixture"), Barracks)) { return false; }
    const int32 Before = Env.GM->CountAliveUnits(ELKTeam::Player);
    Barracks->Tick(0.1f);
    TestEqual(TEXT("Barracks spawns outside its solid body"), Env.GM->CountAliveUnits(ELKTeam::Player), Before + 1);
    for (TActorIterator<ALKUnitHero> It(Env.GetTestWorld()); It; ++It) { It->SetInvulnerable(60.f); }
    for (int32 Frame = 0; Frame < 900; ++Frame)
    {
        if (Frame % 90 == 0)
        {
            Env.GM->GetTeamSilver(ELKTeam::Player)->AddSilver(5.f);
            for (int32 Slot = 0; Slot < 4; ++Slot)
            {
                const FVector Point(-500.f + (Frame / 90 % 3) * 500.f, -300.f, 0.f);
                if (Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, Point) == ELKPlayResult::Success) { break; }
            }
        }
        Env.TickTestWorld(1.f / 30.f);
        for (TActorIterator<ALKUnitHero> It(Env.GetTestWorld()); It; ++It)
        {
            TestTrue(TEXT("Live battle hero body stays inside camp"), FVector::Dist2D(It->GetCampCenter(), It->GetActorLocation()) <= It->GetCampMoveRadius() - It->GetBodyRadius() + 0.1f);
            for (TActorIterator<ALKUnitBase> Obstacle(Env.GetTestWorld()); Obstacle; ++Obstacle)
            {
                if (Obstacle->IsAlive() && Obstacle->IsBuilding())
                { TestTrue(TEXT("Live battle hero avoids buildings"), FVector::Dist2D(It->GetActorLocation(), Obstacle->GetActorLocation()) >= It->GetBodyRadius() + Obstacle->GetBodyRadius() - 0.1f); }
            }
        }
        TestTrue(TEXT("Player hand stays unique under gameplay"), Unique(Env.GM->GetTeamDeck(ELKTeam::Player)->GetHand()));
        TestTrue(TEXT("AI hand stays unique under gameplay"), Unique(Env.GM->GetTeamDeck(ELKTeam::Enemy)->GetHand()));
        for (ELKTeam Team : { ELKTeam::Player, ELKTeam::Enemy })
        {
            const ULKDeckState* Deck = Env.GM->GetTeamDeck(Team);
            TestTrue(TEXT("Both teams retain four filled slots under gameplay"), Deck->GetHandSize() == 4 && !Deck->GetHand().Contains(NAME_None));
        }
    }
    TestTrue(TEXT("AI successfully plays cards"), Env.GM->GetMatchStats().Enemy.CardsPlayed > 0);
    TestTrue(TEXT("Player successfully plays cards"), Env.GM->GetMatchStats().Player.CardsPlayed > 0);
    TestTrue(TEXT("Real world simulation produces damage"), Env.GM->GetMatchStats().Player.Damage + Env.GM->GetMatchStats().Enemy.Damage > 0.f);
    Env.GM->ForceEndMatch(ELKTeam::Player);
    int32 Total, Active; Env.GM->GetProjectilePoolStats(Total, Active);
    TestEqual(TEXT("Live match leaves no active projectile"), Active, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBuildingPreviewTest, "LittleKing.Sprint5.Placement.BuildingAttackRange", Flags)
bool FLKBuildingPreviewTest::RunTest(const FString& Parameters)
{
    FLKBattleTestWorld Env; if (!Env.Open(*this)) { return false; }
    FLKSprint5TestAccess::Row(Env.GM, "Building_ArrowTower").AttackRange = 1357.f;
    ALKUnitBase* Tower = Env.GM->SpawnUnitForTeam("Building_ArrowTower", ELKTeam::Player, FVector(0.f, -300.f, 0.f));
    if (!TestNotNull(TEXT("Tower fixture"), Tower)) { return false; }
    TestEqual(TEXT("Preview reads configured range used by spawned tower"), Env.GM->GetBuildingPlacementAttackRange("Building_ArrowTower"), Tower->GetAttackRange());
    for (FName Id : {FName("Building_Barracks"), FName("Unit_Archer"), FName("Spell_Fireball"), FName("Missing")})
    { TestEqual(TEXT("No turret range for other card kinds"), Env.GM->GetBuildingPlacementAttackRange(Id), 0.f); }
    TestTrue(TEXT("Valid default decks permit battle setup"), Env.GM->HasValidDecks());
    TestTrue(TEXT("Fixture injects unresolved card"), Env.GM->GetTeamDeck(ELKTeam::Player)->TransformCard(Env.GM->GetTeamDeck(ELKTeam::Player)->GetNextCard(), "Missing"));
    TestFalse(TEXT("Unresolved queue card blocks battle before it reaches hand"), Env.GM->HasValidDecks());
    for (int32 i = 0; i < 3; ++i) { Env.GM->DeployHero(ELKTeam::Player, Env.GM->AvailableHeroes[i], FVector((i - 1) * 700.f, -1300.f, 0.f)); }
    Env.GM->ForceStartBattle();
    TestEqual(TEXT("All heroes cannot bypass invalid deck guard"), Env.GM->GetPhase(), ELKGamePhase::Deployment);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSavedConfigTest, "LittleKing.Sprint5.Configuration.SavedDecksAndHUD", Flags)
bool FLKSavedConfigTest::RunTest(const FString& Parameters)
{
    const ULKGameData* Data = LoadObject<ULKGameData>(nullptr, TEXT("/Game/Data/DA_GameData.DA_GameData"));
    if (!TestNotNull(TEXT("Saved game data asset loads"), Data)) { return false; }
    TestEqual(TEXT("Current asset uses four hand slots"), Data->HandSize, 4);
    for (const TArray<FName>* Cards : { &Data->DefaultPlayerDeck, &Data->DefaultEnemyDeck })
    {
        ULKDeckState* Deck = NewObject<ULKDeckState>();
        if (!TestTrue(TEXT("Saved deck supports full unique hand and reserve"), Deck->InitDeck(*Cards, Data->HandSize, 97))) { continue; }
        for (FName Id : Deck->GetAllCards())
        { TestTrue(TEXT("Saved deck IDs resolve in card library"), Data->CardLibrary.ContainsByPredicate([Id](const ULKCardDefinition* Card) { return Card && Card->CardId == Id; })); }
    }
    UClass* Mode = LoadClass<ALKBattleGameMode>(nullptr, TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
    if (TestNotNull(TEXT("Saved GameMode class loads"), Mode))
    {
        const TSubclassOf<AHUD> HUD = Mode->GetDefaultObject<ALKBattleGameMode>()->HUDClass;
        TestTrue(TEXT("Saved GameMode uses native HUD for center line and range previews"), HUD && HUD->IsChildOf(ALKPresentationHUD::StaticClass()));
    }
    return true;
}

#endif
