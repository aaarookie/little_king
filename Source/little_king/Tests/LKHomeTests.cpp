#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "LKWorldMapTestHelpers.h"
#include "Tests/AutomationCommon.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

#include "../ALKHomeBuildingActor.h"
#include "../ALKHomeGameMode.h"
#include "../ALKHomePlayerController.h"
#include "../ALKBattleGameMode.h"
#include "../ALKPlayerController.h"
#include "../ALKUnitHero.h"
#include "../LKEncounterContent.h"
#include "../LKHomeContent.h"
#include "../LKHomeTypes.h"
#include "../LKRunTypes.h"
#include "../LKUnitContent.h"
#include "../ULKCardDefinition.h"
#include "../ULKGameData.h"
#include "../ULKHomeHUDWidget.h"
#include "../ULKProfileSaveGame.h"
#include "../ULKProfileSubsystem.h"
#include "../ULKRunSaveGame.h"
#include "../ULKRunSubsystem.h"

namespace
{
	constexpr EAutomationTestFlags HomeFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FString HomeTestSlotBase = TEXT("LittleKing_ProfileHomeTest");

	/** 每个测试都用自己的永久档槽，绝不触碰玩家真实永久档。 */
	struct FHomeTestScope
	{
		FHomeTestScope()
		{
			ULKProfileSubsystem::SetSlotNameOverrideForTest(HomeTestSlotBase);
			ULKProfileSubsystem::SetPersistentProfileEnabledForTest(true);
			ULKRunSubsystem::SetRunSlotNameOverrideForTest(HomeTestSlotBase + TEXT("_Run"));
			CleanupSlots();
		}
		~FHomeTestScope()
		{
			CleanupSlots();
			ULKProfileSubsystem::ResetTestHooks();
			ULKRunSubsystem::SetRunSlotNameOverrideForTest(FString());
		}
		void CleanupSlots() const
		{
			for (const FString& Slot : { ULKProfileSubsystem::GetSlotNameA(), ULKProfileSubsystem::GetSlotNameB(),
				ULKRunSubsystem::GetRunSlotName() })
			{
				if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) { UGameplayStatics::DeleteGameInSlot(Slot, 0); }
			}
		}
	};

	ULKProfileSubsystem* MakeProfile()
	{
		return NewObject<ULKProfileSubsystem>(NewObject<UGameInstance>());
	}

	TArray<FLKEncounterRow> HomeCatalog()
	{
		TArray<FLKEncounterRow> Rows;
		FString Error;
		LKEncounterContent::BuildCatalog(nullptr, Rows, Error);
		return Rows;
	}

	/** 测试用真实 GameData（含 CardLibrary 与遭遇表引用）；加载失败返回 nullptr */
	ULKGameData* LoadAuthoredGameData()
	{
		return LoadObject<ULKGameData>(nullptr, TEXT("/Game/Data/DA_GameData.DA_GameData"));
	}

	const ULKCardDefinition* ResolveCard(const ULKGameData* Data, FName CardId)
	{
		if (!Data) { return nullptr; }
		for (const ULKCardDefinition* Card : Data->CardLibrary) { if (Card && Card->CardId == CardId) { return Card; } }
		return nullptr;
	}

	FLKBattleOutcome WinOutcome(const FLKBattleContext& Context)
	{
		FLKBattleOutcome Outcome;
		Outcome.RunId = Context.RunId;
		Outcome.NodeId = Context.NodeId;
		Outcome.AttemptId = Context.AttemptId;
		Outcome.bFinalized = true;
		Outcome.Stats.Winner = ELKTeam::Player;
		for (const FLKRunHeroState& State : Context.PlayerHeroes)
		{
			FLKHeroBattleOutcome Hero;
			Hero.InstanceId = State.HeroId;
			Hero.RecoveredState = State;
			Hero.RecoveredState.Health = State.MaxHealth;
			Outcome.PlayerHeroes.Add(MoveTemp(Hero));
		}
		return Outcome;
	}

	FLKBattleOutcome LoseOutcome(const FLKBattleContext& Context)
	{
		FLKBattleOutcome Outcome = WinOutcome(Context);
		Outcome.Stats.Winner = ELKTeam::Enemy;
		return Outcome;
	}

	/** 查找面板按钮（不存在时返回默认禁用项，避免测试里解空指针） */
	FLKHomePanelAction FindAction(const FLKHomePanelModel& Model, FName ActionId)
	{
		const FLKHomePanelAction* Found = Model.Actions.FindByPredicate(
			[ActionId](const FLKHomePanelAction& Action) { return Action.ActionId == ActionId; });
		return Found ? *Found : FLKHomePanelAction();
	}
}

