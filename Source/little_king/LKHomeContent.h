#pragma once

#include "CoreMinimal.h"
#include "LKHomeTypes.h"

class ULKGameData;
class ULKCardDefinition;
class ULKProfileSubsystem;
struct FLKUnitRow;

/**
 * 家园内容与默认数值（代码拥有稳定 ID、用途、解锁资格与等级曲线）。
 *
 * 分工（延续用户认可的配置分工）：C++ 负责"是什么、能不能做"；费用/数值/名称/表现可由配置调整。
 * 神像 1~4 级 40/60/80/100% 与金库 1~5 级曲线为已定/暂定规则，改动按规则修订处理并同步 docs/26、docs/27。
 */
namespace LKHomeContent
{
	int32 TreasuryDepartureGoldCap(int32 Level);
	// ---------- 建筑 ----------
	FName BuildingId(ELKHomeBuilding Building);
	ELKHomeBuilding BuildingFromId(FName BuildingId);
	const TArray<FLKBuildingDefinition>& Buildings();
	const FLKBuildingDefinition* FindBuilding(FName BuildingId);
	ELKHomePanel PanelForBuilding(ELKHomeBuilding Building);
	FText PanelTitle(ELKHomePanel Panel);
	FText BuildingDisplayName(FName BuildingId);

	// ---------- 圣玛丽亚神像（1~4 级，恢复比例 0.4 + 0.2 × (等级-1)） ----------
	constexpr int32 StatueMaxLevel = 4;
	float StatueRecoveryPercent(int32 Level);
	FText StatueRecoveryText(int32 Level);

	// ---------- 金库（1~5 级，产速 ×[1 + 0.1 × (等级-1)]，上限 +（等级-1）） ----------
	constexpr int32 TreasuryMaxLevel = 5;
	float TreasurySpeedMultiplier(int32 Level);
	int32 TreasuryCapBonus(int32 Level);

	/** 升到下一级费用；返回 -1 表示不可升级或已满级 */
	int32 UpgradeCost(FName BuildingId, int32 CurrentLevel);
	/** 某建筑从 1 级升到满级的总花费（0 = 不可升级） */
	int32 TotalUpgradeCost(FName BuildingId);

	// ---------- 区域 ----------
	const TArray<FLKRegionDefinition>& Regions();
	const FLKRegionDefinition* FindRegion(FName RegionId);
	FName DefaultRegionId();

	// ---------- 永久解锁集 ----------
	const TArray<FName>& DefaultUnlockedHeroes();
	const TArray<FName>& DefaultUnlockedCards();
	const TArray<FName>& DefaultUnlockedRegions();
	bool IsDefaultUnlockedHero(FName HeroId);
	bool IsDefaultUnlockedCard(FName CardId);
	bool IsDefaultUnlockedRegion(FName RegionId);
	/** 合法玩家英雄候选（排除敌方死灵法师/骷髅巨人/骷髅王等） */
	bool IsPlayerHeroCandidate(FName HeroId);

	// ---------- 战备 ----------
	int32 MinStartingDeck();
	int32 MaxStartingDeck();
	FLKExpeditionLoadout DefaultLoadout();
	/**
	 * 校验出征配置：恰好 3 名唯一且已解锁英雄；5~8 种唯一且已解锁卡牌。
	 * Data 非空时额外校验卡牌/英雄定义真实存在（家园 GameMode 提交时传入）。
	 */
	ELKLoadoutResult ValidateLoadout(const FLKExpeditionLoadout& Loadout, const ULKGameData* Data, FString& OutError);
	FText LoadoutResultText(ELKLoadoutResult Result);

	// ---------- 金币收益规则 ----------
	FLKHomeRewardRules DefaultRewardRules();
	/** 按遭遇奖励档返回本房胜利金币（档位 <1 按 1 处理） */
	int32 GoldForRewardTier(const FLKHomeRewardRules& Rules, int32 RewardTier);

	// ---------- 家园地图/战斗地图 ----------
	FName DefaultHomeMapName();
	FName DefaultBattleMapName();
	/** 地图资源是否存在（/Game/Maps/<Name>）；不存在时家园给出明确提示而不是切图失败 */
	bool DoesMapExist(FName MapName);

	// ---------- H3：出征输入构造（家园大门与战斗 GameMode 共用，保证同一份校验口径） ----------
	/**
	 * 用永久档的已保存战备组装完整出征输入：校验战备/区域 → 满血英雄 + 0 级卡牌 →
	 * 校验并复制遭遇目录 → 冻结本轮加成快照与收益规则。任何一步失败都不产生部分状态。
	 */
	bool BuildExpeditionStartRequest(const ULKProfileSubsystem& Profile, const ULKGameData* Data, FName RegionId,
		TFunctionRef<const FLKUnitRow*(FName)> ResolveUnitRow,
		TFunctionRef<const ULKCardDefinition*(FName)> ResolveCard,
		FLKExpeditionStartRequest& OutRequest, FString& OutError);
}
