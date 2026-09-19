#include "ULKRunSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "LKEncounterContent.h"
#include "LKHomeContent.h"
#include "LKWorldMapContent.h"
#include "LKLog.h"
#include "ULKProfileSubsystem.h"
#include "ULKRunSaveGame.h"
#include "LKCardPresentation.h"
#include "LKCardRules.h"
#include "ALKBattleGameMode.h"

namespace
{
    FLKDungeonNode MakeNode(FName Id, ELKDungeonNodeType Type, FName Encounter, TArray<FName> Next = {})
    {
        FLKDungeonNode Node;
        Node.NodeId = Id;
        Node.Type = Type;
        Node.EncounterId = Encounter;
        Node.NextNodeIds = MoveTemp(Next);
        return Node;
    }

	bool IsBuiltInTrait(FName TraitId)
	{
		return TraitId == "Trait_MageSpellReach" || TraitId == "Trait_KnightTauntAura"
			|| TraitId == "Trait_Sacrifice" || TraitId == "Trait_FaceFear" || TraitId == "Taunt";
	}

	/** 自动化测试钩子：远征槽名（默认 LittleKing_Run） */
	FString GRunSlotName = TEXT("LittleKing_Run");
}

bool ULKRunSubsystem::ValidateStartingParty(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards) const
{
    if (Heroes.IsEmpty() || Cards.Num() < 5 || LKCardRules::Used(Cards) > LKCardRules::Capacity) { return false; }
    TSet<FName> HeroIds;
    for (const FLKRunHeroState& Hero : Heroes)
    {
        if (Hero.HeroId.IsNone() || HeroIds.Contains(Hero.HeroId) || !FMath::IsFinite(Hero.MaxHealth)
            || !FMath::IsFinite(Hero.Health) || Hero.MaxHealth <= 0.f || Hero.Health <= 0.f || Hero.Health > Hero.MaxHealth
			|| !FMath::IsFinite(Hero.BaseMaxHealth) || Hero.BaseMaxHealth < 0.f)
        { return false; }
		TSet<FName> Traits;
		for (FName TraitId : Hero.Traits)
		{
			if (TraitId.IsNone() || Traits.Contains(TraitId)) { return false; }
			Traits.Add(TraitId);
		}
        HeroIds.Add(Hero.HeroId);
    }
    TSet<FName> CardIds;
    for (const FLKRunCardState& Card : Cards)
    {
        if (Card.CardId.IsNone() || Card.UpgradeLevel < 0 || CardIds.Contains(Card.CardId)) { return false; }
        CardIds.Add(Card.CardId);
    }
    return true;
}

bool ULKRunSubsystem::ValidateEncounterCatalog(const TArray<FLKEncounterRow>& Encounters) const
{
	if (Encounters.IsEmpty()) { return false; }
	TSet<FName> Seen;
	for (const FLKEncounterRow& Source : Encounters)
	{
		FLKEncounterRow Row;
		FString Error;
		if (!LKEncounterContent::NormalizeAndValidate(Source.EncounterId, Source, Row, Error)
			|| Seen.Contains(Row.EncounterId))
		{
			UE_LOG(LogLKBattle, Error, TEXT("[Run] 遭遇目录无效：%s"), Error.IsEmpty() ? TEXT("重复 EncounterId") : *Error);
			return false;
		}
		Seen.Add(Row.EncounterId);
	}
	const bool bHasRoute = Seen.Contains("Encounter_UndeadPatrol") && Seen.Contains("Encounter_UndeadElite")
		&& Seen.Contains("Encounter_SkeletonKing");
	if (!bHasRoute) { UE_LOG(LogLKBattle, Error, TEXT("[Run] 遭遇目录缺少固定三房中的一个或多个稳定 ID")); }
	return bHasRoute;
}

bool ULKRunSubsystem::StartNewRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed)
{
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!LKEncounterContent::BuildCatalog(nullptr, Encounters, Error)) { return false; }
	return StartNewRun(Heroes, Cards, Seed, Encounters);
}

bool ULKRunSubsystem::StartNewRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed,
	const TArray<FLKEncounterRow>& Encounters)
{
	return StartNewRun(MakeStandaloneRequest(Heroes, Cards, Seed, Encounters));
}

bool ULKRunSubsystem::StartNewRun(const FLKExpeditionStartRequest& Request)
{
	return StartRunInternal(Request, false);
}

bool ULKRunSubsystem::RestartRun(const FLKExpeditionStartRequest& Request)
{
	return StartRunInternal(Request, true);
}

FLKExpeditionStartRequest ULKRunSubsystem::MakeStandaloneRequest(const TArray<FLKRunHeroState>& Heroes,
	const TArray<FLKRunCardState>& Cards, int32 Seed, const TArray<FLKEncounterRow>& Encounters) const
{
	// 旧签名只服务独立测试与调试轮：不关联 Profile，也不参与家园金币结算。
	FLKExpeditionStartRequest Request;
	Request.ProfileId = FGuid();
	Request.RegionId = LKHomeContent::DefaultRegionId();
	Request.Seed = Seed;
	Request.Heroes = Heroes;
	Request.Cards = Cards;
	Request.Encounters = Encounters;
	Request.BonusSnapshot = FLKMetaBonusSnapshot();
	Request.BonusSnapshot.RegionId = Request.RegionId;
	Request.RewardRules = LKHomeContent::DefaultRewardRules();
	Request.bEligibleForHomeReward = false;
	Request.bUseWorldMap = false;
	for (const FLKRunHeroState& Hero : Heroes) { Request.Loadout.HeroIds.Add(Hero.HeroId); }
	for (const FLKRunCardState& Card : Cards) { Request.Loadout.CardIds.Add(Card.CardId); }
	return Request;
}

