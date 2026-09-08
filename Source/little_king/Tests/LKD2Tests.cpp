#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

#include "../ALKBattleGameMode.h"
#include "../ALKOpponentBrain.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitBase.h"
#include "../ALKUnitHero.h"
#include "../LKEncounterContent.h"
#include "../ULKGameData.h"
#include "../ULKRunSubsystem.h"

struct FLKD2TestAccess
{
	static ALKOpponentBrain* Brain(ALKBattleGameMode* GM) { return GM ? GM->OpponentBrain.Get() : nullptr; }
	static ULKGameData* AuthoredData(TSubclassOf<ALKBattleGameMode> Mode)
	{
		return Mode ? Mode->GetDefaultObject<ALKBattleGameMode>()->GameData.Get() : nullptr;
	}
	static void SetCooldown(ALKUnitHero* Hero, float Seconds) { if (Hero) { Hero->SkillCooldownRemaining = Seconds; } }
};

namespace
{
constexpr EAutomationTestFlags D2Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

TArray<FLKRunHeroState> D2Heroes()
{
	TArray<FLKRunHeroState> Heroes;
	for (const TPair<FName, float>& Pair : {
		TPair<FName, float>("Hero_Knight", 450.f), TPair<FName, float>("Hero_Mage", 300.f),
		TPair<FName, float>("Hero_Ranger", 350.f) })
	{
		FLKRunHeroState Hero;
		Hero.HeroId = Pair.Key;
		Hero.Health = Hero.MaxHealth = Hero.BaseMaxHealth = Pair.Value;
		if (Pair.Key == "Hero_Knight") { Hero.Traits = { "Trait_KnightTauntAura" }; }
		if (Pair.Key == "Hero_Mage") { Hero.Traits = { "Trait_MageSpellReach" }; }
		Heroes.Add(MoveTemp(Hero));
	}
	return Heroes;
}

TArray<FLKRunCardState> D2Cards()
{
	TArray<FLKRunCardState> Cards;
	for (FName Id : { FName("Unit_Swordsman"), FName("Unit_Archer"), FName("Unit_Shieldbearer"),
		FName("Spell_Fireball"), FName("Spell_HealWave"), FName("Building_ArrowTower"), FName("Building_Barracks") })
	{
		FLKRunCardState Card;
		Card.CardId = Id;
		Cards.Add(Card);
	}
	return Cards;
}

TArray<FName> D2CardIds()
{
	TArray<FName> Ids;
	for (const FLKRunCardState& Card : D2Cards()) { Ids.Add(Card.CardId); }
	return Ids;
}

FLKBattleOutcome D2Outcome(const FLKBattleContext& Context, ELKTeam Winner, float Fraction)
{
	FLKBattleOutcome Outcome;
	Outcome.RunId = Context.RunId;
	Outcome.NodeId = Context.NodeId;
	Outcome.AttemptId = Context.AttemptId;
	Outcome.bFinalized = true;
	Outcome.Stats.Winner = Winner;
	for (const FLKRunHeroState& State : Context.PlayerHeroes)
	{
		FLKHeroBattleOutcome Hero;
		Hero.InstanceId = State.HeroId;
		Hero.RecoveredState = State;
		Hero.RecoveredState.Health = State.MaxHealth * Fraction;
		Hero.HealthBeforeRecovery = FMath::Max(0.f, Hero.RecoveredState.Health - State.MaxHealth * 0.4f);
		Outcome.PlayerHeroes.Add(MoveTemp(Hero));
	}
	return Outcome;
}

TArray<FLKEncounterRow> BuiltInCatalog()
{
	TArray<FLKEncounterRow> Rows;
	FString Error;
	LKEncounterContent::BuildCatalog(nullptr, Rows, Error);
	return Rows;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD2EncounterCatalogTest, "LittleKing.D2.Configuration.EncounterCatalog", D2Flags)
bool FLKD2EncounterCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FLKEncounterRow> BuiltIns = BuiltInCatalog();
	TestEqual(TEXT("D2 ships normal, elite and boss encounters"), BuiltIns.Num(), 3);
	const FLKEncounterRow* Patrol = LKEncounterContent::Find(BuiltIns, "Patrol");
	const FLKEncounterRow* Elite = LKEncounterContent::Find(BuiltIns, "Elite");
	const FLKEncounterRow* Boss = LKEncounterContent::Find(BuiltIns, "Boss");
	if (!TestNotNull(TEXT("Patrol alias resolves"), Patrol) || !TestNotNull(TEXT("Elite alias resolves"), Elite)
		|| !TestNotNull(TEXT("Boss alias resolves"), Boss)) { return false; }
	TestEqual(TEXT("Reward tiers rise with encounter rank"), Patrol->RewardTier + Elite->RewardTier + Boss->RewardTier, 6);
	TestFalse(TEXT("Patrol has no focus tactic"), Patrol->AI.bEnableFocus);
	TestTrue(TEXT("Elite and boss enable encounter-specific focus"), Elite->AI.bEnableFocus && Boss->AI.bEnableFocus);
	TestTrue(TEXT("Boss focus cadence is more aggressive"), Boss->AI.FocusIntervalMax < Elite->AI.FocusIntervalMax);

