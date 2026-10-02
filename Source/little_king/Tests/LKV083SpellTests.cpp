#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "AbilitySystemComponent.h"
#include "PaperSpriteComponent.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../ALKSpellField.h"
#include "../LKSpellExecutor.h"
#include "../LKUnitContent.h"
#include "../LKHomeContent.h"
#include "../LKCardRules.h"
#include "../LKGameplayHelpers.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKUnitStatusComponent.h"
#include "../ULKUnitAnimationComponent.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKUnitAttributeSet.h"
#include "../ULKDeckState.h"
#include "../ULKSilverComponent.h"
#include "../ULKRunSubsystem.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FSpellWorld : FTestWorldWrapper
{
    ALKBattleGameMode* GM = nullptr;
    bool Open(FAutomationTestBase& Test, int32 SpellLevel = 0)
    {
        if (!CreateTestWorld(EWorldType::Game)) { ForwardErrorMessages(&Test); return false; }
        if (SpellLevel > 0)
        {
            auto* Run = GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
            Run->ConfigureStorage(TEXT("LittleKing_V083_Spells_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
            Run->SetAutoSaveEnabled(false);
            TArray<FLKRunHeroState> Heroes;
            for (FName Id : LKHomeContent::DefaultUnlockedHeroes())
            {
                FLKRunHeroState Hero; Hero.HeroId = Id;
                Hero.Health = Hero.MaxHealth = Hero.BaseMaxHealth = LKUnitContent::Find(Id)->BaseHealth;
                if (Id == "Hero_Mage") { Hero.Traits = { "Trait_MageSpellReach" }; }
                if (Id == "Hero_Knight") { Hero.Traits = { "Trait_KnightTauntAura" }; }
                Heroes.Add(Hero);
            }
            TArray<FLKRunCardState> Cards;
            for (FName Id : { FName("Spell_MariaNovice"), FName("Spell_MariaIntermediate"), FName("Spell_MariaAdvanced"), FName("Spell_MariaDivine"),
                FName("Spell_Lightning"), FName("Spell_Freeze"), FName("Spell_DivineBlessing") })
            { FLKRunCardState Card; Card.CardId = Id; Card.UpgradeLevel = SpellLevel; Cards.Add(Card); }
            if (!Test.TestTrue(TEXT("Create an isolated upgraded expedition"), Run->StartNewRun(Heroes, Cards, 833))) { return false; }
        }
        TSubclassOf<ALKBattleGameMode> Mode = ALKBattleGameMode::StaticClass();
        if (SpellLevel > 0)
        {
            // The native mode deliberately stays a standalone battle. The authored mode
            // owns GameData and applies the real RunSubsystem context during BeginPlay.
            Mode = LoadClass<ALKBattleGameMode>(nullptr,
                TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
            if (!Test.TestTrue(TEXT("Authored expedition GameMode loads"), bool(Mode))) { return false; }
        }
        GetTestWorld()->GetWorldSettings()->DefaultGameMode = Mode;
        if (!BeginPlayInTestWorld()) { ForwardErrorMessages(&Test); return false; }
        GM = GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        if (!Test.TestNotNull(TEXT("Real battle authority exists"), GM)) { return false; }
        if (SpellLevel > 0 && !Test.TestTrue(TEXT("Upgraded fixture applies its actual expedition context"), GM->IsExpeditionBattle())) { return false; }
        GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes = false; GM->GetGameData()->bDrawFieldBounds = false;
        GM->GetGameData()->MaxUnitsPerTeam = 80;
        for (int32 I = 0; I < GM->AvailableHeroes.Num(); ++I)
        {
            if (!Test.TestEqual(TEXT("Deploy required player hero"), GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[I], FVector((I - 1) * 700, -1300, 0)), ELKPlayResult::Success)) { return false; }
        }
        GM->ForceStartBattle();
        return Test.TestEqual(TEXT("Enter actual battle phase"), GM->GetPhase(), ELKGamePhase::Battle);
    }
    ALKUnitBase* Spawn(FName Id, FVector P, ELKTeam Team = ELKTeam::Player) const { return GM->SpawnUnitForTeam(Id, Team, P); }
    ALKUnitBase* Hero(FName Id, ELKTeam Team = ELKTeam::Player) const
    {
        for (TActorIterator<ALKUnitBase> It(GetTestWorld()); It; ++It) { if (It->GetUnitId() == Id && It->GetTeam() == Team && It->IsHero()) { return *It; } }
        return nullptr;
    }
    void Health(ALKUnitBase* Unit, float HP) const { Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), HP); }
    void MaxHealth(ALKUnitBase* Unit, float HP) const { Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetMaxHealthAttribute(), HP); }
    TArray<ALKSpellField*> Fields() const
    {
        TArray<ALKSpellField*> Result;
        for (TActorIterator<ALKSpellField> It(GetTestWorld()); It; ++It) { Result.Add(*It); }
        return Result;
    }
    TArray<ALKUnitBase*> Summoned() const
    {
        TArray<ALKUnitBase*> Result;
        for (TActorIterator<ALKUnitBase> It(GetTestWorld()); It; ++It)
        {
            const FString Id = It->GetUnitId().ToString();
            if (It->GetTeam() == ELKTeam::Player && (Id.StartsWith(TEXT("Unit_Angel")) || Id == TEXT("Unit_DivineJudge") || Id == TEXT("Unit_ChosenHighPriest"))) { Result.Add(*It); }
        }
        return Result;
    }
    int32 PutInHand(FAutomationTestBase& Test, FName Id) const
    {
        auto* Deck = GM->GetTeamDeck(ELKTeam::Player);
        if (!Test.TestNotNull(TEXT("Actual player deck exists"), Deck)) { return INDEX_NONE; }
        TArray<FName> Cards = LKHomeContent::DefaultUnlockedCards(); Cards[0] = Id;
        for (int32 Seed = 0; Seed < 128; ++Seed)
        {
            if (!Deck->InitDeck(Cards, 4, Seed)) { return INDEX_NONE; }
            const int32 Slot = Deck->GetHand().IndexOfByKey(Id);
            if (Slot != INDEX_NONE) { return Slot; }
        }
        return INDEX_NONE;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083FreezeSpell, "LittleKing.V083.Spells.FreezeDurationAndTeamFilter", Flags)
