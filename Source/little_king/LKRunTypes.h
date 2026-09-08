#pragma once

#include "CoreMinimal.h"
#include "LKDataTypes.h"
#include "LKRunTypes.generated.h"

/** 远征数据契约。仅保存值、软资源引用和稳定 ID；不保存战场 Actor、ASC 或 GE 句柄。 */
UENUM(BlueprintType)
enum class ELKRunPhase : uint8 { Inactive, ChoosingNode, EnteringBattle, InBattle, ChoosingReward, ResolvingNode, Completed, Failed, Abandoned };
UENUM(BlueprintType)
enum class ELKDungeonNodeType : uint8 { Battle, Elite, Rest, Event, Boss };

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
    BattleEntered	UMETA(DisplayName = "进入战斗")
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SchemaVersion = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid RunId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 12345;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELKRunPhase Phase = ELKRunPhase::Inactive;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CurrentNodeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLKDungeonNode> Nodes;
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