bool ULKRunSubsystem::StartRunInternal(const FLKExpeditionStartRequest& Request, bool bRestart)
{
	if (bRestart) { if (!HasRun() || !IsTerminal()) { return false; } }
	else if (HasRun()) { return false; }

	if (!ValidateStartingParty(Request.Heroes, Request.Cards) || !ValidateEncounterCatalog(Request.Encounters)) { return false; }
	if (Request.RegionId.IsNone()) { UE_LOG(LogLKBattle, Error, TEXT("[Run] 出征缺少 RegionId")); return false; }

	const FLKRunState PreviousState = State;
	const FGuid PreviousRunId = State.RunId;
    State = FLKRunState();
    State.RunId = FGuid::NewGuid();
    State.Seed = Request.Seed;
	State.ProfileId = Request.ProfileId;
	State.RegionId = Request.RegionId;
	State.InitialLoadout = Request.Loadout;
	State.BonusSnapshot = Request.BonusSnapshot;
	State.RewardRules = Request.RewardRules;
	State.bHomeRewardEligible = Request.bEligibleForHomeReward && Request.ProfileId.IsValid();
	State.PendingGold = 0;
	State.StartingGold = Request.StartingGold;
	State.WalletGold = Request.StartingGold;
	State.PendingSettlement = FLKSettlementReceipt();
    State.Heroes = Request.Heroes;
	for (FLKRunHeroState& Hero : State.Heroes)
	{
		if (Hero.BaseMaxHealth <= 0.f) { Hero.BaseMaxHealth = Hero.MaxHealth; }
		Hero.Traits.Sort(FNameLexicalLess());
	}
    State.Cards = Request.Cards;
	for (const FLKEncounterRow& Source : Request.Encounters)
	{
		FLKEncounterRow Normalized;
		FString Error;
		if (!LKEncounterContent::NormalizeAndValidate(Source.EncounterId, Source, Normalized, Error))
		{ State = PreviousState; return false; }
		State.Encounters.Add(MoveTemp(Normalized));
	}

	// D4：生成分层节点图（固定形状：普通 → 普通/休息 → 精英/休息 → 首领）与随机敌阵容遭遇。
	FRandomStream MapStream(State.Seed);
	FString GraphError;
	if (!(Request.bUseWorldMap ? LKWorldMapContent::Generate(State, GraphError) : GenerateGraphAndEncounters(MapStream, GraphError)))
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Run] 生成路线失败：%s"), *GraphError);
		State = PreviousState;
		return false;
	}

    if (!Request.bUseWorldMap)
    {
	State.Nodes[0].bResolved = true;
    State.VisitedNodeIds = {"Node_Start"};
    State.CurrentNodeId = "Node_R1";
    State.Phase = ELKRunPhase::EnteringBattle;
    if (!BuildPendingBattle()) { State = PreviousState; return false; }
    }
	ULKProfileSubsystem* Profile = ProfileSubsystemOverrideForTest.Get();
	if (!Profile && GetGameInstance()) { Profile = GetGameInstance()->GetSubsystem<ULKProfileSubsystem>(); }
	if (Request.StartingGold < 0 || (State.bHomeRewardEligible && (!Profile || Profile->GetProfile().ProfileId != State.ProfileId || !Profile->ReserveDepartureGold(State.RunId, Request.StartingGold))))
	{ State = PreviousState; return false; }
	if (bAutoSaveEnabled && !SaveExpedition())
	{
		State = PreviousState;
		if (Profile) { Profile->ReconcileDepartureGold(State.RunId); }
		return false;
	}
	if (bRestart)
	{
		UE_LOG(LogLKBattle, Log, TEXT("[Run] 从头开始：%s -> %s"), *PreviousRunId.ToString(), *State.RunId.ToString());
	}
	else
	{
		UE_LOG(LogLKBattle, Log, TEXT("[Run] 新远征 %s（区域 %s），当前节点 %s（共 %d 节点）"),
			*State.RunId.ToString(), *State.RegionId.ToString(), *State.CurrentNodeId.ToString(), State.Nodes.Num());
	}
	if (State.bHomeRewardEligible)
	{
		UE_LOG(LogLKBattle, Log, TEXT("[Run] 本轮家园加成：恢复 %.0f%% · 银币 %.3f/s 上限 %.0f（神像 Lv%d / 金库 Lv%d）"),
			State.BonusSnapshot.HeroRecoveryPercent * 100.f, State.BonusSnapshot.PlayerSilverPerSecond,
			State.BonusSnapshot.PlayerSilverCap, State.BonusSnapshot.StatueLevel, State.BonusSnapshot.TreasuryLevel);
	}
    return true;
}

bool ULKRunSubsystem::GenerateGraphAndEncounters(FRandomStream& Stream, FString& OutError)
{
	OutError.Reset();
	State.Nodes.Reset();

	// 模板行（保留波次/AI/经济/奖励档），固定三行由目录校验保证存在。
	const FLKEncounterRow* NormalTemplate = LKEncounterContent::Find(State.Encounters, TEXT("Encounter_UndeadPatrol"));
	const FLKEncounterRow* EliteTemplate = LKEncounterContent::Find(State.Encounters, TEXT("Encounter_UndeadElite"));
	const FLKEncounterRow* BossTemplate = LKEncounterContent::Find(State.Encounters, TEXT("Encounter_SkeletonKing"));
	if (!NormalTemplate || !EliteTemplate || !BossTemplate) { OutError = TEXT("缺少普通/精英/首领模板遭遇"); return false; }
	const FLKEncounterRow NormalCopy = *NormalTemplate, EliteCopy = *EliteTemplate, BossCopy = *BossTemplate;
	NormalTemplate = &NormalCopy; EliteTemplate = &EliteCopy; BossTemplate = &BossCopy;

	// 敌方池：目录驱动（Boss 行英雄=首领池；其余行=英雄池）。
	TArray<FName> HeroPool;
	TArray<FName> BossPool;
	LKEncounterContent::CollectRosterPools(State.Encounters, HeroPool, BossPool);

	// 生成一个战斗遭遇（动态 ID 入 State.Encounters）。
	auto MakeBattle = [this, &Stream, &OutError](FName NodeId, FName EncounterId,
		const FLKEncounterRow& Template, ELKEncounterRank Rank,
		const TArray<FName>& Heroes, const TArray<FName>& Bosses) -> FLKDungeonNode
	{
		FLKEncounterRow Dynamic;
		FString Error;
		if (!LKEncounterContent::MakeDynamicEncounter(EncounterId, Template, Rank, Heroes, Bosses, Stream, Dynamic, Error))
		{
			OutError = FString::Printf(TEXT("节点 %s：%s"), *NodeId.ToString(), *Error);
			return FLKDungeonNode();
		}
		State.Encounters.Add(MoveTemp(Dynamic));
		return MakeNode(NodeId, NodeTypeForRank(Rank), EncounterId);
	};

	// 第二、三排左右槽位：战斗/精英 与 休息 的左右由种子决定。
	const bool bSwapRow2 = Stream.RandRange(0, 1) == 1;
	const bool bSwapRow3 = Stream.RandRange(0, 1) == 1;

	FLKDungeonNode R1 = MakeBattle(TEXT("Node_R1"), TEXT("Enc_Dyn_R1"), *NormalTemplate, ELKEncounterRank::Normal, HeroPool, BossPool);
	FLKDungeonNode R2Battle = MakeBattle(TEXT("Node_R2Battle"), TEXT("Enc_Dyn_R2"), *NormalTemplate, ELKEncounterRank::Normal, HeroPool, BossPool);
	FLKDungeonNode R2Rest = MakeNode(TEXT("Node_R2Rest"), ELKDungeonNodeType::Rest, NAME_None);
	FLKDungeonNode R3Elite = MakeBattle(TEXT("Node_R3Elite"), TEXT("Enc_Dyn_R3"), *EliteTemplate, ELKEncounterRank::Elite, HeroPool, BossPool);
	FLKDungeonNode R3Rest = MakeNode(TEXT("Node_R3Rest"), ELKDungeonNodeType::Rest, NAME_None);
	FLKDungeonNode Boss = MakeBattle(TEXT("Node_Boss"), TEXT("Enc_Dyn_Boss"), *BossTemplate, ELKEncounterRank::Boss, HeroPool, BossPool);
	if (R1.NodeId.IsNone() || R2Battle.NodeId.IsNone() || R3Elite.NodeId.IsNone() || Boss.NodeId.IsNone())
	{ return false; }

	State.Nodes.Add(MakeNode(TEXT("Node_Start"), ELKDungeonNodeType::Event, NAME_None, { TEXT("Node_R1") }));
	R1.NextNodeIds = { TEXT("Node_R2A"), TEXT("Node_R2B") };
	State.Nodes.Add(MoveTemp(R1));

	// 第二排槽位名固定（Node_R2A/Node_R2B）：类型（战斗/休息）按种子分配。
	FLKDungeonNode Row2A = bSwapRow2 ? R2Rest : R2Battle;
	FLKDungeonNode Row2B = bSwapRow2 ? R2Battle : R2Rest;
	Row2A.NodeId = TEXT("Node_R2A");
	Row2B.NodeId = TEXT("Node_R2B");
	Row2A.NextNodeIds = { TEXT("Node_R3A"), TEXT("Node_R3B") };
	Row2B.NextNodeIds = { TEXT("Node_R3A"), TEXT("Node_R3B") };
	State.Nodes.Add(MoveTemp(Row2A));
	State.Nodes.Add(MoveTemp(Row2B));

	// 第三排：精英/休息。
	FLKDungeonNode Row3A = bSwapRow3 ? R3Rest : R3Elite;
	FLKDungeonNode Row3B = bSwapRow3 ? R3Elite : R3Rest;
	Row3A.NodeId = TEXT("Node_R3A");
	Row3B.NodeId = TEXT("Node_R3B");
	Row3A.NextNodeIds = { TEXT("Node_Boss") };
	Row3B.NextNodeIds = { TEXT("Node_Boss") };
	State.Nodes.Add(MoveTemp(Row3A));
	State.Nodes.Add(MoveTemp(Row3B));

	Boss.NextNodeIds = {};
	State.Nodes.Add(MoveTemp(Boss));
	return true;
}

