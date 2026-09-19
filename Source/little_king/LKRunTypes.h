#pragma once

#include "CoreMinimal.h"
#include "LKDataTypes.h"
#include "LKRunTypes.generated.h"

/** 远征数据契约。仅保存值、软资源引用和稳定 ID；不保存战场 Actor、ASC 或 GE 句柄。 */
UENUM(BlueprintType)
enum class ELKRunPhase : uint8 { Inactive, ChoosingNode, EnteringBattle, InBattle, ChoosingReward, ResolvingNode, Completed, Failed, Abandoned };
UENUM(BlueprintType)
enum class ELKDungeonNodeType : uint8 { Battle, Elite, Rest, Event, Boss, Market };

/** 固定世界版图的一个区域；多边形/入口/出口随存档冻结，不随节点随机种子漂移。 */
USTRUCT(BlueprintType)
struct FLKWorldRegion
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector2D> Polygon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> NextRegionIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> EntryNodeIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> ExitNodeIds;
};

/** H0：出征队伍 + 初始牌组（战备处保存；不含局内强化与伤势） */
USTRUCT(BlueprintType)
struct FLKExpeditionLoadout
{
	GENERATED_BODY()

	/** 恰好 3 个，按 HeroId 唯一，必须永久解锁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> HeroIds;
	/** 5~8 种不同 CardId；顺序仅用于展示，不承诺开局抽到顺序 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> CardIds;

	bool IsEmpty() const { return HeroIds.IsEmpty() && CardIds.IsEmpty(); }
};

/**
 * H0：本轮远征冻结的家园加成（创建 RunId 时快照，整轮使用同一份）。
 * 不在下一房按当前建筑等级重算——回家升级只影响下一轮新远征。
 */
USTRUCT(BlueprintType)
struct FLKMetaBonusSnapshot
{
	GENERATED_BODY()

