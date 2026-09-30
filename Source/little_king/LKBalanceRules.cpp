#include "LKBalanceRules.h"

#include "LKUnitContent.h"

namespace
{
	constexpr int32 MaxWaveEntries = 64;

	/**
	 * 从起点出发的最长有向距离：先求"到起点"的最长路径。
	 * 区域图很小（5 个节点），直接对反图做记忆化搜索。
	 */
	int32 LongestDistanceFromStart(const TArray<FLKWorldRegion>& Regions, FName RegionId)
	{
		if (Regions.IsEmpty() || RegionId.IsNone()) { return 0; }
		TMap<FName, const FLKWorldRegion*> ById;
		for (const FLKWorldRegion& Region : Regions) { ById.Add(Region.RegionId, &Region); }
		if (!ById.Contains(RegionId)) { return 0; }

		TMap<FName, int32> Memo;
		TMap<FName, TArray<FName>> Predecessors;
		for (const FLKWorldRegion& Region : Regions)
		{
			for (FName Next : Region.NextRegionIds) { Predecessors.FindOrAdd(Next).Add(Region.RegionId); }
		}
		TFunction<int32(FName, TSet<FName>&)> Depth = [&](FName Id, TSet<FName>& Visiting) -> int32
		{
			if (const int32* Cached = Memo.Find(Id)) { return *Cached; }
			if (Visiting.Contains(Id)) { return 0; }
			Visiting.Add(Id);
			int32 Best = 0;
			if (const TArray<FName>* Parents = Predecessors.Find(Id))
			{
				for (FName Parent : *Parents)
				{
					if (!ById.Contains(Parent)) { continue; }
					Best = FMath::Max(Best, 1 + Depth(Parent, Visiting));
				}
			}
			Visiting.Remove(Id);
			Memo.Add(Id, Best);
			return Best;
		};
		TSet<FName> Visiting;
		return Depth(RegionId, Visiting);
	}
}

