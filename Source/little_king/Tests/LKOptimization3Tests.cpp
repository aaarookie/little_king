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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCharacterCatalogTest, "LittleKing.Optimization3.Content.QualityGradesAndTemporaryCards", Flags)
bool FLKCharacterCatalogTest::RunTest(const FString& Parameters)
{
    ULKGameData* Data = NewObject<ULKGameData>(); Data->EnsureDefaultDecks(); Data->EnsureCardLibrary();
    TestEqual(TEXT("28 native character definitions"), LKUnitContent::Units().Num(), 28);
    TestEqual(TEXT("24 cards including fifteen expedition cards"), Data->CardLibrary.Num(), 24);
    Data->EnsureCardLibrary(); TestEqual(TEXT("Repeated registration does not duplicate"), Data->CardLibrary.Num(), 24);
    for (const FLKTemporaryMercenaryDefinition& Definition : LKExpeditionMercenaryContent::All())
    {
        const FLKUnitRow* Unit = LKUnitContent::Find(Definition.Unit.UnitId);
        if (!TestNotNull(TEXT("Native unit resolves without a data table row"), Unit)) { return false; }
        const TObjectPtr<ULKCardDefinition>* Card = Data->CardLibrary.FindByPredicate([Unit](const TObjectPtr<ULKCardDefinition>& C) { return C && C->CardId == Unit->UnitId; });
        if (!TestNotNull(TEXT("Card automatically resolves"), Card)) { return false; }
        TestTrue(TEXT("Temporary flag"), (*Card)->bExpeditionOnly);
        TestFalse(TEXT("Not initially unlocked"), LKHomeContent::DefaultUnlockedCards().Contains(Unit->UnitId));
        TestFalse(TEXT("Not in default deck"), Data->DefaultPlayerDeck.Contains(Unit->UnitId));
        TestTrue(TEXT("UI describes skill"), !(*Card)->Description.IsEmpty());
        FLKUnitRow Authored = *Unit; Authored.Quality = ELKQuality::Legendary; Authored.Race = ELKRace::Undead;
        Authored.ActiveAbility = ELKActiveAbility::None; Authored.BaseHealth = 999;
        const FLKUnitRow Merged = LKUnitContent::MergeAuthoredTuning(*Unit, &Authored);
        TestEqual(TEXT("Code owns quality"), Merged.Quality, Unit->Quality);
        TestEqual(TEXT("Code owns race"), Merged.Race, Unit->Race);
        TestEqual(TEXT("Table owns health tuning"), Merged.BaseHealth, 999.f);
    }
    TSet<FString> Grades;
    for (int32 Grade = 0; Grade < 14; ++Grade) { Grades.Add(LKCardPresentation::SpellGradeName(ELKSpellGrade(Grade)).ToString()); }
    TestEqual(TEXT("All 14 spell grades have distinct labels"), Grades.Num(), 14);
    TestEqual(TEXT("Elf priest retains excellent baseline health"), LKUnitContent::Find("Unit_ElfPriest")->BaseHealth, 110.f);
    TestEqual(TEXT("Necromancer quality is rare despite not being a skeleton"), LKUnitContent::Find("Hero_Necromancer")->Quality, ELKQuality::Rare);
    TestEqual(TEXT("Necromancer belongs to undead race"), LKUnitContent::Find("Hero_Necromancer")->Race, ELKRace::Undead);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCharacterCapacityTest, "LittleKing.Optimization3.Deck.CapacityReplacementAndRollback", Flags)