// H1：永久档创建、双槽轮换、读回校验与坏档保护。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeProfileTest, "LittleKing.H1.Profile.CreateRotateAndRecover", HomeFlags)
bool FLKHomeProfileTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;

	ULKProfileSubsystem* Profile = MakeProfile();
	if (!TestTrue(TEXT("No profile yet -> creates defaults"), Profile->EnsureProfile())) { return false; }
	TestTrue(TEXT("Default profile is persistent"), Profile->IsProfileUsable());
	TestEqual(TEXT("Starts with zero gold"), Profile->GetGold(), 0);
	TestEqual(TEXT("Statue starts at level one"), Profile->GetBuildingLevel(LKHomeContent::BuildingId(ELKHomeBuilding::Statue)), 1);
	TestEqual(TEXT("Treasury starts at level one"), Profile->GetBuildingLevel(LKHomeContent::BuildingId(ELKHomeBuilding::Treasury)), 1);
	TestEqual(TEXT("Three heroes unlocked"), Profile->GetProfile().UnlockedHeroIds.Num(), 3);
	TestEqual(TEXT("Seven cards unlocked"), Profile->GetProfile().UnlockedCardIds.Num(), 7);
	TestTrue(TEXT("First region unlocked"), Profile->IsRegionUnlocked(LKHomeContent::DefaultRegionId()));
	const FGuid FirstProfileId = Profile->GetProfile().ProfileId;
	const int32 FirstRevision = Profile->GetProfile().Revision;
	TestTrue(TEXT("Default loadout is legal"), Profile->ValidateLoadout(Profile->GetSavedLoadout(), nullptr) == ELKLoadoutResult::Success);

	// 加金币 → 写盘（A/B 轮换）
	if (!TestTrue(TEXT("Gold can be granted"), Profile->AddGold(500))) { return false; }
	TestTrue(TEXT("Slot A exists after first write"), UGameplayStatics::DoesSaveGameExist(ULKProfileSubsystem::GetSlotNameA(), 0));
	Profile->AddGold(100);
	TestTrue(TEXT("Second write rotates to slot B"), UGameplayStatics::DoesSaveGameExist(ULKProfileSubsystem::GetSlotNameB(), 0));

	// 新实例读档：身份与金币一致
	ULKProfileSubsystem* Reloaded = MakeProfile();
	if (!TestTrue(TEXT("Reload finds the highest revision"), Reloaded->EnsureProfile())) { return false; }
	TestEqual(TEXT("Profile identity survives"), Reloaded->GetProfile().ProfileId, FirstProfileId);
	TestTrue(TEXT("Revision advanced"), Reloaded->GetProfile().Revision > FirstRevision);
	TestEqual(TEXT("Gold survives"), Reloaded->GetGold(), 600);

	// 破坏"最新"槽 → 回退到旧修订（保留原文件，不静默新建）
	ULKProfileSaveGame* Corrupt = NewObject<ULKProfileSaveGame>();
	Corrupt->SaveVersion = 1;
	Corrupt->Profile = Reloaded->GetProfile();
	Corrupt->Profile.Revision = 999;
	Corrupt->Profile.ProfileId = FGuid(); // 非法：ProfileId 无效
	UGameplayStatics::SaveGameToSlot(Corrupt, ULKProfileSubsystem::GetSlotNameB(), 0);
	ULKProfileSubsystem* Fallback = MakeProfile();
	if (!TestTrue(TEXT("Corrupt newest slot falls back to the older valid slot"), Fallback->EnsureProfile())) { return false; }
	TestEqual(TEXT("Fallback keeps the same identity"), Fallback->GetProfile().ProfileId, FirstProfileId);
	TestTrue(TEXT("Fallback revision is the older one"), Fallback->GetProfile().Revision < 999);
	TestTrue(TEXT("Corrupt slot file was kept (not deleted)"), UGameplayStatics::DoesSaveGameExist(ULKProfileSubsystem::GetSlotNameB(), 0));

	// 两个槽都坏 → 拒绝创建（保留原文件，永久写禁用）
	UGameplayStatics::SaveGameToSlot(Corrupt, ULKProfileSubsystem::GetSlotNameA(), 0);
	ULKProfileSubsystem* Broken = MakeProfile();
	TestFalse(TEXT("Both slots corrupt -> profile refused"), Broken->EnsureProfile());
	TestFalse(TEXT("Refused profile is not writable"), Broken->IsProfileUsable());
	TestFalse(TEXT("Refused profile does not overwrite files"), Broken->ResetProfile());
	TestTrue(TEXT("Slot A still on disk"), UGameplayStatics::DoesSaveGameExist(ULKProfileSubsystem::GetSlotNameA(), 0));
	return true;
}

// H2：战备校验（数量/唯一/解锁/定义）与跨重启持久化。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeLoadoutTest, "LittleKing.H2.Loadout.ValidationAndPersistence", HomeFlags)
bool FLKHomeLoadoutTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;
	ULKProfileSubsystem* Profile = MakeProfile();
	if (!TestTrue(TEXT("Profile created"), Profile->EnsureProfile())) { return false; }

	const FLKExpeditionLoadout Defaults = LKHomeContent::DefaultLoadout();
	TestTrue(TEXT("Default loadout passes"), Profile->ValidateLoadout(Defaults, nullptr) == ELKLoadoutResult::Success);

	FLKExpeditionLoadout TwoHeroes = Defaults;
	TwoHeroes.HeroIds.Pop();
	TestTrue(TEXT("Two heroes rejected"), Profile->ValidateLoadout(TwoHeroes, nullptr) == ELKLoadoutResult::WrongHeroCount);

	FLKExpeditionLoadout DuplicateHero = Defaults;
	DuplicateHero.HeroIds[2] = DuplicateHero.HeroIds[0];
	TestTrue(TEXT("Duplicate hero rejected"), Profile->ValidateLoadout(DuplicateHero, nullptr) == ELKLoadoutResult::DuplicateHero);

	FLKExpeditionLoadout EnemyHero = Defaults;
	EnemyHero.HeroIds[2] = "Hero_Necromancer";
	TestTrue(TEXT("Enemy hero is not selectable"), Profile->ValidateLoadout(EnemyHero, nullptr) == ELKLoadoutResult::LockedHero);

	FLKExpeditionLoadout FourCards = Defaults;
	FourCards.CardIds.SetNum(4);
	TestTrue(TEXT("Four cards rejected"), Profile->ValidateLoadout(FourCards, nullptr) == ELKLoadoutResult::DeckTooSmall);

	FLKExpeditionLoadout EightCards = Defaults;
	EightCards.CardIds.Add("Unit_Skeleton");
    TestTrue(TEXT("Eight cards fit capacity but locked skeleton still rejected"), Profile->ValidateLoadout(EightCards, nullptr) == ELKLoadoutResult::LockedCard);
    EightCards.CardIds.Add("Unit_SkeletonArcher");
    TestTrue(TEXT("Nine cards exceed troop capacity"), Profile->ValidateLoadout(EightCards, nullptr) == ELKLoadoutResult::DeckTooLarge);

	FLKExpeditionLoadout SkeletonOnly = Defaults;
	SkeletonOnly.CardIds[6] = "Unit_Skeleton";
	TestTrue(TEXT("In-run skeleton card is not a permanent unlock"), Profile->ValidateLoadout(SkeletonOnly, nullptr) == ELKLoadoutResult::LockedCard);

	FLKExpeditionLoadout DuplicateCard = Defaults;
	DuplicateCard.CardIds[6] = DuplicateCard.CardIds[0];
	TestTrue(TEXT("Duplicate card rejected"), Profile->ValidateLoadout(DuplicateCard, nullptr) == ELKLoadoutResult::DuplicateCard);

	// 5 张合法牌组可以保存并跨重启保留
	FLKExpeditionLoadout FiveCards = Defaults;
	FiveCards.CardIds.SetNum(5);
	TestTrue(TEXT("Five-card loadout saves"), Profile->SaveLoadout(FiveCards, nullptr) == ELKLoadoutResult::Success);
	ULKProfileSubsystem* Reloaded = MakeProfile();
	if (!TestTrue(TEXT("Reload after loadout save"), Reloaded->EnsureProfile())) { return false; }
	TestEqual(TEXT("Saved deck size survives"), Reloaded->GetSavedLoadout().CardIds.Num(), 5);
	TestEqual(TEXT("Saved hero count survives"), Reloaded->GetSavedLoadout().HeroIds.Num(), 3);

	// 非法草稿不覆盖已保存配置
	FLKExpeditionLoadout Bad = FiveCards;
	Bad.HeroIds.Reset();
	TestTrue(TEXT("Illegal draft rejected"), Profile->SaveLoadout(Bad, nullptr) == ELKLoadoutResult::WrongHeroCount);
	TestEqual(TEXT("Saved loadout untouched by rejected draft"), Profile->GetSavedLoadout().CardIds.Num(), 5);
	return true;
}

