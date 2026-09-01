#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LKTypes.h"
#include "ULKGameData.generated.h"

class ULKCardDefinition;

/**
 * 全局配置 DataAsset（DA_GameData）。
 * 所有平衡数值集中于此，改数值不动代码。
 */
UCLASS(BlueprintType)
class ULKGameData : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---------- 经济 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0.1"))
	float SilverPerSecond = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "1.0"))
	float SilverCap = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "1"))
	int32 HandSize = 4;

	// ---------- 流程 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow", meta = (ClampMin = "5.0"))
	float DeploymentTime = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow", meta = (ClampMin = "60.0"))
	float BattleTimeLimit = 480.f;

	/** 超时后：每 Tick 对英雄造成最大生命百分比伤害（无平局机制） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow", meta = (ClampMin = "0.0"))
	float OvertimeWeaknessBasePct = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow", meta = (ClampMin = "1.0"))
	float OvertimeWeaknessTick = 5.f;

	/** 每 Tick 虚弱强度增幅（0.02 = 每 5 秒增强 2%） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flow", meta = (ClampMin = "0.0"))
	float OvertimeWeaknessGrowth = 0.02f;

	// ---------- 战场 ----------
	// 轴约定：配合相机 Pitch=-90 俯视，屏幕左右 = 世界 Y，屏幕上下 = 世界 X
	// 玩家左半侧 = Y<=0，敌方右半侧 = Y>=0；中线沿 X 方向（屏幕上为左右分界竖线）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field", meta = (ClampMin = "100.0"))
	float FieldHalfWidth = 1200.f;	// X 方向半宽（屏幕纵向）

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field", meta = (ClampMin = "100.0"))
	float FieldHalfHeight = 2000.f;	// Y 方向半高（屏幕横向）

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field", meta = (ClampMin = "1"))
	int32 MaxHeroesPerTeam = 3;

	/** 同类建筑上限（-1 = 不限；"种类无上限、同类通常<=2"） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field")
	int32 BuildingTypeLimit = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field", meta = (ClampMin = "1.0"))
	float UnitBodyRadius = 50.f;

	/** 攻击停止缓冲（滞回区）：进入攻击判定用 AttackRange，退出攻击判定用 AttackRange+此值，防止射程边界抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field", meta = (ClampMin = "0.0"))
	float AttackStopBuffer = 30.f;

	// ---------- 法术门 ----------
	/** 场上存在法师英雄时，法术卡才可打出（S2 起生效） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	bool bRequireMageForSpells = true;

	// ---------- 弹道 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 3.f;

	// ---------- 调试 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawFieldBounds = true;

	/** 无精灵资源时用色块框绘制单位（占位期可视化） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawDebugShapes = true;

	// ---------- 数据表（编辑器里指定） ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<class UDataTable> UnitTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<class UDataTable> SkillTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<class UDataTable> TraitTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<class UDataTable> WaveTable;

	// ---------- 卡牌库（默认牌库引用的卡必须在此） ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cards")
	TArray<TObjectPtr<ULKCardDefinition>> CardLibrary;

	// ---------- 默认牌库（原型期内置示例，地牢阶段改为动态构筑） ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck")
	TArray<FName> DefaultPlayerDeck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deck")
	TArray<FName> DefaultEnemyDeck;

	/** 确保有可用的默认牌库（编辑器未配置时兜底） */
	void EnsureDefaultDecks();
};