bool FLKV083FreezeSpell::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this)) { return false; }
    const FVector P(0, -200, 0);
    auto* Enemy = Env.Spawn("Unit_Skeleton", P + FVector(100, 0, 0), ELKTeam::Enemy);
    auto* Friend = Env.Spawn("Unit_Swordsman", P + FVector(-100, 0, 0));
    auto* Outside = Env.Spawn("Unit_SkeletonArcher", P + FVector(600, 0, 0), ELKTeam::Enemy);
    if (!Enemy || !Friend || !Outside) { return false; }
    TestTrue(TEXT("Freeze executes through common spell entry"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_Freeze"), ELKTeam::Player, P));
    TestTrue(TEXT("Hostile in the circle is frozen"), Enemy->GetStatusComponent()->IsFrozen());
    TestFalse(TEXT("Friendly in the circle is unaffected"), Friend->GetStatusComponent()->IsFrozen());
    TestFalse(TEXT("Hostile outside the circle is unaffected"), Outside->GetStatusComponent()->IsFrozen());
    Enemy->GetStatusComponent()->TickStatus(2.99f);
    TestTrue(TEXT("Freeze lasts until the full three seconds"), Enemy->GetStatusComponent()->IsFrozen());
    Enemy->GetStatusComponent()->TickStatus(.02f);
    TestFalse(TEXT("Freeze expires after three seconds"), Enemy->GetStatusComponent()->IsFrozen());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083Reinforcements, "LittleKing.V083.Spells.ReinforcementGradesPlacementAndTransaction", Flags)
bool FLKV083Reinforcements::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this)) { return false; }
    auto* Mage = Env.Hero("Hero_Mage"); if (!TestNotNull(TEXT("Mage present"), Mage)) { return false; }
    Mage->Die();
    TestFalse(TEXT("Ordinary enemy-half placement closes when mage is incapacitated"), Env.GM->CanPlaceSpellAt(ELKTeam::Player, FVector(650, 600, 0)));
    const FName SpellIds[] = { "Spell_MariaNovice", "Spell_MariaIntermediate", "Spell_MariaAdvanced", "Spell_MariaDivine" };
    const ELKQuality Qualities[] = { ELKQuality::Uncommon, ELKQuality::Rare, ELKQuality::Epic, ELKQuality::Legendary };
    int32 ExpectedTotal = 0;
    const float HealingBefore = Env.GM->GetMatchStats().Player.Healing;
    for (int32 I = 0; I < 4; ++I)
    {
        const auto* Card = Env.GM->FindCard(SpellIds[I]);
        if (!TestNotNull(TEXT("Registered reinforcement spell exists"), Card)) { return false; }
        TArray<FVector> Planned;
        const FVector Point((I - 1) * 430, 550, 0);
        TestTrue(TEXT("Actual summon positions can be planned"), LKSpellExecutor::PlanSummons(Env.GM, Card, ELKTeam::Player, Point, Planned));
        TestEqual(TEXT("All required summons are planned"), Planned.Num(), I == 3 ? 2 : 3);
        TestEqual(TEXT("Reinforcement ignores the closed mage placement trait"), LKSpellExecutor::Validate(Env.GM, Card, ELKTeam::Player, Point), ELKPlayResult::Success);
        TestTrue(TEXT("Whole reinforcement squad executes"), LKSpellExecutor::Execute(Env.GM, Card, ELKTeam::Player, Point));
        ExpectedTotal += I == 3 ? 2 : 3;
        TestEqual(TEXT("No summon is silently dropped"), Env.Summoned().Num(), ExpectedTotal);
        const auto Summons = Env.Summoned();
        for (FName Id : Card->SummonedUnitIds)
        {
            const auto* Row = LKUnitContent::Find(Id);
            const auto* Unit = Summons.FindByPredicate([Id](const auto* U) { return U->GetUnitId() == Id; });
            if (!TestNotNull(TEXT("Requested summon exists"), Unit) || !Row) { return false; }
            TestEqual(TEXT("Summoned quality matches its reinforcement tier"), Row->Quality, Qualities[I]);
            TestEqual(TEXT("Healer profession is applied to the live unit"), (*Unit)->BasicAttackHeals(), Row->bBasicAttackHeals);
            TestEqual(TEXT("Summon starts at full health"), (*Unit)->GetHealth(), (*Unit)->GetMaxHealth());
        }
    }
    TestEqual(TEXT("Spawning is not reported as healing"), Env.GM->GetMatchStats().Player.Healing, HealingBefore);
    const int32 Slot = Env.PutInHand(*this, "Spell_MariaNovice");
    auto* Deck = Env.GM->GetTeamDeck(ELKTeam::Player); auto* Silver = Env.GM->GetTeamSilver(ELKTeam::Player);
    if (Slot == INDEX_NONE || !Silver) { return false; }
    Silver->SetCap(20); Silver->AddSilver(20);
    const auto BeforeHand = Deck->GetHand(); const float BeforeSilver = Silver->GetSilver();
    TestEqual(TEXT("Outside field rejects a real card play"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, FVector(Env.GM->GetGameData()->FieldHalfWidth + 100, 500, 0)), ELKPlayResult::InvalidLocation);
    TestTrue(TEXT("Rejected location preserves the complete hand"), Deck->GetHand() == BeforeHand);
    TestEqual(TEXT("Rejected location does not charge silver"), Silver->GetSilver(), BeforeSilver);
    Env.GM->GetGameData()->MaxUnitsPerTeam = Env.GM->CountAliveUnits(ELKTeam::Player) + 2;
    TestEqual(TEXT("Three-person squad is rejected before exceeding the cap"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, FVector(0, 950, 0)), ELKPlayResult::UnitLimitReached);
    TestTrue(TEXT("Rejected cap preserves the complete hand"), Deck->GetHand() == BeforeHand);
    TestEqual(TEXT("Rejected cap does not charge silver"), Silver->GetSilver(), BeforeSilver);
    TestEqual(TEXT("Rejected cap creates no partial squad"), Env.Summoned().Num(), ExpectedTotal);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083SummonUpgrade, "LittleKing.V083.Spells.SummonUpgradeUsesInitializationNotHealing", Flags)