// H4：升级事务（费用/预期等级/满级/不可升级/余额）与数值曲线。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeUpgradeTest, "LittleKing.H4.Upgrade.TransactionAndCurves", HomeFlags)
bool FLKHomeUpgradeTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;
	ULKProfileSubsystem* Profile = MakeProfile();
	if (!TestTrue(TEXT("Profile created"), Profile->EnsureProfile())) { return false; }

	const FName Statue = LKHomeContent::BuildingId(ELKHomeBuilding::Statue);
	const FName Treasury = LKHomeContent::BuildingId(ELKHomeBuilding::Treasury);
	const FName Library = LKHomeContent::BuildingId(ELKHomeBuilding::Library);

	// 曲线与费用（已定/暂定规则）
	TestTrue(TEXT("Statue level 1 recovery is 40%"), FMath::IsNearlyEqual(LKHomeContent::StatueRecoveryPercent(1), 0.4f));
	TestTrue(TEXT("Statue level 4 recovery is 100%"), FMath::IsNearlyEqual(LKHomeContent::StatueRecoveryPercent(4), 1.f));
	TestEqual(TEXT("Statue 1->2 costs 60"), LKHomeContent::UpgradeCost(Statue, 1), 60);
	TestEqual(TEXT("Statue 3->4 costs 200"), LKHomeContent::UpgradeCost(Statue, 3), 200);
	TestEqual(TEXT("Statue max level has no cost"), LKHomeContent::UpgradeCost(Statue, 4), -1);
	TestEqual(TEXT("Treasury 1->2 costs 50"), LKHomeContent::UpgradeCost(Treasury, 1), 50);
	TestTrue(TEXT("Treasury level 5 speed is +40%"), FMath::IsNearlyEqual(LKHomeContent::TreasurySpeedMultiplier(5), 1.4f));
	TestEqual(TEXT("Treasury level 5 cap bonus is +8"), LKHomeContent::TreasuryCapBonus(5), 8);
	TestEqual(TEXT("Full statue upgrade costs 380"), LKHomeContent::TotalUpgradeCost(Statue), 380);
	TestEqual(TEXT("Full treasury upgrade costs 550"), LKHomeContent::TotalUpgradeCost(Treasury), 550);

	// 余额不足 / 不可升级 / 满级 / 预期等级不符
	TestTrue(TEXT("Upgrade without gold fails"), Profile->UpgradeBuilding(Statue, 1, FGuid::NewGuid()) == ELKUpgradeResult::InsufficientGold);
	TestTrue(TEXT("Read-only building cannot upgrade"), Profile->UpgradeBuilding(Library, 1, FGuid::NewGuid()) == ELKUpgradeResult::NotUpgradable);
	TestTrue(TEXT("Unknown building rejected"), Profile->UpgradeBuilding("Home_Nope", 1, FGuid::NewGuid()) == ELKUpgradeResult::UnknownBuilding);
	TestTrue(TEXT("Gold granted"), Profile->AddGold(400));
	TestTrue(TEXT("Stale expected level rejected"), Profile->UpgradeBuilding(Statue, 2, FGuid::NewGuid()) == ELKUpgradeResult::LevelMismatch);
	TestEqual(TEXT("Rejected upgrade does not deduct gold"), Profile->GetGold(), 400);

	// 正常升级：扣费 + 等级 + 写盘
	TestTrue(TEXT("Statue 1->2 succeeds"), Profile->UpgradeBuilding(Statue, 1, FGuid::NewGuid()) == ELKUpgradeResult::Success);
	TestEqual(TEXT("Statue is level two"), Profile->GetBuildingLevel(Statue), 2);
	TestEqual(TEXT("Cost deducted once"), Profile->GetGold(), 340);
	TestTrue(TEXT("Recovery follows the new level"), FMath::IsNearlyEqual(Profile->GetStatueRecoveryPercent(), 0.6f));

	// 连升到满级
	TestTrue(TEXT("Statue 2->3"), Profile->UpgradeBuilding(Statue, 2, FGuid::NewGuid()) == ELKUpgradeResult::Success);
	TestTrue(TEXT("Statue 3->4"), Profile->UpgradeBuilding(Statue, 3, FGuid::NewGuid()) == ELKUpgradeResult::Success);
	TestTrue(TEXT("Statue max level rejected"), Profile->UpgradeBuilding(Statue, 4, FGuid::NewGuid()) == ELKUpgradeResult::MaxLevel);
	TestEqual(TEXT("Statue reached level four"), Profile->GetBuildingLevel(Statue), 4);

	// 金库升级影响快照（产速与上限）
	TestTrue(TEXT("Gold for treasury"), Profile->AddGold(1000));
	TestTrue(TEXT("Treasury 1->2 succeeds"), Profile->UpgradeBuilding(Treasury, 1, FGuid::NewGuid()) == ELKUpgradeResult::Success);
	ULKGameData* Data = LoadAuthoredGameData();
	if (!TestNotNull(TEXT("Authored DA_GameData loads"), Data)) { return false; }
	const FLKMetaBonusSnapshot Snapshot = Profile->BuildBonusSnapshot(Data, LKHomeContent::DefaultRegionId());
	TestTrue(TEXT("Snapshot speed applies +10%"), FMath::IsNearlyEqual(Snapshot.PlayerSilverPerSecond, Data->SilverPerSecond * 1.1f, 0.0001f));
	TestTrue(TEXT("Snapshot cap applies +2"), FMath::IsNearlyEqual(Snapshot.PlayerSilverCap, Data->SilverCap + 2.f));
	TestEqual(TEXT("Snapshot records treasury level"), Snapshot.TreasuryLevel, 2);
	TestTrue(TEXT("Snapshot recovery matches statue"), FMath::IsNearlyEqual(Snapshot.HeroRecoveryPercent, 1.f));

	// 跨重启保留
	ULKProfileSubsystem* Reloaded = MakeProfile();
	if (!TestTrue(TEXT("Reload after upgrades"), Reloaded->EnsureProfile())) { return false; }
	TestEqual(TEXT("Statue level survives restart"), Reloaded->GetBuildingLevel(Statue), 4);
	TestEqual(TEXT("Treasury level survives restart"), Reloaded->GetBuildingLevel(Treasury), 2);
	TestEqual(TEXT("Gold survives restart"), Reloaded->GetGold(), 970);
	return true;
}

