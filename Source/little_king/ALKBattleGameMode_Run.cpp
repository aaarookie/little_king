#include "ALKBattleGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"

#include "LKDataTypes.h"
#include "LKEncounterContent.h"
#include "LKLog.h"
#include "LKUnitContent.h"
#include "ULKGameData.h"
#include "ULKRunSubsystem.h"

ULKRunSubsystem* ALKBattleGameMode::GetRunSubsystem() const
{
    const UGameInstance* Instance = GetGameInstance();
    return Instance ? Instance->GetSubsystem<ULKRunSubsystem>() : nullptr;
}

TArray<FLKRunHeroState> ALKBattleGameMode::BuildInitialRunHeroes() const
{
    TArray<FLKRunHeroState> Result;
    for (FName HeroId : AvailableHeroes)
    {
        const FLKUnitRow* Row = GetUnitRow(HeroId);
        if (!Row || (Row->UnitClass != ELKUnitClass::Hero && Row->UnitClass != ELKUnitClass::Boss)
            || !FMath::IsFinite(Row->BaseHealth) || Row->BaseHealth <= 0.f)
        { return {}; }
        FLKRunHeroState State;
        State.HeroId = HeroId;
        State.Health = State.MaxHealth = Row->BaseHealth;
		State.BaseMaxHealth = Row->BaseHealth;
        TSet<FName> Traits;
        for (FName Trait : Row->HeroTraits)
        {
            if (Trait != "Trait_KnightAura" && Trait != "Trait_MageMight" && !Trait.IsNone()) { Traits.Add(Trait); }
        }
        if (const FLKHeroTraitEntry* Defaults = GameData->DefaultHeroTraits.Find(HeroId))
        {
            for (FName Trait : Defaults->Traits) { if (!Trait.IsNone()) { Traits.Add(Trait); } }
        }
        State.Traits = Traits.Array();
        State.Traits.Sort(FNameLexicalLess());
        Result.Add(MoveTemp(State));
    }
    return Result;
}

bool ALKBattleGameMode::BuildEncounterCatalog(TArray<FLKEncounterRow>& OutCatalog, FString& OutError) const
{
	OutCatalog.Reset();
	OutError.Reset();
	if (!GameData) { OutError = TEXT("GameData 为空"); return false; }
	UDataTable* Table = nullptr;
	if (!GameData->EncounterTable.IsNull())
	{
		Table = GameData->EncounterTable.LoadSynchronous();
		if (!Table) { OutError = TEXT("EncounterTable 资源加载失败"); return false; }
	}
	if (!LKEncounterContent::BuildCatalog(Table, OutCatalog, OutError)) { return false; }
	for (const FLKEncounterRow& Encounter : OutCatalog)
	{
		for (FName HeroId : Encounter.EnemyHeroIds)
		{
			const FLKUnitRow* Row = GetUnitRow(HeroId);
			if (!Row || (Row->UnitClass != ELKUnitClass::Hero && Row->UnitClass != ELKUnitClass::Boss))
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的英雄 %s 不存在或类别错误"),
					*Encounter.EncounterId.ToString(), *HeroId.ToString());
				OutCatalog.Reset();
				return false;
			}
		}
		for (const FLKWaveEntry& Wave : Encounter.Waves)
		{
			const FLKUnitRow* Row = GetUnitRow(Wave.UnitId);
			if (!Row || Row->UnitClass == ELKUnitClass::Hero || Row->UnitClass == ELKUnitClass::Boss)
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的波次单位 %s 不存在或类别错误"),
					*Encounter.EncounterId.ToString(), *Wave.UnitId.ToString());
				OutCatalog.Reset();
				return false;
			}
		}
		if (Encounter.bEnemyUsesCards && Encounter.EnemyCards.Num() < GameData->HandSize + 1)
		{
			OutError = FString::Printf(TEXT("遭遇 %s 的敌方牌组少于 HandSize + 1"), *Encounter.EncounterId.ToString());
			OutCatalog.Reset();
			return false;
		}
		for (FName CardId : Encounter.EnemyCards)
		{
			if (!FindCard(CardId))
			{
				OutError = FString::Printf(TEXT("遭遇 %s 引用了未知卡牌 %s"),
					*Encounter.EncounterId.ToString(), *CardId.ToString());
				OutCatalog.Reset();
				return false;
			}
		}
	}
	return true;
}