ELKDungeonNodeType ULKRunSubsystem::NodeTypeForRank(ELKEncounterRank Rank)
{
	switch (Rank)
	{
	case ELKEncounterRank::Normal: return ELKDungeonNodeType::Battle;
	case ELKEncounterRank::Elite: return ELKDungeonNodeType::Elite;
	case ELKEncounterRank::Boss: return ELKDungeonNodeType::Boss;
	default: return ELKDungeonNodeType::Battle;
	}
}

bool ULKRunSubsystem::RestartRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed)
{
	TArray<FLKEncounterRow> Encounters;
	FString Error;
	if (!LKEncounterContent::BuildCatalog(nullptr, Encounters, Error)) { return false; }
	return RestartRun(Heroes, Cards, Seed, Encounters);
}

bool ULKRunSubsystem::RestartRun(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards, int32 Seed,
	const TArray<FLKEncounterRow>& Encounters)
{
	return RestartRun(MakeStandaloneRequest(Heroes, Cards, Seed, Encounters));
}

bool ULKRunSubsystem::BuildPendingBattle()
{
    const FLKDungeonNode* Node = FindNode(State.CurrentNodeId);
	const FLKEncounterRow* Encounter = Node ? LKEncounterContent::Find(State.Encounters, Node->EncounterId) : nullptr;
    if (!Node || Node->EncounterId.IsNone() || !Encounter)
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Run] 节点 %s 缺少遭遇快照 %s"),
			Node ? *Node->NodeId.ToString() : TEXT("None"), Node ? *Node->EncounterId.ToString() : TEXT("None"));
		return false;
	}

    State.PendingBattle = FLKBattleContext();
    State.PendingBattle.bExpedition = true;
    State.PendingBattle.RunId = State.RunId;
    State.PendingBattle.NodeId = Node->NodeId;
    State.PendingBattle.AttemptId = FGuid::NewGuid();
    State.PendingBattle.Seed = State.Seed + GetCurrentRoomIndex() * 1009;
    State.PendingBattle.EncounterId = Node->EncounterId;
	State.PendingBattle.Encounter = *Encounter;
    State.PendingBattle.PlayerHeroes = State.Heroes;
	State.PendingBattle.PlayerCards = State.Cards;
	State.PendingBattle.EnemyHeroIds = Encounter->EnemyHeroIds;
	for (FName CardId : Encounter->EnemyCards)
	{
		State.PendingBattle.EnemyCards.Add(CardId);
	}
	State.PendingBattle.bEnemyUsesCards = Encounter->bEnemyUsesCards;
	State.PendingBattle.BonusSnapshot = State.BonusSnapshot;
	State.PendingBattle.RegionId = State.RegionId;
    return true;
}

bool ULKRunSubsystem::BeginCurrentBattle(FLKBattleContext& OutContext)
{
    if (State.Phase != ELKRunPhase::EnteringBattle && State.Phase != ELKRunPhase::InBattle) { return false; }
    if (!State.PendingBattle.bExpedition || State.PendingBattle.RunId != State.RunId
        || State.PendingBattle.NodeId != State.CurrentNodeId || !State.PendingBattle.AttemptId.IsValid())
    { return false; }
    State.Phase = ELKRunPhase::InBattle;
    OutContext = State.PendingBattle;
    return true;
}

bool ULKRunSubsystem::SubmitBattleOutcome(const FLKBattleOutcome& Outcome)
{
    if (State.Phase != ELKRunPhase::InBattle || !Outcome.bFinalized
        || Outcome.RunId != State.RunId || Outcome.NodeId != State.CurrentNodeId
        || Outcome.AttemptId != State.PendingBattle.AttemptId
        || State.ProcessedAttemptIds.Contains(Outcome.AttemptId))
    { return false; }

    // 跨房继承只允许玩家英雄（BUG-015 规则）：
    // Outcome.EnemyHeroes 仅作 BattleHistory 展示，永不写入 State.Heroes。
    // 下方校验保证 PlayerHeroes 的 HeroId 集合必须与 State.Heroes 完全一致，
    // 任何敌方/未知英雄 ID 混入都会被拒绝，不会进入下一房。
    if (Outcome.EnemyHeroes.Num() > 0)
    {
        UE_LOG(LogLKBattle, Verbose, TEXT("[Run] 敌方英雄快照 %d 条仅记录历史，不参与跨房继承"), Outcome.EnemyHeroes.Num());
    }

    TMap<FName, FLKRunHeroState> Recovered;
    for (const FLKHeroBattleOutcome& HeroOutcome : Outcome.PlayerHeroes)
    {
        const FLKRunHeroState& Hero = HeroOutcome.RecoveredState;
        if (Hero.HeroId.IsNone() || Recovered.Contains(Hero.HeroId) || !FMath::IsFinite(Hero.Health)
            || !FMath::IsFinite(Hero.MaxHealth) || !FMath::IsFinite(Hero.BaseMaxHealth)
			|| Hero.MaxHealth <= 0.f || Hero.BaseMaxHealth <= 0.f || Hero.Health <= 0.f || Hero.Health > Hero.MaxHealth)
        { return false; }
		TSet<FName> Traits;
		for (FName TraitId : Hero.Traits)
		{
			if (TraitId.IsNone() || Traits.Contains(TraitId)) { return false; }
			Traits.Add(TraitId);
		}
        Recovered.Add(Hero.HeroId, Hero);
    }
    if (Recovered.Num() != State.Heroes.Num()) { return false; }
    for (const FLKRunHeroState& Previous : State.Heroes) { if (!Recovered.Contains(Previous.HeroId)) { return false; } }

    const FLKRunState Before = State;
    for (FLKRunHeroState& Hero : State.Heroes) { Hero = Recovered.FindChecked(Hero.HeroId); }
    State.ProcessedAttemptIds.Add(Outcome.AttemptId);
    State.BattleHistory.Add(Outcome);
    if (FLKDungeonNode* Node = FindNode(State.CurrentNodeId)) { Node->bResolved = true; }
    State.VisitedNodeIds.AddUnique(State.CurrentNodeId);
    // 新胜利 = 新的奖励窗口（允许再次发批；推进房间时同样重置）。
    State.bRewardOfferedForCurrentNode = false;

    if (Outcome.Stats.Winner != ELKTeam::Player)
    {
        State.Phase = ELKRunPhase::Failed;
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 房间 %s 失败，远征结束"), *State.CurrentNodeId.ToString());
    }
    else
    {
        // 本房胜利先加入远征钱包（终态全额归还余额）；奖励档来自本房遭遇快照。
        const FLKDungeonNode* Node = FindNode(State.CurrentNodeId);
        const FLKEncounterRow* Encounter = Node ? LKEncounterContent::Find(State.Encounters, Node->EncounterId) : nullptr;
        const int32 RoomGold = LKHomeContent::GoldForRewardTier(State.RewardRules, Encounter ? int32(Encounter->RewardTier) : 1);
        State.PendingGold = int32(FMath::Min<int64>(MAX_int32, int64(State.PendingGold) + RoomGold));
		State.WalletGold = int32(FMath::Min<int64>(MAX_int32, int64(State.WalletGold) + RoomGold));
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 房间 %s 胜利：暂存金币 +%d（本轮累计 %d）"),
            *State.CurrentNodeId.ToString(), RoomGold, State.PendingGold);

        if (Node && Node->NextNodeIds.IsEmpty())
        {
            State.Phase = ELKRunPhase::Completed;
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 首领战完成，远征通关"));
        }
        else
        {
            State.Phase = ELKRunPhase::ChoosingNode;
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 房间 %s 胜利，等待选择下一节点"), *State.CurrentNodeId.ToString());
        }
    }
    FreezeTerminalSettlement(); // 终态时冻结金币回执（幂等：同一轮只冻结一次）
    if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return false; }
    if (IsTerminal()) { ApplyPendingSettlementToProfile(); }
    return true;
}