// H3：出征快照冻结、战斗数值应用与"新轮才生效"。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeExpeditionTest, "LittleKing.H3.Expedition.SnapshotAndApplication", HomeFlags)
bool FLKHomeExpeditionTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;
	ULKProfileSubsystem* Profile = MakeProfile();
	if (!TestTrue(TEXT("Profile created"), Profile->EnsureProfile())) { return false; }
	TestTrue(TEXT("Gold for upgrades"), Profile->AddGold(500));
	const FName Statue = LKHomeContent::BuildingId(ELKHomeBuilding::Statue);
	const FName Treasury = LKHomeContent::BuildingId(ELKHomeBuilding::Treasury);
	TestTrue(TEXT("Statue to level 2"), Profile->UpgradeBuilding(Statue, 1, FGuid::NewGuid()) == ELKUpgradeResult::Success);
	TestTrue(TEXT("Treasury to level 2"), Profile->UpgradeBuilding(Treasury, 1, FGuid::NewGuid()) == ELKUpgradeResult::Success);

	ULKGameData* Data = LoadAuthoredGameData();
	if (!TestNotNull(TEXT("Authored DA_GameData loads"), Data)) { return false; }
	FLKExpeditionStartRequest Request;
	FString Error;
	if (!TestTrue(TEXT("Start request builds from the profile"),
		LKHomeContent::BuildExpeditionStartRequest(*Profile, Data, LKHomeContent::DefaultRegionId(),
			[](FName UnitId) { return LKUnitContent::Find(UnitId); },
			[Data](FName CardId) -> const ULKCardDefinition*
			{
				for (const ULKCardDefinition* Card : Data->CardLibrary) { if (Card && Card->CardId == CardId) { return Card; } }
				return nullptr;
			},
			Request, Error)))
	{ AddError(Error); return false; }
	TestEqual(TEXT("Snapshot carries three heroes"), Request.Heroes.Num(), 3);
	TestEqual(TEXT("Snapshot carries the saved deck"), Request.Cards.Num(), 7);
	TestTrue(TEXT("Snapshot recovery is 60%"), FMath::IsNearlyEqual(Request.BonusSnapshot.HeroRecoveryPercent, 0.6f));
	TestTrue(TEXT("Snapshot silver speed is +10%"), FMath::IsNearlyEqual(Request.BonusSnapshot.PlayerSilverPerSecond, Data->SilverPerSecond * 1.1f, 0.0001f));
	TestTrue(TEXT("Snapshot region set"), Request.RegionId == LKHomeContent::DefaultRegionId());
	TestFalse(TEXT("Snapshot encounters validated"), Request.Encounters.IsEmpty());
	for (const FLKRunHeroState& Hero : Request.Heroes)
	{
		TestTrue(TEXT("Hero starts at full health"), FMath::IsNearlyEqual(Hero.Health, Hero.MaxHealth));
	}

	// 创建远征：所有家园字段在第一次存档前就位
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Run->SetAutoSaveEnabled(false);
    Run->SetProfileSubsystemOverrideForTest(Profile);
	if (!TestTrue(TEXT("Run starts from the home request"), Run->StartNewRun(Request))) { return false; }
	const FLKRunState State = Run->GetRunState();
	TestEqual(TEXT("Run remembers the profile"), State.ProfileId, Profile->GetProfile().ProfileId);
	TestEqual(TEXT("Run remembers the region"), State.RegionId, FName(TEXT("World_Region0")));
	TestTrue(TEXT("Run is eligible for home gold"), State.bHomeRewardEligible);
	TestTrue(TEXT("Run froze the recovery snapshot"), FMath::IsNearlyEqual(State.BonusSnapshot.HeroRecoveryPercent, 0.6f));
	TestTrue(TEXT("Active run blocks a second expedition"), !Run->StartNewRun(Request));

	FLKBattleContext Context;
	TestTrue(TEXT("Explicit route choice reaches a battle"), LKWorldMapTest::SelectBattle(Run));
    if (!TestTrue(TEXT("Battle context available"), Run->BeginCurrentBattle(Context))) { return false; }
	TestTrue(TEXT("Context carries the snapshot"), FMath::IsNearlyEqual(Context.BonusSnapshot.HeroRecoveryPercent, 0.6f));
	TestTrue(TEXT("Context carries the region"), Context.RegionId == FName(TEXT("World_Region0")));

	// 家园再升级：进行中的远征快照不变
	TestTrue(TEXT("Statue to level 3 while the run is active"), Profile->UpgradeBuilding(Statue, 2, FGuid::NewGuid()) == ELKUpgradeResult::Success);
	TestTrue(TEXT("Active run snapshot is unchanged"), FMath::IsNearlyEqual(Run->GetBonusSnapshot().HeroRecoveryPercent, 0.6f));
	return true;
}