TArray<FLKRunCardState> ALKBattleGameMode::BuildInitialRunCards() const
{
    TArray<FLKRunCardState> Result;
    TSet<FName> Seen;
    for (FName CardId : GameData->DefaultPlayerDeck)
    {
        if (CardId.IsNone() || Seen.Contains(CardId) || !FindCard(CardId)) { continue; }
        FLKRunCardState State;
        State.CardId = CardId;
        Result.Add(State);
        Seen.Add(CardId);
    }
    return Result;
}

bool ALKBattleGameMode::ApplyExpeditionContext(const FLKBattleContext& Context)
{
    if (!Context.bExpedition || !Context.RunId.IsValid() || !Context.AttemptId.IsValid()
        || Context.NodeId.IsNone() || Context.EncounterId.IsNone() || Context.PlayerHeroes.IsEmpty()
        || Context.PlayerCards.Num() < GameData->HandSize + 1)
    { return false; }
	FLKEncounterRow Encounter;
	FString EncounterError;
	if (!LKEncounterContent::NormalizeAndValidate(Context.EncounterId, Context.Encounter, Encounter, EncounterError))
	{
		UE_LOG(LogLKBattle, Error, TEXT("[Run] BattleContext 遭遇无效：%s"), *EncounterError);
		return false;
	}
    TSet<FName> HeroIds;
    AvailableHeroes.Reset();
    for (const FLKRunHeroState& Hero : Context.PlayerHeroes)
    {
        if (Hero.HeroId.IsNone() || HeroIds.Contains(Hero.HeroId) || !GetUnitRow(Hero.HeroId)) { return false; }
        HeroIds.Add(Hero.HeroId);
        AvailableHeroes.Add(Hero.HeroId);
    }
    TSet<FName> CardIds;
    GameData->DefaultPlayerDeck.Reset();
    for (const FLKRunCardState& Card : Context.PlayerCards)
    {
        if (Card.CardId.IsNone() || CardIds.Contains(Card.CardId) || !FindCard(Card.CardId)) { return false; }
        CardIds.Add(Card.CardId);
        GameData->DefaultPlayerDeck.Add(Card.CardId);
    }
	for (FName HeroId : Encounter.EnemyHeroIds)
	{
		const FLKUnitRow* Row = GetUnitRow(HeroId);
		if (!Row || (Row->UnitClass != ELKUnitClass::Hero && Row->UnitClass != ELKUnitClass::Boss))
		{
			UE_LOG(LogLKBattle, Error, TEXT("[Encounter] %s 的敌方英雄 %s 不存在或类别错误"),
				*Encounter.EncounterId.ToString(), *HeroId.ToString());
			return false;
		}
	}
	for (const FLKWaveEntry& Wave : Encounter.Waves)
	{
		const FLKUnitRow* Row = GetUnitRow(Wave.UnitId);
		if (!Row || Row->UnitClass == ELKUnitClass::Hero || Row->UnitClass == ELKUnitClass::Boss)
		{
			UE_LOG(LogLKBattle, Error, TEXT("[Encounter] %s 的波次单位 %s 不存在或不是佣兵/建筑"),
				*Encounter.EncounterId.ToString(), *Wave.UnitId.ToString());
			return false;
		}
	}
	if (Encounter.bEnemyUsesCards)
	{
		if (Encounter.EnemyCards.Num() < GameData->HandSize + 1) { return false; }
		for (FName CardId : Encounter.EnemyCards) { if (!FindCard(CardId)) { return false; } }
	}
    BattleContext = Context;
	BattleContext.Encounter = Encounter;
	BattleContext.EnemyHeroIds = Encounter.EnemyHeroIds;
	BattleContext.EnemyCards = Encounter.EnemyCards;
	BattleContext.bEnemyUsesCards = Encounter.bEnemyUsesCards;
	CurrentEncounter = Encounter;
    EnemyHeroIds = Encounter.EnemyHeroIds;
    bEnemyUsesCards = Encounter.bEnemyUsesCards;
	GameData->DefaultEnemyDeck = Encounter.EnemyCards;
    GameData->EnemyEncounterId = Encounter.EncounterId;
    GameData->BattleSeed = Context.Seed;
    BattleRandom.Initialize(Context.Seed);
    SeedTieBreaker = BattleRandom.RandRange(0, 1) == 0 ? ELKTeam::Player : ELKTeam::Enemy;
    MatchStats = FLKMatchStats();
    MatchStats.Seed = Context.Seed;
    BattleOutcome = FLKBattleOutcome();
    bExpeditionBattle = true;
	UE_LOG(LogLKBattle, Log, TEXT("[Run] 加载第 %d/%d 战 %s，强度=%d 奖励档=%d Attempt=%s，继承英雄%d/卡牌%d"),
		GetExpeditionRoomIndex(), GetExpeditionRoomCount(), *Encounter.EncounterId.ToString(), int32(Encounter.Rank), Encounter.RewardTier,
		*Context.AttemptId.ToString(), Context.PlayerHeroes.Num(), Context.PlayerCards.Num());
    return true;
}