bool ULKRunSubsystem::OfferRewardBatch(const TArray<FLKRunRewardOffer>& Offers)
{
    // 只在"胜利且可推进下一间"的窗口接受（SubmitBattleOutcome 已把阶段置为 ChoosingNode）。
    if (State.Phase != ELKRunPhase::ChoosingNode || Offers.IsEmpty() || Offers.Num() > 3) { return false; }
    if (HasPendingRewardChoice() || State.PendingRewardOffers.Num() > 0) { return false; }
    // 同一次胜利只发一批：领取/跳过不能再次发批（防刷奖励）。
    if (State.bRewardOfferedForCurrentNode) { return false; }

    // 每个批次独立 GUID：领取/跳过后清空，双击与旧回调无法重复消费。
    State.PendingRewardBatchId = FGuid::NewGuid();
    State.PendingRewardOffers = Offers;
    State.bRewardOfferedForCurrentNode = true;
    State.Phase = ELKRunPhase::ChoosingReward;
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 奖励批次 %s 就绪：%d 个候选"), *State.PendingRewardBatchId.ToString(), Offers.Num());
    return true;
}

FLKRunRewardOffer ULKRunSubsystem::GetPendingRewardOffer(int32 Index) const
{
    if (Index >= 0 && Index < State.PendingRewardOffers.Num()) { return State.PendingRewardOffers[Index]; }
    return FLKRunRewardOffer();
}

int32 ULKRunSubsystem::GetDeckCapacityUsed() const { return LKCardRules::Used(State.Cards); }

bool ULKRunSubsystem::ChooseReward(int32 Index, FName ReplacedCardId)
{
    TArray<FName> Replaced;
    if (!ReplacedCardId.IsNone()) { Replaced.Add(ReplacedCardId); }
    return ChooseRewardReplacingCards(Index, Replaced);
}

bool ULKRunSubsystem::ChooseRewardReplacingCards(int32 Index, const TArray<FName>& ReplacedCardIds)
{
    if (NeedsDeckReduction()) { return false; }
    if (State.Phase != ELKRunPhase::ChoosingReward || Index < 0 || Index >= State.PendingRewardOffers.Num()
        || !State.PendingRewardBatchId.IsValid()
        || State.ClaimedRewardBatchIds.Contains(State.PendingRewardBatchId))
    { return false; }

    // 应用奖励（先生成"待提交"变化，任何一步失败都不落盘）。
    const FLKRunRewardOffer Offer = State.PendingRewardOffers[Index];
    if (Offer.CardId.IsNone()) { return false; }
    const FLKRunState Before = State;

    if (Offer.Kind == ELKRunRewardKind::UpgradeCard)
    {
        if (!ReplacedCardIds.IsEmpty() || Offer.LevelBefore < 0 || Offer.LevelAfter != Offer.LevelBefore + 1) { return false; }
        FLKRunCardState* Card = State.Cards.FindByPredicate(
            [CardId = Offer.CardId](const FLKRunCardState& Item) { return Item.CardId == CardId; });
        if (!Card) { return false; }
        // 校验与生成时一致（UI 等待期间状态不应变化；防御并发/旧回调）
        if (Card->UpgradeLevel != Offer.LevelBefore) { return false; }
        Card->UpgradeLevel = Offer.LevelAfter;
    }
    else if (Offer.Kind == ELKRunRewardKind::AddCard)
    {
        // 加牌候选只在"未持有"时生成；此处复核，防止重复副本入库。
        if (State.Cards.ContainsByPredicate([CardId = Offer.CardId](const FLKRunCardState& Item)
            { return Item.CardId == CardId; })) { return false; }
        FLKRunCardState NewCard;
        NewCard.CardId = Offer.CardId;
        TArray<FLKRunCardState> Candidate = State.Cards;
        TSet<FName> Removed;
        int32 InsertAt = Candidate.Num();
        for (FName Id : ReplacedCardIds)
        {
            if (Id.IsNone() || Removed.Contains(Id)) { return false; }
            const int32 OldIndex = Candidate.IndexOfByPredicate([Id](const FLKRunCardState& Item) { return Item.CardId == Id; });
            if (OldIndex == INDEX_NONE) { return false; }
            Removed.Add(Id); InsertAt = FMath::Min(InsertAt, OldIndex);
            Candidate.RemoveAt(OldIndex);
        }
        Candidate.Insert(NewCard, FMath::Min(InsertAt, Candidate.Num()));
        if (Candidate.Num() < LKCardRules::MinimumCards || LKCardRules::Used(Candidate) > LKCardRules::Capacity) { return false; }
        State.Cards = MoveTemp(Candidate);
    }
    else { return false; }

    State.ClaimedRewardBatchIds.Add(State.PendingRewardBatchId);
    State.PendingRewardBatchId = FGuid();
    State.PendingRewardOffers.Reset();
    State.Phase = ELKRunPhase::ChoosingNode;
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 领取奖励 #%d：%s %s"), Index,
        Offer.Kind == ELKRunRewardKind::UpgradeCard ? TEXT("升级") : TEXT("加牌"), *Offer.CardId.ToString());
    if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return false; }
    return true;
}

bool ULKRunSubsystem::DiscardExcessCard(FName CardId)
{
    if (!NeedsDeckReduction() || State.Cards.Num() <= LKCardRules::MinimumCards) { return false; }
    const ALKBattleGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ALKBattleGameMode>() : nullptr;
    if (GM && GM->GetPhase() == ELKGamePhase::Battle) { return false; }
    const int32 Index = State.Cards.IndexOfByPredicate([CardId](const FLKRunCardState& Card) { return Card.CardId == CardId; });
    if (Index == INDEX_NONE) { return false; }
    const FLKRunState Before = State;
    State.Cards.RemoveAt(Index);
    if (State.PendingBattle.bExpedition) { State.PendingBattle.PlayerCards = State.Cards; }
    if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return false; }
    return true;
}

bool ULKRunSubsystem::SkipReward()
{
    if (NeedsDeckReduction()) { return false; }
    if (State.Phase != ELKRunPhase::ChoosingReward || !State.PendingRewardBatchId.IsValid()
        || State.ClaimedRewardBatchIds.Contains(State.PendingRewardBatchId))
    { return false; }

    const FLKRunState Before = State;
    State.ClaimedRewardBatchIds.Add(State.PendingRewardBatchId);
    State.PendingRewardBatchId = FGuid();
    State.PendingRewardOffers.Reset();
    State.Phase = ELKRunPhase::ChoosingNode;
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 跳过奖励批次（不可回头补领）"));
    if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return false; }
    return true;
}

bool ULKRunSubsystem::CanEditPermanentState() const
{
	return State.Phase == ELKRunPhase::ChoosingNode || State.Phase == ELKRunPhase::ChoosingReward
		|| State.Phase == ELKRunPhase::ResolvingNode;
}

int32 ULKRunSubsystem::GetNextNodeCount() const
{
	const FLKDungeonNode* Node = FindNode(State.CurrentNodeId);
	return Node ? Node->NextNodeIds.Num() : 0;
}

TArray<FName> ULKRunSubsystem::GetNextNodeIds() const
{
	const FLKDungeonNode* Node = FindNode(State.CurrentNodeId);
	return Node ? Node->NextNodeIds : TArray<FName>();
}

FLKDungeonNode ULKRunSubsystem::GetNode(FName NodeId) const
{
	const FLKDungeonNode* Node = FindNode(NodeId);
	return Node ? *Node : FLKDungeonNode();
}