bool FLKV083SummonUpgrade::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this, 2)) { return false; }
    const float Scale = LKCardRules::UpgradeMultiplier(2);
    TestTrue(TEXT("Actual battle carries expedition card upgrade"), FMath::IsNearlyEqual(Env.GM->GetCardUpgradeScale("Spell_MariaNovice"), Scale));
    const float HealingBefore = Env.GM->GetMatchStats().Player.Healing;
    TestTrue(TEXT("Upgraded reinforcement executes"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_MariaNovice"), ELKTeam::Player, FVector(700, 0, 0)));
    for (auto* Unit : Env.Summoned())
    {
        const auto* Row = LKUnitContent::Find(Unit->GetUnitId());
        TestTrue(TEXT("Life is multiplied once"), FMath::IsNearlyEqual(Unit->GetMaxHealth(), Row->BaseHealth * Scale, .01f));
        TestTrue(TEXT("Current and base maximum life start at the same upgraded value"), FMath::IsNearlyEqual(Unit->GetHealth(), Unit->GetMaxHealth(), .01f) && FMath::IsNearlyEqual(Unit->GetBaseMaxHealth(), Unit->GetMaxHealth(), .01f));
        TestTrue(TEXT("Attack or healing is multiplied once"), FMath::IsNearlyEqual(Unit->GetAttackDamage(), Row->AttackDamage * Scale, .01f));
        TestTrue(TEXT("Attack interval does not change"), FMath::IsNearlyEqual(Unit->GetAttackInterval(), Row->AttackInterval, .01f));
    }
    TestEqual(TEXT("Upgraded squad contains all three roles"), Env.Summoned().Num(), 3);
    TestEqual(TEXT("Initial stat scaling never counts as actual healing"), Env.GM->GetMatchStats().Player.Healing, HealingBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083LightningSpell, "LittleKing.V083.Spells.LightningCurrentHealthRetargetRangeAndResult", Flags)
bool FLKV083LightningSpell::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this)) { return false; }
    const FVector P(0, -100, 0);
    auto* Highest = Env.Spawn("Unit_Colossus", P, ELKTeam::Enemy);
    auto* Lowest = Env.Spawn("Unit_Skeleton", P + FVector(0, 180, 0), ELKTeam::Enemy);
    auto* FutureLowest = Env.Spawn("Unit_Colossus", P + FVector(180, 0, 0), ELKTeam::Enemy);
    auto* Friendly = Env.Spawn("Unit_Colossus", P + FVector(-180, 0, 0));
    auto* Hero = Env.Spawn("Hero_Knight", P + FVector(0, -230, 0), ELKTeam::Enemy);
    if (!Highest || !Lowest || !FutureLowest || !Friendly || !Hero) { return false; }
    Env.MaxHealth(Highest, 2000); Env.Health(Highest, 1000); Env.Health(Lowest, 250); Env.Health(FutureLowest, 500); Env.Health(Friendly, 5);
    const float HeroHP = Hero->GetHealth();
    TestTrue(TEXT("Lightning executes"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_Lightning"), ELKTeam::Player, P));
    TestEqual(TEXT("First hit selects current highest eligible life and caps at 200"), Highest->GetHealth(), 800.f);
    TestEqual(TEXT("Friendly remains untouched despite being weakest"), Friendly->GetHealth(), 5.f);
    TestEqual(TEXT("Hero remains untouched despite being strongest"), Hero->GetHealth(), HeroHP);
    const auto Fields = Env.Fields(); if (!TestEqual(TEXT("One delayed strike is owned by a field actor"), Fields.Num(), 1)) { return false; }
    Env.Health(FutureLowest, 110); Fields[0]->Tick(.19f);
    TestEqual(TEXT("Second hit waits for the full delay"), FutureLowest->GetHealth(), 110.f);
    Fields[0]->Tick(.02f);
    TestEqual(TEXT("Second hit reselects the current weakest at execution time"), FutureLowest->GetHealth(), 10.f);
    TestEqual(TEXT("The previously weakest target is not locked"), Lowest->GetHealth(), 250.f);
    TestTrue(TEXT("Delayed actor releases after striking"), Env.Fields().IsEmpty());

    Env.Health(Highest, 1000); Env.Health(FutureLowest, 110);
    TestTrue(TEXT("Second lightning cast executes"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_Lightning"), ELKTeam::Player, P));
    const auto MovingFields = Env.Fields(); if (MovingFields.Num() != 1) { return false; }
    FutureLowest->SetActorLocation(P + FVector(800, 0, 0));
    MovingFields[0]->Tick(.2f);
    TestEqual(TEXT("Target that left the circle is not hit by the delayed strike"), FutureLowest->GetHealth(), 110.f);
    TestEqual(TEXT("Second strike now selects the weakest still inside"), Lowest->GetHealth(), 150.f);

    Env.Health(Highest, 1000);
    TestTrue(TEXT("Third delayed cast starts before result"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_Lightning"), ELKTeam::Player, P));
    const auto ResultFields = Env.Fields(); if (ResultFields.Num() != 1) { return false; }
    const float LowestBeforeResult = Lowest->GetHealth(); Env.GM->ForceEndMatch(ELKTeam::Player);
    if (IsValid(ResultFields[0])) { ResultFields[0]->Tick(10.f); }
    TestEqual(TEXT("No delayed damage can occur after results"), Lowest->GetHealth(), LowestBeforeResult);
    TestTrue(TEXT("Result clears the delayed field"), Env.Fields().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083BlessingSpell, "LittleKing.V083.Spells.DivineBlessingAlliesIncapacityAndDrawCooldown", Flags)
bool FLKV083BlessingSpell::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this)) { return false; }
    auto* Knight = Env.Hero("Hero_Knight"); auto* Mage = Env.Hero("Hero_Mage");
    auto* Ally = Env.Spawn("Unit_Colossus", FVector(-800, 700, 0));
    auto* Tower = Env.Spawn("Building_ArrowTower", FVector(800, -700, 0));
    auto* Enemy = Env.Spawn("Unit_Colossus", FVector(0, -300, 0), ELKTeam::Enemy);
    if (!Knight || !Mage || !Ally || !Tower || !Enemy) { return false; }
    Env.Health(Knight, Knight->GetMaxHealth() * .25f); Env.Health(Ally, 10); Env.Health(Tower, 100); Env.Health(Enemy, 10); Mage->Die();
    const float HeroBefore = Knight->GetHealth();
    const int32 Slot = Env.PutInHand(*this, "Spell_DivineBlessing");
    auto* Silver = Env.GM->GetTeamSilver(ELKTeam::Player); auto* Deck = Env.GM->GetTeamDeck(ELKTeam::Player);
    if (Slot == INDEX_NONE || !Silver || !Deck) { return false; }
    Silver->SetCap(20); Silver->AddSilver(20); const float SilverBefore = Silver->GetSilver();
    TestEqual(TEXT("Global blessing remains legal without mage and without a circle radius"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, FVector(0, 800, 0)), ELKPlayResult::Success);
    TestEqual(TEXT("Friendly nonhero heals fully anywhere"), Ally->GetHealth(), Ally->GetMaxHealth());
    TestEqual(TEXT("Friendly combat building heals fully"), Tower->GetHealth(), Tower->GetMaxHealth());
    TestTrue(TEXT("Hero heals exactly twenty percent of maximum"), FMath::IsNearlyEqual(Knight->GetHealth(), HeroBefore + Knight->GetMaxHealth() * .2f, .01f));
    TestEqual(TEXT("Enemy is not healed"), Enemy->GetHealth(), 10.f);
    TestEqual(TEXT("Incapacitated mage stays at zero"), Mage->GetHealth(), 0.f); TestTrue(TEXT("Blessing does not revive the mage"), Mage->IsIncapacitated());
    TestEqual(TEXT("Successful blessing charges exactly eight silver"), Silver->GetSilver(), SilverBefore - 8.f);
    TestEqual(TEXT("Successful blessing starts draw cooldown"), Deck->GetCooldownRemaining("Spell_DivineBlessing"), 150.f);
    TestEqual(TEXT("Full hand remains present"), Deck->GetHandSize(), 4);
    TestFalse(TEXT("Cooling blessing is not in the hand"), Deck->GetHand().Contains("Spell_DivineBlessing"));
    for (int32 I = 0; I < 28; ++I)
    {
        TestFalse(TEXT("Next eligible card skips the blessing"), Deck->GetNextCard() == "Spell_DivineBlessing");
        TestFalse(TEXT("Playing a normal card always succeeds"), Deck->PlayCard(I % 4).IsNone());
        TestFalse(TEXT("Cooling blessing cannot be drawn through repeated cycling"), Deck->GetHand().Contains("Spell_DivineBlessing"));
        TestEqual(TEXT("Cycling never empties a hand slot"), Deck->GetHandSize(), 4);
    }
    Deck->AdvanceCooldowns(149.f); TestEqual(TEXT("Cooldown still excludes until expiry"), Deck->GetCooldownRemaining("Spell_DivineBlessing"), 1.f);
    Deck->AdvanceCooldowns(1.f); TestEqual(TEXT("Cooldown expires at 150 seconds"), Deck->GetCooldownRemaining("Spell_DivineBlessing"), 0.f);
    bool bReturned = false;
    for (int32 I = 0; I < 12 && !bReturned; ++I) { Deck->PlayCard(I % 4); bReturned = Deck->GetHand().Contains("Spell_DivineBlessing"); }
    TestTrue(TEXT("After expiry the blessing returns through ordinary cycling"), bReturned);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083CloudField, "LittleKing.V083.Spells.BlackCloudDynamicOverlapDiscoveryAndCleanup", Flags)
bool FLKV083CloudField::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this)) { return false; }
    const FVector P(0, -100, 0);
    auto* Friendly = Env.Spawn("Unit_Colossus", P);
    auto* Enemy = Env.Spawn("Unit_Skeleton", P + FVector(120, 0, 0), ELKTeam::Enemy);
    auto* Observer = Env.Spawn("Unit_Archer", P + FVector(-600, 0, 0));
    if (!Friendly || !Enemy || !Observer) { return false; }
    Observer->SetTarget(Enemy);
    TestTrue(TEXT("Enemy starts discoverable"), Observer->CanDiscoverTarget(Enemy));
    TestTrue(TEXT("First cloud executes"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_BlackCloud"), ELKTeam::Player, P));
    auto Fields = Env.Fields(); if (Fields.Num() != 1) { return false; }
    auto* A = Fields[0]; A->Tick(.01f);
    TestTrue(TEXT("Player unit inside is concealed"), Friendly->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Enemy unit inside is also concealed"), Enemy->GetStatusComponent()->IsConcealed());
    TestFalse(TEXT("Outside observer cannot discover hidden enemy"), Observer->CanDiscoverTarget(Enemy));
    TestTrue(TEXT("Attackers immediately release a target hidden by the cloud"), Observer->GetTarget() != Enemy);
    const float HP = Enemy->GetHealth();
    TestTrue(TEXT("Ground spell can hit a concealed unit"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_Fireball"), ELKTeam::Player, P));
    TestEqual(TEXT("Concealment does not grant damage immunity"), Enemy->GetHealth(), HP - Env.GM->FindCard("Spell_Fireball")->SpellValue);
    Enemy->SetActorLocation(P + FVector(700, 0, 0)); A->Tick(.01f);
    TestFalse(TEXT("Leaving cloud immediately clears its concealment"), Enemy->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Leaving cloud restores discoverability"), Observer->CanDiscoverTarget(Enemy));
    Enemy->SetActorLocation(P + FVector(220, 0, 0)); A->Tick(.01f);
    TestTrue(TEXT("Entering the existing cloud applies concealment dynamically"), Enemy->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Overlapping cloud executes"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_BlackCloud"), ELKTeam::Player, P + FVector(500, 0, 0)));
    Fields = Env.Fields(); if (Fields.Num() != 2) { return false; }
    ALKSpellField* B = Fields[0] == A ? Fields[1] : Fields[0]; B->Tick(.01f);
    TestTrue(TEXT("Distinct live cloud actors own separate concealment sources"), Enemy->GetStatusComponent()->HasConcealment(A->GetFName()) && Enemy->GetStatusComponent()->HasConcealment(B->GetFName()));
    Enemy->SetActorLocation(P + FVector(700, 0, 0)); A->Tick(.01f);
    TestFalse(TEXT("Leaving first cloud removes only its source"), Enemy->GetStatusComponent()->HasConcealment(A->GetFName()));
    TestTrue(TEXT("Second cloud retains concealment"), Enemy->GetStatusComponent()->HasConcealment(B->GetFName()));
    B->Destroy(); TestFalse(TEXT("Destroying the final source restores visibility"), Enemy->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("First cloud still covers the friendly"), Friendly->GetStatusComponent()->IsConcealed());
    A->Tick(5.f); TestFalse(TEXT("Natural expiry releases the friendly"), Friendly->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Natural expiry destroys the cloud actor"), Env.Fields().IsEmpty());
    TestTrue(TEXT("Cloud can start again before result"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_BlackCloud"), ELKTeam::Player, P));
    Env.GM->ForceEndMatch(ELKTeam::Player);
    TestFalse(TEXT("Result clears all cloud concealment"), Friendly->GetStatusComponent()->IsConcealed());
    TestTrue(TEXT("Result releases all timed field actors"), Env.Fields().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083HurricaneField, "LittleKing.V083.Spells.HurricaneDirectionRectangleFixedBuildingsAndExpiry", Flags)
bool FLKV083HurricaneField::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this)) { return false; }
    const FVector P(0, -100, 0);
    auto* Friend = Env.Spawn("Unit_Colossus", P + FVector(-180, 0, 0));
    auto* Enemy = Env.Spawn("Unit_Skeleton", P + FVector(180, 0, 0), ELKTeam::Enemy);
    auto* Outside = Env.Spawn("Unit_Colossus", P + FVector(700, 0, 0));
    auto* Tower = Env.Spawn("Building_ArrowTower", P + FVector(0, 220, 0));
    ALKUnitBase* Camp = nullptr;
    for (TActorIterator<ALKUnitBase> It(Env.GetTestWorld()); It; ++It) { if (It->GetTeam() == ELKTeam::Player && It->IsCamp()) { Camp = *It; break; } }
    if (!Friend || !Enemy || !Outside || !Tower || !Camp) { return false; }
    Camp->SetActorLocation(P + FVector(0, -220, 0));
    const FVector FP = Friend->GetActorLocation(), EP = Enemy->GetActorLocation(), OP = Outside->GetActorLocation(), TP = Tower->GetActorLocation(), CP = Camp->GetActorLocation();
    Friend->GetStatusComponent()->Freeze(3.f);
    TestTrue(TEXT("Hurricane executes"), LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_Hurricane"), ELKTeam::Player, P));
    auto Fields = Env.Fields(); if (Fields.Num() != 1) { return false; }
    auto* Wind = Fields[0]; Wind->Tick(.5f);
    TestTrue(TEXT("Frozen friendly is forced toward battlefield right, world +Y"), Friend->GetActorLocation().Y > FP.Y);
    TestTrue(TEXT("Enemy is also forced toward world +Y"), Enemy->GetActorLocation().Y > EP.Y);
    TestTrue(TEXT("No accidental X drift"), FMath::IsNearlyEqual(Friend->GetActorLocation().X, FP.X, .01f) && FMath::IsNearlyEqual(Enemy->GetActorLocation().X, EP.X, .01f));
    TestEqual(TEXT("Character outside rectangle stays fixed"), Outside->GetActorLocation(), OP);
    TestEqual(TEXT("Attack building stays fixed"), Tower->GetActorLocation(), TP);
    TestEqual(TEXT("Hero camp stays fixed"), Camp->GetActorLocation(), CP);
    TestTrue(TEXT("Hurricane keeps the remaining duration"), FMath::IsNearlyEqual(Wind->GetRemaining(), 4.5f, .01f));
    Wind->Tick(10.f);
    TestTrue(TEXT("Oversized final step ends exactly once"), Env.Fields().IsEmpty());
    const FVector End = Friend->GetActorLocation();
    if (IsValid(Wind)) { Wind->Tick(10.f); }
    TestEqual(TEXT("Expired field never pushes again"), Friend->GetActorLocation(), End);

    // Exercise the engine's real tick groups rather than manually calling the
    // field before animation, which would hide a same-frame ordering regression.
    Enemy->SetActorLocation(P + FVector(500.f, -100.f, 0.f));
    Enemy->SetTarget(nullptr);
    Enemy->GetMovementComponent()->Stop();
    Enemy->GetMovementComponent()->ResetVisualTravel();
    Enemy->GetAnimationComponent()->ResetPresentation();
    Enemy->GetAnimationComponent()->TickComponent(0.f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Enemy initially faces battlefield left"), Enemy->GetAnimationComponent()->IsFacingRight());
    TestTrue(TEXT("Initial enemy artwork is actually mirrored left"), Enemy->GetSpriteComponent()->GetRelativeScale3D().X < 0.f);
    Enemy->GetStatusComponent()->Freeze(3.f);
    const FVector BeforeWorldTick = Enemy->GetActorLocation();
    const float FrozenWalkPhase = Enemy->GetAnimationComponent()->GetWalkPhase();
    if (!TestTrue(TEXT("A fresh hurricane starts for tick-order acceptance"),
        LKSpellExecutor::Execute(Env.GM, Env.GM->FindCard("Spell_Hurricane"), ELKTeam::Player, P))) { return false; }
    // The engine wrapper also increments GFrameCounter; calling World::Tick twice
    // in one automation frame would skip already visited actor/component ticks.
    if (!TestTrue(TEXT("The first real test-world frame advances"), Env.TickTestWorld(.1f))) { return false; }
    TestTrue(TEXT("Real world tick pushes the frozen enemy right this frame"), Enemy->GetActorLocation().Y > BeforeWorldTick.Y);
    TestTrue(TEXT("Facing follows this frame's forced displacement immediately"), Enemy->GetAnimationComponent()->IsFacingRight());
    TestTrue(TEXT("The rendered sprite faces right in the same frame"), Enemy->GetSpriteComponent()->GetRelativeScale3D().X > 0.f);
    TestEqual(TEXT("A frozen unit slides without advancing its walking cycle"), Enemy->GetAnimationComponent()->GetWalkPhase(), FrozenWalkPhase);
    TestFalse(TEXT("A frozen unit does not select the walking pose"), Enemy->GetAnimationComponent()->GetVisualState() == FName("Move"));
    TestTrue(TEXT("The world-ticked field keeps its wind source while inside"), Enemy->GetStatusComponent()->IsWindDriven());
    const auto WorldFields = Env.Fields();
    if (!TestEqual(TEXT("The active hurricane remains present after the first frame"), WorldFields.Num(), 1)) { return false; }
    const float BeforeLeavingFrame = WorldFields[0]->GetRemaining();
    Enemy->SetActorLocation(P + FVector(500.f, 500.f, 0.f));
    if (!TestTrue(TEXT("The second real test-world frame advances"), Env.TickTestWorld(.1f))) { return false; }
    TestTrue(TEXT("The second frame actually executes the field tick"), WorldFields[0]->GetRemaining() < BeforeLeavingFrame);
    TestFalse(TEXT("Leaving the rectangle clears wind through the real world tick"), Enemy->GetStatusComponent()->IsWindDriven());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKV083BadSpellParameters, "LittleKing.V083.Spells.MalformedSummonAndFieldParametersAreAtomic", Flags)
bool FLKV083BadSpellParameters::RunTest(const FString&)
{
    FSpellWorld Env; if (!Env.Open(*this)) { return false; }
    const int32 Slot = Env.PutInHand(*this, "Spell_MariaNovice");
    auto* Deck = Env.GM->GetTeamDeck(ELKTeam::Player); auto* Silver = Env.GM->GetTeamSilver(ELKTeam::Player);
    if (Slot == INDEX_NONE || !Deck || !Silver) { return false; }
    Silver->SetCap(20); Silver->AddSilver(20);
    const auto HandBefore = Deck->GetHand(); const float SilverBefore = Silver->GetSilver();
    const FName BrokenIds[] = { "Unit_AngelArcher_Uncommon", "Unit_AngelPriest_Uncommon" };
    for (FName Id : BrokenIds)
    {
        // Only the GameMode's private merged row is edited, emulating malformed authored tuning.
        const auto* Merged = Env.GM->GetUnitRow(Id);
        if (!Merged || !TestTrue(TEXT("Fault injection never mutates the canonical global registry"), Merged != LKUnitContent::Find(Id))) { return false; }
        auto* BadRow = const_cast<FLKUnitRow*>(Merged); const float OriginalHealth = BadRow->BaseHealth;
        BadRow->BaseHealth = std::numeric_limits<float>::quiet_NaN();
        TArray<FVector> Positions;
        TestFalse(TEXT("Plan rejects malformed second or third role before spawning"), LKSpellExecutor::PlanSummons(Env.GM, Env.GM->FindCard("Spell_MariaNovice"), ELKTeam::Player, FVector(650, 0, 0), Positions));
        TestTrue(TEXT("Failed squad planning publishes no partial positions"), Positions.IsEmpty());
        TestFalse(TEXT("Malformed summon cannot resolve a paid card"), Env.GM->PlayCardForTeam(ELKTeam::Player, Slot, FVector(650, 0, 0)) == ELKPlayResult::Success);
        BadRow->BaseHealth = OriginalHealth;
        TestEqual(TEXT("No partially summoned units remain"), Env.Summoned().Num(), 0);
        TestTrue(TEXT("Malformed tuning preserves the hand"), Deck->GetHand() == HandBefore);
        TestEqual(TEXT("Malformed tuning never charges silver"), Silver->GetSilver(), SilverBefore);
    }
    FLKCombatSource Source; Source.bHasTeam = true; Source.Team = ELKTeam::Player; Source.Kind = ELKCombatSourceKind::Spell;
    auto RejectField = [&](ULKCardDefinition* Card, float Scale, const TCHAR* Reason)
    {
        auto* Field = Env.GetTestWorld()->SpawnActor<ALKSpellField>();
        if (!TestNotNull(TEXT("Field actor exists for direct parameter validation"), Field)) { return; }
        TestFalse(Reason, Field->Initialize(Env.GM, Card, ELKTeam::Player, FVector(0, -100, 0), Scale, Source));
        Field->Destroy();
    };
    auto* BadWind = DuplicateObject<ULKCardDefinition>(Env.GM->FindCard("Spell_Hurricane"), GetTransientPackage());
    BadWind->SpellHalfExtents.Y = 0.f; RejectField(BadWind, 1.f, TEXT("Zero half-height cannot reach the wind visual modulo denominator"));
    BadWind->SpellHalfExtents.Y = std::numeric_limits<float>::quiet_NaN(); RejectField(BadWind, 1.f, TEXT("NaN half-height is rejected"));
    BadWind->SpellHalfExtents.Y = 350.f; BadWind->ForceMoveSpeed = std::numeric_limits<float>::max();
    RejectField(BadWind, 2.f, TEXT("Overflow in upgraded force speed is rejected"));
    RejectField(Env.GM->FindCard("Spell_Hurricane"), std::numeric_limits<float>::quiet_NaN(), TEXT("NaN upgrade scale is rejected"));
    auto* BadLightning = DuplicateObject<ULKCardDefinition>(Env.GM->FindCard("Spell_Lightning"), GetTransientPackage());
    BadLightning->SecondarySpellValue = std::numeric_limits<float>::quiet_NaN(); RejectField(BadLightning, 1.f, TEXT("NaN delayed damage cannot initialize a live field"));
    TestTrue(TEXT("All rejected fields have been removed"), Env.Fields().IsEmpty());
    return true;
}
#endif