bool FLKCharacterCapacityTest::RunTest(const FString& Parameters)
{
    UGameInstance* Instance = NewObject<UGameInstance>();
    ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
    Run->ConfigureStorage(TEXT("LittleKing_Stage3_?:/Unwritable"));
    TestFalse(TEXT("A new nine-card run is rejected"), StartRun(Run, 9));
    if (!TestTrue(TEXT("Start seven-card run"), StartRun(Run)) || !TestTrue(TEXT("Win room"), WinRoom(Run))) { return false; }
    TestTrue(TEXT("Offer eighth card"), Run->OfferRewardBatch({NewCard("Unit_ElfArcher")}));
    TestTrue(TEXT("Seven to eight adds directly"), Run->ChooseReward(0));
    TestEqual(TEXT("Capacity is eight"), Run->GetRunState().Cards.Num(), 8);
    if (!TestTrue(TEXT("Next room"), Run->AdvanceToNextBattle()) || !TestTrue(TEXT("Win second room"), WinRoom(Run))) { return false; }
    TestTrue(TEXT("Offer ninth card"), Run->OfferRewardBatch({NewCard("Unit_GoblinRogue")}));
    const FGuid Batch = Run->GetRunState().PendingRewardBatchId;
    TestFalse(TEXT("No implicit overwrite at capacity"), Run->ChooseReward(0));
    TestFalse(TEXT("Invalid replacement rejected"), Run->ChooseReward(0, "Missing"));
    TestEqual(TEXT("Failed choice keeps batch"), Run->GetRunState().PendingRewardBatchId, Batch);
    Run->SetAutoSaveEnabled(true);
    TestFalse(TEXT("Save failure rolls back replacement"), Run->ChooseReward(0, "Unit_Swordsman"));
    TestEqual(TEXT("Save failure keeps unclaimed reward"), Run->GetRunState().PendingRewardBatchId, Batch);
    TestTrue(TEXT("Save failure retains old card"), Run->GetRunState().Cards.ContainsByPredicate([](const FLKRunCardState& Card) { return Card.CardId == "Unit_Swordsman"; }));
    TestFalse(TEXT("Skipping also rolls back on save failure"), Run->SkipReward());
    TestEqual(TEXT("Failed skip preserves batch"), Run->GetRunState().PendingRewardBatchId, Batch);
    Run->SetAutoSaveEnabled(false);
    TestTrue(TEXT("Explicit replacement succeeds"), Run->ChooseReward(0, "Unit_Swordsman"));
    TestEqual(TEXT("Replacement remains at eight"), Run->GetRunState().Cards.Num(), 8);
    TestFalse(TEXT("Double-click cannot consume twice"), Run->ChooseReward(0, "Unit_Archer"));
    ULKDeckState* Deck = NewObject<ULKDeckState>();
    TArray<FName> Cards; for (const FLKRunCardState& Card : Run->GetRunState().Cards) { Cards.Add(Card.CardId); }
    TestTrue(TEXT("Eight-card hand initializes"), Deck->InitDeck(Cards, 4, 9));
    TestFalse(TEXT("Low-level addition also enforces capacity"), Deck->AddCardToDeck("Unit_Thief"));
    for (int32 I = 0; I < 80; ++I)
    {
        TestFalse(TEXT("Play always succeeds"), Deck->PlayCard(I % 4).IsNone());
        TestEqual(TEXT("Four occupied slots"), Deck->GetHandSize(), 4);
        TSet<FName> Hand(Deck->GetHand()); TestEqual(TEXT("No duplicate cards in hand"), Hand.Num(), 4);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKElfHealingTest, "LittleKing.Optimization3.Combat.ElfHealingAndSharedSpring", Flags)
bool FLKElfHealingTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* Enemy = Env.Spawn("Unit_Swordsman", FVector(0, 400, 0), ELKTeam::Enemy);
    ALKUnitBase* Priest = Env.Spawn("Unit_ElfPriest", FVector(-700, -700, 0));
    ALKUnitBase* Knight = Env.Hero("Hero_Knight"); ALKUnitBase* Mage = Env.Hero("Hero_Mage");
    if (!Enemy || !Priest || !Knight || !Mage) { AddError(TEXT("Failed to spawn healing setup")); return false; }
    Env.Health(Knight, 90.f); Env.Health(Mage, 90.f); // Knight 20%, mage 30%: ratios, not absolute health.
    int32 I = 0;
    for (FName Id : {FName("Unit_ElfArcher"), FName("Unit_ElfWarrior"), FName("Unit_ElfGuard")})
    {
        ALKUnitBase* Elf = Env.Spawn(Id, FVector(-400 + I * 300, -400, 0));
        if (!TestNotNull(TEXT("Elf spawns"), Elf)) { return false; }
        Env.Health(Elf, Elf->GetMaxHealth() * .5f);
        const float Before = Elf->GetHealth(), HeroBefore = Knight->GetHealth();
        const FLKCombatSource Source = LKGameplay::MakeSource(Elf, I == 0 ? ELKCombatSourceKind::Projectile : ELKCombatSourceKind::Attack);
        LKGameplay::ApplyDamage(Enemy, 1.f, Elf, false, &Source);
        const float Expected = Elf->GetMaxHealth() * (.03f + .01f * I);
        TestTrue(TEXT("Attack restores correct maximum-health fraction"), FMath::IsNearlyEqual(Elf->GetHealth() - Before, Expected, .01f));
        TestTrue(TEXT("Priest copies actual healing to lowest ratio hero"), FMath::IsNearlyEqual(Knight->GetHealth() - HeroBefore, Expected, .01f));
        const float SkillBefore = Elf->GetHealth();
        const FLKCombatSource Skill = LKGameplay::MakeSource(Elf, ELKCombatSourceKind::Skill);
        LKGameplay::ApplyDamage(Enemy, 1.f, Elf, false, &Skill);
        TestEqual(TEXT("Skill damage does not trigger attack renewal"), Elf->GetHealth(), SkillBefore);
        Enemy->SetInvulnerable(); LKGameplay::ApplyDamage(Enemy, 1.f, Elf, false, &Source);
        TestEqual(TEXT("Invulnerable target does not trigger renewal"), Elf->GetHealth(), SkillBefore);
        Enemy->SetInvulnerable(0); ++I;
    }
    Env.Health(Priest, Priest->GetMaxHealth() - 2.f);
    const float Before = Knight->GetHealth();
    LKGameplay::ApplyHeal(Priest, 100.f, Priest);
    TestTrue(TEXT("Overhealing copies only two actual points"), FMath::IsNearlyEqual(Knight->GetHealth() - Before, 2.f, .01f));
    const float FullBefore = Knight->GetHealth(); LKGameplay::ApplyHeal(Priest, 100.f, Priest);
    TestEqual(TEXT("Full-health heal is not copied"), Knight->GetHealth(), FullBefore);
    // An elf hero proves copied healing cannot recurse even when it also qualifies by race.
    FLKUnitRow ElfHeroRow = *LKUnitContent::Find("Hero_Ranger"); ElfHeroRow.Race = ELKRace::Elf;
    ALKUnitBase* ElfHero = Env.Hero("Hero_Ranger"); ElfHero->InitUnit(ElfHeroRow, Env.GM->GetGameData());
    ElfHero->SetCombatEnabled(true); Env.Health(ElfHero, 20.f);
    Env.Health(Priest, 50.f); LKGameplay::ApplyHeal(Priest, 3.f, Priest);
    TestEqual(TEXT("Copied healing does not recursively echo through elf hero"), ElfHero->GetHealth(), 23.f);
    ElfHero->Die(); Env.Health(Priest, 50.f); LKGameplay::ApplyHeal(Priest, 3.f, Priest);
    TestTrue(TEXT("Ordinary healing never revives incapacitated heroes"), ElfHero->IsDead());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKRogueTest, "LittleKing.Optimization3.Combat.BackstabAndLoot", Flags)