	/** 战后恢复比例（神像：0.4/0.6/0.8/1.0） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeroRecoveryPercent = 0.4f;
	/** 玩家每场战斗银币产速（已含金库加成；基础值来自 DA_GameData） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PlayerSilverPerSecond = 1.f / 3.f;
	/** 玩家每场战斗银币上限（已含金库加成） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PlayerSilverCap = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StatueLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TreasuryLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
	/** 规则版本：数值表调整后旧档仍按快照复现，不重算 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RuleVersion = 1;
};

/** H4：金币收益规则（本轮快照；调表后读档不重算金额） */
USTRUCT(BlueprintType)
struct FLKHomeRewardRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NormalWin = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 EliteWin = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BossWin = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RunCompletedBonus = 20;
	/** 旧收益规则兼容字段；Schema 6 终态固定全额带回钱包 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FailureKeepPercent = 1.f;
	/** 旧放弃规则兼容字段；新轮不再按百分比折算 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AbandonKeepPercent = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RuleVersion = 2;
};

/** H4：远征终态的金币结算回执（先冻结在 Run 档，再由 Profile 幂等入账） */
USTRUCT(BlueprintType)
struct FLKSettlementReceipt
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid SettlementId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid RunId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid ProfileId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GoldAmount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ELKRunPhase TerminalPhase = ELKRunPhase::Inactive;
	/** 本轮是否参与家园金币结算（v0.6 旧档迁移标记为 false，避免追算与重复赠送） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEligibleForHomeReward = true;
	/** Profile 已完成入账（回执交接状态；重启按同一 SettlementId 重放） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bProfileApplied = false;
};

USTRUCT(BlueprintType)
struct FLKRunHeroState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HeroId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 1.f;
	/** 永久基础最大生命；特性加成在新房 Actor 上重新计算。0 仅兼容 D1 旧快照并按 MaxHealth 迁移。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseMaxHealth = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Traits;
};

USTRUCT(BlueprintType)
struct FLKRunCardState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CardId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 UpgradeLevel = 0;
};

/** H3：一次新远征的完整输入（家园大门组装，第一次 AutoSave 之前全部就位） */
USTRUCT(BlueprintType)
struct FLKExpeditionStartRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid ProfileId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKExpeditionLoadout Loadout;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKRunHeroState> Heroes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKRunCardState> Cards;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKEncounterRow> Encounters;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKMetaBonusSnapshot BonusSnapshot;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKHomeRewardRules RewardRules;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 12345;
	/** 正式远征使用五区域地图；旧签名仅保留小图作兼容回归。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseWorldMap = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StartingGold = 0;
	/** 本轮是否参与家园金币结算（独立测试/调试轮可关闭） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEligibleForHomeReward = true;
};

/** D3 房间胜利奖励种类 */
UENUM(BlueprintType)
enum class ELKRunRewardKind : uint8
{
    /** 已有卡牌升级：该卡生成单位的攻击与生命 +10%（UpgradeLevel +1） */
    UpgradeCard		UMETA(DisplayName = "升级卡牌"),
    /** 将一张新卡加入远征牌组（未持有时才可作为候选） */
    AddCard			UMETA(DisplayName = "加入新卡")
};

/** 单个待选奖励的展示与结算数据；由 GameMode 按遭遇奖励档与独立种子生成，RunSubsystem 原子消费 */
USTRUCT(BlueprintType)
struct FLKRunRewardOffer
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELKRunRewardKind Kind = ELKRunRewardKind::UpgradeCard;
    /** 升级目标卡或新卡 ID（骷髅兵 = Unit_Skeleton、骷髅射手 = Unit_SkeletonArcher） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CardId;
    /** 升级前等级（仅 UpgradeCard 有效；AddCard 恒为 0） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LevelBefore = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LevelAfter = 0;
};

/** D4 节点选择结果 */
UENUM(BlueprintType)
enum class ELKNodeSelectionResult : uint8
{
    /** 选择被拒绝（阶段/邻接/已结算不合法） */
    Rejected		UMETA(DisplayName = "拒绝"),
    /** 休息节点：已应用回血并停留在选择阶段（继续显示下一排） */
    RestResolved	UMETA(DisplayName = "休息结算"),
    /** 战斗节点：已进入该房（世界将重载开战） */
    BattleEntered	UMETA(DisplayName = "进入战斗"),
	ServiceEntered UMETA(DisplayName = "进入非战斗节点")
};

USTRUCT(BlueprintType)
struct FLKDungeonNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELKDungeonNodeType Type = ELKDungeonNodeType::Battle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EncounterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> NextNodeIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bResolved = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D MapPosition = FVector2D::ZeroVector;
	/** 区域内从左向右的层号，跨区边必须进入下游区域的入口。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Layer = 0;
};

USTRUCT(BlueprintType)
struct FLKBattleContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bExpedition = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid RunId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName NodeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid AttemptId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 12345;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EncounterId;
	/** D2 的完整遭遇快照；运行中不再读取共享 DataTable。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKEncounterRow Encounter;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKRunHeroState> PlayerHeroes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> EnemyHeroIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKRunCardState> PlayerCards;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> EnemyCards;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnemyUsesCards = true;
    /** H3：本房需要生效的家园加成（整轮同一份快照；战斗不再读当前建筑等级） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKMetaBonusSnapshot BonusSnapshot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
};

USTRUCT(BlueprintType)
struct FLKHeroBattleOutcome
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName InstanceId;
    UPROPERTY(BlueprintReadOnly) bool bWasIncapacitated = false;
    UPROPERTY(BlueprintReadOnly) float HealthBeforeRecovery = 0.f;
    UPROPERTY(BlueprintReadOnly) FLKRunHeroState RecoveredState;
};

USTRUCT(BlueprintType)
struct FLKBattleOutcome
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid RunId;
    UPROPERTY(BlueprintReadOnly) FName NodeId;
    UPROPERTY(BlueprintReadOnly) FGuid AttemptId;
    UPROPERTY(BlueprintReadOnly) bool bFinalized = false;
    UPROPERTY(BlueprintReadOnly) FLKMatchStats Stats;
    UPROPERTY(BlueprintReadOnly) TArray<FLKHeroBattleOutcome> PlayerHeroes;
    UPROPERTY(BlueprintReadOnly) TArray<FLKHeroBattleOutcome> EnemyHeroes;
};

USTRUCT(BlueprintType)
struct FLKRunState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SchemaVersion = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid RunId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 12345;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELKRunPhase Phase = ELKRunPhase::Inactive;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CurrentNodeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKDungeonNode> Nodes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WorldMapVersion = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKWorldRegion> WorldRegions;
	/** 远征钱包：出发携带 + 节点收益 - 已花费；不受出发携带上限限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StartingGold = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WalletGold = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGuid> ProcessedWalletTransactions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> VisitedNodeIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKRunHeroState> Heroes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKRunCardState> Cards;
	/** 远征开始时复制的已校验遭遇目录，避免运行中资产变化污染本轮。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKEncounterRow> Encounters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKBattleContext PendingBattle;
    /** D3：当前待选奖励批次（生成一次存定，UI 只读；领取/跳过后清空）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid PendingRewardBatchId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKRunRewardOffer> PendingRewardOffers;
    /** D3：当前节点是否已发过奖励批次（同一次胜利只允许发一批，防止刷奖励） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRewardOfferedForCurrentNode = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGuid> ProcessedAttemptIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGuid> ClaimedRewardBatchIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKBattleOutcome> BattleHistory;

    // ---------- H3/H4：家园关联（Schema 5） ----------
    /** 所属永久档；为空表示独立测试/调试轮（不参与家园金币结算） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid ProfileId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId;
    /** 出征时的战备快照；不随局内奖励加牌改写 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKExpeditionLoadout InitialLoadout;
    /** 本轮冻结的家园加成（战后恢复、玩家银币产速/上限） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKMetaBonusSnapshot BonusSnapshot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKHomeRewardRules RewardRules;
    /** 本轮已暂存金币（每房胜利累加；终态按规则结算） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PendingGold = 0;
    /** 终态冻结的结算回执；未终态时 SettlementId 无效 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLKSettlementReceipt PendingSettlement;
    /** 本轮是否参与家园金币结算（v0.6 旧档迁移后为 false，不追算） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHomeRewardEligible = true;
};

namespace LKRunRules
{
    inline float RecoveredHealth(float Current, float Maximum, float Percent = 0.4f)
    {
        if (!FMath::IsFinite(Maximum) || Maximum <= 0.f) { return 0.f; }
        Current = FMath::IsFinite(Current) ? FMath::Clamp(Current, 0.f, Maximum) : 0.f;
        Percent = FMath::IsFinite(Percent) ? FMath::Clamp(Percent, 0.f, 1.f) : 0.4f;
        return FMath::Min(Maximum, Current + Maximum * Percent);
    }
}