ELKNodeSelectionResult ULKRunSubsystem::SelectNode(FName NodeId)
{
    if (NeedsDeckReduction()) { return ELKNodeSelectionResult::Rejected; }
	if (State.Phase != ELKRunPhase::ChoosingNode) { return ELKNodeSelectionResult::Rejected; }
	const FLKDungeonNode* Current = FindNode(State.CurrentNodeId);
	const FLKDungeonNode* Next = FindNode(NodeId);
	if (!Current || !Next || Next->bResolved || !Current->NextNodeIds.Contains(NodeId))
	{
		UE_LOG(LogLKBattle, Log, TEXT("[Run] 拒绝选择 %s：非下一排/已结算/阶段不符"), *NodeId.ToString());
		return ELKNodeSelectionResult::Rejected;
	}
	const FLKRunState Before = State;
	if (State.WorldMapVersion > 0 && (Next->Type == ELKDungeonNodeType::Rest || Next->Type == ELKDungeonNodeType::Market))
	{
		State.CurrentNodeId = NodeId; State.RegionId = Next->RegionId;
		State.Phase = ELKRunPhase::ResolvingNode; State.PendingBattle = FLKBattleContext();
		if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return ELKNodeSelectionResult::Rejected; }
		OnServiceNodeEntered.Broadcast(NodeId, Next->Type);
		return ELKNodeSelectionResult::ServiceEntered;
	}
	if (!LKWorldMapContent::IsCombat(Next->Type) && Next->Type != ELKDungeonNodeType::Rest) { return ELKNodeSelectionResult::Rejected; }

	if (Next->Type == ELKDungeonNodeType::Rest)
	{
		// D4 休息：唯一选项"回血 30% 最大生命"（封顶）；结算后停留在选择阶段显示下一排。
		for (FLKRunHeroState& Hero : State.Heroes)
		{
			Hero.Health = LKRunRules::RecoveredHealth(Hero.Health, Hero.MaxHealth, 0.3f);
		}
	}
	else
	{
		// 战斗/精英/首领：进入该房（新奖励窗口）。
		State.Phase = ELKRunPhase::EnteringBattle;
		State.bRewardOfferedForCurrentNode = false;
	}

	FLKDungeonNode* MutableNext = FindNode(NodeId);
	if (MutableNext) { MutableNext->bResolved = State.WorldMapVersion == 0 || Next->Type == ELKDungeonNodeType::Rest; }
	State.VisitedNodeIds.AddUnique(NodeId);
	State.CurrentNodeId = NodeId;
	if (!Next->RegionId.IsNone()) { State.RegionId = Next->RegionId; }

	if (Next->Type == ELKDungeonNodeType::Rest)
	{
		UE_LOG(LogLKBattle, Log, TEXT("[Run] 休息：全员恢复 30%% 最大生命（封顶），可继续选择下一排"));
		if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return ELKNodeSelectionResult::Rejected; }
		return ELKNodeSelectionResult::RestResolved;
	}

	if (!BuildPendingBattle())
	{
		State = Before;
		return ELKNodeSelectionResult::Rejected;
	}
	UE_LOG(LogLKBattle, Log, TEXT("[Run] 选择 %s（第 %d 战），进入战斗 %s"),
		*NodeId.ToString(), GetCurrentRoomIndex(), *Next->EncounterId.ToString());
	if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return ELKNodeSelectionResult::Rejected; }
	return ELKNodeSelectionResult::BattleEntered;
}

bool ULKRunSubsystem::ResolveServiceNode(FName ActionId)
{
	if (!HasServiceNode()) { return false; }
	FLKDungeonNode* Node = FindNode(State.CurrentNodeId);
	if (!Node || Node->bResolved) { return false; }
	const FLKRunState Before = State;
	if (Node->Type == ELKDungeonNodeType::Rest && ActionId == TEXT("RestHeal"))
	{
		for (FLKRunHeroState& Hero : State.Heroes) { Hero.Health = LKRunRules::RecoveredHealth(Hero.Health, Hero.MaxHealth, .3f); }
	}
	else if (!(Node->Type == ELKDungeonNodeType::Market && ActionId == TEXT("LeaveMarket"))) { return false; }
	Node->bResolved = true; State.VisitedNodeIds.AddUnique(Node->NodeId);
	State.Phase = ELKRunPhase::ChoosingNode;
	if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return false; }
	return true;
}

bool ULKRunSubsystem::ChangeWalletGold(int32 Delta, FGuid TransactionId)
{
	if (!TransactionId.IsValid() || (State.Phase != ELKRunPhase::ResolvingNode && State.Phase != ELKRunPhase::ChoosingNode)) { return false; }
	if (State.ProcessedWalletTransactions.Contains(TransactionId)) { return true; }
	const int64 Balance = int64(State.WalletGold) + Delta;
	if (Balance < 0 || Balance > MAX_int32) { return false; }
	const FLKRunState Before = State;
	State.WalletGold = int32(Balance); State.ProcessedWalletTransactions.Add(TransactionId);
	if (bAutoSaveEnabled && !SaveExpedition()) { State = Before; return false; }
	return true;
}

bool ULKRunSubsystem::ReconcileDepartureFunding()
{
	ULKProfileSubsystem* Profile = ProfileSubsystemOverrideForTest.Get();
	if (!Profile && GetGameInstance()) { Profile = GetGameInstance()->GetSubsystem<ULKProfileSubsystem>(); }
	if (!Profile || !Profile->HasProfile()) { return false; }
	if (!Profile->ReconcileDepartureGold(State.RunId)) { return false; }
	if (State.PendingSettlement.SettlementId.IsValid() && Profile->HasProcessedSettlement(State.PendingSettlement.SettlementId)) { return true; }
	if (State.StartingGold > 0 && !State.PendingSettlement.bProfileApplied)
	{
		const FLKProfileState Home = Profile->GetProfile();
		// A/B 回退可能回到扣款前；按同一 RunId 幂等补齐，不产生免费本金。
		return Home.FundedRunId == State.RunId ? Home.FundedRunGold == State.StartingGold : Profile->ReserveDepartureGold(State.RunId, State.StartingGold);
	}
	return true;
}

bool ULKRunSubsystem::AdvanceToNextBattle()
{
	// 兼容 D1~D3 的"唯一下一节点"调用：自动选择下一排第一个战斗节点（休息节点跳过）。
	if (State.Phase != ELKRunPhase::ChoosingNode) { return false; }
	const FLKDungeonNode* Current = FindNode(State.CurrentNodeId);
	if (!Current) { return false; }
	for (FName NextId : Current->NextNodeIds)
	{
		const FLKDungeonNode* Next = FindNode(NextId);
		if (Next && LKWorldMapContent::IsCombat(Next->Type))
		{
			return SelectNode(NextId) == ELKNodeSelectionResult::BattleEntered;
		}
	}
	return false;
}

bool ULKRunSubsystem::SetHeroBaseMaxHealth(FName HeroId, float NewBaseMaxHealth, bool bPreserveHealthRatio)
{
	if (!CanEditPermanentState() || HeroId.IsNone() || !FMath::IsFinite(NewBaseMaxHealth) || NewBaseMaxHealth <= 0.f)
	{ return false; }
	FLKRunHeroState* Hero = State.Heroes.FindByPredicate([HeroId](const FLKRunHeroState& Item) { return Item.HeroId == HeroId; });
	if (!Hero) { return false; }
	const float OldBase = Hero->BaseMaxHealth > 0.f ? Hero->BaseMaxHealth : Hero->MaxHealth;
	const float OldMaximum = FMath::Max(1.f, Hero->MaxHealth);
	const float TraitScale = FMath::Max(0.01f, OldMaximum / FMath::Max(1.f, OldBase));
	const float NewMaximum = FMath::Max(1.f, NewBaseMaxHealth * TraitScale);
	Hero->Health = bPreserveHealthRatio
		? FMath::Clamp(Hero->Health / OldMaximum * NewMaximum, 0.f, NewMaximum)
		: FMath::Clamp(Hero->Health, 0.f, NewMaximum);
	Hero->BaseMaxHealth = NewBaseMaxHealth;
	Hero->MaxHealth = NewMaximum;
	return true;
}