bool FLKRogueTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* Rogue = Env.Spawn("Unit_GoblinRogue", FVector(0, 500, 0));
    ALKUnitBase* Taunter = Env.Spawn("Unit_Shieldbearer", FVector(150, 500, 0), ELKTeam::Enemy);
    ALKUnitBase* Far = Env.Spawn("Unit_Shieldbearer", FVector(0, 1850, 0), ELKTeam::Enemy);
    if (!Rogue || !Taunter || !Far) { AddError(TEXT("Failed to spawn backstab setup")); return false; }
    Rogue->SetTarget(Far); TestEqual(TEXT("Rogue ignores nearby taunt"), Rogue->GetTarget(), static_cast<AActor*>(Far));
    ULKSilverComponent* Silver = Env.GM->GetTeamSilver(ELKTeam::Player);
    Silver->TrySpend(Silver->GetSilver());
    int32 ProcSeed = 0;
    while (FRandomStream(ProcSeed).FRand() >= .3f) { ++ProcSeed; }
    Env.GM->GetBattleRandom().Initialize(ProcSeed);
    const float Before = Far->GetHealth();
    Rogue->GetActiveComponent()->TickAbility(8.f);
    TestEqual(TEXT("Backstab chooses the farthest enemy"), Rogue->GetActiveComponent()->GetLockedTarget(), Far);
    TestTrue(TEXT("Backstab deals 300 percent attack"), FMath::IsNearlyEqual(Before - Far->GetHealth(), 60.f));
    TestTrue(TEXT("Rogue lands behind enemy facing toward player"), Rogue->GetActorLocation().Y > Far->GetActorLocation().Y);
    TestTrue(TEXT("Blink respects static bodies and bounds"), Rogue->GetMovementComponent()->CanStandAt(Rogue->GetActorLocation()));
    TestEqual(TEXT("Seeded successful steal adds one silver"), Silver->GetSilver(), 1.f);
    Rogue->SetForcedTarget(Taunter, 20.f);
    TestEqual(TEXT("Backstab target lock wins over focus"), Rogue->GetTarget(), static_cast<AActor*>(Far));
    Far->Die(); TestEqual(TEXT("Target death immediately resets cooldown"), Rogue->GetActiveComponent()->GetCooldownRemaining(), 0.f);
    ALKUnitBase* Thief = Env.Spawn("Unit_Thief", FVector(-400, 0, 0));
    ALKUnitBase* Victim = Env.Spawn("Unit_Skeleton", FVector(-400, 160, 0), ELKTeam::Enemy);
    if (!Thief || !Victim) { return false; }
    Thief->SetTarget(Victim); const float LootBefore = Silver->GetSilver();
    LKGameplay::ApplyDamage(Victim, 9999, Env.Hero("Hero_Mage"));
    TestEqual(TEXT("Thief earns loot even when another ally kills its target"), Silver->GetSilver(), LootBefore + 1.f);
    Victim->Die(); TestEqual(TEXT("Repeated death does not duplicate silver"), Silver->GetSilver(), LootBefore + 1.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKMimicTest, "LittleKing.Optimization3.Combat.MimicFullDeckAndFallback", Flags)
