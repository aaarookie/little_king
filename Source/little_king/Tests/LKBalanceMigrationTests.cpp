#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#include "../LKBalanceRules.h"
#include "../LKCardRules.h"
#include "../LKEncounterContent.h"
#include "../LKHomeContent.h"
#include "../LKRunTypes.h"
#include "../LKUnitContent.h"
#include "../ULKRunSaveGame.h"
#include "../ULKRunSubsystem.h"

/**
 * Balance V1 存档迁移行为回归（docs/48 第 7 节）。
 *
 * 只使用唯一的临时槽创建/删除本测试自己的文件，不读写 Saved/SaveGames 里既有存档，
 * 也不使用真实玩家档作为输入；不启动任何引擎构建或命令行流程。
 */
namespace
{
constexpr EAutomationTestFlags BalanceMigrationFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

/** 唯一临时槽名：与既有测试的固定槽隔离，避免并行/重复运行互相覆盖。 */
FString MakeMigrationSlot(const TCHAR* Tag)
{
	return FString::Printf(TEXT("LittleKing_BalanceMig_%s_%s"), Tag,
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

void CleanupMigrationSlot(const FString& Slot)
{
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) { UGameplayStatics::DeleteGameInSlot(Slot, 0); }
}

bool WriteRunStateToSlot(const FString& Slot, const FLKRunState& State)
{
	ULKRunSaveGame* Save = NewObject<ULKRunSaveGame>();
	Save->SaveVersion = 1;
	Save->RunState = State;
	return UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
}

TArray<FLKEncounterRow> BuiltInCatalog()
{
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	LKEncounterContent::BuildCatalog(nullptr, Encounters, Error);
	return Encounters;
}

TArray<FLKRunHeroState> NewBaselineHeroes()
{
	TArray<FLKRunHeroState> Heroes;
	for (FName HeroId : LKHomeContent::DefaultUnlockedHeroes())
	{
		FLKRunHeroState Hero;
		Hero.HeroId = HeroId;
		const FLKUnitRow* Row = LKUnitContent::Find(HeroId);
		Hero.Health = Hero.MaxHealth = Hero.BaseMaxHealth = Row ? Row->BaseHealth : 1.f;
		Heroes.Add(MoveTemp(Hero));
	}
	return Heroes;
}

TArray<FLKRunCardState> MakeDeck(const TArray<FName>& CardIds)
{
	TArray<FLKRunCardState> Cards;
	for (FName CardId : CardIds)
	{
		FLKRunCardState Card;
		Card.CardId = CardId;
		Cards.Add(MoveTemp(Card));
	}
	return Cards;
}

TArray<FLKRunCardState> NewBaselineCards()
{
	return MakeDeck(LKCardRules::ReplacementCardIds());
}

TArray<FName> DeckIds(const TArray<FLKRunCardState>& Cards)
{
	TArray<FName> Ids;
	for (const FLKRunCardState& Card : Cards) { Ids.Add(Card.CardId); }
	return Ids;
}

bool DeckContains(const TArray<FLKRunCardState>& Cards, FName CardId)
{
	return Cards.ContainsByPredicate([CardId](const FLKRunCardState& Card) { return Card.CardId == CardId; });
}

const FLKRunHeroState* FindHero(const FLKRunState& State, FName HeroId)
{
	return State.Heroes.FindByPredicate([HeroId](const FLKRunHeroState& Hero) { return Hero.HeroId == HeroId; });
}

FLKRunHeroState* FindHeroMutable(FLKRunState& State, FName HeroId)
{
	return State.Heroes.FindByPredicate([HeroId](const FLKRunHeroState& Hero) { return Hero.HeroId == HeroId; });
}

const FLKRunCardState* FindRunCard(const FLKRunState& State, FName CardId)
{
	return State.Cards.FindByPredicate([CardId](const FLKRunCardState& Card) { return Card.CardId == CardId; });
}

FLKDungeonNode* FindNodeMutable(FLKRunState& State, FName NodeId)
{
	return State.Nodes.FindByPredicate([NodeId](const FLKDungeonNode& Node) { return Node.NodeId == NodeId; });
}

FLKEncounterRow* FindEncounterMutable(FLKRunState& State, FName EncounterId)
{
	return State.Encounters.FindByPredicate([EncounterId](const FLKEncounterRow& Row) { return Row.EncounterId == EncounterId; });
}

const FLKEncounterRow* FindEncounter(const FLKRunState& State, FName EncounterId)
{
	return State.Encounters.FindByPredicate([EncounterId](const FLKEncounterRow& Row) { return Row.EncounterId == EncounterId; });
}

FLKRunRewardOffer MakeUpgradeOffer(FName CardId, int32 LevelBefore, int32 LevelAfter)
{
	FLKRunRewardOffer Offer;
	Offer.Kind = ELKRunRewardKind::UpgradeCard;
	Offer.CardId = CardId;
	Offer.LevelBefore = LevelBefore;
	Offer.LevelAfter = LevelAfter;
	return Offer;
}

FLKRunRewardOffer MakeAddOffer(FName CardId)
{
	FLKRunRewardOffer Offer;
	Offer.Kind = ELKRunRewardKind::AddCard;
	Offer.CardId = CardId;
	return Offer;
}

FLKBattleOutcome MakeHistoryEntry(FName NodeId, FGuid AttemptId)
{
	FLKBattleOutcome Outcome;
	Outcome.RunId = FGuid::NewGuid();
	Outcome.NodeId = NodeId;
	Outcome.AttemptId = AttemptId;
	Outcome.bFinalized = true;
	Outcome.Stats.Winner = ELKTeam::Player;
	return Outcome;
}

/** 相对误差比较：迁移会做 450→6000 这类同比乘法，不要求逐位相等。 */
bool IsNearlyScaled(float Actual, float Expected)
{
	return FMath::IsNearlyEqual(Actual, Expected, FMath::Max(1.f, FMath::Abs(Expected)) * 1.e-5f);
}

/** 用一次合法新远征造出"Balance V1 之前"的旧档状态（BalanceVersion=0），随后由调用方降级数值。 */
bool MakeLegacyRunState(ULKRunSubsystem* Seed, int32 SeedValue, FLKRunState& OutState)
{
	if (!Seed->StartNewRun(NewBaselineHeroes(), NewBaselineCards(), SeedValue, BuiltInCatalog())) { return false; }
	OutState = Seed->GetRunState();
	OutState.BalanceVersion = 0;
	return true;
}

/**
 * 把一个已完成节点及其遭遇冻结成旧快照（BalanceVersion=0 + 旧稀疏波次），
 * 用于验证迁移不会再改写已完成遭遇。
 */
void FreezeCompletedEncounter(FLKRunState& State, FName NodeId, FName EncounterId)
{
	if (FLKDungeonNode* Node = FindNodeMutable(State, NodeId)) { Node->bResolved = true; }
	if (FLKEncounterRow* Encounter = FindEncounterMutable(State, EncounterId))
	{
		Encounter->BalanceVersion = 0;
		Encounter->Depth = 0;
		Encounter->EnemyHealthScale = 1.f;
		Encounter->EnemyDamageScale = 1.f;
		Encounter->EnemyHeroHealthScale.Reset();
		Encounter->Waves = { { 5.f, FName("Unit_SkeletonArcher"), 1 } };
	}
}
}

// ---------------------------------------------------------------------------
// 1) 生命同比换算：半血保持半血、永久基础升级不丢、0 血不复活、自定义英雄不迁移。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceHealthMigrationTest,
	"LittleKing.BalanceV1.Migration.HealthRatioAndPermanentUpgrades", BalanceMigrationFlags)
