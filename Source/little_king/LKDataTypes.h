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

/** DT_Units 行：单位属性表 */
USTRUCT(BlueprintType)
struct FLKUnitRow : public FTableRowBase
{
	GENERATED_BODY()

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELKAttackType AttackType = ELKAttackType::Melee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ProjectileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELKUnitClass UnitClass = ELKUnitClass::Soldier;

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