bool ULKRunSubsystem::AddHeroTrait(FName HeroId, FName TraitId)
{
	if (!CanEditPermanentState() || HeroId.IsNone() || TraitId.IsNone()) { return false; }
	const bool bKnownTrait = IsBuiltInTrait(TraitId) || State.Heroes.ContainsByPredicate(
		[TraitId](const FLKRunHeroState& Item) { return Item.Traits.Contains(TraitId); });
	if (!bKnownTrait) { return false; }
	FLKRunHeroState* Hero = State.Heroes.FindByPredicate([HeroId](const FLKRunHeroState& Item) { return Item.HeroId == HeroId; });
	if (!Hero || Hero->Traits.Contains(TraitId)) { return false; }
	Hero->Traits.Add(TraitId);
	Hero->Traits.Sort(FNameLexicalLess());
	return true;
}

bool ULKRunSubsystem::RemoveHeroTrait(FName HeroId, FName TraitId)
{
	if (!CanEditPermanentState() || HeroId.IsNone() || TraitId.IsNone()) { return false; }
	FLKRunHeroState* Hero = State.Heroes.FindByPredicate([HeroId](const FLKRunHeroState& Item) { return Item.HeroId == HeroId; });
	return Hero && Hero->Traits.Remove(TraitId) == 1;
}

void ULKRunSubsystem::AbandonRun()
{
    if (HasRun() && !IsTerminal()) { State.Phase = ELKRunPhase::Abandoned; State.PendingBattle = FLKBattleContext(); }
}

void ULKRunSubsystem::FreezeTerminalSettlement()
{
    if (!IsTerminal() || State.PendingSettlement.SettlementId.IsValid()) { return; }

    FLKSettlementReceipt Receipt;
    Receipt.SettlementId = FGuid::NewGuid();
    Receipt.RunId = State.RunId;
    Receipt.ProfileId = State.ProfileId;
    Receipt.TerminalPhase = State.Phase;
    Receipt.bEligibleForHomeReward = State.bHomeRewardEligible;

    int32 Amount = State.WalletGold;
    if (State.Phase == ELKRunPhase::Completed)
    {
        Amount = int32(FMath::Min<int64>(MAX_int32, int64(Amount) + FMath::Max(0, State.RewardRules.RunCompletedBonus)));
    }
    if (!State.bHomeRewardEligible) { Amount = 0; }
    Receipt.GoldAmount = FMath::Max(0, Amount);
    State.PendingSettlement = Receipt;

    UE_LOG(LogLKBattle, Log, TEXT("[Run] 终态结算冻结：%s（暂存 %d -> 入账 %d 金币，结算 %s）"),
        State.Phase == ELKRunPhase::Completed ? TEXT("通关") : (State.Phase == ELKRunPhase::Failed ? TEXT("失败") : TEXT("放弃")),
        State.PendingGold, Receipt.GoldAmount, *Receipt.SettlementId.ToString());
}

bool ULKRunSubsystem::AbandonCurrentRun()
{
    if (!HasRunInProgress()) { return false; }
    const FLKRunState PreviousState = State;
    const FName NodeId = State.CurrentNodeId;
    State.Phase = ELKRunPhase::Abandoned;
    State.PendingBattle = FLKBattleContext();
    FreezeTerminalSettlement();
    if (bAutoSaveEnabled && !SaveExpedition())
    {
        State = PreviousState;
        return false;
    }
    ApplyPendingSettlementToProfile();
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 明确放弃远征（节点 %s，剩余金币全额带回）"), *NodeId.ToString());
    return true;
}

bool ULKRunSubsystem::MarkPendingSettlementApplied()
{
    if (!State.PendingSettlement.SettlementId.IsValid()) { return false; }
    if (State.PendingSettlement.bProfileApplied) { return true; }
    State.PendingSettlement.bProfileApplied = true;
    if (!bAutoSaveEnabled) { return true; }
    if (SaveExpedition()) { return true; }
    State.PendingSettlement.bProfileApplied = false;
    return false;
}

bool ULKRunSubsystem::ApplyPendingSettlementToProfile()
{
    if (!IsTerminal() || !State.PendingSettlement.SettlementId.IsValid()) { return false; }
    if (!State.PendingSettlement.bEligibleForHomeReward) { return false; }
    if (State.PendingSettlement.bProfileApplied) { return true; }

    const UGameInstance* Instance = GetGameInstance();
    ULKProfileSubsystem* Profile = ProfileSubsystemOverrideForTest.Get();
    if (!Profile && Instance) { Profile = Instance->GetSubsystem<ULKProfileSubsystem>(); }
    if (!Profile || !Profile->EnsureProfile() || !Profile->HasProfile()) { return false; }

    const ELKSettlementResult Result = Profile->ApplySettlement(State.PendingSettlement);
    if (Result == ELKSettlementResult::Success || Result == ELKSettlementResult::AlreadyApplied)
    {
        return MarkPendingSettlementApplied();
    }
    UE_LOG(LogLKBattle, Warning, TEXT("[Run] 金币入账未完成（结果 %d）：保留待交接状态，可重试"), int32(Result));
    return false;
}

int32 ULKRunSubsystem::RestoreHeroIdentityTraits(const TMap<FName, TArray<FName>>& IdentityTraits)
{
    if (!HasRun() || IdentityTraits.IsEmpty()) { return 0; }
    int32 Restored = 0;
    for (FLKRunHeroState& Hero : State.Heroes)
    {
        const TArray<FName>* Identity = IdentityTraits.Find(Hero.HeroId);
        if (!Identity) { continue; }
        int32 AddedHere = 0;
        for (FName TraitId : *Identity)
        {
            if (TraitId.IsNone() || Hero.Traits.Contains(TraitId)) { continue; }
            Hero.Traits.Add(TraitId);
            ++AddedHere;
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 身份特性补全：%s + %s（旧档快照缺少代码身份特性）"),
                *Hero.HeroId.ToString(), *TraitId.ToString());
        }
        if (AddedHere > 0) { Hero.Traits.Sort(FNameLexicalLess()); Restored += AddedHere; }
    }
    if (Restored > 0)
    {
        // 关键：待开战上下文是英雄快照的副本，补全后必须同步，
        // 否则部署时仍按旧副本应用特性（AttemptId/种子/遭遇保持不变，恢复语义不变）。
        if (State.PendingBattle.bExpedition && State.PendingBattle.RunId == State.RunId)
        {
            State.PendingBattle.PlayerHeroes = State.Heroes;
        }
        AutoSave();
    }
    return Restored;
}

bool ULKRunSubsystem::HasRunInProgress() const
{
    return HasRun() && State.Phase != ELKRunPhase::Inactive && !IsTerminal();
}

bool ULKRunSubsystem::IsTerminal() const
{
    return State.Phase == ELKRunPhase::Completed || State.Phase == ELKRunPhase::Failed || State.Phase == ELKRunPhase::Abandoned;
}

int32 ULKRunSubsystem::GetTotalRoomCount() const
{
    if (State.WorldMapVersion == 0) { return 4; }
    if (IsTerminal()) { return State.BattleHistory.Num(); }
    // 迭代松弛，避免对存档数组的排列顺序作假设。图加载时已验证无环。
    TMap<FName, int32> Remaining;
    for (int32 Pass = 0; Pass < State.Nodes.Num(); ++Pass)
    {
        bool Changed = false;
        for (const FLKDungeonNode& Node : State.Nodes)
        {
            int32 Next = 0;
            for (FName Id : Node.NextNodeIds) { Next = FMath::Max(Next, Remaining.FindRef(Id)); }
            const int32 Count = Next + (!Node.bResolved && LKWorldMapContent::IsCombat(Node.Type) ? 1 : 0);
            if (Remaining.FindRef(Node.NodeId) != Count) { Remaining.Add(Node.NodeId, Count); Changed = true; }
        }
        if (!Changed) { break; }
    }
    return State.BattleHistory.Num() + Remaining.FindRef(State.CurrentNodeId);
}