bool FLKBalanceHealthMigrationTest::RunTest(const FString& Parameters)
{
	const FString Slot = MakeMigrationSlot(TEXT("Health"));
	CleanupMigrationSlot(Slot);

	const FLKUnitRow* KnightRow = LKUnitContent::Find("Hero_Knight");
	const FLKUnitRow* MageRow = LKUnitContent::Find("Hero_Mage");
	const FLKUnitRow* RangerRow = LKUnitContent::Find("Hero_Ranger");
	if (!TestNotNull(TEXT("Player hero rows resolve"), KnightRow) || !TestNotNull(TEXT("Mage row resolves"), MageRow)
		|| !TestNotNull(TEXT("Ranger row resolves"), RangerRow)) { return false; }

	ULKRunSubsystem* Seed = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Seed->SetAutoSaveEnabled(false);
	FLKRunState Legacy;
	if (!TestTrue(TEXT("Seed run starts"), MakeLegacyRunState(Seed, 6101, Legacy))) { return false; }

	// 旧原生基准：骑士 450 / 法师 300 / 游侠 350（docs/48 §3 之前）。
	FLKRunHeroState* Knight = FindHeroMutable(Legacy, "Hero_Knight");
	FLKRunHeroState* Mage = FindHeroMutable(Legacy, "Hero_Mage");
	FLKRunHeroState* Ranger = FindHeroMutable(Legacy, "Hero_Ranger");
	if (!TestNotNull(TEXT("Knight snapshot exists"), Knight) || !TestNotNull(TEXT("Mage snapshot exists"), Mage)
		|| !TestNotNull(TEXT("Ranger snapshot exists"), Ranger)) { return false; }
	Knight->BaseMaxHealth = 450.f; Knight->MaxHealth = 450.f; Knight->Health = 225.f; // 半血
	// 永久基础升级（600 = 旧原生 2 倍）+ 特性最大生命 1.5 倍；450/900 = 半血。
	Mage->BaseMaxHealth = 600.f; Mage->MaxHealth = 900.f; Mage->Health = 450.f;
	Ranger->BaseMaxHealth = 350.f; Ranger->MaxHealth = 350.f; Ranger->Health = 0.f;   // 不可战斗的 0 血
	// 没有旧基准的自定义英雄：即使有单位行也不得被迁移。
	FLKRunHeroState Custom;
	Custom.HeroId = "Hero_Necromancer";
	Custom.BaseMaxHealth = 180.f; Custom.MaxHealth = 180.f; Custom.Health = 90.f;
	Legacy.Heroes.Add(Custom);

	const FGuid ReceiptId = FGuid::NewGuid();
	const FGuid ClaimedBatchId = FGuid::NewGuid();
	Legacy.WalletGold = 777;
	Legacy.StartingGold = 123;
	Legacy.PendingGold = 45;
	Legacy.PendingSettlement.SettlementId = ReceiptId;
	Legacy.PendingSettlement.GoldAmount = 45;
	Legacy.PendingSettlement.bProfileApplied = true;
	Legacy.ClaimedRewardBatchIds = { ClaimedBatchId };
	if (!TestTrue(TEXT("Legacy save written"), WriteRunStateToSlot(Slot, Legacy))) { return false; }

	ULKRunSubsystem* Loaded = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Loaded->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Legacy save loads"), Loaded->LoadExpeditionFromSlot(Slot))) { return false; }
	const FLKRunState Migrated = Loaded->GetRunState();

	TestEqual(TEXT("Balance version upgraded once"), Migrated.BalanceVersion, LKBalanceRules::CurrentBalanceVersion);
	TestEqual(TEXT("Schema version upgraded to current"), Migrated.SchemaVersion, ULKRunSubsystem::CurrentSchemaVersion);
	TestTrue(TEXT("Migration notice is published for the UI"), !Loaded->GetMigrationNotice().IsEmpty());
	TestTrue(TEXT("Active migration is reported as a rewrite"), Loaded->WasMigratedForBalance());

	// 450 -> 6000 同比：半血仍是新基准的一半。
	const FLKRunHeroState* MigratedKnight = FindHero(Migrated, "Hero_Knight");
	TestTrue(TEXT("Knight keeps 50% health after the 450->6000 rescale"),
		MigratedKnight && IsNearlyScaled(MigratedKnight->BaseMaxHealth, KnightRow->BaseHealth)
		&& IsNearlyScaled(MigratedKnight->MaxHealth, KnightRow->BaseHealth)
		&& IsNearlyScaled(MigratedKnight->Health, KnightRow->BaseHealth * 0.5f));
	// 600/900 -> 8400/12600：永久基础升级与特性倍率都按同比保留，450 血仍是 50%。
	const FLKRunHeroState* MigratedMage = FindHero(Migrated, "Hero_Mage");
	TestTrue(TEXT("Mage keeps the permanent base upgrade (2x old native)"),
		MigratedMage && IsNearlyScaled(MigratedMage->BaseMaxHealth, MageRow->BaseHealth * 2.f));
	TestTrue(TEXT("Mage keeps the trait maximum multiplier (1.5x)"),
		MigratedMage && IsNearlyScaled(MigratedMage->MaxHealth, MageRow->BaseHealth * 3.f));
	TestTrue(TEXT("Mage keeps the health percentage instead of healing to full"),
		MigratedMage && IsNearlyScaled(MigratedMage->Health, MageRow->BaseHealth * 1.5f));
	TestTrue(TEXT("Mage never gains extra health from the old base ratio"),
		MigratedMage && MigratedMage->Health < MigratedMage->MaxHealth * 0.5001f);
	// 0 血仍是失能：只随基准放大上限，不因更新复活。
	const FLKRunHeroState* MigratedRanger = FindHero(Migrated, "Hero_Ranger");
	TestTrue(TEXT("Zero-health ranger stays incapacitated"),
		MigratedRanger && MigratedRanger->Health == 0.f
		&& IsNearlyScaled(MigratedRanger->MaxHealth, RangerRow->BaseHealth)
		&& IsNearlyScaled(MigratedRanger->BaseMaxHealth, RangerRow->BaseHealth));
	// 自定义英雄没有旧基准：三项数值原样保留。
	const FLKRunHeroState* MigratedCustom = FindHero(Migrated, "Hero_Necromancer");
	TestTrue(TEXT("Custom hero without a legacy baseline is not rescaled"),
		MigratedCustom && MigratedCustom->Health == 90.f && MigratedCustom->MaxHealth == 180.f
		&& MigratedCustom->BaseMaxHealth == 180.f);

	// 钱包/回执/历史基数一律不变。
	TestEqual(TEXT("Wallet is untouched"), Migrated.WalletGold, 777);
	TestEqual(TEXT("Departure funding is untouched"), Migrated.StartingGold, 123);
	TestEqual(TEXT("Pending gold is untouched"), Migrated.PendingGold, 45);
	TestTrue(TEXT("Settlement receipt is untouched"), Migrated.PendingSettlement.SettlementId == ReceiptId
		&& Migrated.PendingSettlement.GoldAmount == 45 && Migrated.PendingSettlement.bProfileApplied);
	TestEqual(TEXT("Claimed reward receipts are untouched"), Migrated.ClaimedRewardBatchIds.Num(), 1);
	TestTrue(TEXT("The original claim receipt survives"), Migrated.ClaimedRewardBatchIds.Contains(ClaimedBatchId));

	FString MigratedError;
	TestTrue(TEXT("Migrated state validates"), ULKRunSubsystem::ValidateStoredRun(Migrated, MigratedError));
	CleanupMigrationSlot(Slot);
	return true;
}

