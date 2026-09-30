#pragma once

#include "CoreMinimal.h"
#include "LKRunTypes.h"

/**
 * 第一版战斗数值平衡的唯一规则真源（docs/48）。
 *
 * 关键约束：
 *  - 难度按"地理拓扑深度"推进，不按区域数组序号；两平行分支同深度同难度。
 *  - 倍率只作用于敌方基础生命与攻击，且每个实例只应用一次；复活按实例最大生命。
 *  - 曲线（地图深度）与遭遇预算（普通/精英/Boss 英雄份额）分开，最终实例不重复乘算。
 */
namespace LKBalanceRules
{
	/** 平衡规则版本：写进远征与遭遇快照；读档据此做一次性迁移，避免二次缩放。 */
	constexpr int32 CurrentBalanceVersion = 1;
	/** Balance V1: player approved lowering sacrifice after measured short battles. */
	constexpr float SacrificeHealthPercent = 0.03f;

	/** 每区层数（L∈[0, LayersPerRegion-1]）。 */
	constexpr int32 LayersPerRegion = 5;
	/** 现版世界地图从起点出发的最长有向深度（区域深度 3 × 5 + 层 4）。 */
	constexpr int32 MaxTopologyDepth = 19;

	/** 深度曲线：敌方生命 1.00~2.00；敌方攻击 1.00~1.50。 */
	constexpr float HealthLinear = 0.60f;
	constexpr float HealthQuadratic = 0.40f;
	constexpr float DamageLinear = 0.30f;
	constexpr float DamageQuadratic = 0.20f;

	/** 遭遇预算（独立于进度曲线）：精英每人 0.5，Boss 随从 0.3，自身 1.0（包含多条生命的实测预算）。 */
	constexpr float EliteHeroBudgetScale = 0.50f;
	constexpr float BossMinionBudgetScale = 0.30f;

	// ---------- 地理拓扑深度 ----------
	/** 区域在"起点可达"有向图上的最长距离；起点区域为 0。非法/未知区域返回 0。 */
	int32 ComputeRegionDepth(const TArray<FLKWorldRegion>& Regions, FName RegionId);
	void ComputeRegionDepths(const TArray<FLKWorldRegion>& Regions, TMap<FName, int32>& OutDepths);
	/** 全局深度 d = 5 × 区域深度 + 层号（层号被夹到 0~4）。 */
	int32 NodeDepth(int32 RegionDepth, int32 Layer);
	/** p = Clamp(d / 19, 0, 1)。 */
	float ProgressForDepth(int32 Depth);
	float EnemyHealthScaleForProgress(float Progress);
	float EnemyDamageScaleForProgress(float Progress);
	/** 一个深度上的通用敌方倍率（不含遭遇英雄预算）。 */
	void ScalesForDepth(int32 Depth, float& OutHealthScale, float& OutDamageScale);

	// ---------- 遭遇快照 ----------
	/** 用深度曲线与遭遇预算填充一行遭遇快照（幂等）。深度被夹到 0~MaxTopologyDepth。 */
	void ApplySnapshot(FLKEncounterRow& Row, int32 Depth);
	/** 表行/内置行缺少快照（BalanceVersion=0）时补齐为版本 1 的中性快照：深度 0 + 遭遇预算。 */
	void EnsureSnapshot(FLKEncounterRow& Row);
	/** 单个敌方单位在本遭遇里的实例倍率（英雄按名单下标取预算×曲线；其余走通用曲线）。 */
	void InstanceScalesFor(const FLKEncounterRow& Row, FName UnitId, ELKUnitClass UnitClass,
		float& OutHealthScale, float& OutDamageScale);
	/** 快照字段有限性与规模校验（读档/目录校验用）。 */
	bool ValidateSnapshot(const FLKEncounterRow& Row, FString& OutError);

	// ---------- 增援节奏（内置行与 DT_Encounters 共用同一套数值） ----------
	float ReinforcementInterval(ELKEncounterRank Rank);
	float ReinforcementLastTime(ELKEncounterRank Rank);
	float CircleCooldown(ELKEncounterRank Rank);
	float EnemySilverRate(ELKEncounterRank Rank);
	/** 初始 4 近战 + 2 远程，之后每波 2 近战 + 1 远程直到 LastTime；每行 ≤ 62 条。 */
	void BuildReinforcementWaves(ELKEncounterRank Rank, TArray<FLKWaveEntry>& OutWaves);
	/** 校验波次预算（条数上限、时间单调、单位非空、数量合法）。 */
	bool ValidateReinforcementWaves(const TArray<FLKWaveEntry>& Waves, FString& OutError);
}
