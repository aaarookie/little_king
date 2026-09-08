#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#include "../LKEncounterContent.h"
#include "../LKRunTypes.h"
#include "../ULKRunSaveGame.h"
#include "../ULKRunSubsystem.h"

struct FLKD5TestParty
{
	TArray<FLKRunHeroState> Heroes;
	TArray<FLKRunCardState> Cards;

	FLKD5TestParty()
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
constexpr EAutomationTestFlags D5Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

FLKBattleOutcome D5Win(const FLKBattleContext& Context)
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

void CleanupSlot(const FString& Slot)
{
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0)) { UGameplayStatics::DeleteGameInSlot(Slot, 0); }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD5ValidateTest, "LittleKing.D5.Save.ValidationRules", D5Flags)
bool FLKD5ValidateTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	FLKD5TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }
	if (!TestTrue(TEXT("Run starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 555, Encounters))) { return false; }
	Run->SetAutoSaveEnabled(false);

	// 合法状态通过校验。
	FLKRunState Valid = Run->GetRunState();
	FString ValidationError;
	TestTrue(TEXT("Fresh run state validates"), ULKRunSubsystem::ValidateStoredRun(Valid, ValidationError));

	// 版本过高/过低拒绝（未知版本不静默当新档覆盖）。
	FLKRunState Future = Valid;
	Future.SchemaVersion = 99;
	TestFalse(TEXT("Unknown future schema is rejected"), ULKRunSubsystem::ValidateStoredRun(Future, ValidationError));
	TestTrue(TEXT("Rejection names the version"), ValidationError.Contains(TEXT("版本")));
	FLKRunState Ancient = Valid;
	Ancient.SchemaVersion = 0;
	TestFalse(TEXT("Ancient schema is rejected"), ULKRunSubsystem::ValidateStoredRun(Ancient, ValidationError));

	// 损坏内容拒绝。
	FLKRunState NoRunId = Valid;
	NoRunId.RunId = FGuid();
	TestFalse(TEXT("Missing run id is rejected"), ULKRunSubsystem::ValidateStoredRun(NoRunId, ValidationError));
	FLKRunState BadHero = Valid;
	BadHero.Heroes[0].Health = -5.f;
	TestFalse(TEXT("Negative hero health is rejected"), ULKRunSubsystem::ValidateStoredRun(BadHero, ValidationError));
	FLKRunState TinyDeck = Valid;
	TinyDeck.Cards.Pop();
	TinyDeck.Cards.Pop();
	TinyDeck.Cards.Pop();
	TestFalse(TEXT("Deck below five is rejected"), ULKRunSubsystem::ValidateStoredRun(TinyDeck, ValidationError));
	FLKRunState GhostNode = Valid;
	GhostNode.CurrentNodeId = "Node_Ghost";
	TestFalse(TEXT("Unknown current node is rejected"), ULKRunSubsystem::ValidateStoredRun(GhostNode, ValidationError));

	// 战斗恢复点要求 PendingBattle 完整可解析。
	FLKRunState BattlePoint = Valid;
	BattlePoint.Phase = ELKRunPhase::EnteringBattle;
	FLKRunState EmptyPending = BattlePoint;
	EmptyPending.PendingBattle = FLKBattleContext();
	TestFalse(TEXT("Battle resume point without context is rejected"),
		ULKRunSubsystem::ValidateStoredRun(EmptyPending, ValidationError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD5RoundTripTest, "LittleKing.D5.Save.RoundTripAndNoDoubleClaim", D5Flags)
bool FLKD5RoundTripTest::RunTest(const FString& Parameters)
{
	const FString TestSlot = TEXT("LittleKing_D5_RoundTrip");
	CleanupSlot(TestSlot);

	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	FLKD5TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }
	Run->SetAutoSaveEnabled(false);

	// 打掉第一战并领一个升级奖励后存档。
	if (!TestTrue(TEXT("Run starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 6161, Encounters))) { return false; }
	FLKBattleContext Room1;
	if (!TestTrue(TEXT("Room one begins"), Run->BeginCurrentBattle(Room1))) { return false; }
	if (!TestTrue(TEXT("Room one win"), Run->SubmitBattleOutcome(D5Win(Room1)))) { return false; }
	TArray<FLKRunRewardOffer> Offers;
	Offers.Add(Upgrade(TEXT("Unit_Swordsman"), 0));
	if (!TestTrue(TEXT("Reward offered"), Run->OfferRewardBatch(Offers))) { return false; }
	if (!TestTrue(TEXT("Reward claimed"), Run->ChooseReward(0))) { return false; }
	const FGuid OriginalRunId = Run->GetRunState().RunId;
	TestTrue(TEXT("Save to test slot succeeds"), Run->SaveExpeditionToSlot(TestSlot));

	// 模拟进程重启：新实例从槽载入。
	UGameInstance* Instance2 = NewObject<UGameInstance>();
	ULKRunSubsystem* Restored = NewObject<ULKRunSubsystem>(Instance2);
	Restored->SetAutoSaveEnabled(false);
	TestTrue(TEXT("Load from test slot succeeds"), Restored->LoadExpeditionFromSlot(TestSlot));
	TestTrue(TEXT("Restored run keeps identity"), Restored->GetRunState().RunId == OriginalRunId);
	TestEqual(TEXT("Restored phase is choosing node"), Restored->GetRunPhase(), ELKRunPhase::ChoosingNode);
	const FLKRunCardState* Swordsman = Restored->GetRunState().Cards.FindByPredicate(
		[](const FLKRunCardState& Card) { return Card.CardId == "Unit_Swordsman"; });
	TestTrue(TEXT("Claimed upgrade survives the round trip"), Swordsman && Swordsman->UpgradeLevel == 1);
	TestFalse(TEXT("No pending reward is restored after claim"), Restored->HasPendingRewardChoice());
	TestTrue(TEXT("Next row remains selectable after restore"), Restored->CanSelectNextNode());

	// 再次"重启"载入同一档：奖励不复制、卡等级不翻倍（防重复领取）。
	UGameInstance* Instance3 = NewObject<UGameInstance>();
	ULKRunSubsystem* Reloaded = NewObject<ULKRunSubsystem>(Instance3);
	Reloaded->SetAutoSaveEnabled(false);
	TestTrue(TEXT("Reloading the same save is idempotent"), Reloaded->LoadExpeditionFromSlot(TestSlot));
	const FLKRunCardState* ReloadCard = Reloaded->GetRunState().Cards.FindByPredicate(
		[](const FLKRunCardState& Card) { return Card.CardId == "Unit_Swordsman"; });
	TestTrue(TEXT("Claimed level is not duplicated by reload"), ReloadCard && ReloadCard->UpgradeLevel == 1);
	TestFalse(TEXT("No stale pending reward after reload"), Reloaded->HasPendingRewardChoice());

	// 同胜利窗口的"已发批"标记随存档保留：载入后无法再发一批（跨存档防重复奖励）。
	TArray<FLKRunRewardOffer> Stale;
	Stale.Add(Upgrade(TEXT("Unit_Swordsman"), 0));
	TestFalse(TEXT("Same victory window cannot re-offer after restore"), Reloaded->OfferRewardBatch(Stale));
	// 推进到下一房 = 新窗口，流程继续。
	TestTrue(TEXT("Advance opens a new window after restore"), Reloaded->AdvanceToNextBattle());

	CleanupSlot(TestSlot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD5RecoveryTest, "LittleKing.D5.Save.RecoverySemantics", D5Flags)
bool FLKD5RecoveryTest::RunTest(const FString& Parameters)
{
	const FString TestSlot = TEXT("LittleKing_D5_Recovery");
	CleanupSlot(TestSlot);

	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	FLKD5TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }
	Run->SetAutoSaveEnabled(false);

	// a) 战斗中退出：进入第 2 战（EnteringBattle）后存档 = 恢复点。
	if (!TestTrue(TEXT("Run starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 7777, Encounters))) { return false; }
	FLKBattleContext Room1;
	if (!TestTrue(TEXT("Room one begins"), Run->BeginCurrentBattle(Room1))) { return false; }
	if (!TestTrue(TEXT("Room one win"), Run->SubmitBattleOutcome(D5Win(Room1)))) { return false; }
	if (!TestTrue(TEXT("Advance to room two"), Run->AdvanceToNextBattle())) { return false; }
	const FGuid RoomTwoAttempt = Run->GetRunState().PendingBattle.AttemptId;
	TestEqual(TEXT("Recovery point is entering battle"), Run->GetRunPhase(), ELKRunPhase::EnteringBattle);
	TestTrue(TEXT("Save mid-transition succeeds"), Run->SaveExpeditionToSlot(TestSlot));

	UGameInstance* Instance2 = NewObject<UGameInstance>();
	ULKRunSubsystem* Restored = NewObject<ULKRunSubsystem>(Instance2);
	Restored->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Load recovery point"), Restored->LoadExpeditionFromSlot(TestSlot))) { return false; }
	TestEqual(TEXT("Restored phase stays entering battle"), Restored->GetRunPhase(), ELKRunPhase::EnteringBattle);
	FLKBattleContext RecoveredRoom;
	TestTrue(TEXT("Recovered battle can begin"), Restored->BeginCurrentBattle(RecoveredRoom));
	TestEqual(TEXT("Same attempt replays on recovery"), RecoveredRoom.AttemptId, RoomTwoAttempt);
	TestTrue(TEXT("Recovered battle can settle"), Restored->SubmitBattleOutcome(D5Win(RecoveredRoom)));

	// b) 终态：打完剩余战斗（R3 精英 + Boss）→ Completed → 存档 → 载入仍终态（摘要可查）。
	if (!TestTrue(TEXT("Advance to elite"), Restored->AdvanceToNextBattle())) { return false; }
	FLKBattleContext Elite;
	if (!TestTrue(TEXT("Elite begins"), Restored->BeginCurrentBattle(Elite))) { return false; }
	if (!TestTrue(TEXT("Elite win"), Restored->SubmitBattleOutcome(D5Win(Elite)))) { return false; }
	if (!TestTrue(TEXT("Advance to boss"), Restored->AdvanceToNextBattle())) { return false; }
	FLKBattleContext Boss;
	if (!TestTrue(TEXT("Boss begins"), Restored->BeginCurrentBattle(Boss))) { return false; }
	if (!TestTrue(TEXT("Boss win"), Restored->SubmitBattleOutcome(D5Win(Boss)))) { return false; }
	TestEqual(TEXT("Run completed"), Restored->GetRunPhase(), ELKRunPhase::Completed);
	TestTrue(TEXT("Save terminal state"), Restored->SaveExpeditionToSlot(TestSlot));

	UGameInstance* Instance3 = NewObject<UGameInstance>();
	ULKRunSubsystem* Terminal = NewObject<ULKRunSubsystem>(Instance3);
	Terminal->SetAutoSaveEnabled(false);
	TestTrue(TEXT("Terminal state loads"), Terminal->LoadExpeditionFromSlot(TestSlot));
	TestEqual(TEXT("Terminal phase is restored"), Terminal->GetRunPhase(), ELKRunPhase::Completed);
	TestEqual(TEXT("Battle history survives save"), Terminal->GetRunState().BattleHistory.Num(), 4);

	// 损坏文件（版本不符）拒绝载入且不影响已载入状态。
	ULKRunSaveGame* Corrupt = NewObject<ULKRunSaveGame>();
	Corrupt->SaveVersion = 99;
	Corrupt->RunState = Terminal->GetRunState();
	UGameplayStatics::SaveGameToSlot(Corrupt, TestSlot, 0);
	ULKRunSubsystem* Guard = NewObject<ULKRunSubsystem>(NewObject<UGameInstance>());
	Guard->SetAutoSaveEnabled(false);
	TestFalse(TEXT("Future file version is refused"), Guard->LoadExpeditionFromSlot(TestSlot));
	TestFalse(TEXT("Refused file does not start a run"), Guard->HasRun());

	CleanupSlot(TestSlot);
	return true;
}

// BUG-017：旧档快照缺少"代码身份特性"（法师全场施法/骑士嘲讽光环）时，载入后按当前代码补全。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD5IdentityTraitRepairTest, "LittleKing.D5.Save.IdentityTraitsRestoredOnLoad", D5Flags)
bool FLKD5IdentityTraitRepairTest::RunTest(const FString& Parameters)
{
	const FString TestSlot = TEXT("LittleKing_D5_IdentityTraits");
	CleanupSlot(TestSlot);

	// 旧档模拟：法师/骑士快照里没有身份特性；游侠保留玩家在房间间加的临时特性。
	FLKD5TestParty OldParty;
	for (FLKRunHeroState& Hero : OldParty.Heroes)
	{
		if (Hero.HeroId == "Hero_Ranger") { Hero.Traits = { "Trait_FaceFear" }; }
	}
	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false);
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }
	if (!TestTrue(TEXT("Old-style run starts"), Run->StartNewRun(OldParty.Heroes, OldParty.Cards, 9090, Encounters))) { return false; }
	if (!TestTrue(TEXT("Old-style save is written"), Run->SaveExpeditionToSlot(TestSlot))) { return false; }

	UGameInstance* Instance2 = NewObject<UGameInstance>();
	ULKRunSubsystem* Restored = NewObject<ULKRunSubsystem>(Instance2);
	Restored->SetAutoSaveEnabled(false);
	if (!TestTrue(TEXT("Old-style save loads"), Restored->LoadExpeditionFromSlot(TestSlot))) { return false; }
	const FLKRunState LoadedState = Restored->GetRunState();
	const FLKRunHeroState* LoadedMage = LoadedState.Heroes.FindByPredicate(
		[](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Mage"; });
	TestFalse(TEXT("Loaded old save really lacks the mage identity trait"),
		LoadedMage && LoadedMage->Traits.Contains("Trait_MageSpellReach"));

	// GameMode 载入存档后调用的同一个补全入口（这里直接提供等价的代码身份特性表）。
	TMap<FName, TArray<FName>> Identity;
	Identity.Add("Hero_Mage", { FName("Trait_MageSpellReach") });
	Identity.Add("Hero_Knight", { FName("Trait_KnightTauntAura") });
	TestEqual(TEXT("Repair restores both identity traits"), Restored->RestoreHeroIdentityTraits(Identity), 2);
	const FLKRunState& State = Restored->GetRunState();
	const FLKRunHeroState* Mage = State.Heroes.FindByPredicate(
		[](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Mage"; });
	const FLKRunHeroState* Knight = State.Heroes.FindByPredicate(
		[](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Knight"; });
	const FLKRunHeroState* Ranger = State.Heroes.FindByPredicate(
		[](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Ranger"; });
	TestTrue(TEXT("Mage regains global spell placement"), Mage && Mage->Traits.Contains("Trait_MageSpellReach"));
	TestTrue(TEXT("Knight regains the taunt aura"), Knight && Knight->Traits.Contains("Trait_KnightTauntAura"));
	TestTrue(TEXT("Player-added trait is untouched"), Ranger && Ranger->Traits.Contains("Trait_FaceFear")
		&& !Ranger->Traits.Contains("Trait_MageSpellReach"));
	TestEqual(TEXT("Second repair is a no-op"), Restored->RestoreHeroIdentityTraits(Identity), 0);

	FLKBattleContext Context;
	if (!TestTrue(TEXT("Repaired run can begin a battle"), Restored->BeginCurrentBattle(Context))) { return false; }
	const FLKRunHeroState* ContextMage = Context.PlayerHeroes.FindByPredicate(
		[](const FLKRunHeroState& Hero) { return Hero.HeroId == "Hero_Mage"; });
	TestTrue(TEXT("Battle context carries the restored trait"),
		ContextMage && ContextMage->Traits.Contains("Trait_MageSpellReach"));

	CleanupSlot(TestSlot);
	return true;
}

#endif