// ---------------------------------------------------------------------------
// 2) 终态（Completed/Failed/Abandoned）旧远征：只补版本标记，历史内容一律不改。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceTerminalHistoryTest,
	"LittleKing.BalanceV1.Migration.TerminalHistoryPreserved", BalanceMigrationFlags)
bool FLKBalanceTerminalHistoryTest::RunTest(const FString& Parameters)
{
	const ELKRunPhase TerminalPhases[] = { ELKRunPhase::Completed, ELKRunPhase::Failed, ELKRunPhase::Abandoned };
	int32 CaseIndex = 0;
	for (ELKRunPhase TerminalPhase : TerminalPhases)
	{
		++CaseIndex;
		const FString Slot = MakeMigrationSlot(*FString::Printf(TEXT("Terminal%d"), CaseIndex));
		CleanupMigrationSlot(Slot);

		ULKRunSubsystem* Seed = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
		Seed->SetAutoSaveEnabled(false);
		FLKRunState Legacy;
		if (!TestTrue(TEXT("Seed run starts"), MakeLegacyRunState(Seed, 6200 + CaseIndex, Legacy))) { return false; }

		// 旧档里玩家持有两张敌方专属骷髅卡（旧版本合法获得的既成历史）。
		Legacy.Cards = MakeDeck({ "Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer",
			"Building_ArrowTower", "Building_Barracks", "Unit_Skeleton", "Unit_SkeletonArcher" });
		if (FLKRunCardState* Swordsman = Legacy.Cards.FindByPredicate(
			[](const FLKRunCardState& Card) { return Card.CardId == "Unit_Swordsman"; }))
		{ Swordsman->UpgradeLevel = 2; }
		Legacy.InitialLoadout.CardIds = DeckIds(Legacy.Cards);
		Legacy.PendingRewardBatchId = FGuid::NewGuid();
		Legacy.PendingRewardOffers = { MakeUpgradeOffer("Unit_Skeleton", 1, 2) };
		Legacy.bRewardOfferedForCurrentNode = true;
		const FGuid ClaimedBatchId = FGuid::NewGuid();
		Legacy.ClaimedRewardBatchIds = { ClaimedBatchId };
		const FGuid HistoryAttemptId = FGuid::NewGuid();
		Legacy.BattleHistory = { MakeHistoryEntry("Node_R1", HistoryAttemptId) };
		Legacy.Phase = TerminalPhase;

		FLKRunHeroState* Knight = FindHeroMutable(Legacy, "Hero_Knight");
		if (!TestNotNull(TEXT("Knight snapshot exists"), Knight)) { return false; }
		Knight->BaseMaxHealth = 450.f; Knight->MaxHealth = 450.f; Knight->Health = 225.f;

		const FGuid ReceiptId = FGuid::NewGuid();
		Legacy.WalletGold = 555;
		Legacy.PendingGold = 30;
		Legacy.PendingSettlement.SettlementId = ReceiptId;
		Legacy.PendingSettlement.GoldAmount = 42;
		Legacy.PendingSettlement.TerminalPhase = TerminalPhase;
		Legacy.PendingSettlement.bProfileApplied = false;
		// 已完成节点的遭遇保持旧快照。
		FreezeCompletedEncounter(Legacy, "Node_R1", "Enc_Dyn_R1");
		if (!TestTrue(TEXT("Terminal legacy save written"), WriteRunStateToSlot(Slot, Legacy))) { return false; }

		ULKRunSubsystem* Loaded = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
		Loaded->SetAutoSaveEnabled(false);
		if (!TestTrue(TEXT("Terminal legacy save loads"), Loaded->LoadExpeditionFromSlot(Slot))) { return false; }
		const FLKRunState Migrated = Loaded->GetRunState();

		TestEqual(TEXT("Terminal phase is preserved"), Migrated.Phase, TerminalPhase);
		TestEqual(TEXT("Terminal run only gets the balance marker"), Migrated.BalanceVersion, LKBalanceRules::CurrentBalanceVersion);
		TestEqual(TEXT("Terminal run also upgrades the schema"), Migrated.SchemaVersion, ULKRunSubsystem::CurrentSchemaVersion);
		TestFalse(TEXT("Terminal history is not reported as rewritten"), Loaded->WasMigratedForBalance());
		TestTrue(TEXT("Terminal history notice is still available for the UI"), !Loaded->GetMigrationNotice().IsEmpty());

		// 英雄不变。
		const FLKRunHeroState* MigratedKnight = FindHero(Migrated, "Hero_Knight");
		TestTrue(TEXT("Terminal hero health is untouched"),
			MigratedKnight && MigratedKnight->Health == 225.f && MigratedKnight->MaxHealth == 450.f
			&& MigratedKnight->BaseMaxHealth == 450.f);
		// 牌组不变（含两张敌方专属卡与既有升级）。
		TestEqual(TEXT("Terminal deck size is untouched"), Migrated.Cards.Num(), 7);
		const FLKRunCardState* MigratedSwordsman = FindRunCard(Migrated, "Unit_Swordsman");
		TestTrue(TEXT("Terminal deck keeps the skeleton cards"),
			DeckContains(Migrated.Cards, "Unit_Skeleton") && DeckContains(Migrated.Cards, "Unit_SkeletonArcher"));
		TestTrue(TEXT("Terminal deck keeps the old upgrade level"), MigratedSwordsman && MigratedSwordsman->UpgradeLevel == 2);
		TestTrue(TEXT("Terminal loadout snapshot is untouched"),
			Migrated.InitialLoadout.CardIds.Contains("Unit_Skeleton") || Migrated.InitialLoadout.CardIds.Contains("Unit_SkeletonArcher"));
		// 冻结奖励与领取回执不变。
		if (!TestEqual(TEXT("Terminal pending reward count is untouched"), Migrated.PendingRewardOffers.Num(), 1)) { return false; }
		TestTrue(TEXT("Terminal pending upgrade still points at the frozen skeleton candidate"),
			Migrated.PendingRewardOffers[0].CardId == "Unit_Skeleton"
			&& Migrated.PendingRewardOffers[0].LevelBefore == 1 && Migrated.PendingRewardOffers[0].LevelAfter == 2);
		TestEqual(TEXT("Terminal claimed receipts are untouched"), Migrated.ClaimedRewardBatchIds.Num(), 1);
		TestTrue(TEXT("The original terminal receipt survives"), Migrated.ClaimedRewardBatchIds.Contains(ClaimedBatchId));
		// 历史战报不变。
		if (!TestEqual(TEXT("Terminal battle history count is untouched"), Migrated.BattleHistory.Num(), 1)) { return false; }
		TestTrue(TEXT("Terminal battle history entry is untouched"), Migrated.BattleHistory[0].AttemptId == HistoryAttemptId);
		// 钱包与结算回执不变。
		TestEqual(TEXT("Terminal wallet is untouched"), Migrated.WalletGold, 555);
		TestEqual(TEXT("Terminal pending gold is untouched"), Migrated.PendingGold, 30);
		TestTrue(TEXT("Terminal settlement receipt is untouched"),
			Migrated.PendingSettlement.SettlementId == ReceiptId && Migrated.PendingSettlement.GoldAmount == 42
			&& !Migrated.PendingSettlement.bProfileApplied);
		// 已完成遭遇保持旧快照。
		const FLKEncounterRow* FrozenEncounter = FindEncounter(Migrated, "Enc_Dyn_R1");
		TestTrue(TEXT("Terminal completed encounter keeps the legacy snapshot"),
			FrozenEncounter && FrozenEncounter->BalanceVersion == 0 && FrozenEncounter->Waves.Num() == 1
			&& FrozenEncounter->Waves[0].Count == 1 && FrozenEncounter->EnemyHealthScale == 1.f);

		FString TerminalError;
		TestTrue(TEXT("Terminal migrated state validates with history intact"),
			ULKRunSubsystem::ValidateStoredRun(Migrated, TerminalError));
		CleanupMigrationSlot(Slot);
	}
	return true;
}

