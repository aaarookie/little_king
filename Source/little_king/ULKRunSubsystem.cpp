#include "ULKRunSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "LKEncounterContent.h"
#include "LKLog.h"
#include "ULKRunSaveGame.h"

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
}

bool ULKRunSubsystem::ValidateStartingParty(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards) const
{
    if (Heroes.IsEmpty() || Cards.Num() < 5) { return false; }
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
    if (HasRun() || !ValidateStartingParty(Heroes, Cards) || !ValidateEncounterCatalog(Encounters)) { return false; }
    State = FLKRunState();
    State.RunId = FGuid::NewGuid();
    State.Seed = Seed;
    State.Heroes = Heroes;
	for (FLKRunHeroState& Hero : State.Heroes)
	{
		if (Hero.BaseMaxHealth <= 0.f) { Hero.BaseMaxHealth = Hero.MaxHealth; }
		Hero.Traits.Sort(FNameLexicalLess());
	}
    State.Cards = Cards;
	for (const FLKEncounterRow& Source : Encounters)
	{
		FLKEncounterRow Normalized;
		FString Error;
		if (!LKEncounterContent::NormalizeAndValidate(Source.EncounterId, Source, Normalized, Error))
		{ State = FLKRunState(); return false; }
		State.Encounters.Add(MoveTemp(Normalized));
	}

	// D4：生成分层节点图（固定形状：普通 → 普通/休息 → 精英/休息 → 首领）与随机敌阵容遭遇。
	FRandomStream MapStream(Seed);
	FString GraphError;
	if (!GenerateGraphAndEncounters(MapStream, GraphError))
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Run] 生成路线失败：%s"), *GraphError);
		State = FLKRunState();
		return false;
	}

	State.Nodes[0].bResolved = true;
    State.VisitedNodeIds = {"Node_Start"};
    State.CurrentNodeId = "Node_R1";
    State.Phase = ELKRunPhase::EnteringBattle;
    if (!BuildPendingBattle()) { State = FLKRunState(); return false; }
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 新远征 %s，进入第 1 战 %s（节点 %d）"), *State.RunId.ToString(),
		*State.CurrentNodeId.ToString(), State.Nodes.Num());
    AutoSave();
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
    if (!ValidateStartingParty(Heroes, Cards) || !ValidateEncounterCatalog(Encounters)) { return false; }
    const FGuid Previous = State.RunId;
    State = FLKRunState();
	const bool bStarted = StartNewRun(Heroes, Cards, Seed, Encounters);
    if (bStarted) { UE_LOG(LogLKBattle, Log, TEXT("[Run] 从头开始：%s -> %s"), *Previous.ToString(), *State.RunId.ToString()); }
    return bStarted;
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
    else if (const FLKDungeonNode* Node = FindNode(State.CurrentNodeId); Node && Node->NextNodeIds.IsEmpty())
    {
        State.Phase = ELKRunPhase::Completed;
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 首领战完成，远征通关"));
    }
    else
    {
        State.Phase = ELKRunPhase::ChoosingNode;
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 房间 %s 胜利，等待选择下一节点"), *State.CurrentNodeId.ToString());
    }
    AutoSave(); // 结算/奖励/推进等跨界面状态落盘（安全点）
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

bool ULKRunSubsystem::ChooseReward(int32 Index)
{
    if (State.Phase != ELKRunPhase::ChoosingReward || Index < 0 || Index >= State.PendingRewardOffers.Num()
        || !State.PendingRewardBatchId.IsValid()
        || State.ClaimedRewardBatchIds.Contains(State.PendingRewardBatchId))
    { return false; }

    // 应用奖励（先生成"待提交"变化，任何一步失败都不落盘）。
    const FLKRunRewardOffer& Offer = State.PendingRewardOffers[Index];
    if (Offer.CardId.IsNone()) { return false; }

    if (Offer.Kind == ELKRunRewardKind::UpgradeCard)
    {
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
        State.Cards.Add(NewCard);
    }
    else { return false; }

    State.ClaimedRewardBatchIds.Add(State.PendingRewardBatchId);
    State.PendingRewardBatchId = FGuid();
    State.PendingRewardOffers.Reset();
    State.Phase = ELKRunPhase::ChoosingNode;
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 领取奖励 #%d：%s %s"), Index,
        Offer.Kind == ELKRunRewardKind::UpgradeCard ? TEXT("升级") : TEXT("加牌"), *Offer.CardId.ToString());
    AutoSave();
    return true;
}

