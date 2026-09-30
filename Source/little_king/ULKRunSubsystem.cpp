#include "ULKRunSubsystem.h"
#include "LKResearchContent.h"

#include "Kismet/GameplayStatics.h"
#include "LKBalanceRules.h"
#include "LKEncounterContent.h"
#include "LKExpeditionMercenaryContent.h"
#include "LKUnitContent.h"
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
    FLKDungeonNode MakeNode(FName Id, ELKDungeonNodeType Type, FName Encounter, TArray<FName> Next = {}, int32 Layer = 0)
    {
        FLKDungeonNode Node;
        Node.NodeId = Id;
        Node.Type = Type;
        Node.EncounterId = Encounter;
        Node.NextNodeIds = MoveTemp(Next);
        Node.Layer = Layer;
        // 旧签名小图也按"地理深度"给节点归属：只有起点区域，深度 = 层号。
        const TArray<FLKWorldRegion>& Regions = LKWorldMapContent::Regions();
        if (Regions.Num() > 0) { Node.RegionId = Regions[0].RegionId; }
        return Node;
    }

	bool IsBuiltInTrait(FName TraitId)
	{
		return TraitId == "Trait_MageSpellReach" || TraitId == "Trait_KnightTauntAura"
			|| TraitId == "Trait_Sacrifice" || TraitId == "Trait_FaceFear" || TraitId == "Taunt";
	}

	/**
	 * 终态判定（与 ULKRunSubsystem::IsTerminal 同口径）。
	 * 终态旧远征只补版本标记：英雄、牌组、已完成遭遇、历史与领取回执都属于既成历史。
	 */
	bool IsTerminalPhase(ELKRunPhase Phase)
	{
		return Phase == ELKRunPhase::Completed || Phase == ELKRunPhase::Failed || Phase == ELKRunPhase::Abandoned;
	}

	/**
	 * docs/48 §3 之前的玩家英雄原生基础生命。
	 * 新原生值/旧原生值同比缩放 BaseMaxHealth/MaxHealth/Health；
	 * 不在此表（旧档创建时不存在、或非玩家原生英雄）的自定义英雄没有旧基准，不迁移其数值。
	 */
	bool LegacyNativeBaseHealth(FName HeroId, float& OutBaseHealth)
	{
		static const TMap<FName, float> LegacyRows = {
			{ FName("Hero_Knight"), 450.f }, { FName("Hero_Mage"), 300.f }, { FName("Hero_Ranger"), 350.f } };
		if (const float* Found = LegacyRows.Find(HeroId)) { OutBaseHealth = *Found; return true; }
		return false;
	}

	/** 迁移/禁用卡替换后的家园战备快照同步：只替换禁用卡，不重写历史战报与回执。 */
	void SyncInitialLoadoutAfterReplacement(FLKRunState& InOutState, const TMap<FName, FName>& ReplacedById)
	{
		TArray<FName>& CardIds = InOutState.InitialLoadout.CardIds;
		if (CardIds.IsEmpty()) { return; }
		TArray<FName> Synced;
		TSet<FName> Seen;
		for (FName Entry : CardIds)
		{
			FName CardId = Entry;
			if (!CardId.IsNone() && !LKCardRules::IsPlayerObtainable(CardId))
			{
				if (const FName* Mapped = ReplacedById.Find(CardId)) { CardId = *Mapped; }
				else
				{
					// 快照里有、牌组里没有的禁用卡：按同一七张池确定性替换。
					CardId = NAME_None;
					for (FName Pool : LKCardRules::ReplacementCardIds())
					{
						if (LKCardRules::IsPlayerObtainable(Pool) && !Seen.Contains(Pool)) { CardId = Pool; break; }
					}
				}
			}
			if (CardId.IsNone() || Seen.Contains(CardId)) { continue; }
			Seen.Add(CardId);
			Synced.Add(CardId);
		}
		// 家园面板直接读这份战备：替换后至少保留 5 个合法条目，优先用本轮实际持有的卡。
		auto PadOne = [&InOutState, &Synced, &Seen]() -> bool
		{
			for (const FLKRunCardState& Card : InOutState.Cards)
			{
				if (Card.CardId.IsNone() || !LKCardRules::IsPlayerObtainable(Card.CardId) || Seen.Contains(Card.CardId)) { continue; }
				Seen.Add(Card.CardId); Synced.Add(Card.CardId); return true;
			}
			for (FName Pool : LKCardRules::ReplacementCardIds())
			{
				if (!LKCardRules::IsPlayerObtainable(Pool) || Seen.Contains(Pool)) { continue; }
				Seen.Add(Pool); Synced.Add(Pool); return true;
			}
			return false;
		};
		for (int32 Guard = 0; Synced.Num() < LKCardRules::MinimumCards && Guard < 32 && PadOne(); ++Guard) {}
		CardIds = MoveTemp(Synced);
	}

	/** 迁移提示文本（UI 只读；空 = 本次载入没有发生平衡迁移）。 */
	FText BuildBalanceMigrationNotice(const FLKBalanceMigrationReport& Report)
	{
		if (Report.bTerminalHistoryPreserved)
		{
			return FText::FromString(FString::Printf(
				TEXT("平衡规则已升级（Balance %d → %d）：本轮旧远征已结束，英雄、牌组、已完成遭遇与历史记录保持不变。"),
				Report.PreviousBalanceVersion, LKBalanceRules::CurrentBalanceVersion));
		}
		return FText::FromString(FString::Printf(
			TEXT("平衡规则已升级：英雄生命按新基线同比换算（%d 名），未完成节点的敌人已按地理深度重算（%d 个），%d 张敌方专属卡已替换。"),
			Report.ScaledHeroes, Report.UnfinishedEncounters, Report.ReplacedCards));
	}

	/** 自动化测试钩子：远征槽名（默认 LittleKing_Run） */
	FString GRunSlotName = TEXT("LittleKing_Run");
}

