#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LKTypes.h"
#include "LKDataTypes.h"
#include "LKSkeletonCircle.generated.h"

class ALKBattleGameMode;
class ALKUnitBase;

/**
 * 敌方专属区域法术【骷髅法阵】的战场实例。
 *
 * 时间轴：预警 WarnSeconds（不计入持续，可躲避）→ 生效 Duration 秒，
 * 从生效开始每 SummonInterval 秒“尝试”一次召唤（t=0.5 … 3.0，最多 MaxSummons，不在 t=0 多召）。
 * 尝试次数与成功次数分开：时间点失败也消耗该次；帧率低时补处理跨过的时间点，
 * 但永不超过 MaxSummons，也绝不越过持续时间重试到成功为止；最后一次在到期清理前处理。
 *
 * 区域状态：只影响“敌对且存活可战斗”的单位；按来源登记，离开立即解除，多个法阵取最强减速。
 * 召唤落点：圆盘均匀采样（r=R√u, θ=2πv），半径扣除单位体积，避开实体与边界，每次最多 PlacementAttempts 次。
 * 随机：只由调用方提供的 InSeed 决定（每次施法派生种子由调用方负责），不使用全局随机。
 * 身份：状态来源用本 World 内唯一的 Actor FName，不再使用跨 World 共享的全局序号。
 */
UCLASS()
class ALKSkeletonCircle : public AActor
{
	GENERATED_BODY()

public:
	ALKSkeletonCircle();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 由 GameMode 扣费成功后调用；记录参数并进入预警。返回是否成功初始化。 */
	bool InitializeCircle(ALKBattleGameMode* InGameMode, ELKTeam InTeam, const FVector& InCenter,
		const FLKSkeletonCircleParams& InParams, int32 InSeed, FName InCardId);

	bool IsWarned() const { return bWarning; }
	bool IsFinished() const { return bFinished; }
	float GetWarningRemaining() const { return WarningRemaining; }
	float GetActiveRemaining() const { return ActiveRemaining; }
	/** 生效阶段已推进的时间（不含预警，钳制在 [0, Duration]）。 */
	float GetActiveElapsed() const { return ActiveElapsed; }
	/** 已消耗的召唤时间点数量（成功 + 失败）；与成功次数分开，用于验证“失败也消耗该时间点”。 */
	int32 GetSummonAttempts() const { return SummonAttempts; }
	int32 GetSummonCount() const { return SummonCount; }
	int32 GetSkippedSummons() const { return SkippedSummons; }
	int32 GetMaxSummons() const { return Params.MaxSummons; }
	FVector GetCenter() const { return Center; }
	float GetRadius() const { return Params.Radius; }
	ELKTeam GetCasterTeam() const { return CasterTeam; }
	FName GetSourceId() const { return SourceId; }
	FName GetCardId() const { return CardId; }
	/** 当前受本法阵减速的单位（只读，测试与调试用）。 */
	TArray<TWeakObjectPtr<ALKUnitBase>> GetAffectedUnits() const;

	/**
	 * 兼容入口：已移除跨 World 共享序号，保留为空操作。
	 * 法阵身份改用本 World 内唯一的 Actor FName；随机流由 InitializeCircle 的 InSeed 决定。
	 */
	static void ResetSerial();

	/** 同一时间只有一个法阵时用于测试的显式推进（不依赖世界 Tick 频率）。 */
	void AdvanceForTest(float DeltaSeconds) { Tick(DeltaSeconds); }

protected:
	/** 进入生效阶段：建立持续计时与召唤节点。 */
	void EnterActive();
	/** 结束并清理区域状态（解除减速后销毁自身）。 */
	void Finish();
	/** 解除本法阵来源在全部受影响单位上的减速；离开区域/结束/销毁共用。 */
	void ClearAreaSlow();
	/** 处理到点的召唤节点（含低帧率补齐，最多 MaxSummons 次尝试，失败不补发）。 */
	void ProcessSummonSteps();
	/** 刷新区域内敌对单位的减速状态；离开的单位立即解除。FrameDelta 用于低帧率刷新时长。 */
	void RefreshAreaSlow(float FrameDelta);
	/** 在区域内找一个合法落点并召唤一个骷髅；失败返回 false（不穿模、不在圈外补发）。 */
	bool TrySummonOne();
	void DrawCirclePresentation() const;

private:
	UPROPERTY(Transient) TObjectPtr<ALKBattleGameMode> GameMode;

	FLKSkeletonCircleParams Params;
	ELKTeam CasterTeam = ELKTeam::Enemy;
	FVector Center = FVector::ZeroVector;
	FName CardId;
	FName SourceId;
	FRandomStream Stream;

	bool bInitialized = false;
	bool bWarning = true;
	bool bFinished = false;
	float WarningRemaining = 0.f;
	float ActiveRemaining = 0.f;
	float ActiveElapsed = 0.f;
	float NextSummonAt = 0.f;
	int32 SummonAttempts = 0;
	int32 SummonCount = 0;
	int32 SkippedSummons = 0;
	TSet<TWeakObjectPtr<ALKUnitBase>> AffectedUnits;
};
