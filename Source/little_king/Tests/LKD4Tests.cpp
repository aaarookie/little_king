#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"

#include "../LKEncounterContent.h"
#include "../LKRunTypes.h"
#include "../ULKRunSubsystem.h"

struct FLKD4TestParty
{
	TArray<FLKRunHeroState> Heroes;
	TArray<FLKRunCardState> Cards;

	FLKD4TestParty()
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
constexpr EAutomationTestFlags D4Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

FLKBattleOutcome D4Win(const FLKBattleContext& Context)
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
		Hero.RecoveredState.Health = State.MaxHealth * 0.6f; // 战后恢复 20%→60% 档
		Outcome.PlayerHeroes.Add(MoveTemp(Hero));
	}
	return Outcome;
}

FName FirstOfType(const ULKRunSubsystem* Run, ELKDungeonNodeType Type, const TArray<FName>& Ids)
{
	for (FName Id : Ids)
	{
		const FLKDungeonNode Node = Run->GetNode(Id);
		if (Node.NodeId == Id && Node.Type == Type) { return Id; }
	}
	return NAME_None;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD4MapStructureTest, "LittleKing.D4.Map.RowStructureAndVisibility", D4Flags)
bool FLKD4MapStructureTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
	FLKD4TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }
	if (!TestTrue(TEXT("Run starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 777, Encounters))) { return false; }

	// 图：Start → R1（普通战）→ R2（战/休）→ R3（精英/休）→ Boss
	const FLKDungeonNode Start = Run->GetNode("Node_Start");
	const FLKDungeonNode R1 = Run->GetNode("Node_R1");
	const FLKDungeonNode Boss = Run->GetNode("Node_Boss");
	TestTrue(TEXT("Start is resolved entry"), Start.NodeId == "Node_Start" && Start.bResolved);
	TestEqual(TEXT("R1 is a normal battle with a next row"), R1.Type, ELKDungeonNodeType::Battle);
	TestEqual(TEXT("R1 exposes exactly two next nodes"), R1.NextNodeIds.Num(), 2);
	TestEqual(TEXT("Boss row terminates"), Boss.Type, ELKDungeonNodeType::Boss);

	// 第二排与第三排：战斗/休息与精英/休息各占其一（左右由种子决定）。
	const FLKDungeonNode R2A = Run->GetNode("Node_R2A");
	const FLKDungeonNode R2B = Run->GetNode("Node_R2B");
	TestTrue(TEXT("Row two mixes one battle and one rest"),
		(R2A.Type == ELKDungeonNodeType::Battle && R2B.Type == ELKDungeonNodeType::Rest)
		|| (R2A.Type == ELKDungeonNodeType::Rest && R2B.Type == ELKDungeonNodeType::Battle));
	const FLKDungeonNode R3A = Run->GetNode("Node_R3A");
	const FLKDungeonNode R3B = Run->GetNode("Node_R3B");
	TestTrue(TEXT("Row three mixes one elite and one rest"),
		(R3A.Type == ELKDungeonNodeType::Elite && R3B.Type == ELKDungeonNodeType::Rest)
		|| (R3A.Type == ELKDungeonNodeType::Rest && R3B.Type == ELKDungeonNodeType::Elite));

	// 休息节点之后仍指向下一排（选择休息不会卡死流程）。
	const FName Row2Rest = FirstOfType(Run, ELKDungeonNodeType::Rest, { "Node_R2A", "Node_R2B" });
	const FName Row3Rest = FirstOfType(Run, ELKDungeonNodeType::Rest, { "Node_R3A", "Node_R3B" });
	TestTrue(TEXT("Row two rest leads to row three"), Row2Rest != NAME_None
		&& Run->GetNode(Row2Rest).NextNodeIds.Num() == 2);
	TestTrue(TEXT("Row three rest leads to boss"), Row3Rest != NAME_None
		&& Run->GetNode(Row3Rest).NextNodeIds.Contains("Node_Boss"));

	// 初始不可选择（进入战斗）；房一胜利后只能看到/选择下一排。
	TestFalse(TEXT("No selection while entering battle"), Run->CanSelectNextNode());
	FLKBattleContext Room1;
	if (!TestTrue(TEXT("Room one begins"), Run->BeginCurrentBattle(Room1))) { return false; }
	TestTrue(TEXT("Room one win"), Run->SubmitBattleOutcome(D4Win(Room1)));
	TestTrue(TEXT("Row becomes selectable after win"), Run->CanSelectNextNode());
	const TArray<FName> NextIds = Run->GetNextNodeIds();
	TestEqual(TEXT("Next row has exactly two nodes"), NextIds.Num(), 2);
	TestTrue(TEXT("Next row is the actual second row"),
		(NextIds.Contains("Node_R2A") && NextIds.Contains("Node_R2B")));

	// 看不见后续：第三排节点此刻不可选（状态层拒绝，UI 也只会列 Next）。
	TestEqual(TEXT("Later rows are rejected before their row is reached"),
		Run->SelectNode("Node_R3A"), ELKNodeSelectionResult::Rejected);
	TestEqual(TEXT("Resolved rows are rejected"),
		Run->SelectNode("Node_R1"), ELKNodeSelectionResult::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD4RosterRulesTest, "LittleKing.D4.Roster.RandomizedEncounterRules", D4Flags)
bool FLKD4RosterRulesTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
	FLKD4TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }

	// 同种子两次开局 → 遭遇完全一致（路线与随机阵容确定性）。
	if (!TestTrue(TEXT("Run one starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 2026, Encounters))) { return false; }
	TArray<FName> FirstEncounters;
	for (const FName NodeId : { FName("Node_R1"), FName("Node_R2A"), FName("Node_R2B"),
		FName("Node_R3A"), FName("Node_R3B"), FName("Node_Boss") })
	{
		const FLKDungeonNode Node = Run->GetNode(NodeId);
		if (!Node.EncounterId.IsNone()) { FirstEncounters.Add(Node.EncounterId); }
	}
	TestEqual(TEXT("Four battle nodes own dynamic encounters"), FirstEncounters.Num(), 4);

	// 收集每场遭遇的英雄数量并验证规则。
	const auto RowHeroes = [&Run](FName NodeId)
	{
		const FLKDungeonNode Node = Run->GetNode(NodeId);
		if (Node.EncounterId.IsNone()) { return TArray<FName>(); }
		const FLKRunState State = Run->GetRunState();
		const FLKEncounterRow* Encounter = LKEncounterContent::Find(State.Encounters, Node.EncounterId);
		return Encounter ? Encounter->EnemyHeroIds : TArray<FName>();
	};

	const TArray<FName> R1Heroes = RowHeroes("Node_R1");
	TestEqual(TEXT("Normal room fields one random hero"), R1Heroes.Num(), 1);
	TestTrue(TEXT("Normal hero belongs to the enemy pool"), R1Heroes.Num() == 1
		&& (R1Heroes[0] == "Hero_Necromancer" || R1Heroes[0] == "Hero_SkeletonGiant"));

	const FName Row2Battle = FirstOfType(Run, ELKDungeonNodeType::Battle, { "Node_R2A", "Node_R2B" });
	const TArray<FName> R2Heroes = RowHeroes(Row2Battle);
	TestEqual(TEXT("Second normal room fields one random hero"), R2Heroes.Num(), 1);

	const FName Row3Elite = FirstOfType(Run, ELKDungeonNodeType::Elite, { "Node_R3A", "Node_R3B" });
	const TArray<FName> R3Heroes = RowHeroes(Row3Elite);
	TestEqual(TEXT("Elite room fields two distinct heroes"), R3Heroes.Num(), 2);
	TestTrue(TEXT("Elite heroes are distinct"), R3Heroes.Num() == 2 && R3Heroes[0] != R3Heroes[1]);

	const TArray<FName> BossHeroes = RowHeroes("Node_Boss");
	TestEqual(TEXT("Boss room fields boss plus two distinct heroes"), BossHeroes.Num(), 3);
	TestEqual(TEXT("Boss pool leads the boss roster"), BossHeroes[0], FName("Boss_SkeletonKing"));
	TestTrue(TEXT("Boss roster has no duplicates"), BossHeroes.Num() == 3
		&& BossHeroes[0] != BossHeroes[1] && BossHeroes[1] != BossHeroes[2] && BossHeroes[0] != BossHeroes[2]);

	// 同种子重开 → 同一批遭遇。
	UGameInstance* Instance2 = NewObject<UGameInstance>();
	ULKRunSubsystem* Run2 = NewObject<ULKRunSubsystem>(Instance2);
	TestTrue(TEXT("Run two starts with same seed"), Run2->StartNewRun(Party.Heroes, Party.Cards, 2026, Encounters));
	TArray<FName> SecondEncounters;
	for (const FName NodeId : { FName("Node_R1"), FName("Node_R2A"), FName("Node_R2B"),
		FName("Node_R3A"), FName("Node_R3B"), FName("Node_Boss") })
	{
		const FLKDungeonNode Node = Run2->GetNode(NodeId);
		if (!Node.EncounterId.IsNone()) { SecondEncounters.Add(Node.EncounterId); }
	}
	TestTrue(TEXT("Same seed reproduces identical encounters"), FirstEncounters == SecondEncounters);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLKD4RestAndFullRunTest, "LittleKing.D4.Run.RestHealAndFullClear", D4Flags)
bool FLKD4RestAndFullRunTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	ULKRunSubsystem* Run = NewObject<ULKRunSubsystem>(Instance);
	Run->SetAutoSaveEnabled(false); // 测试不写磁盘槽
	FLKD4TestParty Party;
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!TestTrue(TEXT("Catalog loads"), LKEncounterContent::BuildCatalog(nullptr, Encounters, Error))) { return false; }
	if (!TestTrue(TEXT("Run starts"), Run->StartNewRun(Party.Heroes, Party.Cards, 99, Encounters))) { return false; }

	// 房一胜利（60% 生命）→ 选休息：30% → 90%。
	FLKBattleContext Room1;
	if (!TestTrue(TEXT("Room one begins"), Run->BeginCurrentBattle(Room1))) { return false; }
	TestTrue(TEXT("Room one win"), Run->SubmitBattleOutcome(D4Win(Room1)));
	const FName Row2Rest = FirstOfType(Run, ELKDungeonNodeType::Rest, { "Node_R2A", "Node_R2B" });
	TestEqual(TEXT("Rest node is selectable after win"),
		Run->SelectNode(Row2Rest), ELKNodeSelectionResult::RestResolved);
	TestTrue(TEXT("Rest heals to ninety percent"), FMath::IsNearlyEqual(
		Run->GetRunState().Heroes[0].Health, Run->GetRunState().Heroes[0].MaxHealth * 0.9f, 0.1f));
	TestTrue(TEXT("Still choosing after rest"), Run->CanSelectNextNode());
	TestEqual(TEXT("Rest keeps the same current node"), Run->GetRunState().CurrentNodeId, Row2Rest);

	// 第三排再选休息：90% → 封顶 100%。
	const FName Row3Rest = FirstOfType(Run, ELKDungeonNodeType::Rest, { "Node_R3A", "Node_R3B" });
	TestEqual(TEXT("Second rest is selectable from row three"),
		Run->SelectNode(Row3Rest), ELKNodeSelectionResult::RestResolved);
	TestTrue(TEXT("Rest caps at full health"), FMath::IsNearlyEqual(
		Run->GetRunState().Heroes[0].Health, Run->GetRunState().Heroes[0].MaxHealth, 0.1f));

	// 休息后唯一去向 = 首领房（选择 Boss 节点才进入战斗）。
	TestEqual(TEXT("Boss row is the only option after rests"), Run->GetNextNodeIds().Num() == 1
		&& Run->GetNextNodeIds()[0] == "Node_Boss", true);
	TestEqual(TEXT("Selecting boss enters the battle"),
		Run->SelectNode("Node_Boss"), ELKNodeSelectionResult::BattleEntered);
	FLKBattleContext Boss;
	if (!TestTrue(TEXT("Boss begins after rests"), Run->BeginCurrentBattle(Boss))) { return false; }
	TestEqual(TEXT("Boss encounter reached"), Boss.EncounterId, FName("Enc_Dyn_Boss"));
	TestTrue(TEXT("Boss win completes the run"), Run->SubmitBattleOutcome(D4Win(Boss)));
	TestEqual(TEXT("Run completed"), Run->GetRunPhase(), ELKRunPhase::Completed);
	TestTrue(TEXT("Run is terminal"), Run->IsTerminal());
	return true;
}

#endif
