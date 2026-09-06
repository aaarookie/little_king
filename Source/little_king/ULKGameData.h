#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LKTypes.h"
#include "ULKGameData.generated.h"

class ULKCardDefinition;
class UGameplayAbility;

/** 单个英雄的技能列表（包装结构：UHT 不支持 TMap 值直接嵌套 TArray<TSubclassOf>） */
USTRUCT(BlueprintType)
struct FLKHeroSkillEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills")
	TArray<TSubclassOf<UGameplayAbility>> Abilities;
};

USTRUCT(BlueprintType)
struct FLKHeroTraitEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Traits;
};

/**
 * 全局配置 DataAsset（DA_GameData）。
 * 所有平衡数值集中于此，改数值不动代码。
 */
UCLASS(BlueprintType)
class ULKGameData : public UDataAsset
{
	GENERATED_BODY()

public:
	ULKGameData();
	/** 固定种子用于规则复现；视觉随机独立，不消耗此随机流。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reproducibility")
	int32 BattleSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camps", meta = (ClampMin = "150.0"))
	float HeroCampMoveRadius = 850.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camps", meta = (ClampMin = "10.0"))
	float HeroCampBodyRadius = 65.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traits", meta = (ClampMin = "1.0"))
	float TauntAcquireRadius = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traits", meta = (ClampMin = "1.0"))
	float KnightTauntAuraRadius = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traits")
	TMap<FName, FLKHeroTraitEntry> DefaultHeroTraits;
	/** 当前法师技能为火球系；可在以后内容扩展时调整。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	TArray<FName> FireballSkillHeroIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0.0"))
	float FireballShakeIntensity = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0.0"))
	float AIFocusWarningSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0.0"))
	float AIPushReserveMaxSeconds = 15.f;
	/** 原生 HUD 自动提供血条、营地、放置预览、弹道及飘字。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	bool bNativeDamageText = true;
	// ---------- 经济 ----------
	/** 每秒银币产出（连续累积；1/3 ≈ 每 3 秒涨满 1 个；DA_GameData → Economy 里改） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0.1"))
	float SilverPerSecond = 1.f / 3.f;

	/** 银币上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "1.0"))
	float SilverCap = 5.f;

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

	/** 单阵营场上单位总数上限（英雄+佣兵+建筑；-1 = 不限）。超限出牌/出兵会被拒绝（防单位海卡顿） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field")
	int32 MaxUnitsPerTeam = 20;

	/** 同类建筑上限（-1 = 不限；"种类无上限、同类通常<=2"） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field")
	int32 BuildingTypeLimit = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field", meta = (ClampMin = "1.0"))
	float UnitBodyRadius = 50.f;

	/** 攻击停止缓冲（滞回区）：进入攻击判定用 AttackRange，退出攻击判定用 AttackRange+此值，防止射程边界抖动 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field", meta = (ClampMin = "0.0"))
	float AttackStopBuffer = 30.f;

	// ---------- 法术门 ----------
	/** 弃用兼容配置，不影响新版法术落点规则 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	bool bRequireMageForSpells = false; // 旧资产兼容字段，不参与新版规则。

	// ---------- 弹道 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
	float ProjectileLifetime = 3.f;

	// ---------- 音频（S5） ----------
	/** 音效触发点 ID -> 音频资产；没配的键静默跳过，不报错 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TMap<FName, TSoftObjectPtr<class USoundBase>> SoundMap;

	// ---------- 手感（S5） ----------
	/** 屏幕震动幅度倍率（0 = 关闭震屏） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0.0"))
	float CameraShakeScale = 1.f;

	// ---------- 调试 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawFieldBounds = true;

	/** 无精灵资源时用色块框绘制单位（占位期可视化） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawDebugShapes = true;

	/** 脚下阵营色环（绿=玩家/红=敌方）：有精灵的单位靠它区分敌我；关闭调试形状后仍可单独保留 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawTeamRing = true;

	// ---------- 英雄技能（S3：BP 技能资产按英雄授予） ----------
	/** 英雄 UnitId -> 技能 GA 资产列表；不配 = 该英雄无技能 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills")
	TMap<FName, FLKHeroSkillEntry> HeroAbilityMap;

	// ---------- 敌方 AI（S4） ----------
	/** 集火间隔随机区间（秒）：每轮对玩家血量最低的英雄发起集火 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "5.0"))
	float AIFocusIntervalMin = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "5.0"))
	float AIFocusIntervalMax = 35.f;

	/** 单次集火持续时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "1.0"))
	float AIFocusDuration = 8.f;

	/** 反制评估间隔（秒）：统计玩家近战/远程构成，影响 AI 出牌偏好 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "1.0"))
	float AICounterCheckInterval = 5.f;

	/** 爆发门槛：银币达到该值且己方单位数不劣于玩家时，AI 进入"一波流"连打（应 ≤ 银币上限，否则永远不爆发） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "1.0"))
	float AIPushSilverThreshold = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "2", ClampMax = "5"))
	int32 AIPushMaxCards = 3;

	/** 两次爆发之间的冷却随机区间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "5.0"))
	float AIPushCooldownMin = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "5.0"))
	float AIPushCooldownMax = 35.f;

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
