#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"

#include "../LKEncounterContent.h"
#include "../LKRunTypes.h"
#include "../ULKRunSubsystem.h"

struct FLKD3TestParty
{
	TArray<FLKRunHeroState> Heroes;
	TArray<FLKRunCardState> Cards;

	FLKD3TestParty()
	{
		for (const TPair<FName, float>& Pair : {
			TPair<FName, float>("Hero_Knight", 450.f), TPair<FName, float>("Hero_Mage", 300.f),
			TPair<FName, float>("Hero_Ranger", 350.f) })
		{
			FLKRunHeroState Hero;
			Hero.HeroId = Pair.Key;
			Hero.Health = Hero.MaxHealth = Hero.BaseMaxHealth = Pair.Value;
			Heroes.Add(MoveTemp(Hero));
		}
		for (FName Id : { FName("Unit_Swordsman"), FName("Unit_Archer"), FName("Unit_Shieldbearer"),
			FName("Spell_Fireball"), FName("Spell_HealWave"), FName("Building_ArrowTower"), FName("Building_Barracks") })
		{
			FLKRunCardState Card;
			Card.CardId = Id;
			Cards.Add(MoveTemp(Card));
		}
	}
};

namespace
{
constexpr EAutomationTestFlags D3Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

FLKBattleOutcome MakeWinOutcome(const FLKBattleContext& Context)
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
		Hero.RecoveredState.Health = State.MaxHealth * 0.6f;
		Outcome.PlayerHeroes.Add(MoveTemp(Hero));
	}
	return Outcome;
}

FLKRunRewardOffer Upgrade(FName CardId, int32 Level)
{
	FLKRunRewardOffer Offer;
	Offer.Kind = ELKRunRewardKind::UpgradeCard;
	Offer.CardId = CardId;
	Offer.LevelBefore = Level;
	Offer.LevelAfter = Level + 1;
	return Offer;
}

FLKRunRewardOffer AddCard(FName CardId)
{
	FLKRunRewardOffer Offer;
	Offer.Kind = ELKRunRewardKind::AddCard;
	Offer.CardId = CardId;
	return Offer;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD3RewardStateMachineTest, "LittleKing.D3.Rewards.StateMachineAtomicity", D3Flags)