bool ULKRunSubsystem::SkipReward()
{
    if (State.Phase != ELKRunPhase::ChoosingReward || !State.PendingRewardBatchId.IsValid()
        || State.ClaimedRewardBatchIds.Contains(State.PendingRewardBatchId))
    { return false; }

    State.ClaimedRewardBatchIds.Add(State.PendingRewardBatchId);
    State.PendingRewardBatchId = FGuid();
    State.PendingRewardOffers.Reset();
    State.Phase = ELKRunPhase::ChoosingNode;
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 跳过奖励批次（不可回头补领）"));
    AutoSave();
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
	if (State.Phase != ELKRunPhase::ChoosingNode) { return ELKNodeSelectionResult::Rejected; }
	const FLKDungeonNode* Current = FindNode(State.CurrentNodeId);
	const FLKDungeonNode* Next = FindNode(NodeId);
	if (!Current || !Next || Next->bResolved || !Current->NextNodeIds.Contains(NodeId))
	{
		UE_LOG(LogLKBattle, Log, TEXT("[Run] 拒绝选择 %s：非下一排/已结算/阶段不符"), *NodeId.ToString());
		return ELKNodeSelectionResult::Rejected;
	}

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
	if (MutableNext) { MutableNext->bResolved = true; }
	State.VisitedNodeIds.AddUnique(NodeId);
	State.CurrentNodeId = NodeId;

	if (Next->Type == ELKDungeonNodeType::Rest)
	{
		UE_LOG(LogLKBattle, Log, TEXT("[Run] 休息：全员恢复 30%% 最大生命（封顶），可继续选择下一排"));
		AutoSave();
		return ELKNodeSelectionResult::RestResolved;
	}

	if (!BuildPendingBattle())
	{
		State.Phase = ELKRunPhase::Failed;
		return ELKNodeSelectionResult::Rejected;
	}
	UE_LOG(LogLKBattle, Log, TEXT("[Run] 选择 %s（第 %d 战），进入战斗 %s"),
		*NodeId.ToString(), GetCurrentRoomIndex(), *Next->EncounterId.ToString());
	AutoSave(); // 战斗前快照 = "战斗中退出"的恢复点
	return ELKNodeSelectionResult::BattleEntered;
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
		if (Next && Next->Type != ELKDungeonNodeType::Rest)
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
    return TEXT("LittleKing_Run");
}

bool ULKRunSubsystem::ValidateStoredRun(const FLKRunState& State, FString& OutError)
{
    OutError.Reset();
    // 版本：结构升级到 4；高于当前一律拒绝（未知版本不静默当新档覆盖）。
    if (State.SchemaVersion < 1 || State.SchemaVersion > 4)
    {
        OutError = FString::Printf(TEXT("存档结构版本 %d 不受支持（当前支持 1~4）"), State.SchemaVersion);
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
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))
    {
        UE_LOG(LogLKBattle, Warning, TEXT("[Run] 存档写入失败（槽 %s）；内存状态保留，可继续当前会话"), *SlotName);
        return false;
    }
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
    State.SchemaVersion = 4; // 旧版本结构载入后按当前结构规范化
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 已从存档恢复远征 %s（Phase=%d，第 %d 战）"),
        *State.RunId.ToString(), int32(State.Phase), GetCurrentRoomIndex());
    return true;
}

bool ULKRunSubsystem::SaveExpedition()
{
    return SaveExpeditionToSlot(GetRunSlotName());
}

bool ULKRunSubsystem::LoadExpedition()
{
    return LoadExpeditionFromSlot(GetRunSlotName());
}

bool ULKRunSubsystem::HasSavedExpedition() const
{
    return UGameplayStatics::DoesSaveGameExist(GetRunSlotName(), 0);
}

void ULKRunSubsystem::ClearSavedExpedition()
{
    if (HasSavedExpedition())
    {
        UGameplayStatics::DeleteGameInSlot(GetRunSlotName(), 0);
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 已清除远征存档"));
    }
}

void ULKRunSubsystem::AutoSave()
{
    if (!bAutoSaveEnabled || !HasRun()) { return; }
    SaveExpeditionToSlot(GetRunSlotName());
}
