#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LKTypes.h"
#include "LKDataTypes.generated.h"

/** 特性修改器（原型期用于光环/羁绊，S4 接入） */
USTRUCT(BlueprintType)
struct FLKTraitModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	FName StatName = TEXT("AttackDamage");	// Health / MaxHealth / MoveSpeed / AttackRange / AttackDamage / AttackInterval

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Value = 0.f;						// 正值=加成，负值=削弱（百分比乘区，1.0 = +100%）

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AuraRadius = 0.f;					// 0 = 作用于自身；>0 = 光环，作用于范围内友军
};

/**
 * DT_Units 行：单位属性与表现参数。
 * 内置 ID 的玩法身份字段由 LKUnitContent 在运行时校正；自定义 ID 完全按表读取。
 */
USTRUCT(BlueprintType)
struct FLKUnitRow : public FTableRowBase
{
	GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") ELKQuality Quality = ELKQuality::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") ELKRace Race = ELKRace::Human;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") ELKActiveAbility ActiveAbility = ELKActiveAbility::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "1", ClampMax = "8")) int32 DeckSlots = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") bool bTargetsBuildingsOnly = false;
    /** 普攻治疗受伤友军；职业行为，不占用技能或特性。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") bool bBasicAttackHeals = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0.1")) float EmpowerDuration = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "1")) float EmpowerMoveMultiplier = 1.3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0.1", ClampMax = "1")) float EmpowerIntervalMultiplier = .75f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0")) float SkillDamageMultiplier = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0", ClampMax = "1")) float SkillSilverChance = .3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0")) float SkillDashDistance = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "1")) float SkillDashSpeed = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0")) float SkillHitRadius = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0")) float SkillKnockbackDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0"))
	float BaseHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float MoveSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float AttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float AttackInterval = 1.0f;

	/** 攻击前摇（秒）：进入攻击到命中结算的延迟（S5 打击感节奏；>0.25 会明显迟滞） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float AttackWindup = 0.15f;

	/** 索敌范围（世界单位）：范围内出现敌人才自动锁定战斗；0 = 用 DA_GameData 的 UnitAcquireRadius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float AcquireRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELKAttackType AttackType = ELKAttackType::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ProjectileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELKUnitClass UnitClass = ELKUnitClass::Soldier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bSkeleton = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ELKPassiveAbility PassiveAbility = ELKPassiveAbility::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0")) float PassiveHealPercent = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 RevivalInitialThreshold = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 RevivalThresholdStep = 5;
	/** Alpha=0 使用阵营色；无精灵时由原生 HUD 画方框。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor PlaceholderColor = FLinearColor::Transparent;

	/** 英雄特性 ID 列表（对应 DT_Traits） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> HeroTraits;

	/** 法师身份兼容标记；全场施法由独立特性决定 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsMage = false;

	/** 英雄技能冷却（秒）；0 = 用英雄类默认值（ALKUnitHero::SkillCooldownSeconds=5） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float SkillCooldown = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELKBuildingBehavior BuildingBehavior = ELKBuildingBehavior::None;

	/** 兵营产出的单位 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SpawnUnitId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0"))
	float SpawnInterval = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<class UPaperSprite> Sprite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D SpriteScale = FVector2D(1.f, 1.f);
};

/** DT_Skills 行：技能表（英雄技能/法术，S4 起由 GAS Ability 承载） */
USTRUCT(BlueprintType)
struct FLKSkillRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SkillName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Cooldown = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsPassive = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Description;
};

/** DT_Traits 行：特性表（光环/羁绊） */
USTRUCT(BlueprintType)
struct FLKTraitRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TraitId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText TraitName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELKTraitEffect Effect = ELKTraitEffect::Attributes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0")) float EffectValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0"))
	float EffectRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FLKTraitModifier> Modifiers;
};

/** DT_Waves 行：敌方波次表 */
USTRUCT(BlueprintType)
struct FLKWaveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Count = 1;
};

/** 运行时波次条目（从 DT_Waves 读出或使用内置示例） */
USTRUCT(BlueprintType)
struct FLKWaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Count = 1;
};