// H4：金币结算数学与幂等交接。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeSettlementTest, "LittleKing.H4.Settlement.MathAndHandoff", HomeFlags)
bool FLKHomeSettlementTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;
	ULKProfileSubsystem* Profile = MakeProfile();
	if (!TestTrue(TEXT("Profile created"), Profile->EnsureProfile())) { return false; }

	auto RunToTerminal = [this](ULKRunSubsystem* Run, bool bWinAll, bool bAbandon) -> FLKSettlementReceipt
	{
		for (int32 Guard = 0; Guard < 100 && !Run->IsTerminal(); ++Guard)
		{
			const ELKRunPhase Phase = Run->GetRunPhase();
			if (Phase == ELKRunPhase::EnteringBattle || Phase == ELKRunPhase::InBattle)
			{
				FLKBattleContext Context;
				if (!Run->BeginCurrentBattle(Context)) { break; }
				const FLKDungeonNode Current = Run->GetNode(Run->GetRunState().CurrentNodeId);
				const bool bLastBattle = Current.NextNodeIds.IsEmpty();
				Run->SubmitBattleOutcome((bLastBattle && !bWinAll) ? LoseOutcome(Context) : WinOutcome(Context));
				continue;
			}
			if (Run->HasServiceNode()) { Run->ResolveServiceNode(Run->GetNode(Run->GetRunState().CurrentNodeId).Type == ELKDungeonNodeType::Rest ? TEXT("RestHeal") : TEXT("LeaveMarket")); continue; }
            if (Run->CanSelectNextNode())
			{
				const TArray<FName> Next = Run->GetNextNodeIds();
				if (Next.IsEmpty()) { break; }
				// 只挑战斗节点推进（休息节点会原地停留并继续显示下一排）。
				FName Pick = Next[0];
				for (FName Id : Next)
				{
					if (Run->GetNode(Id).Type != ELKDungeonNodeType::Rest) { Pick = Id; break; }
				}
				if (Run->SelectNode(Pick) == ELKNodeSelectionResult::Rejected) { break; }
				continue;
			}
			break;
		}
		if (bAbandon) { Run->AbandonCurrentRun(); }
		return Run->GetPendingSettlement();
	};

	// 经济数值（与路线无关的纯公式校验）
	const FLKHomeRewardRules Rules = LKHomeContent::DefaultRewardRules();
	TestEqual(TEXT("Normal win pays 10 gold"), LKHomeContent::GoldForRewardTier(Rules, 1), 10);
	TestEqual(TEXT("Elite win pays 20 gold"), LKHomeContent::GoldForRewardTier(Rules, 2), 20);
	TestEqual(TEXT("Boss win pays 40 gold"), LKHomeContent::GoldForRewardTier(Rules, 3), 40);
	TestEqual(TEXT("Full battle route totals 100"), 10 + 10 + 20 + 40 + Rules.RunCompletedBonus, 100);
	TestEqual(TEXT("Two-rest route totals 70"), 10 + 40 + Rules.RunCompletedBonus, 70);

	// a) 全胜通关：本房金币 + 通关奖励
	{
		FLKExpeditionStartRequest Request;
		FString Error;
		ULKGameData* Data = LoadAuthoredGameData();
	if (!TestNotNull(TEXT("Authored DA_GameData loads"), Data)) { return false; }
		if (!TestTrue(TEXT("Request for full clear"),
			LKHomeContent::BuildExpeditionStartRequest(*Profile, Data, LKHomeContent::DefaultRegionId(),
				[](FName UnitId) { return LKUnitContent::Find(UnitId); },
				[Data](FName CardId) -> const ULKCardDefinition*
				{
					for (const ULKCardDefinition* Card : Data->CardLibrary) { if (Card && Card->CardId == CardId) { return Card; } }
					return nullptr;
				}, Request, Error))) { return false; }
		ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
		Run->SetAutoSaveEnabled(false);
    Run->SetProfileSubsystemOverrideForTest(Profile);
		Run->SetProfileSubsystemOverrideForTest(Profile);
		TestTrue(TEXT("Request carries a valid profile id"), Request.ProfileId.IsValid());
		if (!TestTrue(TEXT("Full-clear run starts"), Run->StartNewRun(Request))) { return false; }
		TestTrue(TEXT("Full-clear run is eligible for home gold"), Run->IsHomeRewardEligible());
		const FLKSettlementReceipt Receipt = RunToTerminal(Run, true, false);
		// 路线是随机的（可能走休息节点，实际 2~4 场），因此按本轮实际暂存金币校验公式。
		TestEqual(TEXT("Completed run adds the completion bonus"),
			Receipt.GoldAmount, Run->GetPendingGold() + LKHomeContent::DefaultRewardRules().RunCompletedBonus);
		TestTrue(TEXT("Terminal phase is Completed"), Receipt.TerminalPhase == ELKRunPhase::Completed);
		TestTrue(TEXT("Receipt carries identity"), Receipt.SettlementId.IsValid() && Receipt.RunId == Run->GetRunState().RunId);
		const int32 ExpectedGold = Receipt.GoldAmount;

		// 终态自动交接（真实路径）：Run 让 Profile 入账并标记回执。
		TestTrue(TEXT("Run marks the handoff at terminal"), Run->GetPendingSettlement().bProfileApplied);
		TestEqual(TEXT("Gold credited once at settlement"), Profile->GetGold(), ExpectedGold);
		// 重复入账幂等
		TestTrue(TEXT("Second apply is a no-op"), Profile->ApplySettlement(Receipt) == ELKSettlementResult::AlreadyApplied);
		TestEqual(TEXT("Gold unchanged by repeat"), Profile->GetGold(), ExpectedGold);
		TestTrue(TEXT("Repeated handoff is idempotent"), Run->ApplyPendingSettlementToProfile());
	}

	// b) 最后一战失败：保留已获胜金币的 50%（向下取整）
	{
		FLKExpeditionStartRequest Request;
		FString Error;
		ULKGameData* Data = LoadAuthoredGameData();
	if (!TestNotNull(TEXT("Authored DA_GameData loads"), Data)) { return false; }
		if (!TestTrue(TEXT("Request for failure"),
			LKHomeContent::BuildExpeditionStartRequest(*Profile, Data, LKHomeContent::DefaultRegionId(),
				[](FName UnitId) { return LKUnitContent::Find(UnitId); },
				[Data](FName CardId) -> const ULKCardDefinition*
				{
					for (const ULKCardDefinition* Card : Data->CardLibrary) { if (Card && Card->CardId == CardId) { return Card; } }
					return nullptr;
				}, Request, Error))) { return false; }
		ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
		Run->SetAutoSaveEnabled(false);
    Run->SetProfileSubsystemOverrideForTest(Profile);
		if (!TestTrue(TEXT("Failure run starts"), Run->StartNewRun(Request))) { return false; }
		const FLKSettlementReceipt Receipt = RunToTerminal(Run, false, false);
		TestTrue(TEXT("Failed run keeps all earned gold"), Receipt.GoldAmount == Run->GetPendingGold());
		TestTrue(TEXT("Failure never pays the completion bonus"), Receipt.GoldAmount == Run->GetPendingGold());
	}

	// c) 明确放弃：0 金币
	{
		FLKExpeditionStartRequest Request;
		FString Error;
		ULKGameData* Data = LoadAuthoredGameData();
	if (!TestNotNull(TEXT("Authored DA_GameData loads"), Data)) { return false; }
		if (!TestTrue(TEXT("Request for abandon"),
			LKHomeContent::BuildExpeditionStartRequest(*Profile, Data, LKHomeContent::DefaultRegionId(),
				[](FName UnitId) { return LKUnitContent::Find(UnitId); },
				[Data](FName CardId) -> const ULKCardDefinition*
				{
					for (const ULKCardDefinition* Card : Data->CardLibrary) { if (Card && Card->CardId == CardId) { return Card; } }
					return nullptr;
				}, Request, Error))) { return false; }
		ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
		Run->SetAutoSaveEnabled(false);
    Run->SetProfileSubsystemOverrideForTest(Profile);
		if (!TestTrue(TEXT("Abandon run starts"), Run->StartNewRun(Request))) { return false; }
		FLKBattleContext Context;
		TestTrue(TEXT("Select first battle before abandoning"), LKWorldMapTest::SelectBattle(Run));
        TestTrue(TEXT("Abandon run enters room one"), Run->BeginCurrentBattle(Context));
		TestTrue(TEXT("Room one win"), Run->SubmitBattleOutcome(WinOutcome(Context)));
		TestTrue(TEXT("Abandon succeeds"), Run->AbandonCurrentRun());
		TestEqual(TEXT("Abandoned run returns remaining gold"), Run->GetPendingSettlement().GoldAmount, Run->GetWalletGold());
		TestTrue(TEXT("Abandoned run is terminal"), Run->IsTerminal());
	}

	// d) 身份不匹配拒绝入账
	{
		const int32 GoldBefore = Profile->GetGold();
		FLKSettlementReceipt Foreign;
		Foreign.SettlementId = FGuid::NewGuid();
		Foreign.ProfileId = FGuid::NewGuid();
		Foreign.GoldAmount = 50;
		TestTrue(TEXT("Foreign profile settlement refused"), Profile->ApplySettlement(Foreign) == ELKSettlementResult::IdentityMismatch);
		TestEqual(TEXT("Gold unchanged after refusal"), Profile->GetGold(), GoldBefore);
	}
	return true;
}