// ---------------------------------------------------------------------------
// 3) 禁用卡替换：七张基础卡含法术、补齐五张、奖励候选确定性替换、副本同步、不重写历史。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceDisabledCardReplacementTest,
	"LittleKing.BalanceV1.Migration.DisabledCardAndRewardReplacement", BalanceMigrationFlags)
bool FLKBalanceDisabledCardReplacementTest::RunTest(const FString& Parameters)
{
	const FString Slot = MakeMigrationSlot(TEXT("Cards"));
	CleanupMigrationSlot(Slot);

	ULKRunSubsystem* Seed = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Seed->SetAutoSaveEnabled(false);
	FLKRunState Legacy;
	if (!TestTrue(TEXT("Seed run starts"), MakeLegacyRunState(Seed, 6301, Legacy))) { return false; }

	// 五张非法术候选全部已被占用：只剩火球/治疗波可以替换两张骷髅卡。
	Legacy.Cards = MakeDeck({ "Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer",
		"Building_ArrowTower", "Building_Barracks", "Unit_Skeleton", "Unit_SkeletonArcher" });
	Legacy.InitialLoadout.CardIds = DeckIds(Legacy.Cards);
	Legacy.PendingRewardBatchId = FGuid::NewGuid();
	Legacy.PendingRewardOffers = { MakeUpgradeOffer("Unit_Skeleton", 2, 3), MakeAddOffer("Unit_SkeletonArcher") };
	Legacy.bRewardOfferedForCurrentNode = true;
	Legacy.Phase = ELKRunPhase::ChoosingReward;
	const FGuid ClaimedBatchId = FGuid::NewGuid();
	Legacy.ClaimedRewardBatchIds = { ClaimedBatchId };
	const FGuid HistoryAttemptId = FGuid::NewGuid();
	Legacy.BattleHistory = { MakeHistoryEntry("Node_R1", HistoryAttemptId) };
	// 待开战副本仍带着旧牌组与旧快照：迁移必须同步。
	Legacy.PendingBattle.PlayerCards = MakeDeck({ "Unit_Swordsman", "Unit_Archer", "Unit_Shieldbearer",
		"Building_ArrowTower", "Building_Barracks", "Unit_Skeleton", "Unit_SkeletonArcher" });
	Legacy.PendingBattle.Encounter.EnemySilverPerSecond = 0.123f;
	const FGuid PendingBatchId = Legacy.PendingRewardBatchId;
	if (!TestTrue(TEXT("Legacy save written"), WriteRunStateToSlot(Slot, Legacy))) { return false; }

	ULKRunSubsystem* Loaded = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Loaded->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Legacy save loads"), Loaded->LoadExpeditionFromSlot(Slot))) { return false; }
	const FLKRunState Migrated = Loaded->GetRunState();

	// 牌组：无禁用卡、无重复、仍至少五张，两张骷髅换成两张法术。
	TestTrue(TEXT("Migrated pending encounter enables tactical spells"),Migrated.PendingBattle.Encounter.EnemySpell.bEnabled);
	TestEqual(TEXT("Deck keeps seven unique cards"), Migrated.Cards.Num(), 7);
	bool bAllObtainable = true;
	TSet<FName> DeckSeen;
	for (const FLKRunCardState& Card : Migrated.Cards)
	{
		bAllObtainable &= LKCardRules::IsPlayerObtainable(Card.CardId) && !DeckSeen.Contains(Card.CardId);
		DeckSeen.Add(Card.CardId);
	}
	TestTrue(TEXT("Deck has no enemy-only or duplicate cards"), bAllObtainable);
	TestTrue(TEXT("Spells are eligible replacements for the two skeletons"),
		DeckContains(Migrated.Cards, "Spell_Fireball") && DeckContains(Migrated.Cards, "Spell_HealWave"));
	TestTrue(TEXT("The five occupied non-spell candidates keep their own cards"),
		DeckContains(Migrated.Cards, "Unit_Swordsman") && DeckContains(Migrated.Cards, "Unit_Archer")
		&& DeckContains(Migrated.Cards, "Unit_Shieldbearer") && DeckContains(Migrated.Cards, "Building_ArrowTower")
		&& DeckContains(Migrated.Cards, "Building_Barracks"));
	TestTrue(TEXT("Legacy deck order is preserved for untouched cards"),
		Migrated.Cards[0].CardId == "Unit_Swordsman" && Migrated.Cards[1].CardId == "Unit_Archer"
		&& Migrated.Cards[2].CardId == "Unit_Shieldbearer" && Migrated.Cards[3].CardId == "Building_ArrowTower"
		&& Migrated.Cards[4].CardId == "Building_Barracks");

	// 战备快照同步：同样不再含禁用卡。
	bool bLoadoutObtainable = true;
	TSet<FName> LoadoutSeen;
	for (FName CardId : Migrated.InitialLoadout.CardIds)
	{
		bLoadoutObtainable &= LKCardRules::IsPlayerObtainable(CardId) && !LoadoutSeen.Contains(CardId);
		LoadoutSeen.Add(CardId);
	}
	TestTrue(TEXT("Initial loadout snapshot is synced without enemy-only cards"),
		bLoadoutObtainable && Migrated.InitialLoadout.CardIds.Num() >= LKCardRules::MinimumCards);

	// 待开战副本同步 + 遭遇快照重建（旧副本里的 0.123 标记必须被刷掉）。
	TestTrue(TEXT("Pending battle deck copy matches the migrated deck"),
		DeckIds(Migrated.PendingBattle.PlayerCards) == DeckIds(Migrated.Cards));
	TestTrue(TEXT("Pending battle snapshot is rebuilt from the migrated encounter"),
		FMath::IsNearlyEqual(Migrated.PendingBattle.Encounter.EnemySilverPerSecond,
			LKBalanceRules::EnemySilverRate(Migrated.PendingBattle.Encounter.Rank), 1.e-4f)
		&& Migrated.PendingBattle.Encounter.BalanceVersion == LKBalanceRules::CurrentBalanceVersion);
	TestTrue(TEXT("Pending battle keeps its attempt identity"),
		Migrated.PendingBattle.AttemptId == Legacy.PendingBattle.AttemptId);

	// 待领奖励：候选合法、无重复、等级合法；UpgradeCard 指向骷髅时确定性换成合法升级。
	if (!TestEqual(TEXT("Both reward candidates remain claimable"), Migrated.PendingRewardOffers.Num(), 2)) { return false; }
	TSet<FName> OfferSeen;
	for (const FLKRunRewardOffer& Offer : Migrated.PendingRewardOffers)
	{
		TestTrue(TEXT("Reward candidate is player-obtainable"), LKCardRules::IsPlayerObtainable(Offer.CardId));
		TestFalse(TEXT("Reward candidates are unique inside the batch"), OfferSeen.Contains(Offer.CardId));
		OfferSeen.Add(Offer.CardId);
		if (Offer.Kind == ELKRunRewardKind::UpgradeCard)
		{
			const FLKRunCardState* Target = FindRunCard(Migrated, Offer.CardId);
			TestTrue(TEXT("Upgrade candidate exists in the migrated deck"), Target != nullptr);
			TestTrue(TEXT("Upgrade levels are legal"),
				Target && Offer.LevelBefore == Target->UpgradeLevel && Offer.LevelAfter == Offer.LevelBefore + 1);
		}
		else
		{
			TestTrue(TEXT("Add-card candidate keeps zero levels"), Offer.LevelBefore == 0 && Offer.LevelAfter == 0);
			TestFalse(TEXT("Add-card candidate is not already owned"), DeckContains(Migrated.Cards, Offer.CardId));
		}
	}
	// 无重抽、无回执变化、历史报告不改。
	TestTrue(TEXT("Reward batch identity is preserved (no re-draw)"), Migrated.PendingRewardBatchId == PendingBatchId);
	TestEqual(TEXT("Claimed receipts are untouched"), Migrated.ClaimedRewardBatchIds.Num(), 1);
	TestTrue(TEXT("Original claim receipt survives"), Migrated.ClaimedRewardBatchIds.Contains(ClaimedBatchId));
	TestEqual(TEXT("Battle history is not rewritten"), Migrated.BattleHistory.Num(), 1);
	TestTrue(TEXT("Battle history entry is untouched"), Migrated.BattleHistory[0].AttemptId == HistoryAttemptId);

	// 迁移后的候选真的可以领取（等级校验与生成时一致）。
	FString MigratedError;
	TestTrue(TEXT("Migrated reward state validates"), ULKRunSubsystem::ValidateStoredRun(Migrated, MigratedError));
	const FName FirstOfferCardId = Migrated.PendingRewardOffers[0].CardId;
	const int32 FirstOfferLevelAfter = Migrated.PendingRewardOffers[0].LevelAfter;
	TestTrue(TEXT("Migrated reward can be claimed"), Loaded->ChooseReward(0));
	const FLKRunCardState* UpgradedCard = FindRunCard(Loaded->GetRunState(), FirstOfferCardId);
	TestTrue(TEXT("Claiming the migrated reward applies the promised level"),
		UpgradedCard && UpgradedCard->UpgradeLevel == FirstOfferLevelAfter);
	TestFalse(TEXT("Claimed batch cannot be consumed twice"), Loaded->HasPendingRewardChoice());

	CleanupMigrationSlot(Slot);
	return true;
}

