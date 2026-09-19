#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../LKCardPresentation.h"
#include "../LKCardRules.h"
#include "../ULKUnitStatusComponent.h"
#include "../ALKProjectile.h"
#include "../LKExpeditionMercenaryContent.h"
#include "../LKHomeContent.h"
#include "../LKUnitContent.h"
#include "../LKGameplayHelpers.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKDeckState.h"
#include "../ULKUnitActiveComponent.h"
#include "../ULKUnitPassiveComponent.h"
#include "../ULKUnitMovementComponent.h"
#include "../ULKUnitAttributeSet.h"
#include "../ULKRunSubsystem.h"
#include "../ULKRunSaveGame.h"
#include "../ULKSilverComponent.h"
#include "../ULKBattleHUDWidget.h"
#include "../ULKRunRewardWidget.h"
#include "../ULKHomeListButtonWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"

namespace
{
constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
struct FCharacterWorld : FTestWorldWrapper
{
    ALKBattleGameMode* GM = nullptr;
    bool Open(FAutomationTestBase& Test)
    {
        if (!CreateTestWorld(EWorldType::Game)) { return false; }
        GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
        if (!BeginPlayInTestWorld()) { ForwardErrorMessages(&Test); return false; }
        GM = GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
        GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
        GM->GetGameData()->bDrawDebugShapes = false;
        for (int32 I = 0; I < GM->AvailableHeroes.Num(); ++I)
        {
            if (GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[I], FVector((I - 1) * 700, -1300, 0)) != ELKPlayResult::Success) { return false; }
        }
        GM->ForceStartBattle();
        return Test.TestEqual(TEXT("Battle starts"), GM->GetPhase(), ELKGamePhase::Battle);
    }
    ALKUnitBase* Spawn(FName Id, FVector Position, ELKTeam Team = ELKTeam::Player)
    { return GM->SpawnUnitForTeam(Id, Team, Position); }
    ALKUnitBase* Hero(FName Id)
    {
        for (TActorIterator<ALKUnitBase> It(GetTestWorld()); It; ++It)
        { if (It->GetUnitId() == Id && It->GetTeam() == ELKTeam::Player) { return *It; } }
        return nullptr;
    }
    void Health(ALKUnitBase* Unit, float Value)
    { Unit->GetAbilitySystemComponent()->SetNumericAttributeBase(ULKUnitAttributeSet::GetHealthAttribute(), Value); }
};
bool WinRoom(ULKRunSubsystem* Run)
{
    FLKBattleContext Context;
    if (!Run->BeginCurrentBattle(Context)) { return false; }
    FLKBattleOutcome Outcome;
    Outcome.RunId = Context.RunId; Outcome.NodeId = Context.NodeId; Outcome.AttemptId = Context.AttemptId;
    Outcome.bFinalized = true; Outcome.Stats.Winner = ELKTeam::Player;
    for (const FLKRunHeroState& Hero : Context.PlayerHeroes)
    {
        FLKHeroBattleOutcome Result; Result.InstanceId = Hero.HeroId; Result.RecoveredState = Hero;
        Outcome.PlayerHeroes.Add(Result);
    }
    return Run->SubmitBattleOutcome(Outcome);
}
bool StartRun(ULKRunSubsystem* Run, int32 CardCount = 7)
{
    Run->SetAutoSaveEnabled(false);
    TArray<FLKRunHeroState> Heroes;
    for (FName Id : LKHomeContent::DefaultUnlockedHeroes())
    {
        FLKRunHeroState Hero; Hero.HeroId = Id; Hero.Health = Hero.MaxHealth = Hero.BaseMaxHealth = LKUnitContent::Find(Id)->BaseHealth; Heroes.Add(Hero);
    }
    TArray<FLKRunCardState> Cards;
    TArray<FName> Ids = LKHomeContent::DefaultUnlockedCards(); Ids.Add("Unit_Skeleton"); Ids.Add("Unit_SkeletonArcher");
    for (int32 I = 0; I < CardCount; ++I) { FLKRunCardState Card; Card.CardId = Ids[I]; Cards.Add(Card); }
    return Run->StartNewRun(Heroes, Cards, 815);
}
FLKRunRewardOffer NewCard(FName Id)
{ FLKRunRewardOffer Offer; Offer.Kind = ELKRunRewardKind::AddCard; Offer.CardId = Id; return Offer; }
}