// H1/H3：家园世界（GameMode + 七座建筑 + 面板模型 + 升级刷新）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeWorldTest, "LittleKing.H3.World.BuildingsAndPanels", HomeFlags)
bool FLKHomeWorldTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;

	FTestWorldWrapper World;
	if (!World.CreateTestWorld(EWorldType::Game)) { World.ForwardErrorMessages(this); return false; }
	World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = ALKHomeGameMode::StaticClass();
	if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
	ALKHomeGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKHomeGameMode>();
	World.GetTestWorld()->SpawnActor<ALKHomePlayerController>();
	if (!TestNotNull(TEXT("Home GameMode starts"), GM)) { return false; }
	GM->Tick(0.f);

	TestEqual(TEXT("Seven placeholder buildings exist"), GM->GetBuildingActors().Num(), 7);
	for (const FLKBuildingDefinition& Definition : LKHomeContent::Buildings())
	{
		ALKHomeBuildingActor* Actor = GM->FindBuildingActor(Definition.BuildingId);
		if (!TestNotNull(*FString::Printf(TEXT("Building %s exists"), *Definition.BuildingId.ToString()), Actor)) { continue; }
		TestTrue(TEXT("Building carries its display name"), !Actor->GetBuildingName().IsEmpty());
	}
	// 布局：战备处紧邻大门（世界单位距离 < 900）
	const FVector Gate = ALKHomeGameMode::GetBuildingLayoutLocation(ELKHomeBuilding::Gate);
	const FVector WarRoom = ALKHomeGameMode::GetBuildingLayoutLocation(ELKHomeBuilding::WarRoom);
	TestTrue(TEXT("War room sits next to the gate"), FVector::Dist2D(Gate, WarRoom) < 900.f);

	// 面板模型：神像升级页有等级/恢复/费用/余额
	ULKHomeHUDWidget* HUD = GM->GetHomeHUD();
	TestNotNull(TEXT("Native home HUD is created"), HUD);
	const FLKHomePanelModel StatuePanel = GM->BuildPanelModel(ELKHomePanel::Statue, FLKExpeditionLoadout(), NAME_None, 0);
	TestTrue(TEXT("Statue panel shows the current level"), StatuePanel.Rows.ContainsByPredicate(
		[](const FLKHomePanelRow& Row) { return Row.Label.ToString().Contains(TEXT("等级")); }));
	TestTrue(TEXT("Statue panel shows the recovery value"), StatuePanel.Rows.ContainsByPredicate(
		[](const FLKHomePanelRow& Row) { return Row.Value.ToString().Contains(TEXT("40%")); }));
	TestFalse(TEXT("Statue upgrade button disabled without gold"), FindAction(StatuePanel, TEXT("Upgrade")).bEnabled);

	// 给钱后升级：模型与建筑标签同步
	ULKProfileSubsystem* Profile = GM->GetProfileSubsystem();
	if (!TestNotNull(TEXT("Profile subsystem available"), Profile) || !Profile->EnsureProfile()) { return false; }
	TestTrue(TEXT("Debug gold granted"), Profile->AddGold(200));
	const ELKUpgradeResult Upgrade = GM->RequestUpgrade(LKHomeContent::BuildingId(ELKHomeBuilding::Statue), 1);
	TestTrue(TEXT("Upgrade through the GameMode succeeds"), Upgrade == ELKUpgradeResult::Success);
	TestEqual(TEXT("Building actor shows the new level"),
		GM->FindBuildingActor(LKHomeContent::BuildingId(ELKHomeBuilding::Statue))->GetDisplayedLevel(), 2);
	const FLKHomePanelModel Upgraded = GM->BuildPanelModel(ELKHomePanel::Statue, FLKExpeditionLoadout(), NAME_None, 0);
	TestTrue(TEXT("Panel shows the upgraded recovery"), Upgraded.Rows.ContainsByPredicate(
		[](const FLKHomePanelRow& Row) { return Row.Value.ToString().Contains(TEXT("60%")); }));

	// 图书馆/英雄之家/军营：只读且只列永久解锁内容
	const FLKHomePanelModel Library = GM->BuildPanelModel(ELKHomePanel::Library, FLKExpeditionLoadout(), NAME_None, 0);
	TestEqual(TEXT("Library lists exactly the two unlocked spells"), Library.Rows.Num(), 2);
	TestTrue(TEXT("Library has no purchase action"), !Library.Actions.ContainsByPredicate(
		[](const FLKHomePanelAction& Action) { return Action.ActionId != TEXT("Close"); }));
	const FLKHomePanelModel Heroes = GM->BuildPanelModel(ELKHomePanel::HeroHouse, FLKExpeditionLoadout(), NAME_None, 0);
	TestEqual(TEXT("Hero house lists the three player heroes"), Heroes.Rows.Num(), 3);
	TestTrue(TEXT("Enemy heroes are absent"), !Heroes.Rows.ContainsByPredicate(
		[](const FLKHomePanelRow& Row) { return Row.RowId == "Hero_Necromancer" || Row.RowId == "Boss_SkeletonKing"; }));
	const FLKHomePanelModel Soldiers = GM->BuildPanelModel(ELKHomePanel::Barracks, FLKExpeditionLoadout(), NAME_None, 0);
	const FLKHomePanelModel Buildings = GM->BuildPanelModel(ELKHomePanel::Barracks, FLKExpeditionLoadout(), NAME_None, 1);
	TestEqual(TEXT("Barracks soldiers tab has three mercenaries"), Soldiers.Rows.Num(), 3);
	TestEqual(TEXT("Barracks buildings tab has two combat buildings"), Buildings.Rows.Num(), 2);
	TestTrue(TEXT("Barracks has no training action"), !Soldiers.Actions.ContainsByPredicate(
		[](const FLKHomePanelAction& Action) { return Action.ActionId != TEXT("Close") && Action.ActionId != TEXT("Tab0") && Action.ActionId != TEXT("Tab1"); }));

	// 大门：一个真实区域 + 出征按钮
	const FLKHomePanelModel GatePanel = GM->BuildPanelModel(ELKHomePanel::Gate, FLKExpeditionLoadout(), NAME_None, 0);
	TestEqual(TEXT("Gate lists wallet, three heroes and seven cards"), GatePanel.Rows.Num(), 11);
	TestFalse(TEXT("Gate has no selectable region"), GatePanel.Rows.ContainsByPredicate([](const FLKHomePanelRow& Row) { return Row.RowId == LKHomeContent::DefaultRegionId(); }));
	TestTrue(TEXT("Gate offers the expedition action"), GatePanel.Actions.ContainsByPredicate(
		[](const FLKHomePanelAction& Action) { return Action.ActionId == TEXT("Start") || Action.ActionId == TEXT("Continue"); }));

	// 战备页：草稿状态与非法草稿提示
	FLKExpeditionLoadout Draft = LKHomeContent::DefaultLoadout();
	const FLKHomePanelModel WarRoomPanel = GM->BuildPanelModel(ELKHomePanel::WarRoom, Draft, NAME_None, 0);
	TestTrue(TEXT("War room shows three hero slots"), WarRoomPanel.Rows.FilterByPredicate(
		[](const FLKHomePanelRow& Row) { return Row.Label.ToString().Contains(TEXT("英雄槽")); }).Num() == 3);
	TestTrue(TEXT("War room lists the seven unlocked cards"), WarRoomPanel.Rows.FilterByPredicate(
		[](const FLKHomePanelRow& Row) { return Row.bToggleable; }).Num() == 7);
	Draft.CardIds.SetNum(4);
	const FLKHomePanelModel BadDraft = GM->BuildPanelModel(ELKHomePanel::WarRoom, Draft, NAME_None, 0);
	TestTrue(TEXT("Illegal draft disables save"), !FindAction(BadDraft, TEXT("SaveLoadout")).bEnabled);
	TestTrue(TEXT("Legal draft enables save"), FindAction(WarRoomPanel, TEXT("SaveLoadout")).bEnabled);
	return true;
}