// ---------------------------------------------------------------------------
// 4) 非法快照：拒绝载入、保留调用前的内存远征、迁移提示 flag 不泄漏。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceInvalidSnapshotTest,
	"LittleKing.BalanceV1.Migration.InvalidSnapshotKeepsMemoryState", BalanceMigrationFlags)
bool FLKBalanceInvalidSnapshotTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("平衡迁移结果校验失败"), EAutomationExpectedErrorFlags::Contains, 1);
	// 调用方已经有一个正在使用的远征。
	ULKRunSubsystem* Active = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Active->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Active run starts"), Active->StartNewRun(NewBaselineHeroes(), NewBaselineCards(), 6401, BuiltInCatalog())))
	{ return false; }
	const FLKRunState Before = Active->GetRunState();
	TestTrue(TEXT("Active run is in progress"), Active->HasRunInProgress());
	TestFalse(TEXT("No migration notice before any load"), Active->WasMigratedForBalance());

	ULKRunSubsystem* Seed = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Seed->SetAutoSaveEnabled(false);
	FLKRunState Base;
	if (!TestTrue(TEXT("Seed run starts"), MakeLegacyRunState(Seed, 6402, Base))) { return false; }

	struct FCase { const TCHAR* Tag; FLKRunState State; };
	TArray<FCase> Cases;

	// a) 迁移前合法、迁移后校验失败（Schema 5 → 9 后 ResolvingNode 落在战斗节点）。
	{
		FLKRunState Bad = Base;
		Bad.SchemaVersion = 5;
		Bad.Phase = ELKRunPhase::ResolvingNode;
		Bad.CurrentNodeId = "Node_R1";
		if (FLKDungeonNode* Current = FindNodeMutable(Bad, "Node_R1")) { Current->bResolved = false; }
		Cases.Add(FCase{ TEXT("PostMigration"), Bad });
	}
	// b) 原始数值非法（负生命）。
	{
		FLKRunState Bad = Base;
		Bad.Heroes[0].Health = -5.f;
		Cases.Add(FCase{ TEXT("NegativeHealth"), Bad });
	}
	// c) BalanceVersion=1 的玩家牌组仍含敌方专属卡。
	{
		FLKRunState Bad = Base;
		Bad.BalanceVersion = LKBalanceRules::CurrentBalanceVersion;
		Bad.Cards[0].CardId = "Unit_Skeleton";
		Cases.Add(FCase{ TEXT("EnemyOnlyDeck"), Bad });
	}
	// d) BalanceVersion=1 的待开战副本仍含敌方专属卡。
	{
		FLKRunState Bad = Base;
		Bad.BalanceVersion = LKBalanceRules::CurrentBalanceVersion;
		FLKRunCardState SkeletonCopy;
		SkeletonCopy.CardId = "Unit_SkeletonArcher";
		Bad.PendingBattle.PlayerCards.Add(SkeletonCopy);
		Cases.Add(FCase{ TEXT("EnemyOnlyPendingBattle"), Bad });
	}
	// e) BalanceVersion=1 的待领升级候选仍指向敌方专属卡。
	{
		FLKRunState Bad = Base;
		Bad.BalanceVersion = LKBalanceRules::CurrentBalanceVersion;
		Bad.Phase = ELKRunPhase::ChoosingReward;
		Bad.PendingRewardBatchId = FGuid::NewGuid();
		Bad.PendingRewardOffers = { MakeUpgradeOffer("Unit_Skeleton", 0, 1) };
		Cases.Add(FCase{ TEXT("EnemyOnlyUpgradeOffer"), Bad });
	}

	for (const FCase& TestCase : Cases)
	{
		const FString Slot = MakeMigrationSlot(TestCase.Tag);
		CleanupMigrationSlot(Slot);
		if (!TestTrue(TEXT("Invalid save written"), WriteRunStateToSlot(Slot, TestCase.State))) { return false; }
		TestFalse(TEXT("Invalid save is refused"), Active->LoadExpeditionFromSlot(Slot, true));
		// 内存远征必须保持调用前状态（尤其是迁移后校验失败不再 Reset）。
		const FLKRunState After = Active->GetRunState();
		TestEqual(TEXT("Run identity survives the refused load"), After.RunId, Before.RunId);
		TestEqual(TEXT("Phase survives the refused load"), After.Phase, Before.Phase);
		TestEqual(TEXT("Current node survives the refused load"), After.CurrentNodeId, Before.CurrentNodeId);
		TestEqual(TEXT("Wallet survives the refused load"), After.WalletGold, Before.WalletGold);
		TestEqual(TEXT("Hero count survives the refused load"), After.Heroes.Num(), Before.Heroes.Num());
		TestEqual(TEXT("Card count survives the refused load"), After.Cards.Num(), Before.Cards.Num());
		TestEqual(TEXT("Battle history count survives the refused load"), After.BattleHistory.Num(), Before.BattleHistory.Num());
		TestEqual(TEXT("Processed attempts survive the refused load"), After.ProcessedAttemptIds.Num(), Before.ProcessedAttemptIds.Num());
		TestTrue(TEXT("Run is still usable after the refused load"), Active->HasRun() && Active->HasRunInProgress());
		TestFalse(TEXT("Refused load does not leak the migration flag"), Active->WasMigratedForBalance());
		TestTrue(TEXT("Refused load does not leak a migration notice"), Active->GetMigrationNotice().IsEmpty());
		CleanupMigrationSlot(Slot);
	}
	return true;
}