bool FLKMimicTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* Mage = Env.Spawn("Unit_ApprenticeMage", FVector(0, -400, 0));
    if (!Mage) { return false; }
    ULKDeckState* Deck = Env.GM->GetTeamDeck(ELKTeam::Player);
    TArray<FName> Cards = {"Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer", "Building_ArrowTower", "Spell_HealWave"};
    int32 Seed = 0;
    do { Deck->InitDeck(Cards, 4, Seed++); } while (Deck->GetHand().Contains("Spell_HealWave") && Seed < 100);
    TestFalse(TEXT("Only novice spell is outside current hand"), Deck->GetHand().Contains("Spell_HealWave"));
    const TArray<FName> HandBefore = Deck->GetHand();
    Env.Health(Mage, 20.f);
    const float SilverBefore = Env.GM->GetTeamSilver(ELKTeam::Player)->GetSilver();
    Mage->GetActiveComponent()->TickAbility(10.f);
    TestEqual(TEXT("Mimic discovers novice spell in draw queue"), Mage->GetActiveComponent()->GetLastSpellId(), FName("Spell_HealWave"));
    TestTrue(TEXT("Mimic applies actual heal"), Mage->GetHealth() > 20.f);
    TestTrue(TEXT("Mimic does not cycle hand"), Deck->GetHand() == HandBefore);
    TestEqual(TEXT("Mimic costs no silver"), Env.GM->GetTeamSilver(ELKTeam::Player)->GetSilver(), SilverBefore);
    ULKCardDefinition* Heal = Env.GM->FindCard("Spell_HealWave"); Heal->SpellGrade = ELKSpellGrade::Intermediate1;
    Mage->GetActiveComponent()->TickAbility(10.f);
    TestEqual(TEXT("Higher-grade spell excluded; fallback is fireball"), Mage->GetActiveComponent()->GetLastSpellId(), FName("Spell_Fireball"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKDashTest, "LittleKing.Optimization3.Combat.DashHitsKnockbackAndObstacles", Flags)
bool FLKDashTest::RunTest(const FString& Parameters)
{
    FCharacterWorld Env; if (!Env.Open(*this)) { return false; }
    ALKUnitBase* Blade = Env.Spawn("Unit_GoblinBlade", FVector(-700, 0, 0));
    ALKUnitBase* Enemy = Env.Spawn("Unit_Swordsman", FVector(-400, 0, 0), ELKTeam::Enemy);
    ALKUnitBase* Tower = Env.Spawn("Building_ArrowTower", FVector(-150, 0, 0), ELKTeam::Enemy);
    if (!Blade || !Enemy || !Tower) { AddError(TEXT("Failed to spawn dash setup")); return false; }
    Blade->SetTarget(Enemy); Blade->GetActiveComponent()->TickAbility(5.f);
    TestTrue(TEXT("Dash starts"), Blade->IsSkillMoving());
    Blade->GetActiveComponent()->TickAbility(.1f);
    TestEqual(TEXT("Swept hit deals 200 percent attack"), Enemy->GetHealth(), 70.f);
    TestTrue(TEXT("Enemy pushed perpendicular to dash"), FMath::Abs(Enemy->GetActorLocation().Y) >= 119.f);
    Blade->GetActiveComponent()->TickAbility(.3f);
    TestEqual(TEXT("Enemy hit once per dash"), Enemy->GetHealth(), 70.f);
    TestTrue(TEXT("Dash stops before solid tower"), FVector::Dist2D(Blade->GetActorLocation(), Tower->GetActorLocation()) >= Blade->GetBodyRadius() + Tower->GetBodyRadius());
    TestEqual(TEXT("Tower cannot be displaced"), Tower->GetActorLocation(), FVector(-150, 0, 0));
    TestTrue(TEXT("Tower can receive dash damage"), Tower->GetHealth() < Tower->GetMaxHealth());
    Blade->GetMovementComponent()->MoveSkillDelta(FVector(10000, 0, 0));
    TestTrue(TEXT("Large movement step cannot tunnel through tower"), Blade->GetActorLocation().X < Tower->GetActorLocation().X);
    const FVector Edge = Enemy->GetMovementComponent()->MoveSkillDelta(FVector(0, 10000, 0));
    TestTrue(TEXT("Knockback clamps to battlefield edge"), FMath::Abs(Edge.Y) < Env.GM->GetGameData()->FieldHalfHeight);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKCharacterUITest, "LittleKing.Optimization3.UI.HandLabelsRewardReplacementAndRender", Flags)
bool FLKCharacterUITest::RunTest(const FString& Parameters)
{
    const FString SlotName = TEXT("LittleKing_Stage3UI_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FTestWorldWrapper World;
    if (!World.CreateTestWorld(EWorldType::Game)) { return false; }
    World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
    ULKRunSubsystem* Run = World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
    Run->ConfigureStorage(SlotName);
    if (!StartRun(Run, 8) || !World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
    ALKBattleGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
    World.GetTestWorld()->SpawnActor<ALKPlayerController>(); GM->Tick(0.f);
    ULKBattleHUDWidget* HUD = GM->GetBattleHUDWidget();
    if (!TestNotNull(TEXT("Authored HUD loads"), HUD)) { return false; }
    const TSharedRef<SWidget> HUDSlate = HUD->TakeWidget();
    TArray<FName> PreviewCards = { "Unit_GoblinRogue", "Unit_ElfArcher", "Unit_ApprenticeMage", "Spell_Fireball", "Unit_Thief" };
    GM->GetTeamDeck(ELKTeam::Player)->InitDeck(PreviewCards, 4, 35);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const ULKCardDefinition* Card = GM->FindCard(GM->GetTeamDeck(ELKTeam::Player)->GetHandCard(Index));
        UWidget* CardWidget = HUD->GetWidgetFromName(*FString::Printf(TEXT("CardSlot_%d"), Index));
        if (!TestNotNull(TEXT("Shipped hand slot found"), CardWidget))
        {
            TArray<UWidget*> Widgets; HUD->WidgetTree->GetAllWidgets(Widgets);
            FString Names; for (UWidget* Widget : Widgets) { Names += Widget->GetName() + TEXT(" "); }
            AddInfo(Names); return false;
        }
        TestTrue(TEXT("Actual hand tooltip contains quality or spell grade"), CardWidget->GetToolTipText().ToString().Contains(LKCardPresentation::Classification(*Card).ToString()));
    }
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
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("Screenshots/Optimization3");
        IFileManager::Get().MakeDirectory(*Directory, true);
        TestTrue(TEXT("Save UI preview"), FFileHelper::SaveArrayToFile(PNG, *(Directory / FString::Printf(TEXT("%s-%dx%d.png"), Name, Width, Height))));
    };
    for (int32 I = 0; I < 3; ++I) { GM->DeployHero(ELKTeam::Player, GM->AvailableHeroes[I], FVector((I - 1) * 700, -1300, 0)); }
    GM->ForceStartBattle();
    GM->GetTeamSilver(ELKTeam::Player)->AddSilver(20.f);
    Render(TEXT("Hand"), 1280, 720, HUDSlate);
    GM->ForceEndMatch(ELKTeam::Player);
    ULKRunSaveGame* Save = NewObject<ULKRunSaveGame>(); Save->SaveVersion = 1; Save->RunState = Run->GetRunState();
    Save->RunState.PendingRewardOffers = {NewCard("Unit_ElfPriest"), NewCard("Unit_GoblinRogue"), NewCard("Unit_ApprenticeMage")};
    TestTrue(TEXT("Freeze test reward candidates"), UGameplayStatics::SaveGameToSlot(Save, SlotName, 0));
    if (!TestTrue(TEXT("Reward batch reloads"), Run->LoadExpeditionFromSlot(SlotName, true))) { return false; }
    ULKRunRewardWidget* Reward = CreateWidget<ULKRunRewardWidget>(World.GetTestWorld());
    Reward->InitializeReward(HUD); const TSharedRef<SWidget> Slate = Reward->TakeWidget(); Reward->RefreshReward();
    Render(TEXT("Reward"), 1280, 720, Slate); Render(TEXT("Reward"), 1920, 1080, Slate);
    const FLKRunState Before = Run->GetRunState();
    UButton* Option = Cast<UButton>(Reward->GetWidgetFromName(TEXT("Btn_Reward1")));
    if (!TestNotNull(TEXT("Actual reward option exists"), Option)) { return false; }
    Option->OnClicked.Broadcast();
    TestTrue(TEXT("Full deck opens replacement selector"), Reward->IsChoosingReplacement());
    TestEqual(TEXT("Selecting incoming card does not consume reward"), Run->GetRunState().PendingRewardBatchId, Before.PendingRewardBatchId);
    UUniformGridPanel* Replacement = Cast<UUniformGridPanel>(Reward->GetWidgetFromName(TEXT("ReplacementGrid")));
    if (!TestNotNull(TEXT("Replacement list exists"), Replacement)) { return false; }
    TestEqual(TEXT("All eight old cards offered for replacement"), Replacement->GetChildrenCount(), 8);
    Render(TEXT("Replacement"), 1280, 720, Slate); Render(TEXT("Replacement"), 1920, 1080, Slate);
    UButton* Back = Cast<UButton>(Reward->GetWidgetFromName(TEXT("Btn_SkipReward")));
    Back->OnClicked.Broadcast();
    TestFalse(TEXT("Cancel returns to reward choices"), Reward->IsChoosingReplacement());
    TestEqual(TEXT("Cancel preserves all eight cards"), Run->GetRunState().Cards.Num(), 8);
    Option->OnClicked.Broadcast();
    UUserWidget* Entry = Cast<UUserWidget>(Replacement->GetChildAt(0));
    UButton* ReplaceButton = Entry ? Cast<UButton>(Entry->GetWidgetFromName(TEXT("RowButton"))) : nullptr;
    if (!TestNotNull(TEXT("Replacement row has real button"), ReplaceButton)) { return false; }
    ReplaceButton->OnClicked.Broadcast();
    TestTrue(TEXT("Selecting old card does not consume batch"), Run->HasPendingRewardChoice());
    UButton* Confirm = Cast<UButton>(Reward->GetWidgetFromName(TEXT("Btn_ConfirmReplacement")));
    if (!TestNotNull(TEXT("Explicit confirmation button exists"), Confirm)) { return false; }
    Confirm->OnClicked.Broadcast();
    TestFalse(TEXT("Actual replacement click consumes reward once"), Run->HasPendingRewardChoice());
    TestEqual(TEXT("Replacement stores incoming card in selected position"), Run->GetRunState().Cards[0].CardId, FName("Unit_GoblinRogue"));
    TestEqual(TEXT("Replacement keeps capacity"), Run->GetRunState().Cards.Num(), 8);
    // Round-trip the upgraded old-format overcapacity case without discarding content on load.
    Save->RunState = Run->GetRunState(); FLKRunCardState Extra; Extra.CardId = "Unit_Thief"; Extra.UpgradeLevel = 2; Save->RunState.Cards.Add(Extra);
    Save->RunState.SchemaVersion = 6;
    UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
    TestTrue(TEXT("Legacy nine-card save is readable"), Run->LoadExpeditionFromSlot(SlotName, true));
    TestEqual(TEXT("Stage 2 save migrates to stage 3 contract"), Run->GetRunState().SchemaVersion, ULKRunSubsystem::CurrentSchemaVersion);
    TestEqual(TEXT("Stage 2 wallet is preserved exactly"), Run->GetWalletGold(), Save->RunState.WalletGold);
    TestEqual(TEXT("Stage 2 route seed is preserved"), Run->GetRunState().Seed, Save->RunState.Seed);
    TestTrue(TEXT("Legacy excess requires explicit choice"), Run->NeedsDeckReduction());
    Reward->RefreshReward();
    TestEqual(TEXT("Legacy selector preserves all nine choices"), Replacement->GetChildrenCount(), 9);
    TestFalse(TEXT("Map choices are hidden until excess is resolved"), GM->CanSelectNextNode());
    TestFalse(TEXT("Legacy excess cannot advance to combat"), Run->AdvanceToNextBattle());
    TestTrue(TEXT("Player can discard an old card instead of new temporary card"), Run->DiscardExcessCard("Unit_Skeleton"));
    TestEqual(TEXT("Preserved temporary card keeps upgrade"), Run->GetRunState().Cards.Last().UpgradeLevel, 2);
    TestFalse(TEXT("Legacy capacity repaired"), Run->NeedsDeckReduction());
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    return true;
}
#endif