int32 ULKRunSubsystem::GetCurrentRoomIndex() const
{
	// 语义：当前正在进行的战斗序号 = 已完成战斗数 + 1（休息节点不占战斗序号）。
	return State.BattleHistory.Num() + 1;
}

FLKDungeonNode* ULKRunSubsystem::FindNode(FName NodeId)
{
    return State.Nodes.FindByPredicate([NodeId](const FLKDungeonNode& Node) { return Node.NodeId == NodeId; });
}

const FLKDungeonNode* ULKRunSubsystem::FindNode(FName NodeId) const
{
    return State.Nodes.FindByPredicate([NodeId](const FLKDungeonNode& Node) { return Node.NodeId == NodeId; });
}

// ---------- D5 安全节点存档 ----------
FString ULKRunSubsystem::GetRunSlotName()
{
    return GRunSlotName;
}

void ULKRunSubsystem::SetRunSlotNameOverrideForTest(const FString& InSlotName)
{
    GRunSlotName = InSlotName.IsEmpty() ? FString(TEXT("LittleKing_Run")) : InSlotName;
}

void ULKRunSubsystem::SetProfileSubsystemOverrideForTest(ULKProfileSubsystem* InProfile)
{
    ProfileSubsystemOverrideForTest = InProfile;
}

void ULKRunSubsystem::ConfigureStorage(const FString& SlotName)
{
    StorageSlot = SlotName;
    State = FLKRunState();
}

FString ULKRunSubsystem::GetStorageSlot() const
{
    return StorageSlot.IsEmpty() ? GetRunSlotName() : StorageSlot;
}

bool ULKRunSubsystem::ValidateStoredRun(const FLKRunState& State, FString& OutError)
{
    OutError.Reset();
    // 版本：结构升级到 5（H3/H4 家园字段）；高于当前一律拒绝（未知版本不静默当新档覆盖）。
    if (State.SchemaVersion < 1 || State.SchemaVersion > CurrentSchemaVersion)
    {
        OutError = FString::Printf(TEXT("存档结构版本 %d 不受支持（当前支持 1~%d）"), State.SchemaVersion, CurrentSchemaVersion);
        return false;
    }
    if (!State.RunId.IsValid()) { OutError = TEXT("RunId 无效"); return false; }
    if (State.Heroes.IsEmpty()) { OutError = TEXT("英雄队伍为空"); return false; }
    for (const FLKRunHeroState& Hero : State.Heroes)
    {
        if (Hero.HeroId.IsNone() || !FMath::IsFinite(Hero.Health) || !FMath::IsFinite(Hero.MaxHealth)
            || !FMath::IsFinite(Hero.BaseMaxHealth) || Hero.MaxHealth <= 0.f
            || Hero.Health < 0.f || Hero.Health > Hero.MaxHealth || Hero.BaseMaxHealth < 0.f)
        {
            OutError = FString::Printf(TEXT("英雄 %s 数值非法"), *Hero.HeroId.ToString());
            return false;
        }
    }
    if (State.Cards.Num() < 5) { OutError = TEXT("牌组不足五种"); return false; }
    TSet<FName> CardIds;
    for (const FLKRunCardState& Card : State.Cards)
    {
        if (Card.CardId.IsNone() || Card.UpgradeLevel < 0 || CardIds.Contains(Card.CardId))
        {
            OutError = FString::Printf(TEXT("牌组含空/重复/非法卡 %s"), *Card.CardId.ToString());
            return false;
        }
        CardIds.Add(Card.CardId);
    }
    if (State.Nodes.IsEmpty()
        || !State.Nodes.ContainsByPredicate([&State](const FLKDungeonNode& Node) { return Node.NodeId == State.CurrentNodeId; }))
    {
        OutError = TEXT("节点图为空或当前节点不存在");
        return false;
    }
    if (State.Encounters.IsEmpty()) { OutError = TEXT("遭遇快照为空"); return false; }
    // H3/H4：新档必须带区域与加成快照（旧档由 MigrateStoredRun 补默认值后再校验）。
    if (State.SchemaVersion >= 5)
    {
        if (State.RegionId.IsNone()) { OutError = TEXT("缺少 RegionId"); return false; }
        if (!FMath::IsFinite(State.BonusSnapshot.HeroRecoveryPercent)
            || State.BonusSnapshot.HeroRecoveryPercent < 0.f || State.BonusSnapshot.HeroRecoveryPercent > 1.f)
        { OutError = TEXT("战后恢复比例非法"); return false; }
        if (!FMath::IsFinite(State.BonusSnapshot.PlayerSilverPerSecond) || State.BonusSnapshot.PlayerSilverPerSecond <= 0.f
            || !FMath::IsFinite(State.BonusSnapshot.PlayerSilverCap) || State.BonusSnapshot.PlayerSilverCap <= 0.f)
        { OutError = TEXT("家园银币加成非法"); return false; }
        if (State.PendingGold < 0) { OutError = TEXT("暂存金币为负"); return false; }
        if (State.PendingSettlement.SettlementId.IsValid() && State.PendingSettlement.GoldAmount < 0)
        { OutError = TEXT("结算金额为负"); return false; }
    }
    if (State.SchemaVersion >= 6)
    {
        if (State.StartingGold < 0 || State.WalletGold < 0) { OutError = TEXT("远征钱包金额无效"); return false; }
        if (State.WorldMapVersion > 0 && !LKWorldMapContent::Validate(State, OutError)) { return false; }
        if (State.Phase == ELKRunPhase::ResolvingNode)
        {
            const FLKDungeonNode* Current = State.Nodes.FindByPredicate([&State](const FLKDungeonNode& N) { return N.NodeId == State.CurrentNodeId; });
            if (!Current || Current->bResolved || (Current->Type != ELKDungeonNodeType::Rest && Current->Type != ELKDungeonNodeType::Market))
            { OutError = TEXT("非战斗节点恢复点无效"); return false; }
        }
    }
    // 战斗阶段要求待开战上下文完整可解析（恢复点语义）。
    if (State.Phase == ELKRunPhase::EnteringBattle || State.Phase == ELKRunPhase::InBattle)
    {
        const FLKBattleContext& Pending = State.PendingBattle;
        if (!Pending.bExpedition || Pending.RunId != State.RunId || Pending.NodeId != State.CurrentNodeId
            || !Pending.AttemptId.IsValid() || Pending.PlayerHeroes.IsEmpty()
            || !LKEncounterContent::Find(State.Encounters, Pending.EncounterId))
        {
            OutError = TEXT("战斗恢复点上下文不完整或遭遇不可解析");
            return false;
        }
    }
    return true;
}

bool ULKRunSubsystem::SaveExpeditionToSlot(const FString& SlotName) const
{
    ULKRunSaveGame* Save = NewObject<ULKRunSaveGame>();
    Save->SaveVersion = 1;
    Save->RunState = State;
    Save->SavedAtUtc = FDateTime::UtcNow();
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))
    {
        UE_LOG(LogLKBattle, Warning, TEXT("[Run] 存档写入失败（槽 %s）；内存状态保留，可继续当前会话"), *SlotName);
        return false;
    }
    // 金币转入/节点消费只有在安全点确实落盘后才生效。
    const ULKRunSaveGame* ReadBack = Cast<ULKRunSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    auto SameCards = [](const TArray<FLKRunCardState>& A, const TArray<FLKRunCardState>& B)
    {
        if (A.Num() != B.Num()) { return false; }
        for (int32 Index = 0; Index < A.Num(); ++Index)
        { if (A[Index].CardId != B[Index].CardId || A[Index].UpgradeLevel != B[Index].UpgradeLevel) { return false; } }
        return true;
    };
    if (!ReadBack || ReadBack->RunState.RunId != State.RunId || ReadBack->RunState.Phase != State.Phase
        || ReadBack->RunState.CurrentNodeId != State.CurrentNodeId || ReadBack->RunState.WalletGold != State.WalletGold
        || ReadBack->RunState.ProcessedWalletTransactions != State.ProcessedWalletTransactions
        || !SameCards(ReadBack->RunState.Cards, State.Cards)
        || !SameCards(ReadBack->RunState.PendingBattle.PlayerCards, State.PendingBattle.PlayerCards)
        || ReadBack->RunState.PendingRewardBatchId != State.PendingRewardBatchId
        || ReadBack->RunState.ClaimedRewardBatchIds != State.ClaimedRewardBatchIds
        || ReadBack->RunState.PendingSettlement.bProfileApplied != State.PendingSettlement.bProfileApplied)
    { UE_LOG(LogLKBattle, Warning, TEXT("[Run] 安全点读回校验失败（槽 %s）"), *SlotName); return false; }
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 安全节点已保存（槽 %s，Phase=%d，节点 %s）"),
        *SlotName, int32(State.Phase), *State.CurrentNodeId.ToString());
    return true;
}