// ---------------------------------------------------------------------------
// 5) 已完成遭遇保持旧快照；只有未完成节点及其对应待开战副本按地理深度重算。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceCompletedEncounterTest,
	"LittleKing.BalanceV1.Migration.CompletedEncountersStayHistorical", BalanceMigrationFlags)
bool FLKBalanceCompletedEncounterTest::RunTest(const FString& Parameters)
{
	const FString Slot = MakeMigrationSlot(TEXT("Encounters"));
	CleanupMigrationSlot(Slot);

	ULKRunSubsystem* Seed = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Seed->SetAutoSaveEnabled(false);
	FLKRunState Legacy;
	if (!TestTrue(TEXT("Seed run starts"), MakeLegacyRunState(Seed, 6501, Legacy))) { return false; }

	// 已完成节点（含当前占位）保留旧快照；未完成的 Boss 节点仍是旧稀疏波次，必须被重算。
	FreezeCompletedEncounter(Legacy, "Node_R1", "Enc_Dyn_R1");
	if (FLKEncounterRow* Boss = FindEncounterMutable(Legacy, "Enc_Dyn_Boss"))
	{
		Boss->BalanceVersion = 0;
		Boss->Depth = 0;
		Boss->EnemyHealthScale = 1.f;
		Boss->EnemyDamageScale = 1.f;
		Boss->EnemyHeroHealthScale.Reset();
		Boss->EnemySilverPerSecond = 0.1f;
		Boss->Waves = { { 10.f, FName("Unit_Skeleton"), 1 } };
	}
	// 已完成节点的待开战副本不是恢复点：必须保持原样（标记值不应被刷新）。
	Legacy.PendingBattle.Encounter.EnemySilverPerSecond = 0.25f;
	if (!TestTrue(TEXT("Legacy save written"), WriteRunStateToSlot(Slot, Legacy))) { return false; }

	ULKRunSubsystem* Loaded = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Loaded->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Legacy save loads"), Loaded->LoadExpeditionFromSlot(Slot))) { return false; }
	const FLKRunState Migrated = Loaded->GetRunState();

	const FLKEncounterRow* Completed = FindEncounter(Migrated, "Enc_Dyn_R1");
	TestTrue(TEXT("Completed encounter is not rewritten by the balance migration"),
		Completed && Completed->BalanceVersion == 0 && Completed->EnemyHealthScale == 1.f
		&& Completed->Waves.Num() == 1 && Completed->Waves[0].UnitId == "Unit_SkeletonArcher"
		&& Completed->Waves[0].Count == 1);

	const FLKEncounterRow* Boss = FindEncounter(Migrated, "Enc_Dyn_Boss");
	TestTrue(TEXT("Unfinished encounter gets the dense reinforcement snapshot"),
		Boss && Boss->BalanceVersion == LKBalanceRules::CurrentBalanceVersion
		&& Boss->Waves.Num() > 2
		&& FMath::IsNearlyEqual(Boss->EnemySilverPerSecond, LKBalanceRules::EnemySilverRate(ELKEncounterRank::Boss), 1.e-4f)
		&& Boss->EnemyHeroHealthScale.Num() == Boss->EnemyHeroIds.Num());
	TestTrue(TEXT("Unfinished encounter depth follows its node layer"),
		Boss && Boss->Depth == LKBalanceRules::NodeDepth(0, 4));

	TestTrue(TEXT("Finished battle context is not treated as a resume point"),
		FMath::IsNearlyEqual(Migrated.PendingBattle.Encounter.EnemySilverPerSecond, 0.25f, 1.e-5f));

	CleanupMigrationSlot(Slot);
	return true;
}