TMap<FName, TArray<FName>> ALKBattleGameMode::CollectIdentityTraits() const
{
    // BUG-017：身份特性 = 代码配置（DA_GameData.DefaultHeroTraits）+ 单位行内代码特性。
    // 它们决定英雄的身份能力（法师全场施法、骑士嘲讽光环、盾卫嘲讽），
    // 因此旧档快照缺失时必须能补回来；玩家在房间间临时增删的其他特性不受影响。
    TMap<FName, TArray<FName>> Result;
    if (!GameData) { return Result; }
    for (const TPair<FName, FLKHeroTraitEntry>& Pair : GameData->DefaultHeroTraits)
    {
        Result.FindOrAdd(Pair.Key).Append(Pair.Value.Traits);
    }
    for (const TPair<FName, FLKUnitRow>& Pair : LKUnitContent::Units())
    {
        const FLKUnitRow* Row = GetUnitRow(Pair.Key);
        if (Row && Row->HeroTraits.Num() > 0) { Result.FindOrAdd(Pair.Key).Append(Row->HeroTraits); }
    }
    return Result;
}

bool ALKBattleGameMode::InitializeExpeditionContext(bool bAutoStart)
{
    if (!bAutoStart) { return false; }
    ULKRunSubsystem* Run = GetRunSubsystem();
    if (!Run) { UE_LOG(LogLKBattle, Error, TEXT("[Run] GameInstance 没有 RunSubsystem")); return false; }

    bRecoveredJunction = false;
    bRecoveredTerminal = false;

    // D5：进程冷启动（内存无远征）且存在存档 → 按上次位置恢复。
    if (!Run->HasRun() && Run->HasSavedExpedition())
    {
        if (!Run->LoadExpedition())
        {
            // 损坏/版本不符：拒绝载入（原档保留），按"无存档"走下方新远征（明确用户动作才会覆盖旧档）。
            UE_LOG(LogLKBattle, Error, TEXT("[Run] 存档不可用，本次按新远征开始（原档保留，未覆盖）"));
        }
        else
        {
            // BUG-017：旧档（在"身份特性"写进远征快照之前创建的远征）可能缺少法师全场施法这类
            // 身份特性；载入后按当前代码补全，否则英雄上场时会"有法师却无法在敌方半场施法"。
            const int32 RestoredTraits = Run->RestoreHeroIdentityTraits(CollectIdentityTraits());
            if (RestoredTraits > 0)
            {
                UE_LOG(LogLKBattle, Log, TEXT("[Run] 旧档身份特性补全 %d 条（例：法师全场施法 Trait_MageSpellReach）"),
                    RestoredTraits);
            }
            const ELKRunPhase RunPhase = Run->GetRunPhase();
            if (RunPhase == ELKRunPhase::EnteringBattle || RunPhase == ELKRunPhase::InBattle)
            {
                // 战斗中退出：恢复到该房开战前（同 Encounter/种子/Attempt；弹道/技能等临时状态不保留）。
                FLKBattleContext Context;
                if (Run->BeginCurrentBattle(Context) && ApplyExpeditionContext(Context))
                {
                    UE_LOG(LogLKBattle, Log, TEXT("[Run] 恢复战斗：第 %d 战 %s（将重新开战，临时状态不保留）"),
                        Run->GetCurrentRoomIndex(), *Context.EncounterId.ToString());
                    return true;
                }
                UE_LOG(LogLKBattle, Error, TEXT("[Run] 恢复战斗上下文失败，按新远征开始"));
            }
            else if (RunPhase == ELKRunPhase::ChoosingNode || RunPhase == ELKRunPhase::ChoosingReward
                || RunPhase == ELKRunPhase::ResolvingNode)
            {
                // 路线/奖励选择中退出：进入"恢复中枢"世界——HUD 直接弹对应面板（奖励或下一排选择）。
                bRecoveredJunction = true;
                bExpeditionBattle = true;
                UE_LOG(LogLKBattle, Log, TEXT("[Run] 恢复选择流程：第 %d 战完成，待领取奖励/选择下一排"), Run->GetCurrentRoomIndex());
                return true;
            }
            else if (RunPhase == ELKRunPhase::Completed || RunPhase == ELKRunPhase::Failed || RunPhase == ELKRunPhase::Abandoned)
            {
                // 终态：显示通关/失败摘要，可开始新远征。
                bRecoveredTerminal = true;
                bExpeditionBattle = true;
                UE_LOG(LogLKBattle, Log, TEXT("[Run] 恢复终态摘要（Phase=%d）"), int32(RunPhase));
                return true;
            }
        }
    }

    const TArray<FLKRunHeroState> FreshHeroes = BuildInitialRunHeroes();
    const TArray<FLKRunCardState> FreshCards = BuildInitialRunCards();
	TArray<FLKEncounterRow> FreshEncounters;
	FString EncounterError;
    if (!Run->HasRun())
    {
		if (!BuildEncounterCatalog(FreshEncounters, EncounterError))
		{
			UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 无法开始远征：%s"), *EncounterError);
			return false;
		}
		if (!Run->StartNewRun(FreshHeroes, FreshCards, GameData->BattleSeed, FreshEncounters)) { return false; }
    }
    else if (Run->IsTerminal())
    {
		if (!BuildEncounterCatalog(FreshEncounters, EncounterError))
		{
			UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 无法重新开始远征：%s"), *EncounterError);
			return false;
		}
		if (!Run->RestartRun(FreshHeroes, FreshCards, Run->GetRunState().Seed, FreshEncounters)) { return false; }
    }
    else if (Run->CanAdvance())
    {
        // 兼容旧结算蓝图意外直接重载地图：新世界仍按合法路线前进，不重复结算上一房。
        if (!Run->AdvanceToNextBattle()) { return false; }
    }

    FLKBattleContext Context;
    return Run->BeginCurrentBattle(Context) && ApplyExpeditionContext(Context);
}