namespace LKBalanceRules
{
int32 ComputeRegionDepth(const TArray<FLKWorldRegion>& Regions, FName RegionId)
{
	return LongestDistanceFromStart(Regions, RegionId);
}

void ComputeRegionDepths(const TArray<FLKWorldRegion>& Regions, TMap<FName, int32>& OutDepths)
{
	OutDepths.Reset();
	for (const FLKWorldRegion& Region : Regions)
	{
		OutDepths.Add(Region.RegionId, ComputeRegionDepth(Regions, Region.RegionId));
	}
}

int32 NodeDepth(int32 RegionDepth, int32 Layer)
{
	return LayersPerRegion * FMath::Max(0, RegionDepth) + FMath::Clamp(Layer, 0, LayersPerRegion - 1);
}

float ProgressForDepth(int32 Depth)
{
	return FMath::Clamp(static_cast<float>(Depth) / static_cast<float>(MaxTopologyDepth), 0.f, 1.f);
}

float EnemyHealthScaleForProgress(float Progress)
{
	const float P = FMath::Clamp(Progress, 0.f, 1.f);
	return 1.f + HealthLinear * P + HealthQuadratic * P * P;
}

float EnemyDamageScaleForProgress(float Progress)
{
	const float P = FMath::Clamp(Progress, 0.f, 1.f);
	return 1.f + DamageLinear * P + DamageQuadratic * P * P;
}

void ScalesForDepth(int32 Depth, float& OutHealthScale, float& OutDamageScale)
{
	const float Progress = ProgressForDepth(Depth);
	OutHealthScale = EnemyHealthScaleForProgress(Progress);
	OutDamageScale = EnemyDamageScaleForProgress(Progress);
}

void ApplySnapshot(FLKEncounterRow& Row, int32 Depth)
{
	const int32 ClampedDepth = FMath::Clamp(Depth, 0, MaxTopologyDepth);
	Row.BalanceVersion = CurrentBalanceVersion;
	Row.Depth = ClampedDepth;
	ScalesForDepth(ClampedDepth, Row.EnemyHealthScale, Row.EnemyDamageScale);
	Row.EnemyHeroHealthScale.Reset();
	for (int32 Index = 0; Index < Row.EnemyHeroIds.Num(); ++Index)
	{
		const FName HeroId = Row.EnemyHeroIds[Index];
		// Boss 判定：名单首位且遭遇为 Boss，或单位注册表类别为 Boss（与 CollectRosterPools 同口径）。
		const FLKUnitRow* Unit = LKUnitContent::Find(HeroId);
		const bool bIsBossUnit = (Row.Rank == ELKEncounterRank::Boss && Index == 0)
			|| (Unit && Unit->UnitClass == ELKUnitClass::Boss);
		float Budget = 1.f;
		if (Row.Rank == ELKEncounterRank::Elite) { Budget = EliteHeroBudgetScale; }
		else if (Row.Rank == ELKEncounterRank::Boss && !bIsBossUnit) { Budget = BossMinionBudgetScale; }
		Row.EnemyHeroHealthScale.Add(Budget * Row.EnemyHealthScale);
	}
}

void EnsureSnapshot(FLKEncounterRow& Row)
{
	if (Row.BalanceVersion >= CurrentBalanceVersion
		&& Row.EnemyHeroHealthScale.Num() == Row.EnemyHeroIds.Num())
	{
		return;
	}
	// 表行/单场调试：没有写入深度时按深度 0（曲线 1.0）补齐，只保留遭遇预算差异。
	ApplySnapshot(Row, FMath::Clamp(Row.Depth, 0, MaxTopologyDepth));
}

void InstanceScalesFor(const FLKEncounterRow& Row, FName UnitId, ELKUnitClass UnitClass,
	float& OutHealthScale, float& OutDamageScale)
{
	OutHealthScale = 1.f;
	OutDamageScale = 1.f;

	float BaseHealthScale = FMath::IsFinite(Row.EnemyHealthScale) && Row.EnemyHealthScale > 0.f ? Row.EnemyHealthScale : 1.f;
	float BaseDamageScale = FMath::IsFinite(Row.EnemyDamageScale) && Row.EnemyDamageScale > 0.f ? Row.EnemyDamageScale : 1.f;
	if (Row.BalanceVersion < CurrentBalanceVersion)
	{
		// 未迁移的旧快照按中性倍率运行，绝不在运行时临时放大（避免半迁移）。
		BaseHealthScale = 1.f;
		BaseDamageScale = 1.f;
	}
	OutDamageScale = BaseDamageScale;

	const bool bHeroLike = UnitClass == ELKUnitClass::Hero || UnitClass == ELKUnitClass::Boss;
	if (!bHeroLike) { OutHealthScale = BaseHealthScale; return; }

	const int32 Index = Row.EnemyHeroIds.IndexOfByKey(UnitId);
	if (Index != INDEX_NONE && Row.EnemyHeroHealthScale.IsValidIndex(Index)
		&& FMath::IsFinite(Row.EnemyHeroHealthScale[Index]) && Row.EnemyHeroHealthScale[Index] > 0.f)
	{
		OutHealthScale = Row.EnemyHeroHealthScale[Index];
		return;
	}
	// 名单外的英雄（召唤/调试）：只吃曲线，不再叠加预算。
	OutHealthScale = BaseHealthScale;
}

bool ValidateSnapshot(const FLKEncounterRow& Row, FString& OutError)
{
	OutError.Reset();
	if (Row.BalanceVersion != 0 && Row.BalanceVersion != CurrentBalanceVersion)
	{
		OutError = FString::Printf(TEXT("遭遇 %s 的 BalanceVersion=%d 不受支持（当前 %d）"),
			*Row.EncounterId.ToString(), Row.BalanceVersion, CurrentBalanceVersion);
		return false;
	}
	if (Row.Depth < 0 || Row.Depth > MaxTopologyDepth)
	{
		OutError = FString::Printf(TEXT("遭遇 %s 的深度 %d 超出 0~%d"), *Row.EncounterId.ToString(), Row.Depth, MaxTopologyDepth);
		return false;
	}
	if (Row.BalanceVersion == CurrentBalanceVersion)
	{
		if (!FMath::IsFinite(Row.EnemyHealthScale) || !FMath::IsFinite(Row.EnemyDamageScale)
			|| Row.EnemyHealthScale <= 0.f || Row.EnemyDamageScale <= 0.f
			|| Row.EnemyHealthScale > 8.f || Row.EnemyDamageScale > 8.f)
		{
			OutError = FString::Printf(TEXT("遭遇 %s 的敌方倍率非法（生命 %.4f / 攻击 %.4f）"),
				*Row.EncounterId.ToString(), Row.EnemyHealthScale, Row.EnemyDamageScale);
			return false;
		}
		if (Row.EnemyHeroHealthScale.Num() != Row.EnemyHeroIds.Num())
		{
			OutError = FString::Printf(TEXT("遭遇 %s 的英雄生命预算条目 %d 与名单 %d 不一致"),
				*Row.EncounterId.ToString(), Row.EnemyHeroHealthScale.Num(), Row.EnemyHeroIds.Num());
			return false;
		}
		for (float Scale : Row.EnemyHeroHealthScale)
		{
			if (!FMath::IsFinite(Scale) || Scale <= 0.f || Scale > 8.f)
			{
				OutError = FString::Printf(TEXT("遭遇 %s 的英雄生命倍率非法（%.4f）"), *Row.EncounterId.ToString(), Scale);
				return false;
			}
		}
	}
	return true;
}

float ReinforcementInterval(ELKEncounterRank Rank)
{
	switch (Rank)
	{
	case ELKEncounterRank::Elite: return 7.f;
	case ELKEncounterRank::Boss: return 6.f;
	default: return 8.f;
	}
}

float ReinforcementLastTime(ELKEncounterRank Rank)
{
	switch (Rank)
	{
	case ELKEncounterRank::Elite: return 175.f;
	case ELKEncounterRank::Boss: return 180.f;
	default: return 176.f;
	}
}

float CircleCooldown(ELKEncounterRank Rank)
{
	switch (Rank)
	{
	case ELKEncounterRank::Elite: return 17.f;
	case ELKEncounterRank::Boss: return 14.f;
	default: return 20.f;
	}
}

float EnemySilverRate(ELKEncounterRank Rank)
{
	switch (Rank)
	{
	case ELKEncounterRank::Elite: return 0.40f;
	case ELKEncounterRank::Boss: return 0.45f;
	default: return 0.35f;
	}
}

void BuildReinforcementWaves(ELKEncounterRank Rank, TArray<FLKWaveEntry>& OutWaves)
{
	OutWaves.Reset();
	auto Add = [&OutWaves](float Time, FName UnitId, int32 Count)
	{
		FLKWaveEntry Entry;
		Entry.Time = Time;
		Entry.UnitId = UnitId;
		Entry.Count = Count;
		OutWaves.Add(Entry);
	};
	// 初始：4 近战 + 2 远程；随后每波 2 近战 + 1 远程，直到 LastTime。
	Add(0.f, "Unit_Skeleton", 4);
	Add(0.f, "Unit_SkeletonArcher", 2);
	const float Interval = ReinforcementInterval(Rank);
	const float Last = ReinforcementLastTime(Rank);
	for (float Time = Interval; Time <= Last + KINDA_SMALL_NUMBER; Time += Interval)
	{
		Add(Time, "Unit_Skeleton", 2);
		Add(Time, "Unit_SkeletonArcher", 1);
	}
}

bool ValidateReinforcementWaves(const TArray<FLKWaveEntry>& Waves, FString& OutError)
{
	OutError.Reset();
	if (Waves.IsEmpty() || Waves.Num() > MaxWaveEntries)
	{
		OutError = FString::Printf(TEXT("增援条目必须在 1~%d（当前 %d）"), MaxWaveEntries, Waves.Num());
		return false;
	}
	float Previous = -1.f;
	for (const FLKWaveEntry& Entry : Waves)
	{
		if (!FMath::IsFinite(Entry.Time) || Entry.Time < 0.f || Entry.Time < Previous)
		{
			OutError = TEXT("增援时间非法或未按时间排序");
			return false;
		}
		if (Entry.UnitId.IsNone() || Entry.Count < 1 || Entry.Count > 50)
		{
			OutError = TEXT("增援单位为空或数量超出 1~50");
			return false;
		}
		Previous = Entry.Time;
	}
	return true;
}
}