// ---------------------------------------------------------------------------
// 6) 临时槽读写幂等：迁移只发生一次、钱包不变、第二次载入不再乘生命。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKBalanceSlotIdempotencyTest,
	"LittleKing.BalanceV1.Migration.TempSlotRoundTripIsIdempotent", BalanceMigrationFlags)
bool FLKBalanceSlotIdempotencyTest::RunTest(const FString& Parameters)
{
	const FString Slot = MakeMigrationSlot(TEXT("Idempotent"));
	CleanupMigrationSlot(Slot);

	ULKRunSubsystem* Seed = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Seed->SetAutoSaveEnabled(false);
	FLKRunState Legacy;
	if (!TestTrue(TEXT("Seed run starts"), MakeLegacyRunState(Seed, 6601, Legacy))) { return false; }
	FLKRunHeroState* Knight = FindHeroMutable(Legacy, "Hero_Knight");
	if (!TestNotNull(TEXT("Knight snapshot exists"), Knight)) { return false; }
	Knight->BaseMaxHealth = 450.f; Knight->MaxHealth = 450.f; Knight->Health = 225.f;
	Legacy.WalletGold = 321;
	Legacy.StartingGold = 21;
	if (!TestTrue(TEXT("Legacy save written"), WriteRunStateToSlot(Slot, Legacy))) { return false; }

	// 第一次载入：自动落盘把迁移结果写回同一临时槽（ConfigureStorage 指向临时槽）。
	ULKRunSubsystem* First = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	First->ConfigureStorage(Slot);
	if (!TestTrue(TEXT("First load migrates the legacy slot"), First->LoadExpeditionFromSlot(Slot))) { return false; }
	const FLKRunState Once = First->GetRunState();
	TestEqual(TEXT("First load writes the balance marker"), Once.BalanceVersion, LKBalanceRules::CurrentBalanceVersion);
	TestTrue(TEXT("First load reports the migration"), First->WasMigratedForBalance());
	const FLKRunHeroState* OnceKnight = FindHero(Once, "Hero_Knight");
	if (!TestNotNull(TEXT("Migrated knight exists"), OnceKnight)) { return false; }
	TestTrue(TEXT("Migrated knight is at half of the new base"),
		IsNearlyScaled(OnceKnight->Health, 3000.f) && IsNearlyScaled(OnceKnight->MaxHealth, 6000.f));

	// 第二次载入：同一槽已是 Balance=1，不得再乘一次、也不再提示迁移。
	ULKRunSubsystem* Second = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Second->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Second load reads the migrated slot"), Second->LoadExpeditionFromSlot(Slot))) { return false; }
	const FLKRunState Twice = Second->GetRunState();
	TestEqual(TEXT("Second load keeps the balance marker"), Twice.BalanceVersion, LKBalanceRules::CurrentBalanceVersion);
	const FLKRunHeroState* TwiceKnight = FindHero(Twice, "Hero_Knight");
	TestTrue(TEXT("Second load does not scale health again"),
		TwiceKnight && TwiceKnight->Health == OnceKnight->Health && TwiceKnight->MaxHealth == OnceKnight->MaxHealth
		&& TwiceKnight->BaseMaxHealth == OnceKnight->BaseMaxHealth);
	TestFalse(TEXT("Second load is not a migration"), Second->WasMigratedForBalance());
	TestTrue(TEXT("Second load has no migration notice"), Second->GetMigrationNotice().IsEmpty());
	TestEqual(TEXT("Wallet stays unchanged across migration"), Twice.WalletGold, 321);
	TestEqual(TEXT("Departure funding stays unchanged across migration"), Twice.StartingGold, 21);

	CleanupMigrationSlot(Slot);
	return true;
}

#endif