FText ALKBattleGameMode::GetRunSummaryText() const
{
    const ULKRunSubsystem* Run = GetRunSubsystem();
    if (!Run || !Run->HasRun()) { return FText::GetEmpty(); }
    const FLKRunState State = Run->GetRunState();
    const int32 Battles = State.BattleHistory.Num();
    FString Header;
    if (State.Phase == ELKRunPhase::Completed) { Header = TEXT("远征通关！"); }
    else if (State.Phase == ELKRunPhase::Failed) { Header = TEXT("远征失败…"); }
    else { Header = TEXT("远征结束"); }

    if (Battles == 0) { return FText::FromString(Header); }
    const FLKBattleOutcome& Last = State.BattleHistory.Last();
    const FString WinLoss = Last.Stats.Winner == ELKTeam::Player ? TEXT("最后一战获胜") : TEXT("最后一战落败");
    return FText::FromString(FString::Printf(TEXT("%s\n共 %d 场战斗 · %s\n击杀 %d · 我方伤害 %.0f"),
        *Header, Battles, *WinLoss, Last.Stats.Player.Kills, Last.Stats.Player.Damage));
}

bool ALKBattleGameMode::StartNewRunFromRecovery()
{
    // 仅允许从"终态摘要/恢复中枢"世界开启新远征（不覆盖进行中的内存远征）。
    ULKRunSubsystem* Run = GetRunSubsystem();
    if (!Run) { return false; }
    if (!(bRecoveredTerminal || bRecoveredJunction))
    {
        UE_LOG(LogLKBattle, Log, TEXT("[Run] 开始新远征被拒绝：当前不在恢复/终态世界"));
        return false;
    }
    TArray<FLKEncounterRow> Encounters;
    FString Error;
    if (!BuildEncounterCatalog(Encounters, Error)
        || !Run->RestartRun(BuildInitialRunHeroes(), BuildInitialRunCards(), Run->GetRunState().Seed, Encounters))
    {
        if (!Error.IsEmpty()) { UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 无法从头开始：%s"), *Error); }
        return false;
    }
    bRecoveredJunction = false;
    bRecoveredTerminal = false;
    return ReloadBattleLevel();
}

int32 ALKBattleGameMode::GetExpeditionRoomIndex() const
{
    const ULKRunSubsystem* Run = GetRunSubsystem();
    return bExpeditionBattle && Run ? Run->GetCurrentRoomIndex() : 1;
}

int32 ALKBattleGameMode::GetExpeditionRoomCount() const
{
    const ULKRunSubsystem* Run = GetRunSubsystem();
    return bExpeditionBattle && Run ? Run->GetTotalRoomCount() : 1;
}

bool ALKBattleGameMode::IsResultActionNext() const
{
    // D4：胜利后有下一排可选节点 → 结果流程进入节点选择（不再有"下一关"按钮语义）。
    const ULKRunSubsystem* Run = GetRunSubsystem();
    return Phase == ELKGamePhase::Result && bExpeditionBattle && Run && Run->CanSelectNextNode();
}

FText ALKBattleGameMode::GetResultActionLabel() const
{
    // 胜利且有下一排：按钮被节点选择面板取代（禁用态）；失败/通关：从头开始。
    return FText::FromString(IsResultActionNext() ? TEXT("选择路线") : TEXT("从头开始"));
}

bool ALKBattleGameMode::RequestResultAction()
{
    if (Phase != ELKGamePhase::Result || bResultActionInProgress) { return false; }
    const FText ActionLabel = GetResultActionLabel();
    if (bExpeditionBattle)
    {
        ULKRunSubsystem* Run = GetRunSubsystem();
        if (!Run) { return false; }
        // D3 防呆：待领奖励未处理时禁止推进/重开（必须先三选一或跳过）。
        if (Run->HasPendingRewardChoice())
        {
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 存在待领取奖励，结算按钮被忽略（先选择或跳过奖励）"));
            return false;
        }
        // D4：胜利后有下一排可选节点时，推进由节点选择面板负责（按钮只服务失败/通关重开）。
        if (Run->CanSelectNextNode())
        {
            UE_LOG(LogLKBattle, Log, TEXT("[Run] 存在下一排可选节点，请通过节点选择面板推进"));
            return false;
        }
        // 失败 / 通关 / 无可选节点：从头开始。
        TArray<FLKEncounterRow> Encounters;
        FString Error;
        if (!BuildEncounterCatalog(Encounters, Error)
            || !Run->RestartRun(BuildInitialRunHeroes(), BuildInitialRunCards(), Run->GetRunState().Seed, Encounters))
        {
            if (!Error.IsEmpty()) { UE_LOG(LogLKBattle, Error, TEXT("[Encounter] 无法从头开始：%s"), *Error); }
            return false;
        }
    }
    return ReloadBattleLevel();
}

bool ALKBattleGameMode::ReloadBattleLevel()
{
    bResultActionInProgress = true;
    const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
    if (LevelName.IsEmpty()) { bResultActionInProgress = false; return false; }
    UE_LOG(LogLKBattle, Log, TEXT("[Run] 重载战场 %s"), *LevelName);
    UGameplayStatics::OpenLevel(this, FName(*LevelName));
    return true;
}

// ---------- D4 节点选择 ----------
bool ALKBattleGameMode::CanSelectNextNode() const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	return Run && Run->CanSelectNextNode();
}