struct FLKTrollTestAccess
{
    static void Attack(ALKUnitBase* Unit, ALKUnitBase* Target) { Unit->PerformAttack(Target); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTrollCatalogTest, "LittleKing.TrollCards.ContentAndWeightedDeck", Flags)
bool FLKTrollCatalogTest::RunTest(const FString& Parameters)
{
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
    TestEqual(TEXT("Native unit count"), LKUnitContent::Units().Num(), 28);
    TestEqual(TEXT("Full card count"), Data->CardLibrary.Num(), 24);
    TestEqual(TEXT("Four troll cards cost two slots"), LKCardRules::Slots("Unit_TrollKing"), 2);
    TestEqual(TEXT("Colossus costs three slots"), LKCardRules::Slots("Unit_Colossus"), 3);
    TestEqual(TEXT("Spell costs one slot"), LKCardRules::Slots("Spell_Fireball"), 1);
    for (FName Id : {FName("Unit_TrollWarrior"),FName("Unit_TrollSpearman"),FName("Unit_TrollMage"),FName("Unit_TrollKing"),
        FName("Unit_TwoHeadedDragon"),FName("Building_SiegeCatapult"),FName("Unit_Colossus")})
    {
        const auto* Found = Data->CardLibrary.FindByPredicate([Id](const TObjectPtr<ULKCardDefinition>& C){ return C && C->CardId == Id; });
        if (!TestNotNull(TEXT("New card injected without asset"), Found)) { return false; }
        TestTrue(TEXT("New card remains expedition-only"), (*Found)->bExpeditionOnly);
        TestTrue(TEXT("UI explicitly displays deck slots"), LKCardPresentation::Detail(**Found).ToString().Contains(TEXT("部队占格")));
    }
    const auto* Catapult = Data->CardLibrary.FindByPredicate([](const TObjectPtr<ULKCardDefinition>& C){return C && C->CardId == "Building_SiegeCatapult";});
    TestEqual(TEXT("Catapult enters building deployment path"), (*Catapult)->CardType, ELKCardType::Building);
    const FLKUnitRow* King = LKUnitContent::Find("Unit_TrollKing");
    TestEqual(TEXT("King is a soldier, not hero slot"), King->UnitClass, ELKUnitClass::Soldier);
    FLKUnitRow Wrong = *King; Wrong.DeckSlots = 1;
    TestEqual(TEXT("Native slot identity wins over stale table"), LKUnitContent::MergeAuthoredTuning(*King, &Wrong).DeckSlots, 2);
    TestTrue(TEXT("Treasury level five can afford 12-cost colossus"), 5 + LKHomeContent::TreasuryCapBonus(5) >= 12);
    ULKDeckState* Deck = NewObject<ULKDeckState>();
    TArray<FName> Cards = {"Unit_Colossus","Unit_TrollWarrior","Unit_Swordsman","Spell_Fireball","Spell_HealWave"};
    TestEqual(TEXT("Five-card mixed deck fills eight capacity"), LKCardRules::Used(Cards), 8);
    TestTrue(TEXT("Weighted cards occupy just one hand slot"), Deck->InitDeck(Cards,4,12));
    TestFalse(TEXT("Adding even a one-slot card is rejected at eight"), Deck->AddCardToDeck("Unit_Archer"));
    TestFalse(TEXT("Transform cannot exceed budget"), Deck->TransformCard("Unit_Swordsman","Unit_TrollMage"));
    for (int32 I=0; I<60; ++I)
    {
        Deck->PlayCard(I % 4); TSet<FName> Unique(Deck->GetHand());
        TestEqual(TEXT("Four unique filled hand slots"), Unique.Num(),4);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTrollWeightedRewardTest, "LittleKing.TrollCards.MultiReplacementAtomicity", Flags)
bool FLKTrollWeightedRewardTest::RunTest(const FString& Parameters)
{
    UGameInstance* Instance=NewObject<UGameInstance>();
    ULKRunSubsystem* Run=NewObject<ULKRunSubsystem>(Instance);
    Run->ConfigureStorage(TEXT("LittleKing_Troll_?:/Unwritable"));
    if (!StartRun(Run,8) || !WinRoom(Run) || !Run->OfferRewardBatch({NewCard("Unit_Colossus")})) { return false; }
    const FLKRunState Before=Run->GetRunState();
    TestFalse(TEXT("One removed slot cannot fit three"),Run->ChooseReward(0,"Unit_Swordsman"));
    TestFalse(TEXT("Duplicate removals cannot invent capacity"),Run->ChooseRewardReplacingCards(0,{"Unit_Swordsman","Unit_Swordsman","Unit_Archer"}));
    TestFalse(TEXT("Too many removals cannot break four-slot cycling"),Run->ChooseRewardReplacingCards(0,{"Unit_Swordsman","Unit_Archer","Unit_Shieldbearer","Spell_Fireball","Spell_HealWave"}));
    const TArray<FName> Remove={"Unit_Swordsman","Unit_Archer","Unit_Shieldbearer"};
    Run->SetAutoSaveEnabled(true);
    TestFalse(TEXT("Failed disk write rolls back all three removals"),Run->ChooseRewardReplacingCards(0,Remove));
    TestEqual(TEXT("Batch retained on failed save"),Run->GetRunState().PendingRewardBatchId,Before.PendingRewardBatchId);
    TestEqual(TEXT("All original eight retained"),Run->GetRunState().Cards.Num(),8);
    Run->SetAutoSaveEnabled(false);
    TestTrue(TEXT("Atomic multi-replacement succeeds"),Run->ChooseRewardReplacingCards(0,Remove));
    TestEqual(TEXT("Result has six card types"),Run->GetRunState().Cards.Num(),6);
    TestEqual(TEXT("Result uses exactly eight slots"),Run->GetDeckCapacityUsed(),8);
    TestFalse(TEXT("Cannot consume reward twice"),Run->ChooseRewardReplacingCards(0,Remove));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTrollSupportTest, "LittleKing.TrollCards.KingSupportHealthRatio", Flags)
bool FLKTrollSupportTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* King=Env.Spawn("Unit_TrollKing",FVector(-800,0,0));
    if (!King) { return false; }
    Env.Health(King,King->GetMaxHealth()*.8f);
    const float BaseMax=King->GetMaxHealth(), BaseAttack=King->GetAttackDamage();
    const float Healing=Env.GM->GetMatchStats().Player.Healing;
    ALKUnitBase* W1=Env.Spawn("Unit_TrollWarrior",FVector(-400,0,0));
    ALKUnitBase* W2=Env.Spawn("Unit_TrollWarrior",FVector(0,0,0));
    ALKUnitBase* Spear=Env.Spawn("Unit_TrollSpearman",FVector(400,0,0));
    if (!W1 || !W2 || !Spear) { return false; }
    TestTrue(TEXT("Two warriors grant one 20 percent max-health bonus"),FMath::IsNearlyEqual(King->GetMaxHealth(),BaseMax*1.2f,.01f));
    TestTrue(TEXT("Health ratio stays at eighty percent"),FMath::IsNearlyEqual(King->GetHealth(),BaseMax*1.2f*.8f,.01f));
    TestTrue(TEXT("Spearman grants 20 percent attack"),FMath::IsNearlyEqual(King->GetAttackDamage(),BaseAttack*1.2f,.01f));
    W1->Die();
    TestTrue(TEXT("One survivor keeps health bonus"),FMath::IsNearlyEqual(King->GetMaxHealth(),BaseMax*1.2f,.01f));
    W2->Die();
    TestTrue(TEXT("Last warrior death removes bonus"),FMath::IsNearlyEqual(King->GetMaxHealth(),BaseMax,.01f));
    TestTrue(TEXT("Removal preserves ratio instead of subtracting max bonus"),FMath::IsNearlyEqual(King->GetHealth(),BaseMax*.8f,.01f));
    TestEqual(TEXT("Rescaling generates no healing stats"),Env.GM->GetMatchStats().Player.Healing,Healing);
    ALKUnitBase* EnemyWarrior=Env.Spawn("Unit_TrollWarrior",FVector(800,0,0),ELKTeam::Enemy);
    if (!EnemyWarrior) { return false; }
    TestEqual(TEXT("Enemy warrior gives no support"),King->GetMaxHealth(),BaseMax);
    Spear->Die(); TestEqual(TEXT("Last spear death removes attack bonus"),King->GetAttackDamage(),BaseAttack);
    TestFalse(TEXT("King immune to stun"),King->GetStatusComponent()->Stun(2));
    TestTrue(TEXT("King can still be frozen"),King->GetStatusComponent()->Freeze(1));
    King->GetStatusComponent()->TickStatus(1);
    TestFalse(TEXT("Freeze expires"),King->IsControlled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTrollEmpowerTest, "LittleKing.TrollCards.EmpowerControlAndSixthAttack", Flags)
bool FLKTrollEmpowerTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* Mage=Env.Spawn("Unit_TrollMage",FVector(-700,0,0));
    ALKUnitBase* Warrior=Env.Spawn("Unit_TrollWarrior",FVector(-500,0,0));
    ALKUnitBase* King=Env.Spawn("Unit_TrollKing",FVector(700,0,0));
    ALKUnitBase* Enemy=Env.Spawn("Building_ArrowTower",FVector(-700,400,0),ELKTeam::Enemy);
    if (!Mage || !Warrior || !King || !Enemy) { return false; }
    Mage->GetActiveComponent()->TickAbility(20);
    TestTrue(TEXT("Mage empowers self"),Mage->GetStatusComponent()->IsEmpowered());
    TestTrue(TEXT("King gets priority over nearby troll"),King->GetStatusComponent()->IsEmpowered());
    TestFalse(TEXT("Nearby warrior excluded when king present"),Warrior->GetStatusComponent()->IsEmpowered());
    TestTrue(TEXT("Speed multiplier"),FMath::IsNearlyEqual(Mage->GetMoveSpeed(),250*1.3f,.01f));
    TestTrue(TEXT("Interval multiplier"),FMath::IsNearlyEqual(Mage->GetAttackInterval(),1.2f*.75f,.01f));
    TestTrue(TEXT("Twenty percent damage reduction"),FMath::IsNearlyEqual(LKGameplay::ApplyDamage(Mage,100,Enemy),80.f,.01f));
    for (int32 I=0;I<4;++I) { FLKTrollTestAccess::Attack(Mage,Enemy); }
    TestTrue(TEXT("Fourth actual swing self-stuns"),Mage->GetStatusComponent()->IsStunned());
    TestEqual(TEXT("Stun immediately clears buildup"),Mage->GetStatusComponent()->GetStunMeter(),0.f);
    const float Before=Enemy->GetHealth();
    FLKTrollTestAccess::Attack(Mage,Enemy);
    TestEqual(TEXT("Stun blocks direct attack entry"),Enemy->GetHealth(),Before);
    Mage->GetMovementComponent()->MoveToward(FVector(-700,700,0),1000);
    Mage->GetMovementComponent()->TickComponent(1,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Stun blocks movement component"),Mage->GetActorLocation(),FVector(-700,0,0));
    TestFalse(TEXT("Stun blocks active casting"),Mage->GetActiveComponent()->TryActivate());
    Mage->GetStatusComponent()->TickStatus(2);
    TestFalse(TEXT("Self-stun expires"),Mage->IsControlled());
    Mage->GetStatusComponent()->AfterAttack();
    TestEqual(TEXT("Buildup resumes after stun"),Mage->GetStatusComponent()->GetStunMeter(),.25f);
    Mage->GetStatusComponent()->Stun(2);
    TestEqual(TEXT("External stun also clears meter"),Mage->GetStatusComponent()->GetStunMeter(),0.f);
    Mage->GetStatusComponent()->TickStatus(6);
    TestFalse(TEXT("Empower expires at eight seconds"),Mage->GetStatusComponent()->IsEmpowered());
    TestTrue(TEXT("Speed restores exactly"),FMath::IsNearlyEqual(Mage->GetMoveSpeed(),250.f,.01f));
    ALKUnitBase* Victim=Env.Spawn("Unit_Colossus",FVector(700,400,0),ELKTeam::Enemy);
    if (!Victim) { return false; }
    const float Damage=King->GetAttackDamage(), HP=Victim->GetHealth();
    for (int32 I=0;I<5;++I) { FLKTrollTestAccess::Attack(King,Victim); }
    TestTrue(TEXT("First five attacks are normal"),FMath::IsNearlyEqual(HP-Victim->GetHealth(),Damage*5,.01f));
    TestFalse(TEXT("King does not self-stun during empower"),King->IsControlled());
    const FVector Location=Victim->GetActorLocation();
    FLKTrollTestAccess::Attack(King,Victim);
    TestTrue(TEXT("Sixth attack replaces damage with 150 percent"),FMath::IsNearlyEqual(HP-Victim->GetHealth(),Damage*6.5f,.01f));
    TestTrue(TEXT("Knockback travels three meters"),FMath::IsNearlyEqual(FVector::Dist2D(Location,Victim->GetActorLocation()),300.f,.1f));
    TestTrue(TEXT("Sixth attack stuns target"),Victim->GetStatusComponent()->IsStunned());
    TestEqual(TEXT("Counter resets after sixth attack"),King->GetStatusComponent()->GetAttackCount(),0);
    King->SetCombatEnabled(false);
    TestFalse(TEXT("Battle stop clears empowerment"),King->GetStatusComponent()->IsEmpowered());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTwinBreathTest, "LittleKing.TrollCards.SharedBreathBurnAndPool", Flags)
bool FLKTwinBreathTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* D1=Env.Spawn("Unit_TwoHeadedDragon",FVector(-800,0,0));
    ALKUnitBase* D2=Env.Spawn("Unit_TwoHeadedDragon",FVector(-500,0,0));
    ALKUnitBase* Target=Env.Spawn("Unit_Colossus",FVector(-100,0,0),ELKTeam::Enemy);
    if (!D1 || !D2 || !Target) { return false; }
    ULKUnitStatusComponent* S=Target->GetStatusComponent();
    const FLKCombatSource A=LKGameplay::MakeSource(D1,ELKCombatSourceKind::Projectile,"Attack_IceHead");
    const FLKCombatSource B=LKGameplay::MakeSource(D2,ELKCombatSourceKind::Projectile,"Attack_FireHead");
    TestEqual(TEXT("First ice hit normal"),S->ReceiveBreath(ELKBreathHead::Ice,10,D1,A),10.f);
    TestFalse(TEXT("First ice does not freeze"),S->IsFrozen());
    S->ReceiveBreath(ELKBreathHead::Ice,10,D2,B);
    TestTrue(TEXT("Different dragon shares target ice history"),S->IsFrozen());
    TestEqual(TEXT("Ice to fire deals 120 percent"),S->ReceiveBreath(ELKBreathHead::Fire,10,D2,B),12.f);
    S->ReceiveBreath(ELKBreathHead::Fire,10,D1,A);
    TestEqual(TEXT("Fire-fire starts one burn stack"),S->GetBurnStacks(),1);
    const float Start=Target->GetHealth();
    S->TickStatus(.5f);
    S->ReceiveBreath(ELKBreathHead::Fire,10,D2,B);
    S->TickStatus(.5f);
    TestEqual(TEXT("Repeated ignition stacks and keeps tick phase"),S->GetBurnStacks(),2);
    TestTrue(TEXT("One tick at two percent max health"),FMath::IsNearlyEqual(Start-Target->GetHealth(),10+Target->GetMaxHealth()*.02f,.01f));
    const float BeforeTick=Target->GetHealth(); S->TickStatus(2.5f);
    TestTrue(TEXT("Refreshed burn produces remaining two ticks"),FMath::IsNearlyEqual(BeforeTick-Target->GetHealth(),Target->GetMaxHealth()*.04f,.01f));
    TestEqual(TEXT("Burn expires at refreshed duration"),S->GetBurnStacks(),0);
    Target->SetInvulnerable();
    const ELKBreathHead History=S->GetLastBreath();
    TestEqual(TEXT("Invulnerable hit has no damage"),S->ReceiveBreath(ELKBreathHead::Ice,10,D1,A),0.f);
    TestEqual(TEXT("No-effect hit does not rewrite history"),S->GetLastBreath(),History);
    Target->SetInvulnerable(0);
    S->Clear();
    ALKProjectile* Shot=Env.GM->AcquireProjectile(FVector(-400,0,20),10,ELKTeam::Player,D1,FVector(1,0,0));
    Shot->SetAttackPayload(ELKBreathHead::Fire);
    D1->Die(); Shot->Tick(1);
    TestEqual(TEXT("Projectile keeps selected head after caster death"),S->GetLastBreath(),ELKBreathHead::Fire);
    TestFalse(TEXT("Projectile returned to pool"),Shot->IsPooledActive());
    TestEqual(TEXT("Pool clears elemental payload"),Shot->GetBreathHead(),ELKBreathHead::None);
    const ELKBreathHead Stored=S->GetLastBreath();
    ALKProjectile* Normal=Env.GM->AcquireProjectile(FVector(-400,0,20),1,ELKTeam::Player,D2,FVector(1,0,0));
    Normal->Tick(1);
    TestEqual(TEXT("Reused normal projectile cannot apply old elemental head"),S->GetLastBreath(),Stored);
    TestEqual(TEXT("Reused projectile cannot ignite"),S->GetBurnStacks(),0);
    S->Ignite(B,D2); Target->SetCombatEnabled(false);
    const float EndHealth=Target->GetHealth(); S->TickStatus(4);
    TestEqual(TEXT("No burn damage in results"),Target->GetHealth(),EndHealth);
    TestEqual(TEXT("Room stop clears shared history"),S->GetLastBreath(),ELKBreathHead::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKSiegeTargetTest, "LittleKing.TrollCards.SiegeOnlyBuildingsAcrossMap", Flags)
bool FLKSiegeTargetTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* Siege=Env.Spawn("Building_SiegeCatapult",FVector(-1000,-900,0));
    ALKUnitBase* Taunter=Env.Spawn("Unit_Shieldbearer",FVector(-700,-600,0),ELKTeam::Enemy);
    ALKUnitBase* Tower=Env.Spawn("Building_ArrowTower",FVector(1000,1750,0),ELKTeam::Enemy);
    if (!Siege || !Taunter || !Tower) { return false; }
    TestTrue(TEXT("Spawned catapult is building"),Siege->IsBuilding());
    TestFalse(TEXT("Cannot pursue taunting soldier"),Siege->CanPursueTarget(Taunter));
    TestTrue(TEXT("Can pursue enemy tower across entire map"),Siege->CanPursueTarget(Tower));
    Siege->SetForcedTarget(Taunter,10); Siege->SetTarget(Tower);
    TestEqual(TEXT("Focus and taunt cannot override building filter"),Siege->GetTarget(),static_cast<AActor*>(Tower));
    const FVector Direction=(Tower->GetActorLocation()-Siege->GetActorLocation()).GetSafeNormal2D();
    ALKUnitBase* Blocker=Env.Spawn("Unit_Colossus",Siege->GetActorLocation()+Direction*700,ELKTeam::Enemy);
    if (!Blocker) { return false; }
    const float BlockHealth=Blocker->GetHealth(), HP=Tower->GetHealth();
    FLKTrollTestAccess::Attack(Siege,Tower);
    ALKProjectile* Shot=nullptr;
    for (TActorIterator<ALKProjectile> It(Env.GetTestWorld());It;++It) { if (It->IsPooledActive()) { Shot=*It; break; } }
    if (!TestNotNull(TEXT("Siege launches projectile"),Shot)) { return false; }
    Shot->Tick(10);
    TestEqual(TEXT("Siege reaches tower beyond ordinary lifetime"),HP-Tower->GetHealth(),Siege->GetAttackDamage());
    TestEqual(TEXT("Intervening mercenary never intercepted siege"),Blocker->GetHealth(),BlockHealth);
    TestFalse(TEXT("Spent siege projectile returned"),Shot->IsPooledActive());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKTrollWeightedUITest, "LittleKing.TrollCards.UI.MultiSelectAndRender", Flags)
bool FLKTrollWeightedUITest::RunTest(const FString& Parameters)
{
    const FString SlotName=TEXT("LittleKing_TrollUI_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(SlotName,0); };
    FTestWorldWrapper World;
    if (!World.CreateTestWorld(EWorldType::Game)) { return false; }
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
    ULKRunSubsystem* Run=World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    Run->ConfigureStorage(SlotName);
    if (!StartRun(Run,8)||!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this);return false; }
    ALKBattleGameMode* GM=World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
    World.GetTestWorld()->SpawnActor<ALKPlayerController>();GM->Tick(0);
    ULKBattleHUDWidget* HUD=GM->GetBattleHUDWidget();
    if (!HUD) { return false; }
    const TSharedRef<SWidget> HUDSlate=HUD->TakeWidget();
    auto Render = [&](const TCHAR* Name, int32 Width, int32 Height, const TSharedRef<SWidget>& Content)
    {
        if (!FApp::CanEverRender()) { return; }
        FWidgetRenderer Renderer(true);
        UTextureRenderTarget2D* Target = Renderer.DrawWidget(Content, FVector2D(Width, Height));
        // UMG auto-wrap and ScrollBox arrange after the first layout pass.
        Renderer.DrawWidget(Target, Content, FVector2D(Width, Height), .016f);
        Renderer.DrawWidget(Target, Content, FVector2D(Width, Height), .016f);
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags ReadFlags; ReadFlags.SetLinearToGamma(false);
        if (!TestTrue(TEXT("Read rendered UMG pixels"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) { return; }
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Width, Height, Pixels, PNG);
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("Screenshots/TrollCards");
        IFileManager::Get().MakeDirectory(*Directory, true);
        TestTrue(TEXT("Save UI preview"), FFileHelper::SaveArrayToFile(PNG, *(Directory / FString::Printf(TEXT("%s-%dx%d.png"), Name, Width, Height))));
    };

    for(int32 I=0;I<3;++I){GM->DeployHero(ELKTeam::Player,GM->AvailableHeroes[I],FVector((I-1)*700,-1300,0));}
    GM->ForceStartBattle();
    GM->GetTeamDeck(ELKTeam::Player)->InitDeck({"Unit_Colossus","Unit_TrollKing","Unit_TwoHeadedDragon","Building_SiegeCatapult","Spell_Fireball"},4,14);
    GM->GetTeamSilver(ELKTeam::Player)->SetCap(13); GM->GetTeamSilver(ELKTeam::Player)->AddSilver(13);
    Render(TEXT("WeightedHand"),1280,720,HUDSlate);
    GM->ForceEndMatch(ELKTeam::Player);
    ULKRunSaveGame* Save=NewObject<ULKRunSaveGame>();Save->SaveVersion=1;Save->RunState=Run->GetRunState();
    Save->RunState.PendingRewardOffers={NewCard("Unit_Colossus"),NewCard("Unit_TrollMage"),NewCard("Building_SiegeCatapult")};
    TestTrue(TEXT("Save controlled candidate batch"),UGameplayStatics::SaveGameToSlot(Save,SlotName,0));
    if(!Run->LoadExpeditionFromSlot(SlotName,true)){return false;}
    ULKRunRewardWidget* Reward=CreateWidget<ULKRunRewardWidget>(World.GetTestWorld());
    Reward->InitializeReward(HUD);const TSharedRef<SWidget> Slate=Reward->TakeWidget();Reward->RefreshReward();
    Render(TEXT("Rewards"),1280,720,Slate);Render(TEXT("Rewards"),1920,1080,Slate);
    UButton* Incoming=Cast<UButton>(Reward->GetWidgetFromName(TEXT("Btn_Reward0")));
    if(!TestNotNull(TEXT("Incoming card button"),Incoming)){return false;}
    Incoming->OnClicked.Broadcast();
    UButton* Confirm=Cast<UButton>(Reward->GetWidgetFromName(TEXT("Btn_ConfirmReplacement")));
    UUniformGridPanel* Grid=Cast<UUniformGridPanel>(Reward->GetWidgetFromName(TEXT("ReplacementGrid")));
    if(!Confirm||!Grid){return false;}
    TestFalse(TEXT("Confirm initially disabled"),Confirm->GetIsEnabled());
    auto ClickRow=[&](int32 Index)
    {
        UUserWidget* Entry=Cast<UUserWidget>(Grid->GetChildAt(Index));
        UButton* Button=Entry?Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))):nullptr;
        if(Button){Button->OnClicked.Broadcast();}
        return Button!=nullptr;
    };
    const FGuid Batch=Run->GetRunState().PendingRewardBatchId;
    ClickRow(0);TestFalse(TEXT("One removed slot is insufficient"),Confirm->GetIsEnabled());
    ClickRow(1);TestFalse(TEXT("Two removed slots still insufficient"),Confirm->GetIsEnabled());
    ClickRow(2);TestTrue(TEXT("Three selections allow confirmation"),Confirm->GetIsEnabled());
    TestEqual(TEXT("Selections are only a preview"),Run->GetRunState().PendingRewardBatchId,Batch);
    TestEqual(TEXT("Preview has not removed cards"),Run->GetRunState().Cards.Num(),8);
    Render(TEXT("MultiReplacement"),1280,720,Slate);Render(TEXT("MultiReplacement"),1920,1080,Slate);
    ClickRow(2);TestFalse(TEXT("Toggle off removes capacity again"),Confirm->GetIsEnabled());ClickRow(2);
    Confirm->OnClicked.Broadcast();
    TestFalse(TEXT("One explicit confirmation consumes reward"),Run->HasPendingRewardChoice());
    TestEqual(TEXT("Three replaced by one leaves six cards"),Run->GetRunState().Cards.Num(),6);
    TestEqual(TEXT("Actual resulting capacity is eight"),Run->GetDeckCapacityUsed(),8);
    TestTrue(TEXT("New three-slot card stored"),Run->GetRunState().Cards.ContainsByPredicate([](const FLKRunCardState& C){return C.CardId=="Unit_Colossus";}));
    TestTrue(TEXT("Result persists"),Run->SaveExpedition());
    const auto Cards=Run->GetRunState().Cards;
    TestTrue(TEXT("Weighted deck reloads"),Run->LoadExpeditionFromSlot(SlotName,true));
    TestEqual(TEXT("Weighted capacity restored"),Run->GetDeckCapacityUsed(),8);
    TestEqual(TEXT("Schema eight stored"),Run->GetRunState().SchemaVersion,8);
    UGameplayStatics::DeleteGameInSlot(SlotName,0);
    return true;
}

#endif