bool ULKRunSubsystem::LoadExpeditionFromSlot(const FString& SlotName, bool bAllowExistingRun)
{
    if (HasRun() && !bAllowExistingRun)
    {
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 内存已有远征，拒绝从存档覆盖"));
        return false;
    }
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) { return false; }
    ULKRunSaveGame* Save = Cast<ULKRunSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Save)
    {
        UE_LOG(LogLKBattle, Error, TEXT("[Run] 存档读取失败（槽 %s），不覆盖当前状态"), *SlotName);
        return false;
    }
    if (Save->SaveVersion < 1 || Save->SaveVersion > 1)
    {
        UE_LOG(LogLKBattle, Warning, TEXT("[Run] 存档文件版本 %d 不受支持，拒绝载入（不静默覆盖）"), Save->SaveVersion);
        return false;
    }
    FString Error;
    if (!ValidateStoredRun(Save->RunState, Error))
    {
        UE_LOG(LogLKBattle, Warning, TEXT("[Run] 存档校验失败：%s（保留原档，不静默覆盖）"), *Error);
        return false;
    }
    State = Save->RunState;
    if (State.SchemaVersion < CurrentSchemaVersion)
    {
        // H5：v0.6 及更早的远征档迁移到 Schema 5（补默认区域/加成/战备，旧轮不参与家园金币结算）。
        MigrateStoredRun(State);
    }
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 已从存档恢复远征 %s（Phase=%d，第 %d 战，Schema=%d，区域 %s）"),
        *State.RunId.ToString(), int32(State.Phase), GetCurrentRoomIndex(), State.SchemaVersion, *State.RegionId.ToString());
    return true;
}

void ULKRunSubsystem::MigrateStoredRun(FLKRunState& InOutState) const
{
    const int32 PreviousSchema = InOutState.SchemaVersion;
    if (PreviousSchema >= 6)
    {
        // No wallet/route rewrite. Excess legacy cards await an explicit player choice in the reward UI.
        InOutState.SchemaVersion = CurrentSchemaVersion;
        return;
    }
    if (PreviousSchema == 5)
    {
        // 已有路线/随机内容/冻结回执原样保留；正在进行的旧轮以后全额带回。
        InOutState.WalletGold = InOutState.PendingGold; InOutState.StartingGold = 0;
        InOutState.RewardRules.FailureKeepPercent = 1.f; InOutState.RewardRules.AbandonKeepPercent = 1.f;
        InOutState.SchemaVersion = CurrentSchemaVersion;
        return;
    }
    if (InOutState.RegionId.IsNone()) { InOutState.RegionId = LKHomeContent::DefaultRegionId(); }
    if (InOutState.InitialLoadout.HeroIds.IsEmpty())
    {
        for (const FLKRunHeroState& Hero : InOutState.Heroes) { InOutState.InitialLoadout.HeroIds.Add(Hero.HeroId); }
    }
    if (InOutState.InitialLoadout.CardIds.IsEmpty())
    {
        for (const FLKRunCardState& Card : InOutState.Cards) { InOutState.InitialLoadout.CardIds.Add(Card.CardId); }
    }
    // 旧档没有记录的历史基数不能宣称可精确还原：按 v0.6 代码基线（1/3、上限 5、恢复 40%）冻结一次。
    if (!FMath::IsFinite(InOutState.BonusSnapshot.PlayerSilverPerSecond) || InOutState.BonusSnapshot.PlayerSilverPerSecond <= 0.f
        || InOutState.BonusSnapshot.PlayerSilverPerSecond > 1.f)
    { InOutState.BonusSnapshot.PlayerSilverPerSecond = 1.f / 3.f; }
    if (!FMath::IsFinite(InOutState.BonusSnapshot.PlayerSilverCap) || InOutState.BonusSnapshot.PlayerSilverCap <= 0.f
        || InOutState.BonusSnapshot.PlayerSilverCap > 100.f)
    { InOutState.BonusSnapshot.PlayerSilverCap = 5.f; }
    if (!FMath::IsFinite(InOutState.BonusSnapshot.HeroRecoveryPercent)
        || InOutState.BonusSnapshot.HeroRecoveryPercent <= 0.f || InOutState.BonusSnapshot.HeroRecoveryPercent > 1.f)
    { InOutState.BonusSnapshot.HeroRecoveryPercent = 0.4f; }
    InOutState.BonusSnapshot.RegionId = InOutState.RegionId;
    if (InOutState.BonusSnapshot.StatueLevel <= 0) { InOutState.BonusSnapshot.StatueLevel = 1; }
    if (InOutState.BonusSnapshot.TreasuryLevel <= 0) { InOutState.BonusSnapshot.TreasuryLevel = 1; }
    InOutState.RewardRules = LKHomeContent::DefaultRewardRules();

    // 旧轮标记为不参与家园金币结算，避免追算与重复赠送。
    InOutState.bHomeRewardEligible = false;
    InOutState.PendingGold = 0;
    InOutState.WalletGold = 0; InOutState.StartingGold = 0;
    InOutState.PendingSettlement = FLKSettlementReceipt();

    // 关联当前永久档（若存在），让后续入账身份校验成立。
    if (!InOutState.ProfileId.IsValid())
    {
        if (ProfileIdOverrideForTest.IsValid())
        {
            InOutState.ProfileId = ProfileIdOverrideForTest;
        }
        else
        {
            const UGameInstance* Instance = GetGameInstance();
            const ULKProfileSubsystem* Profile = Instance ? Instance->GetSubsystem<ULKProfileSubsystem>() : nullptr;
            if (Profile && Profile->HasProfile()) { InOutState.ProfileId = Profile->GetProfile().ProfileId; }
        }
    }

    InOutState.SchemaVersion = CurrentSchemaVersion;
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 旧档迁移：Schema %d -> %d（区域 %s，加成按 v0.6 基线冻结，旧轮不结算金币）"),
        PreviousSchema, InOutState.SchemaVersion, *InOutState.RegionId.ToString());
}

bool ULKRunSubsystem::SaveExpedition()
{
    return SaveExpeditionToSlot(GetStorageSlot());
}

bool ULKRunSubsystem::LoadExpedition()
{
    return LoadExpeditionFromSlot(GetStorageSlot());
}

bool ULKRunSubsystem::HasSavedExpedition() const
{
    return UGameplayStatics::DoesSaveGameExist(GetStorageSlot(), 0);
}

void ULKRunSubsystem::ClearSavedExpedition()
{
    if (HasSavedExpedition())
    {
        UGameplayStatics::DeleteGameInSlot(GetStorageSlot(), 0);
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 已清除远征存档"));
    }
}

void ULKRunSubsystem::AutoSave()
{
    if (!bAutoSaveEnabled || !HasRun()) { return; }
    SaveExpeditionToSlot(GetStorageSlot());
}