int32 ALKBattleGameMode::GetNextNodeCount() const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	return Run ? Run->GetNextNodeCount() : 0;
}

FName ALKBattleGameMode::GetNextNodeId(int32 Index) const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run) { return NAME_None; }
	const TArray<FName> Ids = Run->GetNextNodeIds();
	return Ids.IsValidIndex(Index) ? Ids[Index] : NAME_None;
}

ELKDungeonNodeType ALKBattleGameMode::GetNextNodeType(int32 Index) const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run) { return ELKDungeonNodeType::Battle; }
	const FLKDungeonNode Node = Run->GetNode(GetNextNodeId(Index));
	return Node.NodeId.IsNone() ? ELKDungeonNodeType::Battle : Node.Type;
}

FText ALKBattleGameMode::GetNextNodeTitle(int32 Index) const
{
	switch (GetNextNodeType(Index))
	{
	case ELKDungeonNodeType::Elite: return FText::FromString(TEXT("精英战"));
	case ELKDungeonNodeType::Boss: return FText::FromString(TEXT("首领战"));
	case ELKDungeonNodeType::Rest: return FText::FromString(TEXT("休息营地"));
	default: return FText::FromString(TEXT("普通战"));
	}
}

FText ALKBattleGameMode::GetNextNodeSubtitle(int32 Index) const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run) { return FText::GetEmpty(); }
	const FLKDungeonNode Node = Run->GetNode(GetNextNodeId(Index));
	if (Node.NodeId.IsNone()) { return FText::GetEmpty(); }
	if (Node.Type == ELKDungeonNodeType::Rest)
	{
		return FText::FromString(TEXT("恢复 30% 最大生命（全体英雄）"));
	}

	// 战斗类：列出敌方英雄（动态遭遇快照里的名单）。
	const FLKRunState RunState = Run->GetRunState();
	const FLKEncounterRow* Encounter = LKEncounterContent::Find(RunState.Encounters, Node.EncounterId);
	if (!Encounter) { return FText::GetEmpty(); }
	TArray<FString> Names;
	for (FName HeroId : Encounter->EnemyHeroIds)
	{
		const FLKUnitRow* Row = GetUnitRow(HeroId);
		const FString Name = (Row && !Row->DisplayName.IsEmpty()) ? Row->DisplayName.ToString() : HeroId.ToString();
		Names.Add(Name);
	}
	return FText::FromString(TEXT("敌人：") + FString::Join(Names, TEXT("、")));
}