bool ULKRunSubsystem::ValidateStartingParty(const TArray<FLKRunHeroState>& Heroes, const TArray<FLKRunCardState>& Cards) const
{
    if (Heroes.IsEmpty() || Cards.Num() < 5 || LKCardRules::Used(Cards) > 64) { return false; }
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
        // 阵营权限：敌方专属卡（骷髅兵/骷髅射手/骷髅法阵）不能作为新远征的玩家牌组进入战斗。
        if (!LKCardRules::IsPlayerObtainable(Card.CardId)) { return false; }
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
	// 新远征直接用当前平衡版本创建：不需要也不允许再做生命迁移。
	State.BalanceVersion = LKBalanceRules::CurrentBalanceVersion;
	State.ProfileId = Request.ProfileId;
	State.RegionId = Request.RegionId;
	State.InitialLoadout = Request.Loadout;
	State.BonusSnapshot = Request.BonusSnapshot;
	State.RewardRules = Request.RewardRules;
	State.bHomeRewardEligible = Request.bEligibleForHomeReward && Request.ProfileId.IsValid();
	State.PendingGold = 0;
	State.StartingGold = Request.StartingGold;
	State.DeckCapacityMinimum=Request.DeckCapacityMinimum;
	State.DeckCapacityMaximum=Request.DeckCapacityMaximum;
    if (Request.DeckCapacityMinimum<1 || Request.DeckCapacityMaximum<Request.DeckCapacityMinimum || Request.DeckCapacityMaximum>64
        || LKCardRules::Used(Request.Cards)>Request.DeckCapacityMaximum
        || (Request.bUseWorldMap && Request.Cards.Num()<LKCardRules::MinimumDepartureCards)) { State=PreviousState; return false; }
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
    bMigratedForBalance = false;
    MigrationNotice = FText::GetEmpty();
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
		const FLKEncounterRow& Template, ELKEncounterRank Rank, int32 Layer,
		const TArray<FName>& Heroes, const TArray<FName>& Bosses) -> FLKDungeonNode
	{
		FLKEncounterRow Dynamic;
		FString Error;
		if (!LKEncounterContent::MakeDynamicEncounter(EncounterId, Template, Rank, Heroes, Bosses, Stream, Dynamic, Error))
		{
			OutError = FString::Printf(TEXT("节点 %s：%s"), *NodeId.ToString(), *Error);
			return FLKDungeonNode();
		}
		LKBalanceRules::ApplySnapshot(Dynamic, LKBalanceRules::NodeDepth(0, Layer));
		State.Encounters.Add(MoveTemp(Dynamic));
		return MakeNode(NodeId, NodeTypeForRank(Rank), EncounterId, {}, Layer);
	};

	// 第二、三排左右槽位：战斗/精英 与 休息 的左右由种子决定。
	const bool bSwapRow2 = Stream.RandRange(0, 1) == 1;
	const bool bSwapRow3 = Stream.RandRange(0, 1) == 1;

	FLKDungeonNode R1 = MakeBattle(TEXT("Node_R1"), TEXT("Enc_Dyn_R1"), *NormalTemplate, ELKEncounterRank::Normal, 1, HeroPool, BossPool);
	FLKDungeonNode R2Battle = MakeBattle(TEXT("Node_R2Battle"), TEXT("Enc_Dyn_R2"), *NormalTemplate, ELKEncounterRank::Normal, 2, HeroPool, BossPool);
	FLKDungeonNode R2Rest = MakeNode(TEXT("Node_R2Rest"), ELKDungeonNodeType::Rest, NAME_None, {}, 2);
	FLKDungeonNode R3Elite = MakeBattle(TEXT("Node_R3Elite"), TEXT("Enc_Dyn_R3"), *EliteTemplate, ELKEncounterRank::Elite, 3, HeroPool, BossPool);
	FLKDungeonNode R3Rest = MakeNode(TEXT("Node_R3Rest"), ELKDungeonNodeType::Rest, NAME_None, {}, 3);
	FLKDungeonNode Boss = MakeBattle(TEXT("Node_Boss"), TEXT("Enc_Dyn_Boss"), *BossTemplate, ELKEncounterRank::Boss, 4, HeroPool, BossPool);
	if (R1.NodeId.IsNone() || R2Battle.NodeId.IsNone() || R3Elite.NodeId.IsNone() || Boss.NodeId.IsNone())
	{ return false; }

	State.Nodes.Add(MakeNode(TEXT("Node_Start"), ELKDungeonNodeType::Event, NAME_None, { TEXT("Node_R1") }, 0));
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
    for (const FLKRunRewardOffer& Offer : Offers)
    {
        if (!LKCardRules::IsPlayerObtainable(Offer.CardId)) { return false; }
        if (Offer.Kind!=ELKRunRewardKind::AddCard && Offer.Kind!=ELKRunRewardKind::UpgradeCard) { return false; }
        if (Offer.Kind==ELKRunRewardKind::AddCard && !LKCardRules::IsTemporaryMercenary(Offer.CardId)) { return false; }
        if (Offer.Kind==ELKRunRewardKind::UpgradeCard && (LKCardRules::IsSpell(Offer.CardId)||LKCardRules::IsBuilding(Offer.CardId))) { return false; }
    }
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
    // 阵营权限的最终校验：敌方专属卡不能从奖励路径进入玩家牌组（生成层已过滤，这里是防御层）。
    if (!LKCardRules::IsPlayerObtainable(Offer.CardId)) { return false; }
    const FLKRunState Before = State;

    if (Offer.Kind == ELKRunRewardKind::UpgradeCard)
    {
        if (LKCardRules::IsSpell(Offer.CardId) || LKCardRules::IsBuilding(Offer.CardId)) { return false; }
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
        FString ReplacementError;
        if (!LKCardRules::IsTemporaryMercenary(Offer.CardId)
            || !LKCardRules::ValidateReplacement(State.Cards,Offer.CardId,ReplacedCardIds,State.DeckCapacityMinimum,State.DeckCapacityMaximum,ReplacementError)) { return false; }
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
        if (Candidate.Num() < LKCardRules::MinimumCards || LKCardRules::Used(Candidate) > State.DeckCapacityMaximum) { return false; }
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
	if (Next->Type == ELKDungeonNodeType::Rest || Next->Type == ELKDungeonNodeType::Market)
	{
		State.CurrentNodeId = NodeId; if (!Next->RegionId.IsNone()) { State.RegionId = Next->RegionId; }
		State.Phase = ELKRunPhase::ResolvingNode; State.PendingBattle = FLKBattleContext();
        if (Next->Type==ELKDungeonNodeType::Market)
        { LKResearchContent::GenerateMarket(*FindNode(NodeId),State.Seed,LKBalanceRules::ComputeRegionDepth(State.WorldRegions,State.RegionId)); }
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
    AbandonCurrentRun();
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
    Receipt.ResearchMaterials=State.CarriedResearchMaterials;
    if (State.Phase==ELKRunPhase::Failed) { Amount=int32((int64(Amount)*80)/100); }
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
    if (!HasRunInProgress() || State.Phase==ELKRunPhase::InBattle || State.Phase==ELKRunPhase::EnteringBattle) { return false; }
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
    return IsTerminalPhase(State.Phase);
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
    // 平衡快照：已写入的必须自洽（版本/深度/倍率/英雄预算条目）。
    if (State.BalanceVersion < 0 || State.BalanceVersion > LKBalanceRules::CurrentBalanceVersion)
    {
        OutError = FString::Printf(TEXT("平衡版本 %d 不受支持（当前 %d）"), State.BalanceVersion, LKBalanceRules::CurrentBalanceVersion);
        return false;
    }
    for (const FLKEncounterRow& Encounter : State.Encounters)
    {
        if (!LKBalanceRules::ValidateSnapshot(Encounter, OutError)) { return false; }
    }
    // 迁移后的存档不允许再出现敌方专属卡（迁移前允许，正是为了能载入并替换）。
    // 覆盖玩家牌组、待开战副本与待领奖励的所有种类（升级候选同样不能指向敌方专属卡）。
    // 终态（Completed/Failed/Abandoned）旧远征保留历史牌组与冻结奖励，且不可能再进入任何战斗，
    // 因此不对其施加阵营限制——否则“终态历史不改”与载入兼容会互相矛盾。
    if (State.BalanceVersion >= LKBalanceRules::CurrentBalanceVersion && !IsTerminalPhase(State.Phase))
    {
        for (const FLKRunCardState& Card : State.Cards)
        {
            if (!LKCardRules::IsPlayerObtainable(Card.CardId))
            {
                OutError = FString::Printf(TEXT("牌组含玩家不可获得的敌方专属卡 %s"), *Card.CardId.ToString());
                return false;
            }
        }
        for (const FLKRunCardState& Card : State.PendingBattle.PlayerCards)
        {
            if (!LKCardRules::IsPlayerObtainable(Card.CardId))
            {
                OutError = FString::Printf(TEXT("待开战牌组副本含玩家不可获得的敌方专属卡 %s"), *Card.CardId.ToString());
                return false;
            }
        }
        for (const FLKRunRewardOffer& Offer : State.PendingRewardOffers)
        {
            if (!LKCardRules::IsPlayerObtainable(Offer.CardId))
            {
                OutError = FString::Printf(TEXT("待领奖励含玩家不可获得的敌方专属卡 %s（%s）"),
                    *Offer.CardId.ToString(), Offer.Kind == ELKRunRewardKind::UpgradeCard ? TEXT("升级候选") : TEXT("新卡候选"));
                return false;
            }
        }
    }
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
    if (State.SchemaVersion>=10)
    {
        if (State.DeckCapacityMinimum<1 || State.DeckCapacityMaximum<State.DeckCapacityMinimum || State.DeckCapacityMaximum>64
            || !LKResearchContent::ValidateMaterials(State.CarriedResearchMaterials)
            || !LKResearchContent::ValidateMaterials(State.PendingSettlement.ResearchMaterials))
        { OutError=TEXT("容量或研究材料无效"); return false; }
        TSet<FGuid> OfferIds;
        for (const auto& Node:State.Nodes)
        {
            if (Node.MarketOffers.Num()>5 || (!Node.MarketOffers.IsEmpty() && (Node.Type!=ELKDungeonNodeType::Market || !Node.bMarketGenerated)))
            { OutError=TEXT("市场商品快照无效"); return false; }
            for (const auto& O:Node.MarketOffers)
            {
                const auto* R=LKResearchContent::Find(O.CardId);
                if (!O.OfferId.IsValid() || OfferIds.Contains(O.OfferId) || O.Price<1 || O.Price>1000000
                    || (O.Kind==ELKMarketOfferKind::Mercenary?!LKCardRules::IsTemporaryMercenary(O.CardId):!R||R->Kind!=O.Kind))
                { OutError=TEXT("市场商品身份或价格无效"); return false; }
                OfferIds.Add(O.OfferId);
            }
        }
        if (!IsTerminalPhase(State.Phase) && State.BalanceVersion>=LKBalanceRules::CurrentBalanceVersion)
        { for (const auto& O:State.PendingRewardOffers)
            { if (O.Kind==ELKRunRewardKind::AddCard?!LKCardRules::IsTemporaryMercenary(O.CardId):LKCardRules::IsSpell(O.CardId)||LKCardRules::IsBuilding(O.CardId))
                { OutError=TEXT("奖励不符合佣兵获取与休息升级规则"); return false; } } }
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
    auto SameMarkets=[](const TArray<FLKDungeonNode>& A,const TArray<FLKDungeonNode>& B)
    {
        if (A.Num()!=B.Num()) { return false; }
        for (int32 I=0;I<A.Num();++I)
        {
            if (A[I].NodeId!=B[I].NodeId || A[I].bResolved!=B[I].bResolved || A[I].bMarketGenerated!=B[I].bMarketGenerated || A[I].MarketOffers.Num()!=B[I].MarketOffers.Num()) { return false; }
            for (int32 J=0;J<A[I].MarketOffers.Num();++J)
            { const auto& X=A[I].MarketOffers[J]; const auto& Y=B[I].MarketOffers[J]; if (X.OfferId!=Y.OfferId || X.CardId!=Y.CardId || X.Kind!=Y.Kind || X.Price!=Y.Price || X.bSold!=Y.bSold) { return false; } }
        } return true;
    };
    if (!ReadBack || ReadBack->RunState.RunId != State.RunId || ReadBack->RunState.Phase != State.Phase
        || ReadBack->RunState.CurrentNodeId != State.CurrentNodeId || ReadBack->RunState.WalletGold != State.WalletGold
        || ReadBack->RunState.ProcessedWalletTransactions != State.ProcessedWalletTransactions
        || !SameCards(ReadBack->RunState.Cards, State.Cards)
        || !SameCards(ReadBack->RunState.PendingBattle.PlayerCards, State.PendingBattle.PlayerCards)
        || !SameMarkets(ReadBack->RunState.Nodes,State.Nodes)
        || ReadBack->RunState.PendingBattle.AttemptId!=State.PendingBattle.AttemptId
        || ReadBack->RunState.PendingBattle.Seed!=State.PendingBattle.Seed
        || ReadBack->RunState.PendingBattle.EnemyHeroIds!=State.PendingBattle.EnemyHeroIds
        || ReadBack->RunState.PendingRewardBatchId != State.PendingRewardBatchId
        || ReadBack->RunState.ClaimedRewardBatchIds != State.ClaimedRewardBatchIds
        || ReadBack->RunState.PendingSettlement.bProfileApplied != State.PendingSettlement.bProfileApplied
        || ReadBack->RunState.DeckCapacityMinimum!=State.DeckCapacityMinimum || ReadBack->RunState.DeckCapacityMaximum!=State.DeckCapacityMaximum
        || !ReadBack->RunState.CarriedResearchMaterials.OrderIndependentCompareEqual(State.CarriedResearchMaterials)
        || !ReadBack->RunState.PendingSettlement.ResearchMaterials.OrderIndependentCompareEqual(State.PendingSettlement.ResearchMaterials))
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

    // 迁移在本地 Candidate 上进行：只有“迁移结果再次通过校验”后才提交到内存 State。
    // 失败时调用前正在使用的远征（bAllowExistingRun=true）不会被 Reset 掉，提示 flag 也不写入。
    FLKRunState Candidate = Save->RunState;
    if (Candidate.SchemaVersion < CurrentSchemaVersion)
    {
        // H5：v0.6 及更早的远征档迁移到 Schema 5（补默认区域/加成/战备，旧轮不参与家园金币结算）。
        MigrateStoredRun(Candidate);
    }
    const bool bNeedsBalanceMigration = Candidate.BalanceVersion < LKBalanceRules::CurrentBalanceVersion;
    FLKBalanceMigrationReport Report;
    if (bNeedsBalanceMigration)
    {
        // Balance V1：一次性迁移（生命同比换算 + 未完成节点快照刷新 + 禁用卡替代）。
        Report = MigrateRunToBalanceV1(Candidate);
        // 迁移结果必须先通过同一套校验才允许使用/落盘，绝不静默写坏档。
        FString MigratedError;
        if (!ValidateStoredRun(Candidate, MigratedError))
        {
            UE_LOG(LogLKBattle, Error, TEXT("[Run] 平衡迁移结果校验失败：%s（保留原档与原内存状态，不写回）"), *MigratedError);
            return false;
        }
    }

    FString FinalError;
    if (!ValidateStoredRun(Candidate,FinalError)) { UE_LOG(LogLKBattle,Warning,TEXT("[Run] 迁移校验失败：%s"),*FinalError); return false; }
    State = MoveTemp(Candidate);
    // 提交成功后才写提示：失败的载入不会留下“已迁移”痕迹。
    bMigratedForBalance = bNeedsBalanceMigration && Report.bChanged;
    MigrationNotice = bNeedsBalanceMigration ? BuildBalanceMigrationNotice(Report) : FText::GetEmpty();
    if (bNeedsBalanceMigration && bAutoSaveEnabled)
    {
        // 迁移结果立刻落安全点：反复读档不会二次缩放（迁移本身也按版本幂等）。
        if (!SaveExpedition()) { UE_LOG(LogLKBattle, Warning, TEXT("[Run] 平衡迁移后写盘失败；本次会话按内存状态继续")); }
    }
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 已从存档恢复远征 %s（Phase=%d，第 %d 战，Schema=%d，Balance=%d，区域 %s）"),
        *State.RunId.ToString(), int32(State.Phase), GetCurrentRoomIndex(), State.SchemaVersion, State.BalanceVersion, *State.RegionId.ToString());
    return true;
}

void ULKRunSubsystem::MigrateStoredRun(FLKRunState& InOutState) const
{
    const int32 PreviousSchema = InOutState.SchemaVersion;
    if (PreviousSchema<10)
    {
        InOutState.DeckCapacityMinimum=InOutState.DeckCapacityMaximum=8;
        if (!IsTerminalPhase(InOutState.Phase))
        {
            InOutState.RewardRules.FailureKeepPercent=.8f; InOutState.RewardRules.AbandonKeepPercent=1.f; InOutState.RewardRules.RuleVersion=3;
            if (InOutState.BalanceVersion>=LKBalanceRules::CurrentBalanceVersion)
            { InOutState.PendingRewardOffers.RemoveAll([](const auto& O){return O.Kind==ELKRunRewardKind::AddCard?!LKCardRules::IsTemporaryMercenary(O.CardId):LKCardRules::IsSpell(O.CardId)||LKCardRules::IsBuilding(O.CardId);}); }
            if (InOutState.Phase==ELKRunPhase::ChoosingReward && InOutState.PendingRewardOffers.IsEmpty())
            { InOutState.ClaimedRewardBatchIds.AddUnique(InOutState.PendingRewardBatchId); InOutState.PendingRewardBatchId.Invalidate(); InOutState.Phase=ELKRunPhase::ChoosingNode; }
        }
        if (!IsTerminalPhase(InOutState.Phase))
        { for (auto& N:InOutState.Nodes) { LKResearchContent::GenerateMarket(N,InOutState.Seed,LKBalanceRules::ComputeRegionDepth(InOutState.WorldRegions,N.RegionId)); } }
    }
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
        InOutState.RewardRules.FailureKeepPercent = .8f; InOutState.RewardRules.AbandonKeepPercent = 1.f;
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

int32 ULKRunSubsystem::NodeBalanceDepth(const FLKRunState& InOutState, const FLKDungeonNode& Node) const
{
    // 世界地图：区域拓扑深度（不是区域数组序号）；旧小图只有起点区域，深度 = 层号。
    if (InOutState.WorldMapVersion > 0 && !InOutState.WorldRegions.IsEmpty())
    {
        const int32 RegionDepth = LKBalanceRules::ComputeRegionDepth(InOutState.WorldRegions, Node.RegionId);
        return LKBalanceRules::NodeDepth(RegionDepth, InOutState.WorldMapVersion>=2?FMath::RoundToInt(float(Node.Layer)*4.f/14.f):Node.Layer);
    }
    return LKBalanceRules::NodeDepth(0, Node.Layer);
}

int32 ULKRunSubsystem::ReplaceDisabledCards(FLKRunState& InOutState) const
{
    // 稳定顺序替代：优先剑士、弓箭手、盾卫、火球、治疗波、箭塔、兵营；替代卡 Lv0。
    // 这七张基础卡（含火球/治疗波两张法术）由 LKCardRules::ReplacementCardIds() 显式登记。
    // 旧实现额外要求 LKUnitContent::Find 命中；法术没有单位行会被静默跳过，
    // 于是"五张单位/建筑候选已被占用"的合法旧牌组补不满五张。这里只按阵营权限复核。
    TSet<FName> Used;
    for (const FLKRunCardState& Card : InOutState.Cards)
    {
        if (LKCardRules::IsPlayerObtainable(Card.CardId)) { Used.Add(Card.CardId); }
    }
    auto PickReplacement = [&Used]() -> FName
    {
        for (FName Candidate : LKCardRules::ReplacementCardIds())
        {
            if (Candidate.IsNone() || Used.Contains(Candidate)) { continue; }
            if (!LKCardRules::IsPlayerObtainable(Candidate)) { continue; }
            return Candidate;
        }
        return NAME_None;
    };

    TMap<FName, FName> ReplacedById;
    int32 Replaced = 0;
    for (FLKRunCardState& Card : InOutState.Cards)
    {
        if (LKCardRules::IsPlayerObtainable(Card.CardId)) { continue; }
        const FName Replacement = PickReplacement();
        if (!Replacement.IsNone())
        {
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 禁用卡替代：%s -> %s（Lv0）"), *Card.CardId.ToString(), *Replacement.ToString());
            ReplacedById.Add(Card.CardId, Replacement);
            Card.CardId = Replacement;
            Card.UpgradeLevel = 0;
            Used.Add(Replacement);
            ++Replaced;
        }
        else
        {
            UE_LOG(LogLKBattle, Warning, TEXT("[Run] 禁用卡 %s 无可用替代，按移除处理"), *Card.CardId.ToString());
            Card.CardId = NAME_None;
        }
    }
    InOutState.Cards.RemoveAll([](const FLKRunCardState& Card) { return Card.CardId.IsNone(); });
    // 至少 5 张：不足时按稳定顺序补齐（不消耗金币、不重抽已领取奖励）。
    for (int32 Guard = 0; InOutState.Cards.Num() < LKCardRules::MinimumCards && Guard < 32; ++Guard)
    {
        const FName Replacement = PickReplacement();
        if (Replacement.IsNone()) { break; }
        FLKRunCardState Card;
        Card.CardId = Replacement;
        InOutState.Cards.Add(Card);
        Used.Add(Replacement);
        ++Replaced;
    }

    // 家园战备快照同步（面板直接展示 InitialLoadout）：只替换禁用卡，不重写历史战报。
    SyncInitialLoadoutAfterReplacement(InOutState, ReplacedById);

    // 冻结的待选奖励：所有种类都不得指向敌方专属卡。
    // 旧 UpgradeCard 指向敌方专属卡时确定性换成"合法升级或新卡"，不使用随机池、不重抽、不动领取回执。
    if (!InOutState.PendingRewardOffers.IsEmpty())
    {
        TSet<FName> Owned;
        for (const FLKRunCardState& Card : InOutState.Cards) { Owned.Add(Card.CardId); }
        // 与牌组替代共用同一份"七张显式登记基础卡（含法术）"清单，避免两处漂移。
        TArray<FName> RewardPool = LKCardRules::ReplacementCardIds();
        for (const FLKTemporaryMercenaryDefinition& Definition : LKExpeditionMercenaryContent::All())
        { RewardPool.AddUnique(Definition.Unit.UnitId); }

        // 本批已占用的候选：保证批次内无重复候选（升级目标与加牌候选天然不同集合）。
        TSet<FName> ChosenAdd;
        TSet<FName> ChosenUpgrade;
        auto PickAddCandidate = [&RewardPool, &Owned, &ChosenAdd]() -> FName
        {
            for (FName Candidate : RewardPool)
            {
                if (Candidate.IsNone() || !LKCardRules::IsTemporaryMercenary(Candidate)) { continue; }
                if (Owned.Contains(Candidate) || ChosenAdd.Contains(Candidate)) { continue; }
                return Candidate;
            }
            return NAME_None;
        };
        auto PickUpgradeTarget = [&InOutState, &ChosenUpgrade]() -> const FLKRunCardState*
        {
            for (const FLKRunCardState& Card : InOutState.Cards)
            {
                if (Card.CardId.IsNone() || !LKCardRules::IsPlayerObtainable(Card.CardId) || LKCardRules::IsSpell(Card.CardId) || LKCardRules::IsBuilding(Card.CardId)) { continue; }
                if (ChosenUpgrade.Contains(Card.CardId)) { continue; }
                return &Card;
            }
            return nullptr;
        };

        for (FLKRunRewardOffer& Offer : InOutState.PendingRewardOffers)
        {
            const bool bLegalCard = !Offer.CardId.IsNone() && LKCardRules::IsPlayerObtainable(Offer.CardId) && (Offer.Kind==ELKRunRewardKind::AddCard?LKCardRules::IsTemporaryMercenary(Offer.CardId):!LKCardRules::IsSpell(Offer.CardId)&&!LKCardRules::IsBuilding(Offer.CardId));
            if (Offer.Kind == ELKRunRewardKind::UpgradeCard)
            {
                const FLKRunCardState* Target = bLegalCard ? InOutState.Cards.FindByPredicate(
                    [&Offer](const FLKRunCardState& Card) { return Card.CardId == Offer.CardId; }) : nullptr;
                if (Target && !ChosenUpgrade.Contains(Target->CardId))
                {
                    // 等级以存档牌组为准：旧批次的脏 LevelBefore/After 在这里被纠正为合法值。
                    Offer.LevelBefore = Target->UpgradeLevel;
                    Offer.LevelAfter = Target->UpgradeLevel + 1;
                    ChosenUpgrade.Add(Target->CardId);
                    continue;
                }
                const FLKRunCardState* ReplacementTarget = PickUpgradeTarget();
                if (ReplacementTarget)
                {
                    UE_LOG(LogLKBattle, Log, TEXT("[Run] 待领升级候选替代：%s -> %s"), *Offer.CardId.ToString(), *ReplacementTarget->CardId.ToString());
                    Offer.CardId = ReplacementTarget->CardId;
                    Offer.LevelBefore = ReplacementTarget->UpgradeLevel;
                    Offer.LevelAfter = ReplacementTarget->UpgradeLevel + 1;
                    ChosenUpgrade.Add(Offer.CardId);
                    continue;
                }
                const FName NewCard = PickAddCandidate();
                if (!NewCard.IsNone())
                {
                    UE_LOG(LogLKBattle, Log, TEXT("[Run] 待领升级候选降级为新卡候选：%s -> %s"), *Offer.CardId.ToString(), *NewCard.ToString());
                    Offer.Kind = ELKRunRewardKind::AddCard;
                    Offer.CardId = NewCard;
                    Offer.LevelBefore = 0;
                    Offer.LevelAfter = 0;
                    ChosenAdd.Add(NewCard);
                    continue;
                }
                Offer.CardId = NAME_None;
            }
            else if (Offer.Kind == ELKRunRewardKind::AddCard)
            {
                if (bLegalCard && !Owned.Contains(Offer.CardId) && !ChosenAdd.Contains(Offer.CardId))
                {
                    // AddCard 语义：LevelBefore/After 恒为 0。
                    Offer.LevelBefore = 0;
                    Offer.LevelAfter = 0;
                    ChosenAdd.Add(Offer.CardId);
                    continue;
                }
                const FName NewCard = PickAddCandidate();
                if (!NewCard.IsNone())
                {
                    UE_LOG(LogLKBattle, Log, TEXT("[Run] 待领奖励禁用候选替代：%s -> %s"), *Offer.CardId.ToString(), *NewCard.ToString());
                    Offer.CardId = NewCard;
                    Offer.LevelBefore = 0;
                    Offer.LevelAfter = 0;
                    ChosenAdd.Add(NewCard);
                    continue;
                }
                const FLKRunCardState* ReplacementTarget = PickUpgradeTarget();
                if (ReplacementTarget)
                {
                    UE_LOG(LogLKBattle, Log, TEXT("[Run] 待领新卡候选改为升级候选：%s -> %s"), *Offer.CardId.ToString(), *ReplacementTarget->CardId.ToString());
                    Offer.Kind = ELKRunRewardKind::UpgradeCard;
                    Offer.CardId = ReplacementTarget->CardId;
                    Offer.LevelBefore = ReplacementTarget->UpgradeLevel;
                    Offer.LevelAfter = ReplacementTarget->UpgradeLevel + 1;
                    ChosenUpgrade.Add(Offer.CardId);
                    continue;
                }
                Offer.CardId = NAME_None;
            }
            else
            {
                // 未知种类（损坏档）：丢弃候选而不是把非法内容留在面板上。
                Offer.CardId = NAME_None;
            }
        }
        InOutState.PendingRewardOffers.RemoveAll([](const FLKRunRewardOffer& Offer) { return Offer.CardId.IsNone(); });
        if (InOutState.PendingRewardOffers.IsEmpty())
        {
            // 防御性兜底（迁移后牌组至少 5 张合法卡，正常不会走到）：没有合法候选时作废本批，
            // 避免玩家卡在无法领取的面板上；批次 ID 与领取回执的语义与既有跳过入口一致。
            InOutState.ClaimedRewardBatchIds.Add(InOutState.PendingRewardBatchId);
            InOutState.PendingRewardBatchId = FGuid();
            if (InOutState.Phase == ELKRunPhase::ChoosingReward) { InOutState.Phase = ELKRunPhase::ChoosingNode; }
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 待领奖励整批作废（候选全部非法且无确定性替代）"));
        }
    }

    // 待开始战斗的牌组副本必须与迁移后的牌组一致，避免从恢复路径绕过限制。
    // 牌组副本无条件同步（即使上下文已不是恢复点，也不允许留下敌方专属卡）；
    // 英雄副本只在确实是待开战上下文时同步，避免改写无关快照。
    InOutState.PendingBattle.PlayerCards = InOutState.Cards;
    if (InOutState.PendingBattle.bExpedition)
    {
        InOutState.PendingBattle.PlayerHeroes = InOutState.Heroes;
    }
    return Replaced;
}

FLKBalanceMigrationReport ULKRunSubsystem::MigrateRunToBalanceV1(FLKRunState& InOutState) const
{
    FLKBalanceMigrationReport Report;
    Report.PreviousBalanceVersion = InOutState.BalanceVersion;
    if (InOutState.BalanceVersion >= LKBalanceRules::CurrentBalanceVersion) { return Report; }

    // 终态旧远征只补版本标记（Schema 迁移在上层已完成）：
    // 英雄、牌组、已完成遭遇、路线、钱包、历史与领取回执都属于既成历史，不再改写，也不重抽奖励。
    if (IsTerminalPhase(InOutState.Phase))
    {
        InOutState.BalanceVersion = LKBalanceRules::CurrentBalanceVersion;
        InOutState.SchemaVersion = CurrentSchemaVersion;
        Report.bTerminalHistoryPreserved = true;
        Report.bChanged = false;
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 平衡迁移：终态旧远征（Phase=%d）保留历史，只写入 Balance=%d"),
            int32(InOutState.Phase), InOutState.BalanceVersion);
        return Report;
    }

    // 1) 玩家英雄：按"新原生生命 / 旧原生生命"同比缩放 BaseMaxHealth、MaxHealth、Health。
    //    同比缩放同时满足两件事：当前血量相对旧 MaxHealth 的百分比不变（半血仍半血），
    //    且 BaseMaxHealth 相对旧原生基准的永久升级、以及 MaxHealth 上的特性增幅都按同一比例保留。
    //    旧实现的 HealthRatio=Health/BaseMaxHealth 会在存在永久增幅时把血量补回，等于凭空回血。
    for (FLKRunHeroState& Hero : InOutState.Heroes)
    {
        const FLKUnitRow* Row = LKUnitContent::Find(Hero.HeroId);
        if (!Row || !FMath::IsFinite(Row->BaseHealth) || Row->BaseHealth <= 0.f) { continue; }
        float OldNative = 0.f;
        // 自定义英雄（旧档创建时不存在，或非玩家原生英雄）没有旧基准：保持原数值，不参与同比缩放。
        if (!LegacyNativeBaseHealth(Hero.HeroId, OldNative) || OldNative <= 0.f) { continue; }
        const float Scale = Row->BaseHealth / OldNative;
        const float OldMaximum = (FMath::IsFinite(Hero.MaxHealth) && Hero.MaxHealth > 0.f) ? Hero.MaxHealth : OldNative;
        // BaseMaxHealth<=0 仅见于更早的 D1 快照：按旧 MaxHealth 兼容迁移，不让永久基准丢失。
        const float OldBase = (FMath::IsFinite(Hero.BaseMaxHealth) && Hero.BaseMaxHealth > 0.f) ? Hero.BaseMaxHealth : OldMaximum;
        const float WasHealth = (FMath::IsFinite(Hero.Health) && Hero.Health > 0.f) ? Hero.Health : 0.f;
        const float HealthRatio = FMath::Clamp(WasHealth / OldMaximum, 0.f, 1.f);
        Hero.BaseMaxHealth = OldBase * Scale;
        Hero.MaxHealth = OldMaximum * Scale;
        // 0 血仍是失能：比例 0 不会被更新复活。
        Hero.Health = FMath::Clamp(Hero.MaxHealth * HealthRatio, 0.f, Hero.MaxHealth);
        ++Report.ScaledHeroes;
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 平衡迁移：%s 基础 %.0f -> %.0f（同比 ×%.4f），生命 %.0f/%.0f -> %.0f/%.0f"),
            *Hero.HeroId.ToString(), OldBase, Hero.BaseMaxHealth, Scale, WasHealth, OldMaximum, Hero.Health, Hero.MaxHealth);
    }

    // 2) 只迁移未完成节点及其对应遭遇：已完成路线、种子、钱包、领取回执不改。
    //    已完成节点的遭遇保持旧快照（历史兼容）；旧实现用第二个循环对所有快照行 ApplySnapshot，
    //    会把已完成遭遇一起改写，这里不再有全量兜底循环。
    TSet<FName> RefreshedEncounters;
    for (const FLKDungeonNode& Node : InOutState.Nodes)
    {
        if (!LKWorldMapContent::IsCombat(Node.Type) || Node.bResolved) { continue; }
        for (FLKEncounterRow& Encounter : InOutState.Encounters)
        {
            if (Encounter.EncounterId != Node.EncounterId) { continue; }
            LKBalanceRules::BuildReinforcementWaves(Encounter.Rank, Encounter.Waves);
            Encounter.EnemySilverPerSecond = LKBalanceRules::EnemySilverRate(Encounter.Rank);
            Encounter.EnemySilverCap = 10.f;
            Encounter.EnemyStartingSilver = 0.f;
            Encounter.EnemySpell.Cooldown = LKBalanceRules::CircleCooldown(Encounter.Rank);
            Encounter.EnemySpell.bEnabled = true;
            LKBalanceRules::ApplySnapshot(Encounter, NodeBalanceDepth(InOutState, Node));
            RefreshedEncounters.Add(Encounter.EncounterId);
            break;
        }
    }
    Report.UnfinishedEncounters = RefreshedEncounters.Num();
    // 3) 待恢复战斗同步新快照（从既有安全点重建，不复制两套增援；AttemptId/种子不变）。
    if (InOutState.PendingBattle.bExpedition)
    {
        const FLKDungeonNode* PendingNode = InOutState.Nodes.FindByPredicate(
            [&InOutState](const FLKDungeonNode& Node) { return Node.NodeId == InOutState.PendingBattle.NodeId; });
        if (PendingNode && !PendingNode->bResolved && LKWorldMapContent::IsCombat(PendingNode->Type))
        {
            if (const FLKEncounterRow* Source = LKEncounterContent::Find(InOutState.Encounters, InOutState.PendingBattle.EncounterId))
            {
                InOutState.PendingBattle.Encounter = *Source;
                InOutState.PendingBattle.EnemyHeroIds = Source->EnemyHeroIds;
            }
        }
    }

    // 4) 禁用卡替代（牌组/战备快照/待领奖励/待开战副本；不重写历史战报与回执）。
    Report.ReplacedCards = ReplaceDisabledCards(InOutState);
    InOutState.BalanceVersion = LKBalanceRules::CurrentBalanceVersion;
    InOutState.SchemaVersion = CurrentSchemaVersion;
    Report.bChanged = true;

    UE_LOG(LogLKBattle, Log, TEXT("[Run] Balance V1 迁移完成：Balance %d -> %d，英雄 %d 名，未完成节点遭遇 %d 个，禁用卡替代 %d 张"),
        Report.PreviousBalanceVersion, InOutState.BalanceVersion, Report.ScaledHeroes, Report.UnfinishedEncounters, Report.ReplacedCards);
    return Report;
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