/** 单场遭遇的 AI 节奏。卡牌开关关闭时，反制/爆发参数保留但不会运行。 */
USTRUCT(BlueprintType)
struct FLKEncounterAISettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEnableFocus = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEnableCounter = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEnablePush = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float FocusWarningSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0")) float FocusIntervalMin = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0")) float FocusIntervalMax = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float FocusDuration = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float CounterCheckInterval = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float PushSilverThreshold = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "5")) int32 PushMaxCards = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float PushCooldownMin = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float PushCooldownMax = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float PushReserveMaxSeconds = 15.f;
};

/**
 * 敌方战术法术节奏（独立于波次与敌方牌组；共用敌方银币组件）。
 * 波次是"增援预算"，法术是"法术经济"，两套来源互不扣费。
 */
USTRUCT(BlueprintType)
struct FLKEnemySpellSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEnabled = false;
	/** 敌方专属区域法术卡（CardId 驱动统一权限/落点/扣费校验） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SpellId = "Spell_SkeletonCircle";
	/** 开战多久之后才允许第一次施法（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float FirstCastTime = 12.f;
	/** 密度评估间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float CheckInterval = 0.5f;
	/** 成功施法后的释放冷却（秒）；按遭遇强度取 20/17/14 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float Cooldown = 20.f;
	/** 同时存在的法阵数量上限（1 = 不叠场） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "4")) int32 MaxActive = 1;
	/** 优先覆盖的玩家战斗单位数量；达不到时允许只覆盖单英雄 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "10")) int32 PreferredTargets = 2;
	/** 超过该时间不再新增法阵（秒）；已存在的法阵正常走完 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0")) float StopTime = 180.f;
};

/** 骷髅法阵参数（敌方专属区域法术）；由卡牌定义携带，运行时只读。 */
USTRUCT(BlueprintType)
struct FLKSkeletonCircleParams
{
	GENERATED_BODY()

	/** 预警时长（秒）：预警不计入持续 3 秒，期间可躲避 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float WarnSeconds = 0.75f;
	/** 生效持续（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float Duration = 3.f;
	/** 影响半径（UE 单位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "10.0")) float Radius = 320.f;
	/** 区域内移动速度倍率（<1 为减速） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", ClampMax = "1.0")) float MoveMultiplier = 0.65f;
	/** 区域内攻击速度倍率（<1 为变慢）；等价于攻击间隔 ÷ 该值 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", ClampMax = "1.0")) float AttackSpeedMultiplier = 0.70f;
	/** 召唤尝试间隔（秒）；从施法完成后开始，不在 t=0 多召 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1")) float SummonInterval = 0.5f;
	/** 单次法阵最多召唤数量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "12")) int32 MaxSummons = 6;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SummonUnitId = "Unit_Skeleton";
	/** 单个召唤点的最大尝试次数（避开实体与边界） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "64")) int32 PlacementAttempts = 12;
};

/**
 * DT_Encounters 行。整行在远征开始时复制到 RunState，再复制到每房 BattleContext；
 * 后续修改 DataTable 或共享 DA_GameData 不会改变已经开始的远征。
 */
USTRUCT(BlueprintType)
struct FLKEncounterRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EncounterId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ELKEncounterRank Rank = ELKEncounterRank::Normal;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 RewardTier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> EnemyHeroIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FLKWaveEntry> Waves;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bEnemyUsesCards = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> EnemyCards;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float EnemySilverPerSecond = 1.f / 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0")) float EnemySilverCap = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float EnemyStartingSilver = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLKEncounterAISettings AI;
	/** 敌方战术法术（独立通道；不与波次共用预算） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLKEnemySpellSettings EnemySpell;

	// ---------- 平衡快照（由远征生成写入；战斗只消费，不重算） ----------
	/** 0 = 未写入快照（表行/旧档）；非 0 必须等于 LKBalanceRules::CurrentBalanceVersion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 BalanceVersion = 0;
	/** 地理拓扑深度 d = 5 × 区域深度 + 层号；最大 19 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "19")) int32 Depth = 0;
	/** 通用敌方生命倍率（深度曲线） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float EnemyHealthScale = 1.f;
	/** 通用敌方攻击倍率（深度曲线） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0")) float EnemyDamageScale = 1.f;
	/** 与 EnemyHeroIds 对齐的英雄生命倍率（遭遇预算 × 深度曲线），最终实例直接用此值 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<float> EnemyHeroHealthScale;
};