// H5：v0.6 旧档迁移到 Schema 5。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeMigrationTest, "LittleKing.H5.Migration.LegacyRunToSchema5", HomeFlags)
bool FLKHomeMigrationTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;
	ULKProfileSubsystem* Profile = MakeProfile();
	if (!TestTrue(TEXT("Profile created"), Profile->EnsureProfile())) { return false; }
	const FGuid ProfileId = Profile->GetProfile().ProfileId;

	// 造一份 v0.6 结构（Schema 4、没有家园字段）的远征档
	TArray<FLKRunHeroState> Heroes;
	for (const TPair<FName, float>& Pair : { TPair<FName, float>("Hero_Knight", 450.f),
		TPair<FName, float>("Hero_Mage", 300.f), TPair<FName, float>("Hero_Ranger", 350.f) })
	{
		FLKRunHeroState Hero;
		Hero.HeroId = Pair.Key;
		Hero.Health = Hero.MaxHealth = Hero.BaseMaxHealth = Pair.Value;
		Heroes.Add(Hero);
	}
	TArray<FLKRunCardState> Cards;
	for (FName Id : LKHomeContent::DefaultUnlockedCards())
	{
		FLKRunCardState Card;
		Card.CardId = Id;
		Cards.Add(Card);
	}
	const FString Slot = TEXT("LittleKing_ProfileHomeTest_LegacyRun");
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) { UGameplayStatics::DeleteGameInSlot(Slot, 0); }

	ULKRunSubsystem* Legacy = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Legacy->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Legacy run starts"), Legacy->StartNewRun(Heroes, Cards, 4242, HomeCatalog()))) { return false; }
	FLKRunState LegacyState = Legacy->GetRunState();
	// 手动退化成 v0.6 结构：Schema 4 + 清空家园字段
	LegacyState.SchemaVersion = 4;
	LegacyState.ProfileId = FGuid();
	LegacyState.RegionId = NAME_None;
	LegacyState.InitialLoadout = FLKExpeditionLoadout();
	LegacyState.BonusSnapshot = FLKMetaBonusSnapshot();
	LegacyState.RewardRules = FLKHomeRewardRules();
	LegacyState.PendingGold = 0;
	LegacyState.PendingSettlement = FLKSettlementReceipt();
	LegacyState.bHomeRewardEligible = false;
	ULKRunSaveGame* Save = NewObject<ULKRunSaveGame>();
	Save->SaveVersion = 1;
	Save->RunState = LegacyState;
	TestTrue(TEXT("Legacy save written"), UGameplayStatics::SaveGameToSlot(Save, Slot, 0));

	// 用测试钩子指定迁移时关联的 ProfileId（测试实例没有注册的 Profile 子系统）。
	ULKRunSubsystem* Restored = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("Run subsystem created"), Restored)) { return false; }
	Restored->SetProfileIdOverrideForTest(ProfileId);
	Restored->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Legacy save loads"), Restored->LoadExpeditionFromSlot(Slot))) { return false; }
	const FLKRunState Migrated = Restored->GetRunState();
	TestEqual(TEXT("Schema upgraded to current"), Migrated.SchemaVersion, ULKRunSubsystem::CurrentSchemaVersion);
	TestTrue(TEXT("Region defaults to the first region"), Migrated.RegionId == LKHomeContent::DefaultRegionId());
	TestTrue(TEXT("Profile linked"), Migrated.ProfileId == ProfileId);
	TestEqual(TEXT("Initial loadout filled from the party"), Migrated.InitialLoadout.HeroIds.Num(), 3);
	TestTrue(TEXT("Legacy baseline silver frozen at 1/3"), FMath::IsNearlyEqual(Migrated.BonusSnapshot.PlayerSilverPerSecond, 1.f / 3.f, 0.0001f));
	TestTrue(TEXT("Legacy baseline cap frozen at 5"), FMath::IsNearlyEqual(Migrated.BonusSnapshot.PlayerSilverCap, 5.f));
	TestTrue(TEXT("Legacy recovery defaults to 40%"), FMath::IsNearlyEqual(Migrated.BonusSnapshot.HeroRecoveryPercent, 0.4f));
	TestTrue(TEXT("Migrated run does not earn home gold"), Migrated.bHomeRewardEligible == false);
	TestFalse(TEXT("Legacy run has no settlement yet"), Migrated.PendingSettlement.SettlementId.IsValid());
	FString MigratedError;
	TestTrue(TEXT("Migrated state validates"), ULKRunSubsystem::ValidateStoredRun(Migrated, MigratedError));

	// 未来版本仍拒绝且保留原文件
	ULKRunSaveGame* Future = NewObject<ULKRunSaveGame>();
	Future->SaveVersion = 1;
	Future->RunState = Migrated;
	Future->RunState.SchemaVersion = 99;
	UGameplayStatics::SaveGameToSlot(Future, Slot, 0);
	ULKRunSubsystem* Guard = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Guard->SetAutoSaveEnabled(false);
	TestFalse(TEXT("Future schema refused"), Guard->LoadExpeditionFromSlot(Slot));
	TestTrue(TEXT("Refused file kept on disk"), UGameplayStatics::DoesSaveGameExist(Slot, 0));
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	return true;
}