	FLKEncounterRow Invalid = *Patrol;
	const FName DuplicateHeroId = Invalid.EnemyHeroIds[0];
	Invalid.EnemyHeroIds.Add(DuplicateHeroId);
	FLKEncounterRow Normalized;
	FString Error;
	TestFalse(TEXT("Duplicate enemy heroes are rejected"),
		LKEncounterContent::NormalizeAndValidate(Invalid.EncounterId, Invalid, Normalized, Error));
	TestTrue(TEXT("Validation provides a useful reason"), Error.Contains(TEXT("重复")));

	UDataTable* MissingTable = NewObject<UDataTable>();
	MissingTable->RowStruct = FLKEncounterRow::StaticStruct();
	MissingTable->AddRow(Patrol->EncounterId, *Patrol);
	MissingTable->AddRow(Elite->EncounterId, *Elite);
	TArray<FLKEncounterRow> Loaded;
	TestFalse(TEXT("Configured table missing the boss row fails explicitly"),
		LKEncounterContent::BuildCatalog(MissingTable, Loaded, Error));
	TestTrue(TEXT("Missing-row error names the absent stable ID"), Error.Contains(TEXT("Encounter_SkeletonKing")));

	MissingTable->AddRow(Boss->EncounterId, *Boss);
	TestTrue(TEXT("Complete configured table loads"), LKEncounterContent::BuildCatalog(MissingTable, Loaded, Error));
	TestEqual(TEXT("Complete table returns all three rows"), Loaded.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD2RunStateTest, "LittleKing.D2.Run.EncounterSnapshotAndPermanentState", D2Flags)
bool FLKD2RunStateTest::RunTest(const FString& Parameters)
{
	TArray<FLKEncounterRow> Catalog = BuiltInCatalog();
	FLKEncounterRow* Patrol = Catalog.FindByPredicate([](const FLKEncounterRow& Row)
		{ return Row.EncounterId == "Encounter_UndeadPatrol"; });
	if (!TestNotNull(TEXT("Mutable patrol fixture"), Patrol)) { return false; }
	Patrol->EnemyHeroIds = { "Hero_SkeletonGiant" };
	Patrol->Waves = { { 3.f, "Unit_SkeletonArcher", 2 } };
	Patrol->bEnemyUsesCards = true;
	Patrol->EnemyCards = D2CardIds();
	Patrol->EnemyStartingSilver = 2.f;
	Patrol->AI.bEnableFocus = true;
	Patrol->AI.FocusIntervalMin = Patrol->AI.FocusIntervalMax = 7.f;

	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
	const TArray<FLKRunHeroState> Defaults = D2Heroes();
	TestTrue(TEXT("Run accepts a complete custom encounter catalog"), Run->StartNewRun(Defaults, D2Cards(), 1234, Catalog));
	Patrol->EnemyHeroIds = { "Boss_SkeletonKing" };
	Patrol->Waves[0].Count = 49;

	FLKBattleContext Room1;
	TestTrue(TEXT("Room one starts from the copied catalog"), Run->BeginCurrentBattle(Room1));
	TestEqual(TEXT("Copied enemy roster ignores source mutation"), Room1.Encounter.EnemyHeroIds[0], FName("Hero_SkeletonGiant"));
	TestEqual(TEXT("Copied wave ignores source mutation"), Room1.Encounter.Waves[0].Count, 2);
	TestTrue(TEXT("Enemy deck and AI parameters enter BattleContext"), Room1.bEnemyUsesCards
		&& Room1.EnemyCards.Num() == 7 && Room1.Encounter.AI.FocusIntervalMin == 7.f);
	TestTrue(TEXT("Room one result accepted"), Run->SubmitBattleOutcome(D2Outcome(Room1, ELKTeam::Player, 0.6f)));

	TestTrue(TEXT("Between rooms can upgrade base maximum health"), Run->SetHeroBaseMaxHealth("Hero_Knight", 600.f));
	TestTrue(TEXT("Between rooms can add a permanent trait"), Run->AddHeroTrait("Hero_Ranger", "Trait_FaceFear"));
	TestFalse(TEXT("Duplicate permanent trait is rejected"), Run->AddHeroTrait("Hero_Ranger", "Trait_FaceFear"));
	TestFalse(TEXT("Unknown permanent trait is rejected before the next room"), Run->AddHeroTrait("Hero_Ranger", "Trait_DoesNotExist"));
	TestTrue(TEXT("Between rooms can remove a default trait"), Run->RemoveHeroTrait("Hero_Mage", "Trait_MageSpellReach"));
	TestFalse(TEXT("Removing an absent trait is a no-op"), Run->RemoveHeroTrait("Hero_Mage", "Trait_MageSpellReach"));
	TestTrue(TEXT("Advance after permanent changes"), Run->AdvanceToNextBattle());

	FLKBattleContext Room2;
	TestTrue(TEXT("Room two starts"), Run->BeginCurrentBattle(Room2));
	const FLKRunHeroState* Knight = Room2.PlayerHeroes.FindByPredicate([](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Knight"; });
	const FLKRunHeroState* Mage = Room2.PlayerHeroes.FindByPredicate([](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Mage"; });
	const FLKRunHeroState* Ranger = Room2.PlayerHeroes.FindByPredicate([](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Ranger"; });
	if (!Knight || !Mage || !Ranger) { AddError(TEXT("Room two is missing a player hero")); return false; }
	TestEqual(TEXT("Base maximum upgrade crosses rooms"), Knight->BaseMaxHealth, 600.f);
	TestTrue(TEXT("Existing health remains clamped under upgraded maximum"), Knight->Health <= Knight->MaxHealth);
	TestFalse(TEXT("Removed default trait does not return in next context"), Mage->Traits.Contains("Trait_MageSpellReach"));
	TestTrue(TEXT("Added trait appears in next context"), Ranger->Traits.Contains("Trait_FaceFear"));
	TestFalse(TEXT("Permanent state cannot mutate during combat"), Run->SetHeroBaseMaxHealth("Hero_Knight", 700.f));

	TestTrue(TEXT("A defeat completes the current run"), Run->SubmitBattleOutcome(D2Outcome(Room2, ELKTeam::Enemy, 0.4f)));
	TestTrue(TEXT("Restart uses clean caller-owned defaults"), Run->RestartRun(Defaults, D2Cards(), 1234, BuiltInCatalog()));
	const FLKRunHeroState* FreshKnight = Run->GetRunState().Heroes.FindByPredicate(
		[](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Knight"; });
	TestTrue(TEXT("New run does not inherit previous maximum upgrade"), FreshKnight && FreshKnight->BaseMaxHealth == 450.f);
	TestFalse(TEXT("Caller default traits were never mutated"), Defaults[2].Traits.Contains("Trait_FaceFear"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD2HeroBoundaryTest, "LittleKing.D2.Heroes.RoomBoundaryReset", D2Flags)
bool FLKD2HeroBoundaryTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper World;
	if (!World.CreateTestWorld(EWorldType::Game)) { World.ForwardErrorMessages(this); return false; }
	World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKBattleGameMode::StaticClass();
	if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
	ALKBattleGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
	World.GetTestWorld()->SpawnActor<ALKPlayerController>();
	GM->Tick(0.f);
	ALKUnitHero* Mage = Cast<ALKUnitHero>(GM->SpawnUnitForTeam("Hero_Mage", ELKTeam::Player, FVector(0.f, -500.f, 0.f)));
	ALKUnitBase* Enemy = GM->SpawnUnitForTeam("Unit_Skeleton", ELKTeam::Enemy, FVector(0.f, 500.f, 0.f));
	if (!TestNotNull(TEXT("Mage fixture"), Mage) || !TestNotNull(TEXT("Enemy fixture"), Enemy)) { return false; }
	Mage->SetForcedTarget(Enemy, 30.f);
	Mage->AddAuraTauntSource(Enemy);
	Mage->SetFocusWarning(10.f);
	FLKD2TestAccess::SetCooldown(Mage, 9.f);
	TestTrue(TEXT("Fixture has temporary combat state"), Mage->HasForcedTarget() && Mage->IsTaunting()
		&& Mage->IsUnderFocusWarning() && Mage->GetSkillCooldownRemaining() > 0.f);

	FLKRunHeroState State;
	State.HeroId = "Hero_Mage";
	State.Health = 550.f;
	State.MaxHealth = State.BaseMaxHealth = 600.f;
	TestTrue(TEXT("Run state applies to the new room hero"), Mage->ApplyRunHeroState(State));
	TestEqual(TEXT("Permanent base maximum is restored"), Mage->GetBaseMaxHealth(), 600.f);
	TestEqual(TEXT("Inherited health is restored"), Mage->GetHealth(), 550.f);
	TestFalse(TEXT("Removed default trait stays removed"), Mage->HasTrait("Trait_MageSpellReach"));
	TestTrue(TEXT("Forced target, incoming aura, warning and cooldown are cleared"), !Mage->HasForcedTarget()
		&& !Mage->IsTaunting() && !Mage->IsUnderFocusWarning() && Mage->GetSkillCooldownRemaining() == 0.f);

	State.BaseMaxHealth = 200.f;
	State.MaxHealth = 600.f;
	State.Health = 550.f;
	TestTrue(TEXT("A lower current configuration remains a valid migration input"), Mage->ApplyRunHeroState(State));
	TestEqual(TEXT("Health clamps after maximum-health recomputation"), Mage->GetHealth(), 200.f);
	State.Traits = { "Trait_DoesNotExist" };
	TestFalse(TEXT("Unknown permanent trait rejects room restoration"), Mage->ApplyRunHeroState(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD2AuthoredIntegrationTest, "LittleKing.D2.Integration.AuthoredEncounterSnapshot", D2Flags)
bool FLKD2AuthoredIntegrationTest::RunTest(const FString& Parameters)
{
	// D5：真实 GM 会写固定存档槽；测试前清理避免读到其它测试/残留恢复状态。
	UGameplayStatics::DeleteGameInSlot(ULKRunSubsystem::GetRunSlotName(), 0);
	TSubclassOf<ALKBattleGameMode> AuthoredMode = LoadClass<ALKBattleGameMode>(
		nullptr, TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
	if (!TestTrue(TEXT("Authored GameMode loads"), bool(AuthoredMode))) { return false; }
	ULKGameData* AuthoredData = FLKD2TestAccess::AuthoredData(AuthoredMode);
	if (!TestNotNull(TEXT("Authored GameData exists"), AuthoredData)) { return false; }
	const FName AuthoredEncounterBefore = AuthoredData->EnemyEncounterId;

	FTestWorldWrapper World;
	if (!World.CreateTestWorld(EWorldType::Game)) { World.ForwardErrorMessages(this); return false; }
	World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AuthoredMode;
	if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
	ALKBattleGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
	World.GetTestWorld()->SpawnActor<ALKPlayerController>();
	GM->Tick(0.f);
	if (!TestNotNull(TEXT("Authored GameMode starts"), GM)) { return false; }
	TestTrue(TEXT("Runtime GameData is a private duplicate"), GM->GetGameData() != AuthoredData);
	TestEqual(TEXT("Shared authored encounter value was not changed"), AuthoredData->EnemyEncounterId, AuthoredEncounterBefore);
	UDataTable* EncounterTable = GM->GetGameData()->EncounterTable.LoadSynchronous();
	TestTrue(TEXT("DT_Encounters asset is assigned and uses FLKEncounterRow"), EncounterTable
		&& EncounterTable->GetRowStruct() == FLKEncounterRow::StaticStruct());
	const FLKEncounterRow Current = GM->GetCurrentEncounter();
	TestEqual(TEXT("First room uses normal reward tier"), Current.RewardTier, 1);
	TestEqual(TEXT("First room rank is normal"), Current.Rank, ELKEncounterRank::Normal);
	TestEqual(TEXT("Player deployment roster remains all three heroes"), GM->GetRequiredHeroCount(), 3);
	ALKOpponentBrain* Brain = FLKD2TestAccess::Brain(GM);
	if (!TestNotNull(TEXT("Encounter brain exists"), Brain)) { return false; }
	TestFalse(TEXT("Patrol disables focus and card play from its snapshot"), Brain->IsFocusEnabled() || Brain->UsesCards());
	TestEqual(TEXT("Patrol uses its authored wave count"), Brain->GetWaves().Num(), Current.Waves.Num());
	ULKRunSubsystem* Run = World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
	// D4：目录三行模板 + 动态生成四战遭遇（R1/R2/R3/Boss）共 7 行快照。
	TestTrue(TEXT("Run owns a seven-row snapshot (3 templates + 4 dynamic)"), Run && Run->GetRunState().Encounters.Num() == 7);
	GM->GetGameData()->EnemyEncounterId = "Runtime_Only";
	TestEqual(TEXT("Runtime config edits do not mutate the run catalog"), Run->GetRunState().Encounters.Num(), 7);
	TestEqual(TEXT("Runtime config edits do not mutate the shared asset"), AuthoredData->EnemyEncounterId, AuthoredEncounterBefore);
	// 清理本测试自动写下的固定槽。
	UGameplayStatics::DeleteGameInSlot(ULKRunSubsystem::GetRunSlotName(), 0);
	return true;
}

#endif
