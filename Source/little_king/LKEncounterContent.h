#pragma once

#include "CoreMinimal.h"
#include "LKDataTypes.h"

class UDataTable;
class ULKGameData;
class ULKCardDefinition;

/** D2 遭遇内容入口：内置零资产回退、DT_Encounters 读取、规范化与结构校验。 */
namespace LKEncounterContent
{
	const TMap<FName, FLKEncounterRow>& BuiltInEncounters();
	FName CanonicalId(FName EncounterId);
	const FLKEncounterRow* FindBuiltIn(FName EncounterId);

	/** 校验并规范化单行；RowName 非空时作为稳定 ID，行内冲突会明确失败。 */
	bool NormalizeAndValidate(FName RowName, const FLKEncounterRow& Source, FLKEncounterRow& OutRow, FString& OutError);

	/**
	 * Table 为空时复制三个内置遭遇；配置表时整表为权威来源，并要求固定三房 ID 全部存在。
	 * 额外合法行会一并进入远征快照，供调试和后续路线扩展使用。
	 */
	bool BuildCatalog(const UDataTable* Table, TArray<FLKEncounterRow>& OutRows, FString& OutError);
	const FLKEncounterRow* Find(const TArray<FLKEncounterRow>& Rows, FName EncounterId);

	/**
	 * H3：不依赖战斗世界的目录构造与校验（家园出征与战斗 GameMode 共用）。
	 * ResolveUnitRow / ResolveCard 由调用方提供，保证"自定义 UnitId / 卡牌定义"的口径一致。
	 */
	bool BuildValidatedCatalog(const ULKGameData* Data,
		TFunctionRef<const FLKUnitRow*(FName)> ResolveUnitRow,
		TFunctionRef<const ULKCardDefinition*(FName)> ResolveCard,
		TArray<FLKEncounterRow>& OutRows, FString& OutError);

	// ---------- D4：随机敌阵容与动态遭遇 ----------
	/** 从目录收集敌方"英雄池"与"首领池"（数据驱动：以后往表里加新遭遇行即自动入池）。 */
	void CollectRosterPools(const TArray<FLKEncounterRow>& Catalog, TArray<FName>& OutHeroPool, TArray<FName>& OutBossPool);
	/**
	 * 以模板行（保留波次/AI/经济/奖励档/显示名）为基础生成动态遭遇，只替换敌阵容：
	 * Normal=英雄池随机 1；Elite=随机 2（不同）；Boss=首领池 1 + 英雄池随机 2（不同）。
	 * 池不足时取全部可用；用 Stream 保证同种子同结果。
	 */
	bool MakeDynamicEncounter(FName NewEncounterId, const FLKEncounterRow& Template, ELKEncounterRank Rank,
		const TArray<FName>& HeroPool, const TArray<FName>& BossPool,
		FRandomStream& Stream, FLKEncounterRow& OutRow, FString& OutError);
}