bool FLKD3RewardStateMachineTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
	FLKD3TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	TestTrue(TEXT("Built-in three-room catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error));

	TestTrue(TEXT("Run starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 1234, Encounters));
	FLKBattleContext Room1;
	TestTrue(TEXT("Room one starts"), Run->BeginCurrentBattle(Room1));
	TestTrue(TEXT("Room one victory accepted"), Run->SubmitBattleOutcome(MakeWinOutcome(Room1)));

	// 房1 胜利后进入 ChoosingNode：提交奖励批次（升级×2 + 新卡）。
	TArray<FLKRunRewardOffer> Offers;
	Offers.Add(Upgrade(TEXT("Unit_Swordsman"), 0));
	Offers.Add(AddCard(TEXT("Unit_Skeleton")));
	Offers.Add(Upgrade(TEXT("Unit_Archer"), 0));
	TestFalse(TEXT("Offer requires non-empty and ≤3 candidates"), Run->OfferRewardBatch({}));
	TestFalse(TEXT("Offer rejects overlong batch"), Run->OfferRewardBatch({
		Upgrade(TEXT("Unit_Swordsman"), 0), Upgrade(TEXT("Unit_Archer"), 0),
		Upgrade(TEXT("Unit_Shieldbearer"), 0), AddCard(TEXT("Unit_Skeleton")) }));
	TestTrue(TEXT("Valid batch is offered"), Run->OfferRewardBatch(Offers));
	TestTrue(TEXT("Pending reward choice is visible"), Run->HasPendingRewardChoice());
	TestEqual(TEXT("Three candidates are readable"), Run->GetPendingRewardCount(), 3);
	TestEqual(TEXT("Second candidate is the skeleton card"), Run->GetPendingRewardOffer(1).Kind, ELKRunRewardKind::AddCard);
	TestFalse(TEXT("Cannot advance while reward is pending"), Run->CanAdvance());
	TestFalse(TEXT("Cannot claim out-of-range option"), Run->ChooseReward(3));
	TestTrue(TEXT("First skip succeeds"), Run->SkipReward());
	TestFalse(TEXT("Second skip is rejected"), Run->SkipReward());
	TestTrue(TEXT("Skipped batch consumes the choice"), !Run->HasPendingRewardChoice() && Run->CanAdvance());
	TestFalse(TEXT("Same victory cannot offer a second batch"), Run->OfferRewardBatch(Offers));

	// 领取升级：下一房 PlayerCards 应带 UpgradeLevel。
	TestTrue(TEXT("Room two starts after skip"), Run->AdvanceToNextBattle());
	FLKBattleContext Room2;
	TestTrue(TEXT("Room two begins"), Run->BeginCurrentBattle(Room2));
	TestTrue(TEXT("Room two victory accepted"), Run->SubmitBattleOutcome(MakeWinOutcome(Room2)));

	TArray<FLKRunRewardOffer> UpgradeOnly;
	UpgradeOnly.Add(Upgrade(TEXT("Unit_Swordsman"), 0));
	TestTrue(TEXT("Second batch offered"), Run->OfferRewardBatch(UpgradeOnly));
	TestTrue(TEXT("Upgrade reward is claimed"), Run->ChooseReward(0));
	TestFalse(TEXT("Claim cannot repeat after success"), Run->ChooseReward(0));
	TestFalse(TEXT("Skip cannot repeat after claim"), Run->SkipReward());
	TestTrue(TEXT("Claim returns to choosing node"), Run->CanAdvance());

	TestTrue(TEXT("Room three starts after upgrade"), Run->AdvanceToNextBattle());
	FLKBattleContext Room3;
	TestTrue(TEXT("Room three begins"), Run->BeginCurrentBattle(Room3));
	const FLKRunCardState* Swordsman = Room3.PlayerCards.FindByPredicate(
		[](const FLKRunCardState& Card) { return Card.CardId == "Unit_Swordsman"; });
	const FLKRunCardState* Skeleton = Room3.PlayerCards.FindByPredicate(
		[](const FLKRunCardState& Card) { return Card.CardId == "Unit_Skeleton"; });
	TestTrue(TEXT("Upgraded card level crosses rooms"), Swordsman && Swordsman->UpgradeLevel == 1);
	TestFalse(TEXT("Skipped card was not added"), Skeleton != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD3RewardDefenseTest, "LittleKing.D3.Rewards.ClaimBoundaries", D3Flags)
bool FLKD3RewardDefenseTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
	FLKD3TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }
	if (!TestTrue(TEXT("Run starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 4321, Encounters))) { return false; }

	// 战斗中不允许提交奖励（阶段窗口错误）。
	FLKBattleContext Battle;
	TestTrue(TEXT("Battle context begins"), Run->BeginCurrentBattle(Battle));
	TArray<FLKRunRewardOffer> InBattleOffers;
	InBattleOffers.Add(AddCard(TEXT("Unit_Skeleton")));
	TestFalse(TEXT("Offer rejected while in battle"), Run->OfferRewardBatch(InBattleOffers));

	TestTrue(TEXT("Victory accepted"), Run->SubmitBattleOutcome(MakeWinOutcome(Battle)));
	TestTrue(TEXT("Add-card batch offered"), Run->OfferRewardBatch(InBattleOffers));
	TestTrue(TEXT("Skeleton card claimed once"), Run->ChooseReward(0));

	// 已持有卡的 AddCard 候选：生成层不会产出，防御层仍拒绝（不产生重复副本）。
	TestTrue(TEXT("Room two starts"), Run->AdvanceToNextBattle());
	FLKBattleContext Room2;
	TestTrue(TEXT("Room two begins"), Run->BeginCurrentBattle(Room2));
	TestTrue(TEXT("Room two victory accepted"), Run->SubmitBattleOutcome(MakeWinOutcome(Room2)));
	TArray<FLKRunRewardOffer> DuplicateOffer;
	DuplicateOffer.Add(AddCard(TEXT("Unit_Skeleton"))); // 已持有 → 领取必须失败
	TestTrue(TEXT("Duplicate-card batch still offered by state layer"), Run->OfferRewardBatch(DuplicateOffer));
	TestFalse(TEXT("Duplicate add-card claim is rejected"), Run->ChooseReward(0));
	TestTrue(TEXT("Player can still skip after rejection"), Run->SkipReward() && Run->CanAdvance());

	// 跳过之后同一胜利窗口不能再发第二批（防刷奖励）。
	TArray<FLKRunRewardOffer> StaleOffer;
	StaleOffer.Add(Upgrade(TEXT("Unit_Archer"), 5)); // 实际等级 0 → 领取层拒绝
	TestFalse(TEXT("Same-window second batch is rejected after skip"), Run->OfferRewardBatch(StaleOffer));

	// 精英房（第 3 战）：旧等级候选即使发批也会在领取层被拒，玩家可跳过继续。
	TestTrue(TEXT("Elite room starts"), Run->AdvanceToNextBattle());
	FLKBattleContext Room3;
	TestTrue(TEXT("Elite room begins"), Run->BeginCurrentBattle(Room3));
	TestTrue(TEXT("Elite victory accepted"), Run->SubmitBattleOutcome(MakeWinOutcome(Room3)));
	TestTrue(TEXT("Stale batch offered by state layer"), Run->OfferRewardBatch(StaleOffer));
	TestFalse(TEXT("Stale level upgrade claim is rejected"), Run->ChooseReward(0));
	TestTrue(TEXT("Skip remains available after stale claim"), Run->SkipReward());

	// 首领房（第 4 战）：胜利后远征直接 Completed，不再发奖励。
	TestTrue(TEXT("Boss room starts"), Run->AdvanceToNextBattle());
	FLKBattleContext BossRoom;
	TestTrue(TEXT("Boss room begins"), Run->BeginCurrentBattle(BossRoom));
	TestTrue(TEXT("Boss victory accepted"), Run->SubmitBattleOutcome(MakeWinOutcome(BossRoom)));
	TestFalse(TEXT("No reward after boss completion"), Run->OfferRewardBatch(DuplicateOffer));
	TestTrue(TEXT("Run terminal after boss room"), Run->IsTerminal());
	return true;
}

#endif