// H3 回归：安全点"暂回家园"后继续远征，战斗必须能正常开始（用户报告：双方面对面却不攻击）。
// 复现路径与真实流程一致：打一房 → 跳过奖励停在节点选择（安全点）→ 保存 → 新 GameInstance 先载入存档
// （家园 GameMode 的做法）→ 再开战斗地图 → 部署三英雄 → 必须能进入 Battle 阶段。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKHomeResumeBattleTest, "LittleKing.H3.Resume.SafePointLeaveAndContinue", HomeFlags)
bool FLKHomeResumeBattleTest::RunTest(const FString& Parameters)
{
	FHomeTestScope Scope;
	TSubclassOf<ALKBattleGameMode> AuthoredMode = LoadClass<ALKBattleGameMode>(
		nullptr, TEXT("/Game/blueprint/BP_ALKBattleGameMode.BP_ALKBattleGameMode_C"));
	if (!TestTrue(TEXT("Authored battle GameMode loads"), bool(AuthoredMode))) { return false; }

	const FString Slot = ULKRunSubsystem::GetRunSlotName();
	const TArray<FName> Roster = { FName("Hero_Knight"), FName("Hero_Mage"), FName("Hero_Ranger") };
	const TArray<FVector> Camps = { FVector(-700.f, -1300.f, 0.f), FVector(0.f, -1300.f, 0.f), FVector(700.f, -1300.f, 0.f) };

	// 1) 第一房正常打完，停在安全点（节点选择）并保存 = 点击"暂回家园"
	{
		FTestWorldWrapper World;
		if (!World.CreateTestWorld(EWorldType::Game)) { World.ForwardErrorMessages(this); return false; }
		World.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AuthoredMode;
        if (!TestTrue(TEXT("Explicit first route choice"), LKWorldMapTest::StartSelectedBattle(World.GetTestWorld()->GetGameInstance()))) { return false; }
		if (!World.BeginPlayInTestWorld()) { World.ForwardErrorMessages(this); return false; }
		ALKBattleGameMode* GM = World.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
		World.GetTestWorld()->SpawnActor<ALKPlayerController>();
		if (!TestNotNull(TEXT("Room one GameMode"), GM)) { return false; }
		GM->Tick(0.f);
		for (int32 Index = 0; Index < Roster.Num(); ++Index)
		{
			TestEqual(TEXT("Room one hero deploys"), GM->DeployHero(ELKTeam::Player, Roster[Index], Camps[Index]), ELKPlayResult::Success);
		}
		TestTrue(TEXT("Room one can start"), GM->CanStartBattle());
		GM->ForceStartBattle();
		TestEqual(TEXT("Room one reaches Battle"), GM->GetPhase(), ELKGamePhase::Battle);
		GM->ForceEndMatch(ELKTeam::Player);

		ULKRunSubsystem* Run = World.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
		if (!TestNotNull(TEXT("Room one run subsystem"), Run)) { return false; }
		if (Run->HasPendingRewardChoice()) { TestTrue(TEXT("Reward skipped"), Run->SkipReward()); }
		TestTrue(TEXT("Safe point exposes the next row"), Run->CanSelectNextNode());
		TestTrue(TEXT("Safe point saved (暂回家园)"), Run->SaveExpeditionToSlot(Slot));
	}

	// 2) 回到家园后继续：新 GameInstance 由家园侧载入存档，再打开战斗地图
	FTestWorldWrapper World2;
	if (!World2.CreateTestWorld(EWorldType::Game)) { World2.ForwardErrorMessages(this); return false; }
	ULKRunSubsystem* HomeRun = World2.GetTestWorld()->GetGameInstance()->GetSubsystem<ULKRunSubsystem>();
	if (!TestNotNull(TEXT("Home-side run subsystem"), HomeRun)) { return false; }
	if (!TestTrue(TEXT("Home loads the saved run"), HomeRun->LoadExpeditionFromSlot(Slot))) { return false; }
	TestTrue(TEXT("Loaded run is still in progress"), HomeRun->HasRunInProgress());

	if (!TestTrue(TEXT("Choose next battle before travelling"), LKWorldMapTest::SelectBattle(HomeRun))) { return false; }
    World2.GetTestWorld()->GetWorldSettings()->DefaultGameMode = AuthoredMode;
	if (!World2.BeginPlayInTestWorld()) { World2.ForwardErrorMessages(this); return false; }
	ALKBattleGameMode* GM2 = World2.GetTestWorld()->GetAuthGameMode<ALKBattleGameMode>();
	World2.GetTestWorld()->SpawnActor<ALKPlayerController>();
	if (!TestNotNull(TEXT("Resumed battle GameMode"), GM2)) { return false; }
	GM2->Tick(0.f);

	TestTrue(TEXT("Resumed world is an expedition battle"), GM2->IsExpeditionBattle());
	TestEqual(TEXT("Resumed room index is two"), GM2->GetExpeditionRoomIndex(), 2);
	TestEqual(TEXT("Resumed roster keeps three heroes"), GM2->GetRequiredHeroCount(), 3);
	TestTrue(TEXT("Enemy roster deployed"), GM2->GetEnemyHeroIds().Num() > 0);

	// 3) 部署三英雄后必须能开始（修复前这里会静默失败，单位保持冻结、面对面不攻击）
	for (int32 Index = 0; Index < Roster.Num(); ++Index)
	{
		TestEqual(TEXT("Resumed hero deploys"), GM2->DeployHero(ELKTeam::Player, Roster[Index], Camps[Index]), ELKPlayResult::Success);
	}
	TestEqual(TEXT("All three heroes deployed"), GM2->GetDeployedPlayerHeroCount(), GM2->GetRequiredHeroCount());
	TestTrue(TEXT("Decks stay valid after resume"), GM2->HasValidDecks());
	TestTrue(TEXT("Can start after resume"), GM2->CanStartBattle());
	GM2->ForceStartBattle();
	TestEqual(TEXT("Resumed battle reaches Battle phase"), GM2->GetPhase(), ELKGamePhase::Battle);

	// 4) 战斗真的打起来：把骑士放到敌人身边，双方必须互相造成伤害
	//    （不等长途行军，避免测试依赖耗时；面对面能打才说明单位没有被冻结）
	ALKUnitHero* Knight = nullptr;
	ALKUnitHero* EnemyHero = nullptr;
	for (TActorIterator<ALKUnitHero> It(World2.GetTestWorld()); It; ++It)
	{
		if (It->GetTeam() == ELKTeam::Player && It->GetUnitId() == "Hero_Knight") { Knight = *It; }
		if (It->GetTeam() == ELKTeam::Enemy && !EnemyHero) { EnemyHero = *It; }
	}
	if (!TestNotNull(TEXT("Player knight exists on the resumed field"), Knight)) { return false; }
	if (!TestNotNull(TEXT("Enemy hero exists on the resumed field"), EnemyHero)) { return false; }

	Knight->SetActorLocation(EnemyHero->GetActorLocation() + FVector(0.f, -110.f, 0.f));
	const float KnightHealth = Knight->GetHealth();
	const float EnemyHealth = EnemyHero->GetHealth();
	float KnightHealthNow = KnightHealth;
	float EnemyHealthNow = EnemyHealth;
	for (int32 Frame = 0; Frame < 600 && (KnightHealthNow >= KnightHealth && EnemyHealthNow >= EnemyHealth); ++Frame)
	{
		World2.TickTestWorld(1.f / 30.f);
		KnightHealthNow = Knight->GetHealth();
		EnemyHealthNow = EnemyHero->GetHealth();
	}
	TestTrue(TEXT("Adjacent units trade damage after resume"),
		KnightHealthNow < KnightHealth - 1.f || EnemyHealthNow < EnemyHealth - 1.f);
	return true;
}

#endif