ELKNodeSelectionResult ALKBattleGameMode::SelectNextNode(int32 Index)
{
	ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run) { return ELKNodeSelectionResult::Rejected; }
	const FName NodeId = GetNextNodeId(Index);
	if (NodeId.IsNone()) { return ELKNodeSelectionResult::Rejected; }
	const ELKNodeSelectionResult Result = Run->SelectNode(NodeId);
	if (Result == ELKNodeSelectionResult::BattleEntered)
	{
		// 进入战斗房：重载战场（由新世界 BeginPlay 消费 PendingBattle）。
		ReloadBattleLevel();
	}
	return Result;
}

bool ALKBattleGameMode::BuildRunRewardOffers(TArray<FLKRunRewardOffer>& OutOffers) const
{
	OutOffers.Reset();
	const ULKRunSubsystem* Run = GetRunSubsystem();
	if (!Run || !Run->CanAdvance()) { return false; }
	const FLKRunState State = Run->GetRunState();

	// 确定性种子：Run.Seed + 已完成战斗数×7919 + 本房奖励档（UI 打开次数与刷新不改变内容）。
	FRandomStream Stream(State.Seed + State.BattleHistory.Num() * 7919 + CurrentEncounter.RewardTier);

	auto Shuffle = [&Stream](TArray<FLKRunRewardOffer>& Items)
	{
		for (int32 i = Items.Num() - 1; i > 0; --i)
		{
			const int32 J = Stream.RandRange(0, i);
			Items.Swap(i, J);
		}
	};

	// 1) 新卡池：骷髅兵 / 骷髅射手（1 费文字卡；只有未持有才可作为加牌候选）
	TArray<FLKRunRewardOffer> NewCards;
	for (const FName Id : { FName(TEXT("Unit_Skeleton")), FName(TEXT("Unit_SkeletonArcher")) })
	{
		if (!FindCard(Id)) { continue; }
		if (State.Cards.ContainsByPredicate([Id](const FLKRunCardState& Card) { return Card.CardId == Id; })) { continue; }
		FLKRunRewardOffer Offer;
		Offer.Kind = ELKRunRewardKind::AddCard;
		Offer.CardId = Id;
		NewCards.Add(Offer);
	}

	// 2) 升级池：持有且"出单位"的卡（Unit/Building；法术没有攻击/生命，不作为升级候选）
	TArray<FLKRunRewardOffer> Upgrades;
	for (const FLKRunCardState& Card : State.Cards)
	{
		const ULKCardDefinition* Def = FindCard(Card.CardId);
		if (!Def || (Def->CardType != ELKCardType::Unit && Def->CardType != ELKCardType::Building)) { continue; }
		const FName UnitId = Def->CardType == ELKCardType::Building ? Def->BuildingUnitId : Def->SpawnUnitId;
		if (UnitId.IsNone() || !GetUnitRow(UnitId)) { continue; }
		FLKRunRewardOffer Offer;
		Offer.Kind = ELKRunRewardKind::UpgradeCard;
		Offer.CardId = Card.CardId;
		Offer.LevelBefore = Card.UpgradeLevel;
		Offer.LevelAfter = Card.UpgradeLevel + 1;
		Upgrades.Add(Offer);
	}

	// 3) 组合：奖励档越高给的新卡越多（tier1 至多 1 张、tier2 至多 2 张），缺额用随机升级补，最终 2~3 项。
	Shuffle(NewCards);
	Shuffle(Upgrades);
	const int32 NewCap = FMath::Clamp(CurrentEncounter.RewardTier, 1, 2);
	for (int32 i = 0; i < NewCards.Num() && OutOffers.Num() < NewCap; ++i) { OutOffers.Add(NewCards[i]); }
	for (int32 i = 0; i < Upgrades.Num() && OutOffers.Num() < 3; ++i) { OutOffers.Add(Upgrades[i]); }
	Shuffle(OutOffers);

	if (OutOffers.Num() == 0)
	{
		UE_LOG(LogLKBattle, Warning, TEXT("[Run] 本房没有可用奖励候选（牌组异常或目录缺失）"));
		return false;
	}
	UE_LOG(LogLKBattle, Log, TEXT("[Run] 生成奖励 %d 项（档 %d，种子 %d）"), OutOffers.Num(), CurrentEncounter.RewardTier, Stream.GetInitialSeed());
	return true;
}

bool ALKBattleGameMode::HasPendingRewardChoice() const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	return Run && Run->HasPendingRewardChoice();
}

int32 ALKBattleGameMode::GetPendingRewardCount() const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	return Run ? Run->GetPendingRewardCount() : 0;
}

FLKRunRewardOffer ALKBattleGameMode::GetRunRewardOffer(int32 Index) const
{
	const ULKRunSubsystem* Run = GetRunSubsystem();
	return Run ? Run->GetPendingRewardOffer(Index) : FLKRunRewardOffer();
}

bool ALKBattleGameMode::ChooseRunReward(int32 Index)
{
	ULKRunSubsystem* Run = GetRunSubsystem();
	return Run && Run->ChooseReward(Index);
}

bool ALKBattleGameMode::SkipRunReward()
{
	ULKRunSubsystem* Run = GetRunSubsystem();
	return Run && Run->SkipReward();
}
